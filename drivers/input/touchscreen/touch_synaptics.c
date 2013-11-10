/* Touch_synaptics.c
 *
 * Copyright (C) 2011 LGE.
 *
 * Author: yehan.ahn@lge.com, hyesung.shin@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include <linux/gpio.h>

#include <linux/earlysuspend.h>
#include <linux/input/mt.h>
#include <linux/input/lge_touch_core.h>
#include <linux/input/touch_synaptics.h>

#include "SynaImage.h"
#include <linux/regulator/machine.h>

<<<<<<< HEAD
/* RMI4 spec from (RMI4 spec)511-000136-01_revD
 * Function     Purpose                                      See page
 * $01          RMI Device Control                              29
 * $08          BIST(Built-in Self Test)                        38
 * $09          BIST(Built-in Self Test)                        42
 * $11          2-D TouchPad sensors                            46
 * $19          0-D capacitive button sensors                   69
 * $30          GPIO/LEDs (includes mechanical buttons)         76
 * $32          Timer                                           89
 * $34          Flash Memory Management                         93
=======
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE_QPNP_PON
#include <linux/input/sweep2wake.h>
#endif

static struct workqueue_struct *synaptics_wq;

/* RMI4 spec from 511-000405-01 Rev.D
 * Function      Purpose                           See page
 * $01      RMI Device Control                      45
 * $1A      0-D capacitive button sensors           61
 * $05      Image Reporting                         68
 * $07      Image Reporting                         75
 * $08      BIST                                    82
 * $09      BIST                                    87
 * $11      2-D TouchPad sensors                    93
 * $19      0-D capacitive button sensors           141
 * $30      GPIO/LEDs                               148
 * $31      LEDs                                    162
 * $34      Flash Memory Management                 163
 * $36      Auxiliary ADC                           174
 * $54      Test Reporting                          176
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well
 */
#define RMI_DEVICE_CONTROL                      0x01
#define TOUCHPAD_SENSORS                        0x11
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
#define CAPACITIVE_BUTTON_SENSORS               0x1A
#define GPIO_LEDS                               0x31
#define ANALOG_CONTROL                          0x54
#else
#define CAPACITIVE_BUTTON_SENSORS               0x19
#define GPIO_LEDS                               0x30
#endif
#define TIMER                                   0x32
#define FLASH_MEMORY_MANAGEMENT                 0x34

/* Register Map & Register bit mask
 * - Please check "One time" this map before using this device driver
 */

#define MANUFACTURER_ID_REG (ts->common_dsc.query_base)    /* Manufacturer ID */
#define FW_REVISION_REG     (ts->common_dsc.query_base+3)  /* FW revision */
#define PRODUCT_ID_REG      (ts->common_dsc.query_base+11) /* Product ID */
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
#define FW_VERSION          (ts->flash_dsc.control_base)
#endif

#define DEVICE_CONTROL_REG (ts->common_dsc.control_base)  /* Device Control */
#define DEVICE_CONTROL_NORMAL_OP    0x00    /* sleep mode : go to doze mode after 500 ms */
#define DEVICE_CONTROL_SLEEP        0x01    /* sleep mode : go to sleep */
#define DEVICE_CONTROL_SPECIFIC     0x02    /* sleep mode : go to doze mode after 5 sec */
#define DEVICE_CONTROL_NOSLEEP      0x04
#define DEVICE_CONTROL_CONFIGURED   0x80

#define INTERRUPT_ENABLE_REG (ts->common_dsc.control_base+1) /* Interrupt Enable 0 */

#define DEVICE_STATUS_REG    (ts->common_dsc.data_base)    /* Device Status */
#define DEVICE_FAILURE_MASK         0x03
#define DEVICE_CRC_ERROR_MASK       0x04
#define DEVICE_STATUS_FLASH_PROG    0x40
#define DEVICE_STATUS_UNCONFIGURED  0x80

#define INTERRUPT_STATUS_REG  (ts->common_dsc.data_base+1) /* Interrupt Status */
#define BUTTON_DATA_REG       (ts->button_dsc.data_base)   /* Button Data */
#define MAX_NUM_OF_BUTTON 4

#define FINGER_STATE_REG      (ts->finger_dsc.data_base)   /* Finger State */
#define FINGER_DATA_REG_START (ts->finger_dsc.data_base+3) /* Finger Data Register */
#define FINGER_STATE_MASK           0x03
#define REG_X_POSITION              0
#define REG_Y_POSITION              1
#define REG_YX_POSITION             2
#define REG_WY_WX                   3
#define REG_Z                       4

#define TWO_D_REPORTING_MODE (ts->finger_dsc.control_base+0) /* 2D Reporting Mode */
#define REPORT_MODE_CONTINUOUS      0x00
#define REPORT_MODE_REDUCED         0x01
#define ABS_FILTER                  0x08
#define PALM_DETECT_REG      (ts->finger_dsc.control_base+1) /* Palm Detect */
#define DELTA_X_THRESH_REG  (ts->finger_dsc.control_base+2) /* Delta-X Thresh */
#define DELTA_Y_THRESH_REG   (ts->finger_dsc.control_base+3) /* Delta-Y Thresh */
#define SENSOR_MAX_X_POS     (ts->finger_dsc.control_base+6) /* SensorMaxXPos */
#define SENSOR_MAX_Y_POS     (ts->finger_dsc.control_base+8) /* SensorMaxYPos */
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
/*do nothing*/
#else
#define GESTURE_ENABLE_1_REG (ts->finger_dsc.control_base+10) /* Gesture Enables 1 */
#define GESTURE_ENABLE_2_REG (ts->finger_dsc.control_base+11) /* Gesture Enables 2 */
#endif

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
#define PAGE_SELECT_REG             0xFF           /* Button exists Page 02 */
#define ANALOG_CONTROL_REG          (ts->analog_dsc.control_base)
#define ANALOG_COMMAND_REG          (ts->analog_dsc.command_base)
#define FAST_RELAXATION_RATE        (ts->analog_dsc.control_base+16)
#define FORCE_FAST_RELAXATION       0x04
#define FORCE_UPDATE                0x04
#else
#define MELT_CONTROL_REG            0xF0
#define MELT_CONTROL_NO_MELT        0x00
#define MELT_CONTROL_MELT           0x01
#define MELT_CONTROL_NUKE_MELT      0x80
#endif

#define DEVICE_COMMAND_REG          (ts->common_dsc.command_base)
#define FINGER_COMMAND_REG          (ts->finger_dsc.command_base)
#define BUTTON_COMMAND_REG          (ts->button_dsc.command_base)

#define FLASH_CONTROL_REG           (ts->flash_dsc.data_base+18)     /* Flash Control */
#define FLASH_STATUS_MASK           0xF0
#define FW_OFFSET_PRODUCT_ID        0x40
#define FW_OFFSET_IMAGE_VERSION     0xB100

/* Get user-finger-data from register.
 */
#define TS_SNTS_GET_X_POSITION(_high_reg, _low_reg) \
	(((u16)(((_high_reg) << 4) & 0x0FF0) | (u16)((_low_reg) & 0x0F)))
#define TS_SNTS_GET_Y_POSITION(_high_reg, _low_reg) \
	(((u16)(((_high_reg) << 4) & 0x0FF0) | (u16)(((_low_reg) >> 4) & 0x0F)))
#define TS_SNTS_GET_WIDTH_MAJOR(_width) \
	((((((_width) & 0xF0) >> 4) - ((_width) & 0x0F)) > 0) ? \
		((_width) & 0xF0) >> 4 : (_width) & 0x0F)
