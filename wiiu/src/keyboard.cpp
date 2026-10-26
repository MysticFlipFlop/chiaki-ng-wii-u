// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

// ---------------------------------------------------------------------------
// Teclado USB (gui-105): cliente HID del sistema (nsyshid).
//
// HIDSetup + HIDAddClient registran un cliente que recibe attach y
// detach de los dispositivos HID de la consola (los ya enchufados
// llegan al registrar el cliente — es lo que permite la tarjeta de
// "Teclado conectado" al arrancar). Solo se aceptan interfaces boot-
// keyboard (protocolo 1); los ratones (protocolo 2) y los dispositivos
// sin protocolo quedan fuera.
//
// El reporte boot de 8 bytes [modificadores][reservado][6 teclas] se
// traduce a un bitmap de teclas mantenidas por ranura y a su union: la
// consulta es por NIVEL (sin key-repeat), que es lo que consume tanto
// el modal de mapeo como keyboard_apply. Los callbacks del SO corren en
// su propio hilo: todo el estado va bajo el mismo mutex.
//
// Riesgo de hardware conocido: si el servicio HID del OS no acepta
// clientes homebrew, keyboard_init() devuelve false, la fila de
// Ajustes no aparece nunca y el log lo dice en el arranque.
// ---------------------------------------------------------------------------

#include "keyboard.h"

#include <nsyshid/hid.h>

#include <chiaki/thread.h>

#include <coreinit/time.h> // OSGetTime/OSMillisecondsToTicks
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define KB_SLOTS 2

// Fallos consecutivos (lectura o sondeo) = se retira el
// teclado; un error aislado se re-arma y se perdona.
#define KB_ERR_LIMIT 2

// Periodo del sondeo de presencia con SET_IDLE.
#define KB_PROBE_MS 2000

struct KbSlot
{
	bool used;
	uint32_t handle;
	uint16_t vid;
	uint16_t pid;
	uint8_t iface;    // interfaceIndex para los sondeos
	uint8_t rbuf[8];  // buffer de lectura activa (estatica: vive aqui)
	bool held[256];
	int errs;         // fallos consecutivos (lectura/sondeo)
	bool probe_busy;  // sondeo en vuelo (no solapar)
	uint64_t last_probe;
};

static HIDClient g_kb_client;
static KbSlot g_kb_slots[KB_SLOTS];
static bool g_kb_held[256]; // union de las ranuras (bajo g_kb_mu)
static int g_kb_count;
static char g_kb_desc[32];
static ChiakiMutex g_kb_mu;

// Cola de eventos crudos del driver (attach/detach/fallos)
// para que main los loguee aunque no cambie el recuento:
// ring de 4 bajo el mismo mutex; si se llena, el nuevo se
// descarta (los sucesos importantes se repiten por ciclo).
#define KB_EV_MAX 4
#define KB_EV_LEN 72
static char g_kb_ev[KB_EV_MAX][KB_EV_LEN];
static int g_kb_ev_head;
static int g_kb_ev_count;

static void kb_note_locked(const char *fmt, ...)
{
	if(g_kb_ev_count >= KB_EV_MAX)
		return;
	char *dst = g_kb_ev[(g_kb_ev_head + g_kb_ev_count) % KB_EV_MAX];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(dst, KB_EV_LEN, fmt, ap);
	va_end(ap);
	g_kb_ev_count++;
}

// Reconstruye la union desde las ranuras. Llamar con el mutex tomado.
static void kb_rebuild_union_locked(void)
{
	memset(g_kb_held, 0, sizeof(g_kb_held));
	for(int s = 0; s < KB_SLOTS; s++)
	{
		if(!g_kb_slots[s].used)
			continue;
		for(int k = 0; k < 256; k++)
			if(g_kb_slots[s].held[k])
				g_kb_held[k] = true;
	}
}

// Libera una ranura y mantiene recuento/union al dia.
// Llamar con el mutex tomado.
static void kb_drop_slot_locked(int s)
{
	KbSlot *sl = &g_kb_slots[s];
	sl->used = false;
	sl->errs = 0;
	sl->probe_busy = false;
	memset(sl->held, 0, sizeof(sl->held));
	if(g_kb_count > 0)
		g_kb_count--;
	kb_rebuild_union_locked();
}

static void kb_read_cb(uint32_t handle, int32_t error, uint8_t *buf,
		       uint32_t n, void *userContext);

