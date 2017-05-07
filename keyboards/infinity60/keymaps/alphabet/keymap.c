#include "infinity60.h"
#include "infinity60_led.h"
#include "led.h"

#define ______ KC_TRNS
#define GRAVE_MODS (MOD_BIT(KC_LSHIFT) | MOD_BIT(KC_RSHIFT))

static uint8_t backlight_levels[BACKLIGHT_LEVELS + 1] = {
	0, 25, 50, 75, 100, 125, 150, 175, 200, 225, 255,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Layer 0: Default Layer
     * ,-----------------------------------------------------------.
     * |Esc|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|   Bksp|
     * |-----------------------------------------------------------|
     * |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|  \  |
     * |-----------------------------------------------------------|
     * |Contro|  A|  S|  D|  F|  G|  H|  J|  K|  L|  ;|  '|   Enter|
     * |-----------------------------------------------------------|
     * |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|     Shift|
     * |-----------------------------------------------------------'
     * |LCTL|Fn1 |LAlt|         Space/Fn1       |RAlt|Fn1|Fn2 |RCTL|
     * `-----------------------------------------------------------'
     */
    [0] = KEYMAP(
        F(0),    KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINS, KC_EQL,  KC_BSPC,  KC_NO,    \
        KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC, KC_BSLS,            \
        KC_LCTL, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT, KC_ENT,                      \
        KC_LSPO, KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH, KC_RSPC, KC_NO,                       \
        KC_LCTL, MO(2),   KC_LALT,               LT(2, KC_SPC),                 KC_RALT, MO(2),   MO(3),   KC_RCTL),

    /* Layer 1: Gaming layer
     */
    [1] = KEYMAP(
        ______,  KC_1,    ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,    KC_NO,   \
        ______,  ______,  KC_W,    ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,             \
        ______,  KC_A,    KC_S,    KC_D,    ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,                      \
        KC_LSFT, ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  KC_NO,                       \
        ______,  ______,  ______,                    KC_SPC,                    ______,  ______,  ______,  ______),

    /* Layer 2: FN layer
     */
    [2] = KEYMAP(
        KC_GRV,  KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,  KC_DEL,   KC_NO,    \
        KC_CAPS, ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,             \
        ______,  ______,  ______,  ______,  ______,  ______,  KC_LEFT, KC_DOWN, KC_UP,   KC_RGHT, ______,  ______,  ______,                      \
        ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  KC_NO,                       \
        ______,  ______,  ______,                    ______,                    ______,  ______,  ______,  ______),

    /* Layer 3:
     */
    [3] = KEYMAP(
        ______,  TG(1),   ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  RESET,    KC_NO,    \
        ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,             \
        ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  F(1),    ______,  ______,  ______,                      \
        ______,  ______,  ______,  ______,  ______,  ______,  ______,  ______,  F(2),    F(3),    ______,  ______,  KC_NO,                       \
        ______,  ______,  ______,                    ______,                    ______,  ______,  ______,  ______),
};

void action_function(keyrecord_t *record, uint8_t id, uint8_t opt)
{
	static uint8_t mods_pressed;
	static bool mod_flag;

	switch (id) {
	case 0:
		mods_pressed = get_mods() & GRAVE_MODS;

		if (record->event.pressed) {
			if (mods_pressed) {
				mod_flag = true;
				add_key(KC_GRV);
				send_keyboard_report();
			} else {
				add_key(KC_ESC);
				send_keyboard_report();
			}
		} else {
			if (mod_flag) {
				mod_flag = false;
				del_key(KC_GRV);
				send_keyboard_report();
			} else {
				del_key(KC_ESC);
				send_keyboard_report();
			}
		}
		break;

	case 1:
		if (record->event.pressed) {
			backlight_toggle();
		}
		break;

	case 2:
		if (record->event.pressed) {
			backlight_decrease();
		}
		break;

	case 3:
		if (record->event.pressed) {
			backlight_increase();
		}
		break;
	}
}

const uint16_t PROGMEM fn_actions[] = {
	[0] = ACTION_FUNCTION(0),
	[1] = ACTION_FUNCTION(1),
	[2] = ACTION_FUNCTION(2),
	[3] = ACTION_FUNCTION(3),
};

uint8_t key_get_led_pos(uint16_t key)
{
	int8_t i, j, k;

	for (i = 0; i < ARRAY_SIZE(keymaps); i++) {
		for (j = 0; j < MATRIX_ROWS; j++) {
			for (k = 0; k < MATRIX_COLS; k++) {
				if (keymaps[i][j][k] == key) {
					return k * MATRIX_ROWS + j;
				}
			}
		}
	}

	return 63;
}

void backlight_set(uint8_t level)
{
	if (level >= ARRAY_SIZE(backlight_levels)) {
		backlight_level(ARRAY_SIZE(backlight_levels) - 1);
		return;
	}

	led_set_backlight(backlight_levels[level]);
}

void led_set_user(uint8_t usb_led)
{
	if (usb_led & (1 << USB_LED_CAPS_LOCK)) {
		led_lock(key_get_led_pos(KC_CAPS));
	} else {
		led_unlock(key_get_led_pos(KC_CAPS));
	}
}

/* Runs just one time when the keyboard initializes. */
void matrix_init_user(void)
{
	led_controller_init();
}

/* Runs constantly in the background, in a loop. */
void matrix_scan_user(void)
{
	static uint8_t prev_layer = 0;
	uint8_t layer;

	layer = layer_state ? __builtin_ffs(layer_state) - 1 : 0;

	if (prev_layer != layer) {
		led_set_layer(layer);
	}

	prev_layer = layer;
};