#define TS_SNTS_GET_WIDTH_MINOR(_width) \
	((((((_width) & 0xF0) >> 4) - ((_width) & 0x0F)) > 0) ? \
		(_width) & 0x0F : ((_width) & 0xF0) >> 4)
#define TS_SNTS_GET_ORIENTATION(_width) \
	((((((_width) & 0xF0) >> 4) - ((_width) & 0x0F)) >= 0) ? 0 : 1)
#define TS_SNTS_GET_PRESSURE(_pressure) (_pressure)

#define FINGER_STATE_NO_PRESENT         0
#define FINGER_STATE_PRESENT_VALID      1
#define FINGER_STATE_PRESENT_NOVALID    2
#define FINGER_STATE_REVSERVED          3

#define CHARGER_CONNECTED               0x20

static inline int get_highest_id(u32 fs)
{
	int id = 10;
	int tmp = 0x000C0000;
	while(!(fs & tmp)) {
		id--;
		tmp >>= 2;
	}
	return id;
}

<<<<<<< HEAD
int synaptics_ts_get_data(struct i2c_client *client, struct t_data* data,
					struct b_data* button, u8* total_num)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
=======
/* Debug mask value
 * usage: echo [debug_mask] > /sys/module/touch_synaptics/parameters/debug_mask
 */
static u32 touch_debug_mask = DEBUG_BASE_INFO;
module_param_named(debug_mask, touch_debug_mask, int, S_IRUGO|S_IWUSR|S_IWGRP);

static int touch_power_cntl(struct synaptics_ts_data *ts, int onoff);
static int touch_ic_init(struct synaptics_ts_data *ts);
static void touch_init_func(struct work_struct *work_init);
static void safety_reset(struct synaptics_ts_data *ts);
static void release_all_ts_event(struct synaptics_ts_data *ts);
static int synaptics_ts_get_data(struct i2c_client *client,
						struct t_data* data);
static int synaptics_ts_power(struct i2c_client *client, int power_ctrl);
static int synaptics_init_panel(struct i2c_client *client,
					struct synaptics_ts_fw_info *fw_info);
static int get_ic_info(struct synaptics_ts_data *ts,
					struct synaptics_ts_fw_info *fw_info);

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
/* gives back true if only one touch is recognized */
extern bool is_single_touch(struct synaptics_ts_data *);
#endif

/* touch_asb_input_report
 *
 * finger status report
 */