// Attach/detach de un dispositivo HID. El attach arma la primera
// lectura; cada completado rearma la siguiente (lectura continua).
static int32_t kb_attach_cb(HIDClient *client, HIDDevice *device,
			    HIDAttachEvent attach)
{
	(void)client;

	if(attach == HID_DEVICE_DETACH)
	{
		chiaki_mutex_lock(&g_kb_mu);
		int s = -1;
		for(int i = 0; i < KB_SLOTS; i++)
			if(g_kb_slots[i].used &&
			   g_kb_slots[i].handle == device->handle)
			{
				s = i;
				break;
			}
		if(s < 0)
		{
			// El handle del detach no siempre cuadra con el del
			// attach: empareja por vid:pid (caso tipico de un
			// solo teclado enchufado).
			for(int i = 0; i < KB_SLOTS; i++)
				if(g_kb_slots[i].used &&
				   g_kb_slots[i].vid == device->vid &&
				   g_kb_slots[i].pid == device->pid)
				{
					s = i;
					break;
				}
			if(s >= 0)
				kb_note_locked("detach sin handle; emparejado %04x:%04x",
					       (unsigned)device->vid,
					       (unsigned)device->pid);
			else
				kb_note_locked("detach %04x:%04x sin ranura",
					       (unsigned)device->vid,
					       (unsigned)device->pid);
		}
		if(s >= 0)
			kb_drop_slot_locked(s);
		chiaki_mutex_unlock(&g_kb_mu);
		return 0;
	}

	// Solo interfaces boot-keyboard (protocolo 1).
	if(device->protocol != 1)
		return 0;

	chiaki_mutex_lock(&g_kb_mu);
	int slot = -1;
	for(int s = 0; s < KB_SLOTS; s++)
		if(g_kb_slots[s].used && g_kb_slots[s].handle == device->handle)
			slot = s; // re-attach del mismo handle
	for(int s = 0; s < KB_SLOTS && slot < 0; s++)
		if(!g_kb_slots[s].used)
		{
			slot = s;
			break;
		}
	if(slot < 0)
	{
		chiaki_mutex_unlock(&g_kb_mu);
		return 0;
	}
	KbSlot *sl = &g_kb_slots[slot];
	if(!sl->used)
	{
		sl->used = true;
		memset(sl->held, 0, sizeof(sl->held));
		g_kb_count++;
	}
	sl->handle = device->handle;
	sl->vid = device->vid;
	sl->pid = device->pid;
	sl->iface = device->interfaceIndex;
	sl->errs = 0;
	sl->probe_busy = false;
	sl->last_probe = OSGetTime(); // primer sondeo ~2 s despues
	if(g_kb_count == 1)
		snprintf(g_kb_desc, sizeof(g_kb_desc), "%04x:%04x",
			 device->vid, device->pid);
	kb_rebuild_union_locked();
	chiaki_mutex_unlock(&g_kb_mu);

	HIDRead(sl->handle, sl->rbuf, sizeof(sl->rbuf), kb_read_cb, sl);
	return 0;
}

// Reporte boot: actualiza la ranura y rearma la lectura. Una ranura
// liberada o con handle viejo no rearma (el detach la limpia).
static void kb_read_cb(uint32_t handle, int32_t error, uint8_t *buf,
		       uint32_t n, void *userContext)
{
	KbSlot *sl = (KbSlot *)userContext;
	bool rearm = false;

	chiaki_mutex_lock(&g_kb_mu);
	if(!sl->used || sl->handle != handle)
	{
		// Ranura liberada o handle viejo: sin re-arme.
		chiaki_mutex_unlock(&g_kb_mu);
		return;
	}
	if(error || n < 2)
	{
		// Al retirar el teclado la lectura pendiente falla y el
		// evento detach no siempre llega: cuenta el fallo, rearma
		// y a KB_ERR_LIMIT seguidos retira el dispositivo.
		sl->errs++;
		if(sl->errs >= KB_ERR_LIMIT)
		{
			kb_note_locked("lectura fallo (err %d, %u b); retirado",
				       (int)error, (unsigned int)n);
			kb_drop_slot_locked((int)(sl - g_kb_slots));
		}
		else
			rearm = true;
	}
	else
	{
		bool now[256];
		memset(now, 0, sizeof(now));
		for(int b = 0; b < 8; b++)
			if(buf[0] & (1u << b))
				now[0xe0 + b] = true; // modificadores
		for(uint32_t i = 2; i < n && i < 8; i++)
		{
			uint8_t k = buf[i];
			// 0x00 = sin tecla; 0x01/02 = errores de rollover.
			if(k >= 0x04)
				now[k] = true;
		}
		memcpy(sl->held, now, sizeof(sl->held));
		kb_rebuild_union_locked();
		sl->errs = 0;
		rearm = true;
	}
	chiaki_mutex_unlock(&g_kb_mu);

	if(rearm)
		HIDRead(handle, sl->rbuf, sizeof(sl->rbuf), kb_read_cb, sl);
}

