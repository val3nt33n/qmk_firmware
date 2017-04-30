#include "ch.h"
#include "keycode.h"
#include "keymap.h"
#include "hal.h"
#include "print.h"
#include "suspend.h"
#include "usb_main.h"

#include "infinity60_led.h"

#define IS31_ADDR		0x74

#define IS31_REG_CMD		0xFD
#define IS31_REG_SHUTDOWN	0x0A
#define IS31_REG_FUNC		0x0B

#define IS31_REG_PICTDISP	0x01
#define IS31_REG_DISPOPT	0x05

#define IS31_LED_OFFSET		0x00
#define IS31_BLINK_OFFSET	0x12
#define IS31_PMW_OFFSET		0x24

#define IS31_PAGE_SIZE		0xB4
#define IS31_PAGE_COUNT		8
#define IS31_TIMEOUT		10000

#define LED_MAILBOX_NUM_MSGS	5
#define LED_BUFFERS		2

#define LED_MSG(cmd, arg) (cmd << 8 | arg)

enum {
	LED_ENABLE,
	LED_DISABLE,
	LED_LOCK,
	LED_UNLOCK,
	LED_SET_LAYER,
	LED_SET_BACKLIGHT,
};

static msg_t led_mailbox_queue[LED_MAILBOX_NUM_MSGS];
static mailbox_t led_mailbox;

static THD_WORKING_AREA(led_thread_stack, 1024);

static uint8_t is31_buffer[IS31_PAGE_SIZE];

static const I2CConfig i2ccfg = {
	400000 /* clock speed (Hz); 400kHz max for IS31 */
};

static const uint8_t is31_ic60_leds[0x12] = {
	/* CA, CB */
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0xFF, 0x00,
	0x7F, 0x00,
	0x00, 0x00,
};

static uint8_t led_buffer[LED_BUFFERS][64];
static uint8_t led_framebuffer[64] = { 0 };
static uint8_t led_blink_mask[0x12] = { 0 };

static msg_t is31_select_page(uint8_t page)
{
	uint8_t tx[2] = { IS31_REG_CMD, page };

	return i2cMasterTransmitTimeout(&I2CD1, IS31_ADDR, tx,
					2, NULL, 0,
					US2ST(IS31_TIMEOUT));
}

static msg_t is31_set_register(uint8_t page, uint8_t reg, uint8_t value,
			       uint8_t size)
{
	is31_buffer[0] = reg;
	__builtin_memset(is31_buffer + 1, value, size);

	is31_select_page(page);

	return i2cMasterTransmitTimeout(&I2CD1, IS31_ADDR, is31_buffer,
					size + 1, NULL, 0,
					US2ST(IS31_TIMEOUT));
}

static msg_t is31_write_data(uint8_t page, uint8_t reg, const uint8_t *buffer,
			     uint8_t size)
{
	is31_buffer[0] = reg;
	__builtin_memcpy(is31_buffer + 1, buffer, size);

	is31_select_page(page);

	return i2cMasterTransmitTimeout(&I2CD1, IS31_ADDR, is31_buffer,
					size + 1, NULL, 0,
					US2ST(IS31_TIMEOUT));
}

static msg_t is31_read_register(uint8_t page, uint8_t reg, uint8_t *result)
{
	is31_select_page(page);

	return i2cMasterTransmitTimeout(&I2CD1, IS31_ADDR, &reg,
					1, result, 1,
					US2ST(IS31_TIMEOUT));
}

static msg_t is31_write_register(uint8_t page, uint8_t reg, uint8_t value)
{
	uint8_t tx[2] = { reg, value };

	is31_select_page(page);

	return i2cMasterTransmitTimeout(&I2CD1, IS31_ADDR, tx,
					2, NULL, 0,
					US2ST(IS31_TIMEOUT));
}

static void i2c_init(void)
{
	i2cStart(&I2CD1, &i2ccfg);

	I2CD1.i2c->C2 |= I2Cx_C2_HDRS;
	I2CD1.i2c->FLT = 4;

	chThdSleepMilliseconds(10);
}

static void is31_init(void)
{
	is31_set_register(IS31_REG_FUNC, 0, 0, 0xd);

	/* Disable HW shutdown. */
	palSetPadMode(GPIOB, 16, PAL_MODE_OUTPUT_PUSHPULL);
	palSetPad(GPIOB, 16);
	chThdSleepMilliseconds(10);

	/* Shutdown */
	is31_write_register(IS31_REG_FUNC, IS31_REG_SHUTDOWN, 0);
	chThdSleepMilliseconds(10);

	is31_set_register(IS31_REG_FUNC, 0, 0, 0xd);
	chThdSleepMilliseconds(10);

	/* Turn on */
	is31_write_register(IS31_REG_FUNC, IS31_REG_SHUTDOWN, 1);
	chThdSleepMilliseconds(10);
}

static void led_buffer_reset(uint8_t buffer, uint8_t value)
{
	__builtin_memset(led_buffer[buffer], value,
			 ARRAY_SIZE(led_buffer[buffer]));
}

static void led_buffer_set(uint8_t buffer, uint8_t led, uint8_t level)
{
	if (led >= ARRAY_SIZE(led_buffer[buffer])) {
		return;
	}

	led_buffer[buffer][led] = level;
}

