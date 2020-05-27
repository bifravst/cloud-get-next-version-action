/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <logging/log.h>

#include "ui.h"
#include "led_pwm.h"

LOG_MODULE_REGISTER(ui, CONFIG_UI_LOG_LEVEL);

static enum ui_led_pattern current_led_state;

#if !defined(CONFIG_UI_LED_USE_PWM)
static struct k_delayed_work leds_update_work;

/**@brief Update LEDs state. */
static void leds_update(struct k_work *work)
{
	static bool led_on;
	static bool passive_mode;
	static u8_t current_led_on_mask;
	u8_t led_on_mask;

	if (current_led_state == UI_LED_PASSIVE_MODE) {
		passive_mode = true;
	} else {
		passive_mode = false;
	}

	led_on_mask = UI_LED_GET_ON(current_led_state);
	led_on = !led_on;

	if (led_on) {
		led_on_mask |= UI_LED_GET_BLINK(current_led_state);
	} else {
		led_on_mask &= ~UI_LED_GET_BLINK(current_led_state);
	}

	if (led_on_mask != current_led_on_mask) {
#if defined(CONFIG_DK_LIBRARY)
		dk_set_leds(led_on_mask);
#endif
		current_led_on_mask = led_on_mask;
	}

	if (work) {
		if (led_on) {
			k_delayed_work_submit(&leds_update_work,
					      UI_LED_ON_PERIOD_NORMAL);
		} else {
			if (passive_mode) {
				k_delayed_work_submit(&leds_update_work,
						      UI_LED_OFF_PERIOD_LONG);
			} else {
				k_delayed_work_submit(&leds_update_work,
						      UI_LED_OFF_PERIOD_NORMAL);
			}
		}
	}
}
#endif /* CONFIG_UI_LED_USE_PWM */

void ui_led_set_pattern(enum ui_led_pattern state)
{
	current_led_state = state;
#ifdef CONFIG_UI_LED_USE_PWM
	ui_led_set_effect(state);
#endif /* CONFIG_UI_LED_USE_PWM */
}

enum ui_led_pattern ui_led_get_pattern(void)
{
	return current_led_state;
}

int ui_init(void)
{
	int err = 0;

#ifdef CONFIG_UI_LED_USE_PWM
	err = ui_leds_init();
	if (err) {
		LOG_ERR("Error when initializing PWM controlled LEDs");
		return err;
	}
#else
	err = dk_leds_init();
	if (err) {
		LOG_ERR("Could not initialize leds, err code: %d\n", err);
		return err;
	}

	err = dk_set_leds_state(0x00, DK_ALL_LEDS_MSK);
	if (err) {
		LOG_ERR("Could not set leds state, err code: %d\n", err);
		return err;
	}

	k_delayed_work_init(&leds_update_work, leds_update);
	k_delayed_work_submit(&leds_update_work, K_NO_WAIT);
#endif /* CONFIG_UI_LED_USE_PWM */

	return 0;
}

void ui_stop_leds(void)
{
#ifdef CONFIG_UI_LED_USE_PWM
	ui_leds_stop();
#endif
}