static void touch_abs_input_report(struct synaptics_ts_data *ts)
{
	int	id;

	for (id = 0; id < ts->pdata->max_id; id++) {
		if (!ts->ts_data.curr_data[id].state)
			continue;

		input_mt_slot(ts->input_dev, id);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER,
				ts->ts_data.curr_data[id].state != ABS_RELEASE);

		if (ts->ts_data.curr_data[id].state != ABS_RELEASE) {
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
			detect_sweep2wake(ts->ts_data.curr_data[id].x_position, ts->ts_data.curr_data[id].y_position, is_single_touch(ts));
#endif
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					ts->ts_data.curr_data[id].x_position);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					ts->ts_data.curr_data[id].y_position);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
					ts->ts_data.curr_data[id].pressure);

			/* Only support circle region */
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
					ts->ts_data.curr_data[id].width_major);
		}
		else {
			ts->ts_data.curr_data[id].state = 0;
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
			if (s2w_switch > 0) {
				sweep2wake_reset();
			}
#endif
		}
	}
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	u32 finger_status=0;
	u8 id=0;
	u8 cnt;

	if (unlikely(touch_debug_mask & DEBUG_TRACE))
		TOUCH_DEBUG_MSG("\n");

	if (unlikely(touch_i2c_read(client, DEVICE_STATUS_REG,
			sizeof(ts->ts_data.device_status_reg),
			&ts->ts_data.device_status_reg) < 0)) {
		TOUCH_ERR_MSG("DEVICE_STATUS_REG read fail\n");
		goto err_synaptics_getdata;
	}

	/* ESD damage check */
	if ((ts->ts_data.device_status_reg & DEVICE_FAILURE_MASK) ==
							DEVICE_FAILURE_MASK) {
		TOUCH_ERR_MSG("ESD damage occured. Reset Touch IC\n");
		goto err_synaptics_device_damage;
	}

	/* Internal reset check */
	if (((ts->ts_data.device_status_reg & DEVICE_STATUS_UNCONFIGURED) >> 7) == 1) {
		TOUCH_ERR_MSG("Touch IC resetted internally. "
				"Reconfigure register setting\n");
		goto err_synaptics_device_damage;
	}

	if (unlikely(touch_i2c_read(client, INTERRUPT_STATUS_REG,
			sizeof(ts->ts_data.interrupt_status_reg),
			&ts->ts_data.interrupt_status_reg) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
		goto err_synaptics_getdata;
	}

	if (unlikely(touch_debug_mask & DEBUG_GET_DATA))
		TOUCH_INFO_MSG("Interrupt_status : 0x%x\n",
					ts->ts_data.interrupt_status_reg);

	/* IC bug Exception handling - Interrupt status reg is 0 when interrupt occur */
	if (ts->ts_data.interrupt_status_reg == 0) {
		TOUCH_ERR_MSG("Interrupt_status reg is 0. -> ignore\n");
		goto err_synaptics_ignore;
	}

	/* Because of ESD damage... */
	if (unlikely(ts->ts_data.interrupt_status_reg & ts->interrupt_mask.flash)) {
		TOUCH_ERR_MSG("Impossible Interrupt\n");
		goto err_synaptics_device_damage;
	}

	/* Finger */
	if (likely(ts->ts_data.interrupt_status_reg & ts->interrupt_mask.abs)) {
		if (unlikely(touch_i2c_read(client, FINGER_STATE_REG,
				sizeof(ts->ts_data.finger.finger_status_reg),
				ts->ts_data.finger.finger_status_reg) < 0)) {
			TOUCH_ERR_MSG("FINGER_STATE_REG read fail\n");
			goto err_synaptics_getdata;
		}

		finger_status = ts->ts_data.finger.finger_status_reg[0] |
			ts->ts_data.finger.finger_status_reg[1] << 8 |
			(ts->ts_data.finger.finger_status_reg[2] & 0xF) << 16;

		if (unlikely(touch_debug_mask & DEBUG_GET_DATA)) {
			TOUCH_INFO_MSG("Finger_status : 0x%x, 0x%x, 0x%x\n",
				ts->ts_data.finger.finger_status_reg[0],
				ts->ts_data.finger.finger_status_reg[1],
				ts->ts_data.finger.finger_status_reg[2]);
			TOUCH_INFO_MSG("Touch_bit_mask: 0x%x\n", finger_status);
		}

		if (finger_status) {
			int max_id = get_highest_id(finger_status);
			if (unlikely(touch_i2c_read(ts->client,
					FINGER_DATA_REG_START,
					NUM_OF_EACH_FINGER_DATA_REG * max_id,
					ts->ts_data.finger.finger_reg[0]) < 0)) {
				TOUCH_ERR_MSG("FINGER_STATE_REG read fail\n");
				goto err_synaptics_getdata;
			}
		}

		for (id = 0; id < ts->pdata->caps->max_id; id++) {
			switch (((finger_status >> (id*2)) & 0x3)) {
			case FINGER_STATE_PRESENT_VALID:
				data[id].state = ABS_PRESS;
				data[id].x_position = TS_SNTS_GET_X_POSITION(
					ts->ts_data.finger.finger_reg[id][REG_X_POSITION],
					ts->ts_data.finger.finger_reg[id][REG_YX_POSITION]);
				data[id].y_position = TS_SNTS_GET_Y_POSITION(ts->ts_data.finger.finger_reg[id][REG_Y_POSITION], ts->ts_data.finger.finger_reg[id][REG_YX_POSITION]);
				data[id].width_major = TS_SNTS_GET_WIDTH_MAJOR(ts->ts_data.finger.finger_reg[id][REG_WY_WX]);
				data[id].width_minor = TS_SNTS_GET_WIDTH_MINOR(ts->ts_data.finger.finger_reg[id][REG_WY_WX]);
				data[id].tool_type = MT_TOOL_FINGER;
				data[id].width_orientation = TS_SNTS_GET_ORIENTATION(ts->ts_data.finger.finger_reg[id][REG_WY_WX]);
				data[id].pressure = TS_SNTS_GET_PRESSURE(ts->ts_data.finger.finger_reg[id][REG_Z]);

				if (unlikely(touch_debug_mask & DEBUG_GET_DATA))
					TOUCH_INFO_MSG("[%d] pos(%4d,%4d) w_m[%2d] w_n[%2d] w_o[%2d] p[%2d]\n",
						id,
						data[id].x_position,
						data[id].y_position,
						data[id].width_major,
						data[id].width_minor,
						data[id].width_orientation,
						data[id].pressure);
				(*total_num)++;
				break;

			case FINGER_STATE_NO_PRESENT:
				if (data[id].state == ABS_PRESS)
					data[id].state = ABS_RELEASE;
				break;

			default:
				/* Do nothing including inacurate data */
				break;
			}

		}
		if (unlikely(touch_debug_mask & DEBUG_GET_DATA))
			TOUCH_INFO_MSG("Total_num: %d\n", *total_num);
	}

	 /* Button */
	if ((ts->button_dsc.id != 0) && (ts->ts_data.interrupt_status_reg &
						ts->interrupt_mask.button)) {

		if (unlikely(touch_i2c_write_byte(client,
					PAGE_SELECT_REG, 0x02) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}

		if (unlikely(touch_i2c_read(client, BUTTON_DATA_REG,
				sizeof(ts->ts_data.button_data_reg),
				&ts->ts_data.button_data_reg) < 0)) {
			TOUCH_ERR_MSG("BUTTON_DATA_REG read fail\n");
			goto err_synaptics_getdata;
		}

		if (unlikely(touch_i2c_write_byte(client,
				PAGE_SELECT_REG, 0x00) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}

		if (unlikely(touch_debug_mask & DEBUG_BUTTON))
			TOUCH_DEBUG_MSG("Button register: 0x%x\n",
					ts->ts_data.button_data_reg);

		if (ts->ts_data.button_data_reg) {
			/* pressed - find first one */
			for (cnt = 0; cnt < ts->pdata->caps->number_of_button; cnt++) {
				if ((ts->ts_data.button_data_reg >> cnt) & 1)
				{
					ts->ts_data.button.key_code =
						ts->pdata->caps->button_name[cnt];
					button->key_code =
						ts->ts_data.button.key_code;
					button->state = 1;
					break;
				}
			}
		} else {
			/* release */
			button->key_code = ts->ts_data.button.key_code;
			button->state = 0;
		}
	}

	return 0;

err_synaptics_device_damage:
err_synaptics_getdata:
	return -EIO;
err_synaptics_ignore:
	return -EINVAL;
}

static int read_page_description_table(struct i2c_client* client)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	struct ts_function_descriptor buffer;
	unsigned short u_address;

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	memset(&buffer, 0x0, sizeof(struct ts_function_descriptor));

	ts->common_dsc.id = 0;
	ts->finger_dsc.id = 0;
	ts->button_dsc.id = 0;
	ts->flash_dsc.id  = 0;
	ts->analog_dsc.id = 0;

	for (u_address = DESCRIPTION_TABLE_START; u_address > 10;
			u_address -= sizeof(struct ts_function_descriptor)) {
		if (unlikely(touch_i2c_read(client, u_address, sizeof(buffer),
					(unsigned char *)&buffer) < 0)) {
			TOUCH_ERR_MSG("RMI4 Function Descriptor read fail\n");
			return -EIO;
		}

		if (buffer.id == 0)
			break;

		switch (buffer.id) {
		case RMI_DEVICE_CONTROL:
			ts->common_dsc = buffer;
			break;
		case TOUCHPAD_SENSORS:
			ts->finger_dsc = buffer;
			break;
		case FLASH_MEMORY_MANAGEMENT:
			ts->flash_dsc = buffer;
			break;
		}
	}

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, 0x01) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

<<<<<<< HEAD
	if (unlikely(touch_i2c_read(client, ANALOG_TABLE_START, sizeof(buffer), (unsigned char *)&buffer) < 0)) {
		TOUCH_ERR_MSG("RMI4 Function Descriptor read fail\n");
		return -EIO;
=======
	if (ts->curr_pwr_state == POWER_ON) {
		disable_irq(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
		if (irq_wake) {
			irq_wake = false;
			disable_irq_wake(ts->client->irq);
		}
#endif
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well
	}

	if (buffer.id == ANALOG_CONTROL) {
		ts->analog_dsc = buffer;
	}

	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, 0x02) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(ts->client, BUTTON_TABLE_START, sizeof(buffer), (unsigned char *)&buffer))) {
		TOUCH_ERR_MSG("Button ts_function_descriptor read fail\n");
		return -EIO;
	}

	if (buffer.id == CAPACITIVE_BUTTON_SENSORS)
		ts->button_dsc = buffer;

	if (unlikely(touch_i2c_write_byte(client, PAGE_SELECT_REG, 0x00) < 0)) {
		TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
		return -EIO;
	}