static void led_buffer_add_layer(uint8_t buffer, uint8_t layer, uint8_t level)
{
	uint8_t i = 0, j = 0, led = 0;

	for (j = 0; j < MATRIX_COLS; j++) {
		for (i = 0; i < MATRIX_ROWS; i++) {
			if (keymaps[layer][i][j] != KC_NO &&
			    keymaps[layer][i][j] != KC_TRNS) {
				led_buffer[buffer][led] = level;
			}

			led++;
		}
	}
}

static void led_blink_enable(uint8_t led)
{
	uint8_t idx, val;

	idx = (led / 8) * 0x2;
	val = 1 << (led % 8);

	led_blink_mask[idx] |= val;
}

static void led_blink_disable(uint8_t led)
{
	uint8_t idx, val;

	idx = (led / 8) * 0x2;
	val = 1 << (led % 8);

	led_blink_mask[idx] &= ~val;
}

static void led_framebuffer_clear(void)
{
	__builtin_memset(led_framebuffer, 0, ARRAY_SIZE(led_framebuffer));
}

static void led_framebuffer_blend(uint8_t buffer, uint8_t brightness)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(led_framebuffer); i++) {
		led_framebuffer[i] |= (led_buffer[buffer][i] * brightness) / 100;
	}
}

static void led_display_update(void)
{
	uint8_t i, page;

	is31_read_register(IS31_REG_FUNC, IS31_REG_PICTDISP, &page);

	for (i = 0; i < 8; i++) {
		is31_write_data(1 - page, IS31_PMW_OFFSET + i * 0x10,
				led_framebuffer + i * 8, 8);
	}
	is31_write_data(1 - page, IS31_BLINK_OFFSET, led_blink_mask,
			ARRAY_SIZE(led_blink_mask));

	is31_write_register(IS31_REG_FUNC, IS31_REG_PICTDISP, 1 - page);
}

static THD_FUNCTION(led_thread, unused)
{
	uint8_t cmd, arg, brightness = 100;
	msg_t msg;

	chRegSetThreadName("LED_thread");

	/* Enable blinking */
	is31_write_register(IS31_REG_FUNC, IS31_REG_DISPOPT, 1 << 3 | 5);

	while (1) {
		chMBFetch(&led_mailbox, &msg, TIME_INFINITE);

		cmd = msg >> 8;
		arg = msg & 0xff;

		switch (cmd) {
		case LED_ENABLE:
			led_buffer_set(0, arg, 255);
			break;

		case LED_DISABLE:
			led_buffer_set(0, arg, 0);
			break;

		case LED_LOCK:
			led_buffer_set(1, arg, 255);
			led_blink_enable(arg);
			break;

		case LED_UNLOCK:
			led_buffer_set(1, arg, 0);
			led_blink_disable(arg);
			break;

		case LED_SET_LAYER:
			led_buffer_reset(0, 0);
			led_buffer_add_layer(0, arg, 255);
			break;

		case LED_SET_BACKLIGHT:
			brightness = (arg * 100) / 255;
			break;

		default:
			continue;
		}

		led_framebuffer_clear();
		led_framebuffer_blend(0, brightness);
		led_framebuffer_blend(1, 100);

		led_display_update();
	}
}

void led_set_kb(uint8_t usb_led)
{
	if (host_keyboard_leds() != usb_led) {
		return;
	}

	led_set_user(usb_led);
}

void led_controller_init(void)
{
	int i;

	palSetPadMode(GPIOB, 0, PAL_MODE_ALTERNATIVE_2);
	palSetPadMode(GPIOB, 1, PAL_MODE_ALTERNATIVE_2);

	i2c_init();
	is31_init();

	for (i = 0; i < IS31_PAGE_COUNT; i++) {
		/* Enable LEDs. */
		is31_write_data(i, IS31_LED_OFFSET, is31_ic60_leds,
				ARRAY_SIZE(is31_ic60_leds));
	}

	/* Start thread to handle LED changes. */
	chMBObjectInit(&led_mailbox, led_mailbox_queue, LED_MAILBOX_NUM_MSGS);
	chThdCreateStatic(led_thread_stack, sizeof(led_thread_stack),
			  LOWPRIO, led_thread, NULL);

	/* Turn on LEDs for layer 0. */
	led_set_layer(0);
}

void led_enable(uint8_t led)
{
	chMBPost(&led_mailbox, LED_MSG(LED_ENABLE, led), TIME_IMMEDIATE);
}

void led_disable(uint8_t led)
{
	chMBPost(&led_mailbox, LED_MSG(LED_DISABLE, led), TIME_IMMEDIATE);
}

void led_lock(uint8_t led)
{
	chMBPost(&led_mailbox, LED_MSG(LED_LOCK, led), TIME_IMMEDIATE);
}

void led_unlock(uint8_t led)
{
	chMBPost(&led_mailbox, LED_MSG(LED_UNLOCK, led), TIME_IMMEDIATE);
}

void led_set_layer(uint8_t layer)
{
	chMBPost(&led_mailbox, LED_MSG(LED_SET_LAYER, layer), TIME_IMMEDIATE);
}

void led_set_backlight(uint8_t level)
{
	chMBPost(&led_mailbox, LED_MSG(LED_SET_BACKLIGHT, level), TIME_IMMEDIATE);
}
