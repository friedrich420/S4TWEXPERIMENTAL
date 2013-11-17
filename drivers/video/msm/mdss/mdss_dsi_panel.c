/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/err.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/ctype.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
#include <linux/input/sweep2wake.h>
#endif
#include "mdss_dsi.h"

#define DT_CMD_HDR 6

static struct dsi_buf dsi_panel_tx_buf;
static struct dsi_buf dsi_panel_rx_buf;

static struct dsi_cmd_desc *dsi_panel_on_cmds;
static struct dsi_cmd_desc *dsi_panel_off_cmds;
static int num_of_on_cmds;
static int num_of_off_cmds;
static char *on_cmds, *off_cmds;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1)},
	led_pwm1
};

static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;

	pr_debug("%s: level=%d\n", __func__, level);

	led_pwm1[1] = (unsigned char)level;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &backlight_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

void mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);

	if (enable) {
		if (mdss_panel_id == PANEL_LGE_JDI_ORISE_VIDEO ||
			mdss_panel_id == PANEL_LGE_JDI_ORISE_CMD ||
			mdss_panel_id == PANEL_LGE_JDI_NOVATEK_VIDEO ||
			mdss_panel_id == PANEL_LGE_JDI_NOVATEK_CMD) {
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
			usleep(20 * 1000);
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			usleep(15 * 1000);
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			udelay(20);
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			usleep(10 * 1000);
		} else {
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			msleep(20);
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			udelay(200);
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			msleep(20);
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
		}
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		if (mdss_panel_id == PANEL_LGE_JDI_ORISE_VIDEO ||
			mdss_panel_id == PANEL_LGE_JDI_ORISE_CMD ||
			mdss_panel_id == PANEL_LGE_JDI_NOVATEK_VIDEO ||
			mdss_panel_id == PANEL_LGE_JDI_NOVATEK_CMD) {
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
			if (s2w_switch == 0)
#endif
			{
				if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
					gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
				usleep(20 * 1000);
				gpio_set_value((ctrl_pdata->rst_gpio), 0);
			}
		} else {
#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
			if (s2w_switch == 0)
#endif
			{
				gpio_set_value((ctrl_pdata->rst_gpio), 0);
				if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
					gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
			}
		}
	}
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
#if defined(CONFIG_BACKLIGHT_LM3630)
		lm3630_lcd_backlight_set_level(bl_level);
#else
		led_trigger_event(bl_led_trigger, bl_level);
#endif
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
		mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
}
>>>>>>> 7860be2... mdss_dsi_panel: keep the panel gpio active and don't trigger a reset

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;

	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s:%d, debug info (mode) : %d\n", __func__, __LINE__,
		 mipi->mode);

	if (mipi->mode == DSI_VIDEO_MODE) {
		mdss_dsi_cmds_tx(pdata, &dsi_panel_tx_buf, dsi_panel_on_cmds,
			num_of_on_cmds);
	} else {
		pr_err("%s:%d, CMD MODE NOT SUPPORTED", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;

	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s:%d, debug info\n", __func__, __LINE__);

	if (!gpio_get_value(ctrl->disp_en_gpio))
		return 0;

#ifdef CONFIG_TOUCHSCREEN_SWEEP2WAKE
	if (s2w_switch) {
		ctrl->off_cmds.cmds[1].payload[0] = 0x11;
	} else {
		ctrl->off_cmds.cmds[1].payload[0] = 0x10;
	}
	pr_info("[sweep2wake] payload = %x \n", ctrl->off_cmds.cmds[1].payload[0]);
#endif

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

	pr_info("%s:\n", __func__);
	return 0;
}

static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, blen);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len > sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			return -ENOMEM;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		kfree(buf);
		return -ENOMEM;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		return -ENOMEM;