<<<<<<< HEAD
=======
	else {
		enable_irq(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
		if (!irq_wake) {
			irq_wake = true;
			enable_irq_wake(ts->client->irq);
		}
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well
#endif

	/* set interrupt mask */
	ts->interrupt_mask.flash = 0x1;
	ts->interrupt_mask.status = 0x2;

	if (ts->button_dsc.id == 0) {
		ts->interrupt_mask.abs = 0x4;
	} else {
		if (ts->finger_dsc.data_base > ts->button_dsc.data_base) {
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
			ts->interrupt_mask.abs = 0x4;
			ts->interrupt_mask.button = 0x20;
#else
			ts->interrupt_mask.abs = 0x8;
			ts->interrupt_mask.button = 0x4;
#endif
		} else {
			ts->interrupt_mask.abs = 0x4;
			ts->interrupt_mask.button = 0x8;
		}
	}

	if (ts->common_dsc.id == 0 || ts->finger_dsc.id == 0 ||
						ts->flash_dsc.id == 0) {
		TOUCH_ERR_MSG("common_dsc/finger_dsc/flash_dsc are not initiailized\n");
		return -EPERM;
	}

	if (touch_debug_mask & DEBUG_BASE_INFO)
		TOUCH_INFO_MSG("common[%d] finger[%d] flash[%d] button[%d]\n",
				ts->common_dsc.id, ts->finger_dsc.id,
				ts->flash_dsc.id, ts->button_dsc.id);

	return 0;
}

int get_ic_info(struct synaptics_ts_data* ts, struct touch_fw_info* fw_info)
{
#if defined(ARRAYED_TOUCH_FW_BIN)
	int cnt;
#endif
	u8 device_status = 0;
	u8 flash_control = 0;

<<<<<<< HEAD
	read_page_description_table(ts->client);
=======
	enable_irq(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (!irq_wake) {
		irq_wake = true;
		enable_irq_wake(ts->client->irq);
	}
#endif
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	memset(fw_info, 0, sizeof(fw_info));

<<<<<<< HEAD
	if (unlikely(touch_i2c_read(ts->client, FW_REVISION_REG,
			sizeof(fw_info->fw_rev), &fw_info->fw_rev) < 0)) {
		TOUCH_ERR_MSG("FW_REVISION_REG read fail\n");
		return -EIO;
	}
=======
/* touch_recover_func
 *
 * In order to reduce the booting-time,
 * we used delayed_work_queue instead of msleep or mdelay.
 */
static void touch_recover_func(struct work_struct *work_recover)
{
	struct synaptics_ts_data *ts =
			container_of(work_recover,
				struct synaptics_ts_data, work_recover);

	disable_irq(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (irq_wake) {
		irq_wake = false;
		disable_irq_wake(ts->client->irq);
	}
#endif
	safety_reset(ts);
	enable_irq(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (!irq_wake) {
		irq_wake = true;
		enable_irq_wake(ts->client->irq);
	}
#endif
	touch_ic_init(ts);
}
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	if (unlikely(touch_i2c_read(ts->client, MANUFACTURER_ID_REG,
					sizeof(fw_info->manufacturer_id),
					&fw_info->manufacturer_id) < 0)) {
		TOUCH_ERR_MSG("MANUFACTURER_ID_REG read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(ts->client, PRODUCT_ID_REG,
					sizeof(fw_info->product_id) - 1,
					fw_info->product_id) < 0)) {
		TOUCH_ERR_MSG("PRODUCT_ID_REG read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(ts->client, FW_VERSION,
					sizeof(fw_info->fw_version) - 1,
					fw_info->fw_version) < 0)) {
		TOUCH_ERR_MSG("FW_VERSION read fail\n");
		return -EIO;
	}

	ts->ic_panel_type = IC7020_G2_H_PTN;
	TOUCH_INFO_MSG("IC is 7020, H pattern, panel is G2, Firmware: %s.", fw_info->fw_version);

#if defined(ARRAYED_TOUCH_FW_BIN)
	for (cnt = 0; cnt < sizeof(SynaFirmware)/sizeof(SynaFirmware[0]); cnt++) {
		strncpy(fw_info->fw_image_product_id,
				&SynaFirmware[cnt][FW_OFFSET_PRODUCT_ID], 10);
		if (!(strncmp(fw_info->product_id,
				fw_info->fw_image_product_id, 10)))
			break;
	}
	fw_info->fw_start = (unsigned char *)&SynaFirmware[cnt][0];
	fw_info->fw_size = sizeof(SynaFirmware[0]);
#else
	fw_info->fw_start = (unsigned char *)&SynaFirmware[0];
	fw_info->fw_size = sizeof(SynaFirmware);
#endif

	strncpy(fw_info->fw_image_product_id,
				&fw_info->fw_start[FW_OFFSET_PRODUCT_ID], 10);
	strncpy(fw_info->fw_image_version,
					&fw_info->fw_start[FW_OFFSET_IMAGE_VERSION],4);

	if (unlikely(touch_i2c_read(ts->client, FLASH_CONTROL_REG,
				sizeof(flash_control), &flash_control) < 0)) {
		TOUCH_ERR_MSG("FLASH_CONTROL_REG read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(ts->client, DEVICE_STATUS_REG,
				sizeof(device_status), &device_status) < 0)) {
		TOUCH_ERR_MSG("DEVICE_STATUS_REG read fail\n");
		return -EIO;
	}

	/* Firmware has a problem, so we should firmware-upgrade */
	if (device_status & DEVICE_STATUS_FLASH_PROG
			|| (device_status & DEVICE_CRC_ERROR_MASK) != 0
			|| (flash_control & FLASH_STATUS_MASK) != 0) {
		TOUCH_ERR_MSG("Firmware has a unknown-problem, "
				"so it needs firmware-upgrade.\n");
		TOUCH_ERR_MSG("FLASH_CONTROL[%x] DEVICE_STATUS_REG[%x]\n",
				(u32)flash_control, (u32)device_status);

		fw_info->fw_rev = 0;
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
		fw_info->fw_force_rework = true;
#endif
	}

<<<<<<< HEAD
	ts->fw_info = fw_info;
=======
err_out_retry:
	ts->ic_init_err_cnt++;
	disable_irq_nosync(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (irq_wake) {
		irq_wake = false;
		disable_irq_wake(ts->client->irq);
	}
#endif
	safety_reset(ts);
	queue_delayed_work(synaptics_wq, &ts->work_init, msecs_to_jiffies(10));
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	return 0;
}

int synaptics_ts_init(struct i2c_client* client, struct touch_fw_info* fw_info)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	struct lge_touch_data *lg_ts =
			(struct lge_touch_data *) i2c_get_clientdata(client);
	u8 buf;

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	if (!ts->is_probed)
		if (unlikely(get_ic_info(ts, fw_info) < 0))
			return -EIO;

	if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
		DEVICE_CONTROL_NOSLEEP | DEVICE_CONTROL_CONFIGURED |
		(lg_ts->charger_type ? CHARGER_CONNECTED : 0)) < 0)) {
		TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(client, INTERRUPT_ENABLE_REG,
		1, &buf) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_ENABLE_REG read fail\n");
		return -EIO;
	}
<<<<<<< HEAD
=======
	if (unlikely(touch_i2c_write_byte(client, INTERRUPT_ENABLE_REG,
			buf | INTERRUPT_MASK_ABS0 | INTERRUPT_MASK_BUTTON) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_ENABLE_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(client, TWO_D_REPORTING_MODE, 1, &buf) < 0)) {
		TOUCH_ERR_MSG("TWO_D_REEPORTING_MODE read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
			(buf & ~7) | REPORT_MODE_REDUCED) < 0)) {
		TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_write_byte(client, DELTA_X_THRESH_REG, 1) < 0)) {
		TOUCH_ERR_MSG("DELTA_X_THRESH_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_write_byte(client, DELTA_Y_THRESH_REG, 1) < 0)) {
		TOUCH_ERR_MSG("DELTA_Y_THRESH_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(client, INTERRUPT_STATUS_REG, 1, &buf) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(client, FINGER_STATE_REG,
			sizeof(fdata.finger_status_reg),
			fdata.finger_status_reg) < 0)) {
		TOUCH_ERR_MSG("FINGER_STATE_REG read fail\n");
		return -EIO;
	}

	ts->is_probed = 1;

	return 0;
}

static int synaptics_ts_power(struct i2c_client *client, int power_ctrl)
{
	struct synaptics_ts_data *ts =
			(struct synaptics_ts_data *)get_touch_handle(client);

	TOUCH_DEBUG_TRACE("%s\n", __func__);

	switch (power_ctrl) {
	case POWER_OFF:
		gpio_set_value(ts->pdata->reset_gpio, 0);
		synaptics_t1320_power_on(client, 0);
		break;
	case POWER_ON:
		synaptics_t1320_power_on(client, 1);
		gpio_set_value(ts->pdata->reset_gpio, 1);
		break;
	case POWER_SLEEP:
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_SLEEP | DEVICE_CONTROL_CONFIGURED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
		break;
	case POWER_WAKE:
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_NORMAL_OP | DEVICE_CONTROL_CONFIGURED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
		break;
	default:
		return -EIO;
		break;
	}

	return 0;
}

