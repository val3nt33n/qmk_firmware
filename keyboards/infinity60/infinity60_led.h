#ifndef INFINITY60_LED_H
#define INFINITY60_LED_H

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

void led_set_user(uint8_t usb_led);

void led_controller_init(void);
void led_enable(uint8_t led);
void led_disable(uint8_t led);
void led_lock(uint8_t led);
void led_unlock(uint8_t led);
void led_set_layer(uint8_t layer);
void led_set_backlight(uint8_t level);

#endif /* INFINITY60_LED_H */