>>>>>>> 49312bd... [PATCH] mdss_dsi: do not power down panel when sweep2wake is enabled

	if (mipi->mode == DSI_VIDEO_MODE) {
		mdss_dsi_cmds_tx(pdata, &dsi_panel_tx_buf, dsi_panel_off_cmds,
			num_of_off_cmds);
	} else {
		pr_debug("%s:%d, CMD mode not supported", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int mdss_panel_parse_dt(struct platform_device *pdev,
			    struct mdss_panel_common_pdata *panel_data)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6], tmp;
	int rc, i, len;
	int cmd_plen, data_offset;
	const char *data;

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-res", res, 2);
	if (rc) {
		pr_err("%s:%d, panel resolution not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.xres = (!rc ? res[0] : 640);
	panel_data->panel_info.yres = (!rc ? res[1] : 480);

	rc = of_property_read_u32(np, "qcom,mdss-pan-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, panel bpp not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	panel_data->panel_info.bpp = (!rc ? tmp : 24);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-porch-values", res, 6);
	panel_data->panel_info.lcdc.h_back_porch = (!rc ? res[0] : 6);
	panel_data->panel_info.lcdc.h_pulse_width = (!rc ? res[1] : 2);
	panel_data->panel_info.lcdc.h_front_porch = (!rc ? res[2] : 6);
	panel_data->panel_info.lcdc.v_back_porch = (!rc ? res[3] : 6);
	panel_data->panel_info.lcdc.v_pulse_width = (!rc ? res[4] : 2);
	panel_data->panel_info.lcdc.v_front_porch = (!rc ? res[5] : 6);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-underflow-clr", &tmp);
	panel_data->panel_info.lcdc.underflow_clr = (!rc ? tmp : 0xff);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-bl-levels", res, 2);
	panel_data->panel_info.bl_min = (!rc ? res[0] : 0);
	panel_data->panel_info.bl_max = (!rc ? res[1] : 255);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mode", &tmp);
	panel_data->panel_info.mipi.mode = (!rc ? tmp : DSI_VIDEO_MODE);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-h-pulse-mode", &tmp);
	panel_data->panel_info.mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-h-power-stop", res, 3);
	panel_data->panel_info.mipi.hbp_power_stop = (!rc ? res[0] : false);
	panel_data->panel_info.mipi.hsa_power_stop = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.hfp_power_stop = (!rc ? res[2] : false);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-bllp-power-stop", res, 2);
	panel_data->panel_info.mipi.bllp_power_stop =
					(!rc ? res[0] : false);
	panel_data->panel_info.mipi.eof_bllp_power_stop =
					(!rc ? res[1] : false);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-traffic-mode", &tmp);
	panel_data->panel_info.mipi.traffic_mode =
			(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-dst-format", &tmp);
	panel_data->panel_info.mipi.dst_format =
			(!rc ? tmp : DSI_VIDEO_DST_FORMAT_RGB888);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-vc", &tmp);
	panel_data->panel_info.mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-rgb-swap", &tmp);
	panel_data->panel_info.mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);

	rc = of_property_read_u32_array(np,
		"qcom,mdss-pan-dsi-data-lanes", res, 4);
	panel_data->panel_info.mipi.data_lane0 = (!rc ? res[0] : true);
	panel_data->panel_info.mipi.data_lane1 = (!rc ? res[1] : false);
	panel_data->panel_info.mipi.data_lane2 = (!rc ? res[2] : false);
	panel_data->panel_info.mipi.data_lane3 = (!rc ? res[3] : false);

	rc = of_property_read_u32_array(np, "qcom,mdss-pan-dsi-t-clk", res, 2);
	panel_data->panel_info.mipi.t_clk_pre = (!rc ? res[0] : 0x24);
	panel_data->panel_info.mipi.t_clk_post = (!rc ? res[1] : 0x03);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-stream", &tmp);
	panel_data->panel_info.mipi.stream = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-mdp-tr", &tmp);
	panel_data->panel_info.mipi.mdp_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.mdp_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-dma-tr", &tmp);
	panel_data->panel_info.mipi.dma_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (panel_data->panel_info.mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		panel_data->panel_info.mipi.dma_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-pan-dsi-frame-rate", &tmp);
	panel_data->panel_info.mipi.frame_rate = (!rc ? tmp : 60);

	data = of_get_property(np, "qcom,panel-on-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read ON cmds", __func__, __LINE__);
		goto error;
	}

	on_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!on_cmds)
		return -ENOMEM;

	memcpy(on_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = on_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_on_cmds++;
	}
	if (!num_of_on_cmds) {
		pr_err("%s:%d, No ON cmds specified", __func__, __LINE__);
		goto error;
	}

	dsi_panel_on_cmds =
		kzalloc((num_of_on_cmds * sizeof(struct dsi_cmd_desc)),
						GFP_KERNEL);
	if (!dsi_panel_on_cmds)
		return -ENOMEM;

	data_offset = 0;
	for (i = 0; i < num_of_on_cmds; i++) {
		dsi_panel_on_cmds[i].dtype = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].last = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].vc = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].ack = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].wait = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].dlen = on_cmds[data_offset++];
		dsi_panel_on_cmds[i].payload = &on_cmds[data_offset];
		data_offset += (dsi_panel_on_cmds[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect ON command entries",
						__func__, __LINE__);
		goto error;
	}

	data = of_get_property(np, "qcom,panel-off-cmds", &len);
	if (!data) {
		pr_err("%s:%d, Unable to read OFF cmds", __func__, __LINE__);
		goto error;
	}

	off_cmds = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!off_cmds)
		return -ENOMEM;

	memcpy(off_cmds, data, len);

	data_offset = 0;
	cmd_plen = 0;
	while ((len - data_offset) >= DT_CMD_HDR) {
		data_offset += (DT_CMD_HDR - 1);
		cmd_plen = off_cmds[data_offset++];
		data_offset += cmd_plen;
		num_of_off_cmds++;
	}
	if (!num_of_off_cmds) {
		pr_err("%s:%d, No OFF cmds specified", __func__, __LINE__);
		goto error;
	}

	dsi_panel_off_cmds = kzalloc(num_of_off_cmds
				* sizeof(struct dsi_cmd_desc),
					GFP_KERNEL);
	if (!dsi_panel_off_cmds)
		return -ENOMEM;

	data_offset = 0;
	for (i = 0; i < num_of_off_cmds; i++) {
		dsi_panel_off_cmds[i].dtype = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].last = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].vc = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].ack = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].wait = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].dlen = off_cmds[data_offset++];
		dsi_panel_off_cmds[i].payload = &off_cmds[data_offset];
		data_offset += (dsi_panel_off_cmds[i].dlen);
	}

	if (data_offset != len) {
		pr_err("%s:%d, Incorrect OFF command entries",
						__func__, __LINE__);
		goto error;
	}

	return 0;