static int synaptics_ts_ic_ctrl(struct i2c_client *client, u8 code, u16 value)
{
	struct synaptics_ts_data *ts =
			(struct synaptics_ts_data *)get_touch_handle(client);
	u8 buf = 0;

	switch (code) {
	case IC_CTRL_BASELINE:
		switch (value) {
		case BASELINE_OPEN:
			if (unlikely(synaptics_ts_page_data_write_byte(client,
					ANALOG_PAGE, ANALOG_CONTROL_REG,
					FORCE_FAST_RELAXATION) < 0)) {
				TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
				return -EIO;
			}

			msleep(10);

			if (unlikely(synaptics_ts_page_data_write_byte(client,
					ANALOG_PAGE, ANALOG_COMMAND_REG,
					FORCE_UPDATE) < 0)) {
				TOUCH_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
				return -EIO;
			}

			TOUCH_DEBUG_GHOST("BASELINE_OPEN\n");

			break;
		case BASELINE_FIX:
			if (unlikely(synaptics_ts_page_data_write_byte(client,
				ANALOG_PAGE, ANALOG_CONTROL_REG, 0x00) < 0)) {
				TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
				return -EIO;
			}

			msleep(10);

			if (unlikely(synaptics_ts_page_data_write_byte(client,
					ANALOG_PAGE, ANALOG_COMMAND_REG,
					FORCE_UPDATE) < 0)) {
				TOUCH_ERR_MSG("ANALOG_COMMAND_REG write fail\n");
				return -EIO;
			}

			TOUCH_DEBUG_GHOST("BASELINE_FIX\n");

			break;
		case BASELINE_REBASE:
			/* rebase base line */
			if (likely(ts->finger_fc.dsc.id != 0)) {
				if (unlikely(touch_i2c_write_byte(client,
						FINGER_COMMAND_REG, 0x1) < 0)) {
					TOUCH_ERR_MSG("finger baseline reset command write fail\n");
					return -EIO;
				}
			}
			break;
		default:
			break;
		}
		break;
	case IC_CTRL_READ:
		if (touch_i2c_read(client, value, 1, &buf) < 0) {
			TOUCH_ERR_MSG("IC register read fail\n");
			return -EIO;
		}
		break;
	case IC_CTRL_WRITE:
		if (touch_i2c_write_byte(client, ((value & 0xFF00) >> 8),
						(value & 0xFF)) < 0) {
			TOUCH_ERR_MSG("IC register write fail\n");
			return -EIO;
		}
		break;
	case IC_CTRL_RESET_CMD:
		if (unlikely(touch_i2c_write_byte(client,
					DEVICE_COMMAND_REG, 0x1) < 0)) {
			TOUCH_ERR_MSG("IC Reset command write fail\n");
			return -EIO;
		}
		break;
	default:
		break;
	}

	return buf;
}

static ssize_t show_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", ts->fw_info.config_id);
}

/* show_fw_info
 *
 * show only the firmware information
 */
static ssize_t show_fw_info(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);
	int ret = 0;

	ret = sprintf(buf, "====== Firmware Info ======\n");
	ret += sprintf(buf+ret, "manufacturer_id  = %d\n",
			ts->fw_info.manufacturer_id);
	ret += sprintf(buf+ret, "product_id       = %s\n",
			ts->fw_info.product_id);
	ret += sprintf(buf+ret, "fw_version       = %s\n",
			ts->fw_info.config_id);
	ret += sprintf(buf+ret, "fw_image_version = %s\n",
			ts->fw_info.image_config_id);
	return ret;
}

static ssize_t store_fw_upgrade(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	int repeat = 0;
	char path[256] = {0};
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);

	sscanf(buf, "%d %s", &value, path);

	TOUCH_INFO_MSG("Firmware image path: %s\n", path[0] != 0 ? path : "Internal");

	if (value) {
		for (repeat = 0; repeat < value; repeat++) {
			/* sync for n-th repeat test */
			while (ts->fw_info.fw_upgrade.is_downloading)
				;

			msleep(BOOTING_DELAY * 2);
			TOUCH_INFO_MSG("Firmware image upgrade: No.%d", repeat+1);

			/* for n-th repeat test - because ts->fw_info.fw_upgrade is setted 0 after FW upgrade */
			memcpy(ts->fw_info.fw_upgrade.fw_path,
				path, sizeof(ts->fw_info.fw_upgrade.fw_path)-1);

			/* set downloading flag for sync for n-th test */
			ts->fw_info.fw_upgrade.is_downloading = UNDER_DOWNLOADING;
			ts->fw_info.fw_upgrade.fw_force_upgrade = 1;
			queue_work(synaptics_wq, &ts->work_fw_upgrade);
		}

		/* sync for fw_upgrade test */
		while (ts->fw_info.fw_upgrade.is_downloading)
			;
	}

	return count;
}

static ssize_t ic_register_ctrl(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned char string[7];
	char reg_s[11];
	char value_s[11];
	int reg;
	int value;
	int ret = 0;
	u8 data;
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);

	sscanf(buf, "%6s %10s %10s", string, reg_s, value_s);

	if (kstrtoint(reg_s, 0, &reg) != 0) {
		TOUCH_ERR_MSG("Invalid parameter\n");
		return count;
	}

	if (kstrtoint(value_s, 0, &value) != 0) {
		value = 0;
	}

	if (ts->curr_pwr_state == POWER_ON || ts->curr_pwr_state == POWER_WAKE) {
		if (!strncmp(string, "read", 4)) {
			do {
				ret = synaptics_ts_page_data_read(ts->client,
						reg >> 8, reg & 0xFF, 1, &data);
				if (!ret)
					TOUCH_INFO_MSG("register[0x%x] = 0x%x\n", reg, data);
				else
					TOUCH_ERR_MSG("cannot read register[0x%x]\n", reg);
				reg++;
			} while (--value > 0);
		} else if (!strncmp(string, "write", 4)) {
			data = value;
			ret = synaptics_ts_page_data_write(ts->client,
					reg >> 8, reg & 0xFF, 1, &data);
			if (!ret)
				TOUCH_INFO_MSG("register[0x%x] is set to 0x%x\n", reg, data);
			else
				TOUCH_ERR_MSG("cannot write register[0x%x]\n", reg);
		} else {
			TOUCH_INFO_MSG("Usage: echo [read | write] reg_num value > ic_rw\n");
			TOUCH_INFO_MSG(" - reg_num : register address\n");
			TOUCH_INFO_MSG(" - value [read] : number of register starting form reg_num\n");
			TOUCH_INFO_MSG(" - value [write] : set value into reg_num\n");
		}
	} else {
		TOUCH_INFO_MSG("state=[suspend]. we cannot use I2C, now\n");
	}

	return count;
}

