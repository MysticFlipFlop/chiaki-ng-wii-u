// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

// ---------------------------------------------------------------------------
// Teclado USB (gui-105): estado de teclas por nsyshid y mapeo a
// ChiakiControllerState. El servicio HID del sistema entrega attach/
// detach y reportes por callback; el resto (UI, keymap persistido) vive
// en main.cpp. Ver keyboard.cpp para el detalle del driver.
// ---------------------------------------------------------------------------

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <chiaki/controller.h>

#include "config.h"

// Arranca el cliente HID del sistema: detecta los teclados ya
// enchufados al arrancar y los hotplug. false si el servicio no abre.
bool keyboard_init(void);

// Dispositivos boot-keyboard conectados ahora mismo.
int keyboard_present(void);

// Latido de presencia (~2 s): sondeo SET_IDLE contra el
// dispositivo (ver kb_probe_cb en keyboard.cpp).
void keyboard_tick(void);

// Descola un evento crudo del driver (attach/detach/fallos
// de lectura o sondeo) para que main lo loguee; false si
// no queda ninguno pendiente.
bool keyboard_take_event(char *buf, size_t buf_size);

// "vvvv:pppp" del primer teclado detectado (para el log).
const char *keyboard_desc(void);

// Primera tecla mantenida (usage 0x04..0xff) o false si no hay ninguna.
bool keyboard_any_held(uint8_t *out);

// Si la tecla esta mantenida ahora mismo.
bool keyboard_key_held(uint8_t key);

// Nombre legible de un usage HID (tabla estatica: solo hilo principal).
const char *keyboard_key_name(uint8_t key);

// Aplica el keymap (target -> tecla, ver CONFIG_KEYMAP_TARGETS en
// config.h) sobre el estado de sesion: OR de botones, L2/R2 a tope y
// sticks a deflexion completa por encima de lo que traiga el GamePad.
void keyboard_apply(ChiakiControllerState *state, const uint8_t *keymap);