error:
	kfree(dsi_panel_on_cmds);
	kfree(dsi_panel_off_cmds);
	kfree(on_cmds);
	kfree(off_cmds);

	return -EINVAL;
}

static int __devinit mdss_dsi_panel_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct mdss_panel_common_pdata *vendor_pdata = NULL;
	static const char *panel_name;

	if (pdev->dev.parent == NULL) {
		pr_err("%s: parent device missing\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s:%d, debug info id=%d", __func__, __LINE__, pdev->id);
	if (!pdev->dev.of_node)
		return -ENODEV;

	panel_name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!panel_name)
		pr_info("%s:%d, panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	vendor_pdata = devm_kzalloc(&pdev->dev,
			sizeof(*vendor_pdata), GFP_KERNEL);
	if (!vendor_pdata)
		return -ENOMEM;

	rc = mdss_panel_parse_dt(pdev, vendor_pdata);
	if (rc) {
		devm_kfree(&pdev->dev, vendor_pdata);
		vendor_pdata = NULL;
		return rc;
	}
	vendor_pdata->on = mdss_dsi_panel_on;
	vendor_pdata->off = mdss_dsi_panel_off;

	rc = dsi_panel_device_register(pdev, vendor_pdata);
	if (rc)
		return rc;

	return 0;
}

static const struct of_device_id mdss_dsi_panel_match[] = {
	{.compatible = "qcom,mdss-dsi-panel"},
	{}
};

static struct platform_driver this_driver = {
	.probe  = mdss_dsi_panel_probe,
	.driver = {
		.name   = "dsi_panel",
		.of_match_table = mdss_dsi_panel_match,
	},
};

static int __init mdss_dsi_panel_init(void)
{
	mdss_dsi_buf_alloc(&dsi_panel_tx_buf, DSI_BUF_SIZE);
	mdss_dsi_buf_alloc(&dsi_panel_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}
module_init(mdss_dsi_panel_init);