static ssize_t store_ts_reset(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct synaptics_ts_data *ts = dev_get_drvdata(dev);
	unsigned char string[5];
	u8 saved_state = ts->curr_pwr_state;
	int ret = 0;

	sscanf(buf, "%s", string);

	disable_irq_nosync(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (irq_wake) {
		irq_wake = false;
		disable_irq_wake(ts->client->irq);
	}
#endif

	cancel_delayed_work_sync(&ts->work_init);

	release_all_ts_event(ts);

	if (saved_state == POWER_ON || saved_state == POWER_WAKE) {
		if (!strncmp(string, "soft", 4)) {
			synaptics_ts_ic_ctrl(ts->client, IC_CTRL_RESET_CMD, 0);
		} else if (!strncmp(string, "pin", 3)) {
			gpio_set_value(ts->pdata->reset_gpio, 0);
			msleep(RESET_DELAY);
			gpio_set_value(ts->pdata->reset_gpio, 1);
		} else if (!strncmp(string, "vdd", 3)) {
			touch_power_cntl(ts, POWER_OFF);
			touch_power_cntl(ts, POWER_ON);
		} else {
			TOUCH_INFO_MSG("Usage: echo [soft | pin | vdd] > power_control\n");
			TOUCH_INFO_MSG(" - soft : reset using IC register setting\n");
			TOUCH_INFO_MSG(" - soft : reset using reset pin\n");
			TOUCH_INFO_MSG(" - hard : reset using VDD\n");
		}

		if (ret < 0)
			TOUCH_ERR_MSG("reset fail\n");
		else
			atomic_set(&ts->device_init, 0);

		msleep(BOOTING_DELAY);

	} else {
		TOUCH_INFO_MSG("Touch is suspend state. Don't need reset\n");
	}

	enable_irq(ts->client->irq);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (!irq_wake) {
		irq_wake = true;
		enable_irq_wake(ts->client->irq);
	}
#endif

	if (saved_state == POWER_ON || saved_state == POWER_WAKE)
		touch_ic_init(ts);

	return count;
}

static struct device_attribute synaptics_device_attrs[] = {
	__ATTR(firmware, S_IRUGO | S_IWUSR, show_fw_info, store_fw_upgrade),
	__ATTR(reg_control, S_IRUGO | S_IWUSR, NULL, ic_register_ctrl),
	__ATTR(power_control, S_IRUGO | S_IWUSR, NULL, store_ts_reset),
	__ATTR(version, S_IRUGO | S_IWUSR, show_fw_ver, NULL),
};

static int synaptics_get_dt_coords(struct device *dev, char *name,
				u32 *x, u32 *y)
{
	u32 coords[SYNAPTICS_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;

	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != SYNAPTICS_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	*x = coords[2];
	*y = coords[3];

	return 0;
}

static int synaptics_parse_dt(struct device *dev, struct touch_platform_data *pdata)
{
	int rc;
	u32 temp_val;
	struct device_node *np = dev->of_node;

	rc = synaptics_get_dt_coords(dev, "synaptics,panel-coords",
					&pdata->max_x, &pdata->max_y);
	if (rc)
		return rc;

	rc = synaptics_get_dt_coords(dev, "synaptics,display-coords",
					&pdata->lcd_x, &pdata->lcd_y);
	if (rc)
		return rc;

	rc = of_property_read_u32(np, "synaptics,max_id", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read max_id\n");
		return rc;
	} else {
		pdata->max_id = temp_val;
	}
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	if (!lg_ts->pdata->caps->button_support) {
		buf &= ~ts->interrupt_mask.button;
		ts->interrupt_mask.button = 0;
	}

	if (unlikely(touch_i2c_write_byte(client, INTERRUPT_ENABLE_REG,
		buf | ts->interrupt_mask.abs | ts->interrupt_mask.button) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_ENABLE_REG write fail\n");
		return -EIO;
	}

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
	if (unlikely(touch_i2c_read(client, TWO_D_REPORTING_MODE, 1, &buf) < 0)) {
		TOUCH_ERR_MSG("TWO_D_REPORTING_MODE read fail\n");
		return -EIO;
	}

	if (ts->pdata->role->report_mode == CONTINUOUS_REPORT_MODE) {
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				(buf & ~7) | REPORT_MODE_CONTINUOUS) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}
	} else {	/* REDUCED_REPORT_MODE */
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				(buf & ~7) | REPORT_MODE_REDUCED) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}

		if (unlikely(touch_i2c_write_byte(client, DELTA_X_THRESH_REG,
				ts->pdata->role->delta_pos_threshold) < 0)) {
			TOUCH_ERR_MSG("DELTA_X_THRESH_REG write fail\n");
			return -EIO;
		}
		if (unlikely(touch_i2c_write_byte(client, DELTA_Y_THRESH_REG,
				ts->pdata->role->delta_pos_threshold) < 0)) {
			TOUCH_ERR_MSG("DELTA_Y_THRESH_REG write fail\n");
			return -EIO;
		}
	}
#else
	if (unlikely(touch_i2c_write_byte(client,
					GESTURE_ENABLE_1_REG, 0x00) < 0)) {
		TOUCH_ERR_MSG("GESTURE_ENABLE_1_REG write fail\n");
		return -EIO;
	}
	if (unlikely(touch_i2c_write_byte(client, GESTURE_ENABLE_2_REG, 0x00) < 0)) {
		TOUCH_ERR_MSG("GESTURE_ENABLE_2_REG write fail\n");
		return -EIO;
	}

	if (unlikely(touch_i2c_read(client, TWO_D_REPORTING_MODE, 1, &buf) < 0)) {
		TOUCH_ERR_MSG("TWO_D_REPORTING_MODE read fail\n");
		return -EIO;
	}

	if (ts->pdata->role->report_mode == CONTINUOUS_REPORT_MODE) {
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				(buf & ~7) | REPORT_MODE_CONTINUOUS) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}
	} else {	/* REDUCED_REPORT_MODE */
		if (unlikely(touch_i2c_write_byte(client, TWO_D_REPORTING_MODE,
				(buf & ~7) | REPORT_MODE_REDUCED) < 0)) {
			TOUCH_ERR_MSG("TWO_D_REPORTING_MODE write fail\n");
			return -EIO;
		}

		if (unlikely(touch_i2c_write_byte(client, DELTA_X_THRESH_REG,
				ts->pdata->role->delta_pos_threshold) < 0)) {
			TOUCH_ERR_MSG("DELTA_X_THRESH_REG write fail\n");
			return -EIO;
		}
		if (unlikely(touch_i2c_write_byte(client, DELTA_Y_THRESH_REG,
				ts->pdata->role->delta_pos_threshold) < 0)) {
			TOUCH_ERR_MSG("DELTA_Y_THRESH_REG write fail\n");
			return -EIO;
		}
	}
<<<<<<< HEAD
#endif

	if (unlikely(touch_i2c_read(client, INTERRUPT_STATUS_REG, 1, &buf) < 0)) {
		TOUCH_ERR_MSG("INTERRUPT_STATUS_REG read fail\n");
		return -EIO;	/* it is critical problem because interrupt will not occur. */
	}
=======
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (s2w_switch == 0)
#endif
	{
	ts->curr_resume_state = 0;
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	if (unlikely(touch_i2c_read(client, FINGER_STATE_REG,
				sizeof(ts->ts_data.finger.finger_status_reg),
				ts->ts_data.finger.finger_status_reg) < 0)) {
		TOUCH_ERR_MSG("FINGER_STATE_REG read fail\n");
		/* it is critical problem because interrupt will not occur on some FW. */
		return -EIO;
	}

<<<<<<< HEAD
	ts->is_probed = 1;
=======
	disable_irq(ts->client->irq);

	cancel_delayed_work_sync(&ts->work_init);
	release_all_ts_event(ts);
	touch_power_cntl(ts, POWER_OFF);
	}
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	return 0;
}

