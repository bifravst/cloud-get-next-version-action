/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <dk_buttons_and_leds.h>
#include <zephyr.h>
#include <stdio.h>
#include <uart.h>
#include <string.h>
#include <logging/log.h>
#include <misc/reboot.h>
#include <mqtt_behaviour.h>
#include <modem_stats.h>
#include <device.h>
#include <sensor.h>
#include <gps_controller.h>

#define PUBLISH_INTERVAL	0
#define GPS_DELAYED_TIME	0
#define TRACKER_ID			"CT3001"

static char mqtt_assembly_line_d[100] = "\0";

static struct gps_data gps_data;

K_SEM_DEFINE(my_sem, 0, 1);

struct k_poll_event events[1] = {
    K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_SEM_AVAILABLE,
                                    K_POLL_MODE_NOTIFY_ONLY,
                                    &my_sem, 0)
};

static struct k_work request_battery_status_work;
static struct k_work publish_gps_data_work;
static struct k_work delete_publish_string_and_set_led_work;

static void base_string_set(char *mqtt_assembly_line_d) {
	strcat(mqtt_assembly_line_d, TRACKER_ID);	
	strcat(mqtt_assembly_line_d, ",");
}

static void request_battery_status_work_fn(struct k_work *work)
{
	request_battery_status(mqtt_assembly_line_d);
	strcat(mqtt_assembly_line_d, ",");
}

static void publish_gps_data_work_fn(struct k_work *work)
{
	publish_gps_data(mqtt_assembly_line_d, sizeof(mqtt_assembly_line_d));
}

static void delete_publish_string_and_set_led_work_fn(struct k_work *work)
{
	memset(mqtt_assembly_line_d,0,strlen(mqtt_assembly_line_d));
	base_string_set(mqtt_assembly_line_d);
}

static void work_init() {
	k_work_init(&request_battery_status_work, request_battery_status_work_fn);
	k_work_init(&publish_gps_data_work, publish_gps_data_work_fn);
	k_work_init(&delete_publish_string_and_set_led_work, delete_publish_string_and_set_led_work_fn);
}

static void adxl362_trigger_handler(struct device *dev, struct sensor_trigger *trig)
{
	switch (trig->type) {
	case SENSOR_TRIG_THRESHOLD:
		printk("The cat has awoken, send cat data to the broker\n");
		break;
	default:
		printk("Unknown trigger\n");
	}	
}

static void adxl362_init(void)
{
	struct device *dev = device_get_binding(DT_ADI_ADXL362_0_LABEL);
	if (dev == NULL) {
		printk("Device get binding device\n");
		return;
	}

	if (IS_ENABLED(CONFIG_ADXL362_TRIGGER)) {
		struct sensor_trigger trig = { .chan = SENSOR_CHAN_ACCEL_XYZ };

		trig.type = SENSOR_TRIG_THRESHOLD;
		if (sensor_trigger_set(dev, &trig, adxl362_trigger_handler)) {
			printk("Trigger set error\n");
			return;
		}
	}	
}

static void lte_connect(void)
{
	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already turned on
			* and connected.
			*/
	} else {
		int err;

		printk("LTE Link Connecting ...\n");
		err = lte_lc_init_and_connect();
		__ASSERT(err == 0, "LTE link could not be established.");
		printk("LTE Link Connected!\n");
	}

	lte_lc_psm_req(true);
	
}

static void gps_control_handler(struct device *dev, struct gps_trigger *trigger) {

	switch(trigger->type) {
		case GPS_TRIG_FIX:
			printk("gps control handler triggered!\n");
			gps_control_on_trigger();
			gps_control_stop(GPS_DELAYED_TIME);
			gps_sample_fetch(dev);
			gps_channel_get(dev, GPS_CHAN_NMEA, &gps_data);
			strcat(mqtt_assembly_line_d, gps_data.nmea.buf);
			k_sem_give(events[0].sem);
		break;

		default:
		break;
	}

}

void main(void)
{

	printk("The cat tracker has started\n");
	work_init();
	lte_connect();
	adxl362_init();
	base_string_set(mqtt_assembly_line_d);
	gps_control_init(gps_control_handler);

	while(1) {
		k_work_submit(&request_battery_status_work);
		gps_control_start(GPS_DELAYED_TIME);

		k_poll(events, 1, K_FOREVER);
		if (events[0].state == K_POLL_STATE_SEM_AVAILABLE) {
			k_sem_take(events[0].sem, 0);
			k_work_submit(&publish_gps_data_work);
			k_work_submit(&delete_publish_string_and_set_led_work);
		}
        events[0].state = K_POLL_STATE_NOT_READY;
		k_sleep(PUBLISH_INTERVAL);
	}

}