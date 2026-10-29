// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

// ---------------------------------------------------------------------------
// Persistencia de consolas registradas (fase 4a).
//
// Fichero: sd:/wiiu/apps/chiaki/config.ini (lineas "clave=valor"):
//
//   account_id=<16 hex>                       cuenta PSN global
//   last_host=<ip>                            ultima consola conectada
//   log_enabled=<0|1>                        log.txt en la SD (default 0)
//   vibration_enabled=<0|1>                  vibracion del GamePad (default 1)

//   mando_mode=<0|1|2>                       modo de sesion: 0 normal, 1 solo mando, 2 sin video (default 0)
//   keymap=<tt:kk,...>                     mapeo teclado USB (gui-105)
//   lang=<id>                              idioma UI: vacio=auto
//                                          (eng si hay eng.pak, si no
//                                          esp) (gui-107)
//   console=<host>|<nombre>|<regist hex>|<morning hex>|<accountid hex>|<tipo>
//                                        tipo opcional (gui-110): ps4/ps5;
//                                        vacio = desconocido (se autocompleta
//                                        con el regist o el discovery)
//
// Una linea "console=" por consola guardada. El modulo solo serializa/
// parsea (sin FS): las entradas las hace main.cpp por el mismo cliente FS
// que el log. Gracias a esto el registro (PIN) se hace UNA vez por consola
// y sobrevive a reinicios y reconstrucciones del binario.
// ---------------------------------------------------------------------------
#pragma once

#include <stddef.h>
#include <stdint.h>

#define CONFIG_REGIST_KEY_SIZE 0x10
#define CONFIG_ACCOUNT_ID_SIZE 8
#define CONFIG_NAME_SIZE 0x20
#define CONFIG_HOST_SIZE 48
#define CONFIG_MAX_CONSOLES 8
// Tipo de consola en config.ini (gui-110): se rellena al registrar y con
// cada respuesta del discovery; UNKNOWN = linea vieja de 5 campos (se
// autocompleta sola). Decide el arte del carrusel (PS4/PS5).
#define CONFIG_CONSOLE_TYPE_UNKNOWN 0
#define CONFIG_CONSOLE_TYPE_PS4 1
#define CONFIG_CONSOLE_TYPE_PS5 2
// Objetivos de mapeo del teclado USB (gui-105): 16 botones digitales
// (orden = bits de ChiakiControllerButton) + L2/R2 + 4+4 direcciones
// de stick a deflexion completa. El keymap guarda target -> tecla HID
// (0 = sin asignar).
#define CONFIG_KEYMAP_TARGETS 26

struct ConfigConsole
{
	bool used;
	char host[CONFIG_HOST_SIZE];
	char name[CONFIG_NAME_SIZE + 1];
	uint8_t regist_key[CONFIG_REGIST_KEY_SIZE];
	uint8_t morning[CONFIG_REGIST_KEY_SIZE];
	uint8_t account_id[CONFIG_ACCOUNT_ID_SIZE];
	// Tipo PS4/PS5 (gui-110, CONFIG_CONSOLE_TYPE_*): dibuja el arte
	// correcto del carrusel aunque la consola este apagada.
	uint8_t type;
};

struct Config
{
	// Cuenta PSN global en hex (para registrar consolas nuevas sin
	// volver a teclearla).
	char account_id_hex[CONFIG_ACCOUNT_ID_SIZE * 2 + 1];
	// Ultima consola conectada: al arrancar se preselecciona su IP,
	// claves y cuenta para conectar con A sin registrar de nuevo.
	char last_host[CONFIG_HOST_SIZE];
	// Perfil de calidad de video (fase 4c): indice de preset, ver
	// g_video_presets en main.cpp. 0 = 720p60 5 Mbps (validado en
	// hardware). Ojo: los indices cambiaron en gui-45 (antes 0 era
	// 10 Mbps) — un config viejo caera en otro preset; re-elegir
	// con ZL/ZR lo corrige.
	int video_preset;
	// VSync (gui-59): 1 = activado (default; GX2SetSwapInterval(1),
	// presentacion bloqueada al retrace, sin tearing); 0 =
	// desactivado (swap inmediato: quita la espera de vsync de la
	// latencia, puede enseñar tearing). Se aplica al arrancar y al
	// alternar la fila de Ajustes.
	int vsync;
	// Volumen de captura del micrófono en % (gui-67):0..200, default
	// 100. Se ajusta con el slider que sale al mantener presionado el
	// icono del micrófono durante la sesión.
	int mic_gain_pct;
	// Generacion del log.txt en la SD (gui-82): 0 = desactivado
	// (default; al arrancar el log.txt existente se elimina), 1 =
	// activado (append). Se alterna en Ajustes (fila Log.txt) y se
	// persiste en config.ini.
	int log_enabled;
	// Vibracion del GamePad (gui-93): 1 = activada (default; los
	// eventos CHIAKI_EVENT_RUMBLE de la sesion mueven el motor con
	// VPADControlMotor y el bucle principal renueva la ventana);
	// 0 = desactivada (motor parado al instante). Se alterna en
	// Ajustes (fila Vibracion) y se persiste en config.ini.
	int vibration_enabled;
	// Modo de sesión (gui-102/103): 1 = sin audio ni vídeo (mando
	// puro: la consola no manda stream y el GamePad es solo
	// mando); 2 = sin vídeo (solo audio + mando); 0 = normal
	// (default). Táctil, micrófono y vibración siguen en todos.
	// Se elige en Ajustes (fila Modo), persiste en config.ini y
	// aplica en la próxima sesión.
	int mando_mode;
	// keymap del teclado USB (gui-105): tecla HID asignada a cada
	// target (0 = sin asignar; ver CONFIG_KEYMAP_TARGETS). Solo se
	// usa con teclado conectado, pero los mapeos persisten siempre.
	uint8_t keymap_keys[CONFIG_KEYMAP_TARGETS];
	// Idioma de la UI (gui-107): id corto del pak ("eng") o vacio =
	// auto (ingles si hay eng.pak en la SD, si no espanol). Se
	// resuelve al arrancar y se persiste al cambiarlo en Ajustes
	// (fila Idioma). Solo afecta a textos visibles; el log sigue
	// en espanol.
	char lang[8];
	ConfigConsole consoles[CONFIG_MAX_CONSOLES];
};

// Vacia todo (sin cuenta, sin consolas).
void config_init(Config *cfg);

// Serializa a "clave=valor\n"; devuelve los bytes utiles (0 = no cabe).
size_t config_serialize(const Config *cfg, char *buf, size_t buf_size);

// Parsea sobre cfg (sin limpiar lo que no aparezca). len = bytes validos.
void config_parse(Config *cfg, const char *buf, size_t len);

// Indice de la consola con esa IP, o -1.
int config_find(const Config *cfg, const char *host);

// Anade o reemplaza (por host) una consola. Devuelve su indice o -1 si
// esta llena. '|' en nombre/host se sanitiza para no romper el formato.
int config_set_console(Config *cfg, const char *host, const char *name,
		       const uint8_t regist_key[CONFIG_REGIST_KEY_SIZE],
		       const uint8_t morning[CONFIG_REGIST_KEY_SIZE],
		       const uint8_t account_id[CONFIG_ACCOUNT_ID_SIZE],
		       uint8_t type);

// Elimina la consola idx y COMPACTA el resto (los indices del menu son
// slots consecutivos sin huecos). Si last_host apuntaba a esa consola,
// se limpia para no preseleccionar un fantasma. false si idx no vale.
bool config_del_console(Config *cfg, int idx);

int config_console_count(const Config *cfg);