int synaptics_ts_power(struct i2c_client* client, int power_ctrl)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	struct lge_touch_data *lg_ts =
			(struct lge_touch_data *) i2c_get_clientdata(client);

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	switch (power_ctrl) {
	case POWER_OFF:
		if (ts->pdata->reset_pin > 0)
			gpio_set_value(ts->pdata->reset_pin, 0);

		if (ts->pdata->pwr->use_regulator) {
			regulator_disable(ts->regulator_vio);
			regulator_disable(ts->regulator_vdd);
		}
		else
			ts->pdata->pwr->power(0);
		break;
	case POWER_ON:
		if (ts->pdata->pwr->use_regulator) {
			regulator_enable(ts->regulator_vdd);
			regulator_enable(ts->regulator_vio);
		}
		else
			ts->pdata->pwr->power(1);

<<<<<<< HEAD
		if (ts->pdata->reset_pin > 0)
			gpio_set_value(ts->pdata->reset_pin, 1);
		break;
	case POWER_SLEEP:
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_SLEEP |
				(lg_ts->charger_type ? CHARGER_CONNECTED : 0) |
				DEVICE_CONTROL_CONFIGURED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
		break;
	case POWER_WAKE:
		if (unlikely(touch_i2c_write_byte(client, DEVICE_CONTROL_REG,
				DEVICE_CONTROL_SPECIFIC |
				(lg_ts->charger_type ? CHARGER_CONNECTED : 0) |
				DEVICE_CONTROL_CONFIGURED) < 0)) {
			TOUCH_ERR_MSG("DEVICE_CONTROL_REG write fail\n");
			return -EIO;
		}
=======
	switch (event) {
	case LCD_EVENT_ON_START:
		synaptics_ts_start(ts);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
		scr_suspended = false;
#endif
		break;
	case LCD_EVENT_OFF_START:
		synaptics_ts_stop(ts);
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
		scr_suspended = true;
#endif
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well
		break;
	default:
		return -EIO;
		break;
	}

	return 0;
}

<<<<<<< HEAD
int synaptics_ts_probe(struct i2c_client* client)
=======
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
static ssize_t touch_synaptics_sweep2wake_show(struct device *dev,
               struct device_attribute *attr, char *buf)
{
       size_t count = 0;

       count += sprintf(buf, "%d\n", s2w_switch);

       return count;
}

static ssize_t touch_synaptics_sweep2wake_dump(struct device *dev,
               struct device_attribute *attr, const char *buf, size_t count)
{
       if (buf[0] >= '0' && buf[0] <= '2' && buf[1] == '\n')
                if (s2w_switch != buf[0] - '0')
                       s2w_switch = buf[0] - '0';

       return count;
}

static DEVICE_ATTR(sweep2wake, (S_IWUSR|S_IRUGO),
       touch_synaptics_sweep2wake_show, touch_synaptics_sweep2wake_dump);
#endif

static struct kobject *android_touch_kobj;

static int touch_synaptics_sysfs_init(void)
{
       int ret ;

       android_touch_kobj = kobject_create_and_add("android_touch", NULL) ;
       if (android_touch_kobj == NULL) {
               pr_debug("[touch_synaptics]%s: subsystem_register failed\n", __func__);
               ret = -ENOMEM;
               return ret;
       }
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
       ret = sysfs_create_file(android_touch_kobj, &dev_attr_sweep2wake.attr);
       if (ret) {
               printk(KERN_ERR "[sweep2wake]%s: sysfs_create_file failed\n", __func__);
               return ret;
       }
#endif
       return 0 ;
}

static void touch_synaptics_sysfs_deinit(void)
{
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
       sysfs_remove_file(android_touch_kobj, &dev_attr_sweep2wake.attr);
#endif
       kobject_del(android_touch_kobj);
}

static int synaptics_ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well
{
	struct synaptics_ts_data* ts;
	int ret = 0;

<<<<<<< HEAD
	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");
=======
	TOUCH_DEBUG_TRACE("%s\n", __func__);

	touch_synaptics_sysfs_init();

	synaptics_wq = create_singlethread_workqueue("synaptics_wq");
	if (!synaptics_wq)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		TOUCH_ERR_MSG("i2c functionality check error\n");
		return -EPERM;
	}
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	ts = kzalloc(sizeof(struct synaptics_ts_data), GFP_KERNEL);
	if (!ts) {
		TOUCH_ERR_MSG("Can not allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	set_touch_handle(client, ts);

	ts->client = client;
	ts->pdata = client->dev.platform_data;
#if defined(CONFIG_TOUCH_REG_MAP_TM2000)
	ts->ic_panel_type = IC7020_G2_H_PTN;
#elif defined(CONFIG_TOUCH_REG_MAP_TM2372)
	ts->ic_panel_type = IC7020_GFF_H_PTN;
#endif

	if (ts->pdata->pwr->use_regulator) {
		ts->regulator_vdd =
			regulator_get_exclusive(NULL, ts->pdata->pwr->vdd);
		if (IS_ERR(ts->regulator_vdd)) {
			TOUCH_ERR_MSG("FAIL: regulator_get_vdd - %s\n",
							ts->pdata->pwr->vdd);
			ret = -EPERM;
			goto err_get_vdd_failed;
		}

		ts->regulator_vio = regulator_get_exclusive(NULL,
							ts->pdata->pwr->vio);
		if (IS_ERR(ts->regulator_vio)) {
			TOUCH_ERR_MSG("FAIL: regulator_get_vio - %s\n",
							ts->pdata->pwr->vio);
			ret = -EPERM;
			goto err_get_vio_failed;
		}

		if (ts->pdata->pwr->vdd_voltage > 0) {
			ret = regulator_set_voltage(ts->regulator_vdd,
						ts->pdata->pwr->vdd_voltage,
						ts->pdata->pwr->vdd_voltage);
			if (ret < 0) {
				TOUCH_ERR_MSG("FAIL: VDD voltage setting"
						" - (%duV)\n",
						ts->pdata->pwr->vdd_voltage);
				ret = -EPERM;
				goto err_set_voltage;
			}
		}

		if (ts->pdata->pwr->vio_voltage > 0) {
			ret = regulator_set_voltage(ts->regulator_vio,
						ts->pdata->pwr->vio_voltage,
						ts->pdata->pwr->vio_voltage);
			if (ret < 0) {
				TOUCH_ERR_MSG("FAIL: VIO voltage setting"
						" - (%duV)\n",
						ts->pdata->pwr->vio_voltage);
				ret = -EPERM;
				goto err_set_voltage;
			}
		}
	}

	return ret;

err_set_voltage:
	if (ts->pdata->pwr->use_regulator) {
		regulator_put(ts->regulator_vio);
	}
err_get_vio_failed:
	if (ts->pdata->pwr->use_regulator) {
		regulator_put(ts->regulator_vdd);
	}
err_get_vdd_failed:
	kfree(ts);
err_alloc_data_failed:
	return ret;
}

void synaptics_ts_remove(struct i2c_client* client)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);

	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	if (ts->pdata->pwr->use_regulator) {
		regulator_put(ts->regulator_vio);
		regulator_put(ts->regulator_vdd);
	}

	kfree(ts);
}

int synaptics_ts_fw_upgrade(struct i2c_client* client, const char* fw_path)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	int ret = 0;

	ts->is_probed = 0;

	ret = FirmwareUpgrade(ts, fw_path);

	/* update IC info */
	get_ic_info(ts, ts->fw_info);

	return ret;
}