// Latido de presencia (gui-105b): mientras nadie teclea el teclado
// no manda reportes, asi que la lectura pendiente no sirve de latido.
// Un SET_IDLE cada KB_PROBE_MS toca el dispositivo siempre (semantica
// "solo al cambiar": inocua) y su fallo repetido retira el teclado
// aunque no llegue ni detach ni error de lectura.
static void kb_probe_cb(uint32_t handle, int32_t error, uint8_t *buf,
			uint32_t n, void *userContext)
{
	(void)buf;
	(void)n;

	KbSlot *sl = (KbSlot *)userContext;
	chiaki_mutex_lock(&g_kb_mu);
	if(sl->used && sl->handle == handle)
	{
		sl->probe_busy = false;
		if(error)
		{
			sl->errs++;
			if(sl->errs >= KB_ERR_LIMIT)
			{
				kb_note_locked("sondeo fallo (err %d); retirado",
					       (int)error);
				kb_drop_slot_locked((int)(sl - g_kb_slots));
			}
		}
		else
			sl->errs = 0;
	}
	chiaki_mutex_unlock(&g_kb_mu);
}

void keyboard_tick(void)
{
	uint64_t now = OSGetTime();
	uint32_t handles[KB_SLOTS];
	uint8_t ifaces[KB_SLOTS];
	void *ctxs[KB_SLOTS];
	int n = 0;

	// Recolecta con el mutex tomado y lanza las llamadas fuera: si
	// el sistema completara el callback en sincrono, no hay deadlock.
	chiaki_mutex_lock(&g_kb_mu);
	for(int s = 0; s < KB_SLOTS; s++)
	{
		KbSlot *sl = &g_kb_slots[s];
		if(!sl->used || sl->probe_busy)
			continue;
		if(now - sl->last_probe < OSMillisecondsToTicks(KB_PROBE_MS))
			continue;
		sl->probe_busy = true;
		sl->last_probe = now;
		handles[n] = sl->handle;
		ifaces[n] = sl->iface;
		ctxs[n] = sl;
		n++;
	}
	chiaki_mutex_unlock(&g_kb_mu);

	for(int i = 0; i < n; i++)
	{
		int32_t r = HIDSetIdle(handles[i], ifaces[i], 0, 0,
					kb_probe_cb, ctxs[i]);
		if(r)
		{
			// Error inmediato (sin callback): no dejar el
			// latido clavado; el proximo tick reintenta.
			chiaki_mutex_lock(&g_kb_mu);
			KbSlot *sl = (KbSlot *)ctxs[i];
			if(sl->used && sl->probe_busy)
				sl->probe_busy = false;
			chiaki_mutex_unlock(&g_kb_mu);
		}
	}
}

bool keyboard_init(void)
{
	chiaki_mutex_init(&g_kb_mu, false);
	memset(&g_kb_client, 0, sizeof(g_kb_client));
	memset(g_kb_slots, 0, sizeof(g_kb_slots));

	int32_t err = HIDSetup();
	if(err)
		return false;
	err = HIDAddClient(&g_kb_client, kb_attach_cb);
	if(err)
	{
		HIDTeardown();
		return false;
	}
	return true;
}

int keyboard_present(void)
{
	chiaki_mutex_lock(&g_kb_mu);
	int n = g_kb_count;
	chiaki_mutex_unlock(&g_kb_mu);
	return n;
}

// Descola el evento mas antiguo de la cola del driver. false si no
// hay ninguno pendiente.
bool keyboard_take_event(char *buf, size_t buf_size)
{
	if(!buf || !buf_size)
		return false;
	chiaki_mutex_lock(&g_kb_mu);
	if(!g_kb_ev_count)
	{
		chiaki_mutex_unlock(&g_kb_mu);
		return false;
	}
	snprintf(buf, buf_size, "%s", g_kb_ev[g_kb_ev_head]);
	g_kb_ev_head = (g_kb_ev_head + 1) % KB_EV_MAX;
	g_kb_ev_count--;
	chiaki_mutex_unlock(&g_kb_mu);
	return true;
}

const char *keyboard_desc(void)
{
	chiaki_mutex_lock(&g_kb_mu);
	const char *d = g_kb_desc;
	chiaki_mutex_unlock(&g_kb_mu);
	return d; // apunta a estatica fija: seguro tras soltar el mutex
}

bool keyboard_key_held(uint8_t key)
{
	chiaki_mutex_lock(&g_kb_mu);
	bool held = g_kb_held[key];
	chiaki_mutex_unlock(&g_kb_mu);
	return held;
}

bool keyboard_any_held(uint8_t *out)
{
	chiaki_mutex_lock(&g_kb_mu);
	for(int k = 0x04; k < 256; k++)
	{
		if(!g_kb_held[k])
			continue;
		*out = (uint8_t)k;
		chiaki_mutex_unlock(&g_kb_mu);
		return true;
	}
	chiaki_mutex_unlock(&g_kb_mu);
	return false;
}