int synaptics_ts_ic_ctrl(struct i2c_client *client, u8 code, u16 value)
{
	struct synaptics_ts_data* ts =
			(struct synaptics_ts_data*)get_touch_handle(client);
	u8 buf = 0;
	u8 new;

	switch (code) {
	case IC_CTRL_BASELINE:
		switch (value) {
		case BASELINE_OPEN:
			if (!ts->analog_dsc.id) /* If not supported, ignore */
				break;

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
			if (unlikely(touch_i2c_write_byte(client,
						PAGE_SELECT_REG, 0x01) < 0)) {
				TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
				return -EIO;
			}

			if (unlikely(touch_i2c_write_byte(client,
						ANALOG_CONTROL_REG,
						FORCE_FAST_RELAXATION) < 0)) {
				TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
				return -EIO;
			}

			msleep(10);

			if (unlikely(touch_i2c_write_byte(client,
						ANALOG_COMMAND_REG,
						FORCE_UPDATE) < 0)) {
				TOUCH_ERR_MSG("force fast relaxation command write fail\n");
				return -EIO;
			}

			if (unlikely(touch_i2c_write_byte(client,
						PAGE_SELECT_REG, 0x00) < 0)) {
				TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
				return -EIO;
			}

			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("BASELINE_OPEN  ~~~~~~~~\n");
#else
			if (unlikely(touch_i2c_write_byte(client,
						MELT_CONTROL_REG,
						MELT_CONTROL_MELT) < 0)) {
				TOUCH_ERR_MSG("MELT_CONTROL_REG write fail\n");
				return -EIO;
			}
#endif
			break;
		case BASELINE_FIX:
			if (!ts->analog_dsc.id) /* If not supported, ignore */
				break;

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
			if (unlikely(touch_i2c_write_byte(client,
						PAGE_SELECT_REG, 0x01) < 0)) {
				TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
				return -EIO;
			}

			if (unlikely(touch_i2c_write_byte(client,
						ANALOG_CONTROL_REG, 0x0) < 0)) {
				TOUCH_ERR_MSG("ANALOG_CONTROL_REG write fail\n");
				return -EIO;
			}

<<<<<<< HEAD
			msleep(10);
=======
	ret = request_threaded_irq(client->irq, NULL, touch_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND, client->name, ts);
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

			if (unlikely(touch_i2c_write_byte(client,
						ANALOG_COMMAND_REG,
						FORCE_UPDATE) < 0)) {
				TOUCH_ERR_MSG("force fast relaxation command write fail\n");
				return -EIO;
			}

			if (unlikely(touch_i2c_write_byte(client,
						PAGE_SELECT_REG, 0x00) < 0)) {
				TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
				return -EIO;
			}

			if (unlikely(touch_debug_mask & DEBUG_GHOST))
				TOUCH_INFO_MSG("BASELINE_FIX  ~~~~~~~~\n");
#else
			if (unlikely(touch_i2c_write_byte(client,
						MELT_CONTROL_REG,
						MELT_CONTROL_NO_MELT) < 0)) {
				TOUCH_ERR_MSG("MELT_CONTROL_REG write fail\n");
				return -EIO;
			}
#endif
			break;
		case BASELINE_REBASE:
			/* rebase base line */
			if (likely(ts->finger_dsc.id != 0)) {
				if (unlikely(touch_i2c_write_byte(client,
						FINGER_COMMAND_REG, 0x1) < 0)) {
					TOUCH_ERR_MSG("finger baseline reset "
							"command write fail\n");
					return -EIO;
				}
			}

#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
/* do nothing */
#else
			if (unlikely(ts->button_dsc.id != 0)) {
				if (unlikely(touch_i2c_write_byte(client,
						BUTTON_COMMAND_REG, 0x1) < 0)) {
					TOUCH_ERR_MSG("finger baseline reset "
							"command write fail\n");
					return -EIO;
				}
			}
#endif
			break;
		default:
			break;
		}
		break;
	case IC_CTRL_READ:
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
		if (unlikely(touch_i2c_write_byte(client,
			PAGE_SELECT_REG, ((value & 0xFF00) >> 8)) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}

		if (touch_i2c_read(client, (value & 0xFF), 1, &buf) < 0) {
			TOUCH_ERR_MSG("IC register read fail\n");
			return -EIO;
		}

		if (unlikely(touch_i2c_write_byte(client,
						PAGE_SELECT_REG, 0x00) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}
#else
		if (touch_i2c_read(client, value, 1, &buf) < 0) {
			TOUCH_ERR_MSG("IC register read fail\n");
			return -EIO;
		}
#endif
		break;
	case IC_CTRL_WRITE:
#if defined(CONFIG_TOUCH_REG_MAP_TM2000) || defined(CONFIG_TOUCH_REG_MAP_TM2372)
		if (unlikely(touch_i2c_write_byte(client,
			PAGE_SELECT_REG, ((value & 0xFF0000) >> 16)) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}

		if (touch_i2c_write_byte(client,
				((value & 0xFF00) >> 8), (value & 0xFF)) < 0) {
			TOUCH_ERR_MSG("IC register write fail\n");
			return -EIO;
		}

		if (unlikely(touch_i2c_write_byte(client,
						PAGE_SELECT_REG, 0x00) < 0)) {
			TOUCH_ERR_MSG("PAGE_SELECT_REG write fail\n");
			return -EIO;
		}
#else
		if (touch_i2c_write_byte(client,
				((value & 0xFF00) >> 8), (value & 0xFF)) < 0) {
			TOUCH_ERR_MSG("IC register write fail\n");
			return -EIO;
		}
#endif
		break;
	case IC_CTRL_RESET_CMD:
		if (unlikely(touch_i2c_write_byte(client,
						DEVICE_COMMAND_REG, 0x1) < 0)) {
			TOUCH_ERR_MSG("IC Reset command write fail\n");
			return -EIO;
		}
		break;
	case IC_CTRL_CHARGER:
		if (touch_i2c_read(client, DEVICE_CONTROL_REG, 1, &buf) < 0) {
			TOUCH_ERR_MSG("IC register read fail\n");
			return -EIO;
		}

		new = buf & ~CHARGER_CONNECTED;
		new |= value ? CHARGER_CONNECTED : 0;

		if (new != buf) {
			if (unlikely(touch_i2c_write_byte(client,
					DEVICE_CONTROL_REG, new) < 0)) {
				TOUCH_ERR_MSG("IC Reset command write fail\n");
				return -EIO;
			}
			TOUCH_INFO_MSG("CHARGER = %d\n", !!value);
		}

<<<<<<< HEAD
		break;
	default:
		break;
	}
=======
	touch_synaptics_sysfs_deinit();

	/* Power off */
	touch_power_cntl(ts, POWER_OFF);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	kfree(ts);
>>>>>>> cdd22a0... drivers/input/touchscreen/touch_synaptics: patch with sweep2wake hooks as well

	return buf;
}

int synaptics_ts_fw_upgrade_check(struct lge_touch_data *ts)
{
	if (ts->fw_info.fw_force_rework || ts->fw_upgrade.fw_force_upgrade) {
		TOUCH_INFO_MSG("FW-upgrade Force Rework.\n");
	} else {
		if (((int)simple_strtoul(&ts->fw_info.fw_version[1], NULL, 10) >= (int)simple_strtoul(&ts->fw_info.fw_image_version[1], NULL, 10))) {
			TOUCH_INFO_MSG("DO NOT UPDATE 7020 G2 H " "pattern FW-upgrade is not executed\n");
			return -1;
		} else {
			TOUCH_INFO_MSG("7020 G2 H pattern FW-upgrade " "is executed\n");
		}
	}

	return 0;
}

struct touch_device_driver synaptics_ts_driver = {
	.probe            = synaptics_ts_probe,
	.remove           = synaptics_ts_remove,
	.init             = synaptics_ts_init,
	.data             = synaptics_ts_get_data,
	.power            = synaptics_ts_power,
	.fw_upgrade       = synaptics_ts_fw_upgrade,
	.ic_ctrl          = synaptics_ts_ic_ctrl,
	.fw_upgrade_check = synaptics_ts_fw_upgrade_check,
};

static int __devinit touch_init(void)
{
	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	return touch_driver_register(&synaptics_ts_driver);
}

static void __exit touch_exit(void)
{
	if (touch_debug_mask & DEBUG_TRACE)
		TOUCH_DEBUG_MSG("\n");

	touch_driver_unregister();
}

module_init(touch_init);
module_exit(touch_exit);

MODULE_AUTHOR("yehan.ahn@lge.com, hyesung.shin@lge.com");
MODULE_DESCRIPTION("LGE Touch Driver");
MODULE_LICENSE("GPL");