void keyboard_apply(ChiakiControllerState *state, const uint8_t *keymap)
{
	chiaki_mutex_lock(&g_kb_mu);
	if(!g_kb_count)
	{
		chiaki_mutex_unlock(&g_kb_mu);
		return;
	}

	int lx = 0, ly = 0, rx = 0, ry = 0;
	for(int t = 0; t < CONFIG_KEYMAP_TARGETS; t++)
	{
		uint8_t k = keymap[t];
		if(!k || !g_kb_held[k])
			continue;
		if(t < 16)
			state->buttons |= (1u << t); // mismos bits que ChiakiControllerButton
		else if(t == 16)
			state->l2_state = 0xff;
		else if(t == 17)
			state->r2_state = 0xff;
		else if(t == 18)
			ly = -32767; // stick izq: arriba (chiaki: negativo)
		else if(t == 19)
			ly = 32767;
		else if(t == 20)
			lx = -32767;
		else if(t == 21)
			lx = 32767;
		else if(t == 22)
			ry = -32767;
		else if(t == 23)
			ry = 32767;
		else if(t == 24)
			rx = -32767;
		else if(t == 25)
			rx = 32767;
	}
	// Deflexion completa del teclado manda sobre la del GamePad
	// eje a eje; el eje que el teclado no toca sigue el mando.
	if(lx)
		state->left_x = (int16_t)lx;
	if(ly)
		state->left_y = (int16_t)ly;
	if(rx)
		state->right_x = (int16_t)rx;
	if(ry)
		state->right_y = (int16_t)ry;

	chiaki_mutex_unlock(&g_kb_mu);
}

const char *keyboard_key_name(uint8_t key)
{
	static char buf[24];
	static const char *letters[26] = {
		"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",
		"L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V",
		"W", "X", "Y", "Z",
	};
	static const char *digits[9] = {
		"1", "2", "3", "4", "5", "6", "7", "8", "9",
	};
	static const char *kp_digits[9] = {
		"KP 1", "KP 2", "KP 3", "KP 4", "KP 5",
		"KP 6", "KP 7", "KP 8", "KP 9",
	};

	if(key >= 0x04 && key <= 0x1d)
		return letters[key - 0x04];
	if(key >= 0x1e && key <= 0x26)
		return digits[key - 0x1e];

	switch(key)
	{
		case 0x27: return "0";
		case 0x28: return "Intro";
		case 0x29: return "Esc";
		case 0x2a: return "Retroceso";
		case 0x2b: return "Tab";
		case 0x2c: return "Espacio";
		case 0x2d: return "-_";
		case 0x2e: return "=+";
		case 0x2f: return "[{";
		case 0x30: return "]}";
		case 0x31: return "\\|";
		case 0x32: return ";:";
		case 0x33: return "'\"";
		case 0x34: return "`~";
		case 0x35: return ",<";
		case 0x36: return ".>";
		case 0x37: return "/?";
		case 0x39: return "Bloq Mayús";
		case 0x46: return "ImpPant";
		case 0x47: return "Bloq Despl";
		case 0x48: return "Pausa";
		case 0x49: return "Insert";
		case 0x4a: return "Inicio";
		case 0x4b: return "Re Pág";
		case 0x4c: return "Supr";
		case 0x4d: return "Fin";
		case 0x4e: return "Av Pág";
		case 0x4f: return "Dcha";
		case 0x50: return "Izq";
		case 0x51: return "Abajo";
		case 0x52: return "Arriba";
		case 0x53: return "Bloq Num";
		case 0x54: return "KP /";
		case 0x55: return "KP *";
		case 0x56: return "KP -";
		case 0x57: return "KP +";
		case 0x58: return "KP Intro";
		case 0x62: return "KP 0";
		case 0x63: return "KP ,";
		case 0x65: return "Menú";
		case 0xe0: return "Ctrl izq";
		case 0xe1: return "Mayús izq";
		case 0xe2: return "Alt izq";
		case 0xe3: return "Win izq";
		case 0xe4: return "Ctrl der";
		case 0xe5: return "Mayús der";
		case 0xe6: return "Alt der";
		case 0xe7: return "Win der";
		default: break;
	}
	if(key >= 0x3a && key <= 0x45)
	{
		snprintf(buf, sizeof(buf), "F%d", key - 0x39);
		return buf;
	}
	if(key >= 0x59 && key <= 0x61)
		return kp_digits[key - 0x59];

	snprintf(buf, sizeof(buf), "Tecla 0x%02X", key);
	return buf;
}
