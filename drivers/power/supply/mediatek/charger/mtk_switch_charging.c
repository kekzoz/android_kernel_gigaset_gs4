/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 *
 * Filename:
 * ---------
 *    mtk_switch_charging.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 *   This Module defines functions of Battery charging
 *
 * Author:
 * -------
 * Wy Chuang
 *
 */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>

#include <mt-plat/mtk_boot.h>
/* #include <musb_core.h> */ /* FIXME */
#include "mtk_charger_intf.h"
#include "mtk_switch_charging.h"
#if defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
#include <linux/of.h>
#endif
#if defined(CONFIG_PRIZE_CHARGE_CTRL_POLICY)
extern int g_charge_is_screen_on;
#endif
//end add by sunshuai for Bright screen current limit   20181130
//prize added by huarui, wireless charge soft start, 20190111-start
#if defined(CONFIG_PRIZE_WLC_SOFT_START)
#include <linux/of.h>
static void wlc_cur_step_work_func(struct work_struct *data);
static enum hrtimer_restart wlc_cur_step_timeout(struct hrtimer *timer);
static int wlc_init(struct charger_manager *info);
//static void wlc_parse_dts(struct charger_manager *info);

struct wlc_info_t {
	int chg_phase;//0:idle 1:increase 2:cc
	int cur_step_count;
	int cur_limit_input;
	int cur_limit_output;

	int cur_step_pattern_num;
	int *cur_step_pattern_input;
	int *cur_step_pattern_output;

	struct hrtimer cur_step_timer;
	struct work_struct cur_step_work;
	struct wakeup_source cur_step_ws;
	struct charger_manager *cm_info;
};
static struct wlc_info_t wlc_info;
#endif
//prize added by huarui, wireless charge soft start, 20190111-end

//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-start
#if defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
struct MT5715_wlc_t {
     struct wakeup_source cur_step_ws_wk;
	 struct hrtimer MT5715_cur_step_timer;
	 int ret_ldo_status;
	 int MT5715_up_status;
	 int ret_ldo_status_last;
	 int fast_vol_status;
	 struct work_struct MT5715_cur_step_work;
	 int MT5715_chg_phase;  //0:idle 1:increase 2:cc
	 int MT5715_cur_step_count;
	 int MT5715_wait_cunt;
	 int *mt5715_input_vol;
	 int *mt5715_vol;
	 int  cur_step_pattern_num;
	 int fake_sam_wait_cunt;
	 struct charger_manager *mt5715_cm_info;
};

static struct MT5715_wlc_t MT5715_wlc_info;
static void MT5715_cur_step_work_func(struct work_struct *data);
static enum hrtimer_restart MT5715_cur_step_timeout(struct hrtimer *timer);
static int MT5715_wlc_parse_dts(struct charger_manager *info);
static int MT5715_init(struct charger_manager *info);
extern int get_MT5715on_status(void);
extern int get_lod_status(void);
extern int  fast_sv(int temp);
extern int get_is_samsung_charge (void);
extern int  fast_sv_no_samsung(int temp);
extern int set_is_samsung_charge(int temp);
extern int confirm_MT5715_works(void);


#endif
//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-end

static int _uA_to_mA(int uA)
{
	if (uA == -1)
		return -1;
	else
		return uA / 1000;
}

static void _disable_all_charging(struct charger_manager *info)
{
	charger_dev_enable(info->chg1_dev, false);

	if (mtk_pe20_get_is_enable(info)) {
		mtk_pe20_set_is_enable(info, false);
		if (mtk_pe20_get_is_connect(info))
			mtk_pe20_reset_ta_vchr(info);
	}

	if (mtk_pe_get_is_enable(info)) {
		mtk_pe_set_is_enable(info, false);
		if (mtk_pe_get_is_connect(info))
			mtk_pe_reset_ta_vchr(info);
	}

#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
	if (mtk_pe50_get_is_enable(info)) {
		if (mtk_pe50_get_is_connect(info))
			mtk_pe50_stop_algo(info, true);
	}
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
	if (mtk_pe40_get_is_enable(info)) {
		if (mtk_pe40_get_is_connect(info))
			mtk_pe40_end(info, 3, true);
	}

	if (mtk_pdc_check_charger(info))
		mtk_pdc_reset(info);
}

static void swchg_select_charging_current_limit(struct charger_manager *info)
{
	struct charger_data *pdata;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	u32 ichg1_min = 0, aicr1_min = 0;
	int ret = 0;
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
	if (info->pe5.online) {
		chr_err("In PE5.0\n");
		return;
	}
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
	pdata = &info->chg1_data;
	mutex_lock(&swchgalg->ichg_aicr_access_mutex);

	/* AICL */
	if (!mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info) && !mtk_pdc_check_charger(info)) {
		charger_dev_run_aicl(info->chg1_dev,
				&pdata->input_current_limit_by_aicl);
		if (info->enable_dynamic_mivr) {
			if (pdata->input_current_limit_by_aicl >
				info->data.max_dmivr_charger_current)
				pdata->input_current_limit_by_aicl =
					info->data.max_dmivr_charger_current;
		}
	}

	if (pdata->force_charging_current > 0) {

		pdata->charging_current_limit = pdata->force_charging_current;
		if (pdata->force_charging_current <= 450000) {
			pdata->input_current_limit = 500000;
		} else {
			pdata->input_current_limit =
					info->data.ac_charger_input_current;
			pdata->charging_current_limit =
					info->data.ac_charger_current;
		}
		goto done;
	}

	if (info->usb_unlimited) {
		pdata->input_current_limit =
					info->data.ac_charger_input_current;
		pdata->charging_current_limit =
					info->data.ac_charger_current;
		goto done;
	}

	if (info->water_detected) {
		pdata->input_current_limit = info->data.usb_charger_current;
		pdata->charging_current_limit = info->data.usb_charger_current;
		goto done;
	}

	if ((get_boot_mode() == META_BOOT) ||
	    (get_boot_mode() == ADVMETA_BOOT)) {
		pdata->input_current_limit = 200000; /* 200mA */
		goto done;
	}

	if (info->atm_enabled == true && (info->chr_type == STANDARD_HOST ||
	    info->chr_type == CHARGING_HOST)) {
		pdata->input_current_limit = 100000; /* 100mA */
		goto done;
	}

	if (mtk_is_TA_support_pd_pps(info)) {
		pdata->input_current_limit =
			info->data.pe40_single_charger_input_current;
		pdata->charging_current_limit =
			info->data.pe40_single_charger_current;
	} else if (is_typec_adapter(info)) {
		if (adapter_dev_get_property(info->pd_adapter, TYPEC_RP_LEVEL)
			== 3000) {
			pdata->input_current_limit = 3000000;
			pdata->charging_current_limit = 3000000;
		} else if (adapter_dev_get_property(info->pd_adapter,
			TYPEC_RP_LEVEL) == 1500) {
			pdata->input_current_limit = 1500000;
			pdata->charging_current_limit = 2000000;
		} else {
			chr_err("type-C: inquire rp error\n");
			pdata->input_current_limit = 500000;
			pdata->charging_current_limit = 500000;
		}

		chr_err("type-C:%d current:%d\n",
			info->pd_type,
			adapter_dev_get_property(info->pd_adapter,
				TYPEC_RP_LEVEL));
	} else if (mtk_pdc_check_charger(info)) {
		int vbus = 0, cur = 0, idx = 0;

		ret = mtk_pdc_get_setting(info, &vbus, &cur, &idx);
		if (ret != -1 && idx != -1) {
			pdata->input_current_limit = cur * 1000;
			pdata->charging_current_limit =
				info->data.pd_charger_current;
			mtk_pdc_setup(info, idx);
		} else {
			pdata->input_current_limit =
				info->data.usb_charger_current_configured;
			pdata->charging_current_limit =
				info->data.usb_charger_current_configured;
		}
		chr_err("[%s]vbus:%d input_cur:%d idx:%d current:%d\n",
			__func__, vbus, cur, idx,
			info->data.pd_charger_current);

	} else if (info->chr_type == STANDARD_HOST) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)) {
			if (info->usb_state == USB_SUSPEND)
				pdata->input_current_limit =
					info->data.usb_charger_current_suspend;
			else if (info->usb_state == USB_UNCONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;
			else if (info->usb_state == USB_CONFIGURED)
				pdata->input_current_limit =
				info->data.usb_charger_current_configured;
			else
				pdata->input_current_limit =
				info->data.usb_charger_current_unconfigured;

			pdata->charging_current_limit =
					pdata->input_current_limit;
		} else {
			pdata->input_current_limit =
					info->data.usb_charger_current;
			/* it can be larger */
			pdata->charging_current_limit =
					info->data.usb_charger_current;
		}
	} else if (info->chr_type == NONSTANDARD_CHARGER) {
		pdata->input_current_limit =
				info->data.non_std_ac_charger_current;
		pdata->charging_current_limit =
				info->data.non_std_ac_charger_current;

//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-start
#if defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
        if(MT5715_wlc_info.MT5715_cur_step_count > 0 && MT5715_wlc_info.MT5715_cur_step_count < MT5715_wlc_info.cur_step_pattern_num){
			pdata->input_current_limit = MT5715_wlc_info.mt5715_input_vol[MT5715_wlc_info.MT5715_cur_step_count];
			pdata->charging_current_limit = MT5715_wlc_info.mt5715_vol[MT5715_wlc_info.MT5715_cur_step_count];
			chr_err("%s MT5715_wlc_info.mt5715_vol[%d] =%d\n", __func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.mt5715_vol[MT5715_wlc_info.MT5715_cur_step_count]);
			chr_err("%s MT5715_wlc_info.mt5715_input_vol[%d] =%d\n", __func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.mt5715_input_vol[MT5715_wlc_info.MT5715_cur_step_count]);
        }
#endif
//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-end
				//prize added by huarui, wireless charge soft start, 20190111-start
#if defined(CONFIG_PRIZE_WLC_SOFT_START)
	if ((info->chr_type == NONSTANDARD_CHARGER)&&wlc_info.cur_step_pattern_num){
		if (pdata->input_current_limit > wlc_info.cur_limit_input){
			pdata->input_current_limit = wlc_info.cur_limit_input;
		}
		if (pdata->charging_current_limit > wlc_info.cur_limit_output){
			pdata->charging_current_limit = wlc_info.cur_limit_output;
		}
	}
#endif
//prize added by huarui, wireless charge soft start, 20190111-end
	} else if (info->chr_type == STANDARD_CHARGER) {
		pdata->input_current_limit =
				info->data.ac_charger_input_current;
		pdata->charging_current_limit =
				info->data.ac_charger_current;
		mtk_pe20_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
		mtk_pe_set_charging_current(info,
					&pdata->charging_current_limit,
					&pdata->input_current_limit);
	} else if (info->chr_type == CHARGING_HOST) {
		pdata->input_current_limit =
				info->data.charging_host_charger_current;
		pdata->charging_current_limit =
				info->data.charging_host_charger_current;
	} else if (info->chr_type == APPLE_1_0A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_1_0a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_1_0a_charger_current;
	} else if (info->chr_type == APPLE_2_1A_CHARGER) {
		pdata->input_current_limit =
				info->data.apple_2_1a_charger_current;
		pdata->charging_current_limit =
				info->data.apple_2_1a_charger_current;
	}

/*prize-add by sunshuai for for Vietnam customer  20200106 start  */
#if defined(CONFIG_PRIZE_CHARGE_CTRL_VIETNAM)
		if(info->step_info.current_step == STEP_T1){
		   if(pdata->charging_current_limit > 780000)
			  pdata->charging_current_limit = 780000;

		   if (pdata->input_current_limit > 780000)
			pdata->input_current_limit = 780000;
		}

		if(info->step_info.current_step == STEP_T3){
		   if(pdata->charging_current_limit > 1500000)
			  pdata->charging_current_limit = 1500000;

		   if (pdata->input_current_limit > 1500000)
			pdata->input_current_limit = 1500000;
		}
#endif
/*prize-add by sunshuai for for Vietnam customer  20200106 end	*/

#if defined(CONFIG_PRIZE_CHARGE_CTRL_VIETNAM)
		 if (g_charge_is_screen_on){
			switch (info->step_info.current_step) {
				case STEP_T1:
					if (pdata->charging_current_limit > 780000){
					  pdata->charging_current_limit = 780000;
					}

					if (pdata->input_current_limit > 780000){
					   pdata->input_current_limit = 780000;
					}
					chr_err("swchg_select_charging_current_limit  select  info->step_info.current_step =%d\n",info->step_info.current_step);
				break;

				case STEP_T2:
					if (pdata->charging_current_limit > 1300000){
					  pdata->charging_current_limit = 1300000;
					}

					if (pdata->input_current_limit > 1300000){
					   pdata->input_current_limit = 1300000;
					}
					chr_err("swchg_select_charging_current_limit  select  info->step_info.current_step =%d\n",info->step_info.current_step);
				break;

				case STEP_T3:
					if (pdata->charging_current_limit > 500000){
					  pdata->charging_current_limit = 500000;
					}

					if (pdata->input_current_limit > 500000){
					   pdata->input_current_limit = 500000;
					}
					chr_err("swchg_select_charging_current_limit  select  info->step_info.current_step =%d\n",info->step_info.current_step);
				break;

				default:
					chr_err("swchg_select_charging_current_limit default select  info->step_info.current_step =%d\n",info->step_info.current_step);
					break;
				}
		 }
#endif

	if (info->enable_sw_jeita) {
		if (IS_ENABLED(CONFIG_USBIF_COMPLIANCE)
		    && info->chr_type == STANDARD_HOST)
			pr_debug("USBIF & STAND_HOST skip current check\n");
		else {
			if (info->sw_jeita.sm == TEMP_T0_TO_T1) {
				pdata->input_current_limit = 500000;
				pdata->charging_current_limit = 350000;
			}
		}
	}
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
#ifndef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
	//start add by sunshuai for Bright screen current limit  20181130
#if defined(CONFIG_PRIZE_CHARGE_CTRL_POLICY)
		if (g_charge_is_screen_on){
			if (pdata->charging_current_limit > 1000000){
				pdata->charging_current_limit = 1000000;
			}
			if (pdata->input_current_limit > 1000000){
				pdata->input_current_limit = 1000000;
			}
//start add by sunshuai for Bright screen current limit  for master charge	2019-0429
			if ((mtk_pe20_get_is_enable(info) && mtk_pe20_get_is_connect(info))
				|| (mtk_pe_get_is_enable(info) && mtk_pe_get_is_connect(info))){
				pdata->input_current_limit = 700000;
				pdata->charging_current_limit = 1000000;
			}
		}
		printk("PRIZE master  charge current %d:%d\n",pdata->input_current_limit,pdata->charging_current_limit);
//end add by sunshuai for Bright screen current limit  for master charge  2019-0429

#endif
//end add by sunshuai for Bright screen current limit	   20181130
#endif
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
	if (pdata->thermal_charging_current_limit != -1) {
		if (pdata->thermal_charging_current_limit <
		    pdata->charging_current_limit)
			pdata->charging_current_limit =
					pdata->thermal_charging_current_limit;
	}

	if (pdata->thermal_input_current_limit != -1) {
		if (pdata->thermal_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->thermal_input_current_limit;
	}

	if (mtk_pe40_get_is_connect(info)) {
		if (info->pe4.pe4_input_current_limit != -1 &&
		    info->pe4.pe4_input_current_limit <
		    pdata->input_current_limit)
			pdata->input_current_limit =
				info->pe4.pe4_input_current_limit;

		info->pe4.input_current_limit = pdata->input_current_limit;

		if (info->pe4.pe4_input_current_limit_setting != -1 &&
		    info->pe4.pe4_input_current_limit_setting <
		    pdata->input_current_limit)
			pdata->input_current_limit =
				info->pe4.pe4_input_current_limit_setting;
	}

	if (pdata->input_current_limit_by_aicl != -1 &&
	    !mtk_pe20_get_is_connect(info) && !mtk_pe_get_is_connect(info) &&
	    !mtk_is_TA_support_pd_pps(info)) {
		if (pdata->input_current_limit_by_aicl <
		    pdata->input_current_limit)
			pdata->input_current_limit =
					pdata->input_current_limit_by_aicl;
	}
done:
	ret = charger_dev_get_min_charging_current(info->chg1_dev, &ichg1_min);
	if (ret != -ENOTSUPP && pdata->charging_current_limit < ichg1_min)
		pdata->charging_current_limit = 0;

	ret = charger_dev_get_min_input_current(info->chg1_dev, &aicr1_min);
	if (ret != -ENOTSUPP && pdata->input_current_limit < aicr1_min)
		pdata->input_current_limit = 0;

	chr_err("force:%d thermal:%d,%d pe4:%d,%d,%d setting:%d %d type:%d usb_unlimited:%d usbif:%d usbsm:%d aicl:%d atm:%d\n",
		_uA_to_mA(pdata->force_charging_current),
		_uA_to_mA(pdata->thermal_input_current_limit),
		_uA_to_mA(pdata->thermal_charging_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit),
		_uA_to_mA(info->pe4.pe4_input_current_limit_setting),
		_uA_to_mA(info->pe4.input_current_limit),
		_uA_to_mA(pdata->input_current_limit),
		_uA_to_mA(pdata->charging_current_limit),
		info->chr_type, info->usb_unlimited,
		IS_ENABLED(CONFIG_USBIF_COMPLIANCE), info->usb_state,
		pdata->input_current_limit_by_aicl, info->atm_enabled);

	charger_dev_set_input_current(info->chg1_dev,
					pdata->input_current_limit);
	charger_dev_set_charging_current(info->chg1_dev,
					pdata->charging_current_limit);

	/* If AICR < 300mA, stop PE+/PE+20 */
	if (pdata->input_current_limit < 300000) {
		if (mtk_pe20_get_is_enable(info)) {
			mtk_pe20_set_is_enable(info, false);
			if (mtk_pe20_get_is_connect(info))
				mtk_pe20_reset_ta_vchr(info);
		}

		if (mtk_pe_get_is_enable(info)) {
			mtk_pe_set_is_enable(info, false);
			if (mtk_pe_get_is_connect(info))
				mtk_pe_reset_ta_vchr(info);
		}
	}

	/*
	 * If thermal current limit is larger than charging IC's minimum
	 * current setting, enable the charger immediately
	 */
	if (pdata->input_current_limit > aicr1_min &&
	    pdata->charging_current_limit > ichg1_min && info->can_charging)
		charger_dev_enable(info->chg1_dev, true);
	mutex_unlock(&swchgalg->ichg_aicr_access_mutex);
}

static void swchg_select_cv(struct charger_manager *info)
{
	u32 constant_voltage;

	if (info->enable_sw_jeita)
		if (info->sw_jeita.cv != 0) {
			charger_dev_set_constant_voltage(info->chg1_dev,
							info->sw_jeita.cv);
			return;
		}

/*prize-add by sunshuai for for Vietnam customer  20200106 start  */
#if defined(CONFIG_PRIZE_CHARGE_CTRL_VIETNAM)
		if(info->step_info.current_step == STEP_T3){
			if(info->step_info.temp_stp3_cv_voltage != 0){
			   chr_err("swchg_select_cv  info->step_info.temp_stp3_cv_voltage =%d\n",info->step_info.temp_stp3_cv_voltage);
			   constant_voltage = info->step_info.temp_stp3_cv_voltage;
			   mtk_get_dynamic_cv(info, &constant_voltage);

			   charger_dev_set_constant_voltage(info->chg1_dev, constant_voltage);
			   return;
			}
		}
#endif
/*prize-add by sunshuai for for Vietnam customer  20200106 end	*/

	/* dynamic cv*/
	constant_voltage = info->data.battery_cv;
	mtk_get_dynamic_cv(info, &constant_voltage);

	charger_dev_set_constant_voltage(info->chg1_dev, constant_voltage);
}

static void swchg_turn_on_charging(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	bool charging_enable = true;

	if (swchgalg->state == CHR_ERROR) {
		charging_enable = false;
		chr_err("[charger]Charger Error, turn OFF charging !\n");
	} else if ((get_boot_mode() == META_BOOT) ||
			((get_boot_mode() == ADVMETA_BOOT))) {
		charging_enable = false;
		info->chg1_data.input_current_limit = 200000; /* 200mA */
		charger_dev_set_input_current(info->chg1_dev,
					info->chg1_data.input_current_limit);
		chr_err("In meta mode, disable charging and set input current limit to 200mA\n");
	} else {
		mtk_pe20_start_algorithm(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_start_algorithm(info);

		swchg_select_charging_current_limit(info);
		if (info->chg1_data.input_current_limit == 0
		    || info->chg1_data.charging_current_limit == 0) {
			charging_enable = false;
			chr_err("[charger]charging current is set 0mA, turn off charging !\n");
		} else {
			swchg_select_cv(info);
		}
	}

	charger_dev_enable(info->chg1_dev, charging_enable);
}
//prize added by huarui, ne6153 support, 20190111-start
#if defined(CONFIG_PRIZE_NE6153_SUPPORT)
extern void ne6153_chk_set_vout(int mv);
extern int ne6153_get_tune_state(void);
#endif
//prize added by huarui, ne6153 support, 20190111-end

static int mtk_switch_charging_plug_in(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

//prize added by huarui,ne6153 support, 20190111-start
#if defined(CONFIG_PRIZE_NE6153_SUPPORT)
	if (info->chr_type == NONSTANDARD_CHARGER){
		ne6153_chk_set_vout(9000);
	}
#endif
//prize added by huarui, ne6153 support, 20190111-end
//prize added by huarui, wireless charge soft start, 20190111-start
#if defined(CONFIG_PRIZE_WLC_SOFT_START)
	if (info->chr_type == NONSTANDARD_CHARGER){
		if (wlc_info.cur_step_pattern_num){
			wlc_info.chg_phase = 1;
			wlc_info.cur_step_count = 0;
			wlc_info.cur_limit_input = 0;
			wlc_info.cur_limit_output = 0;
			__pm_stay_awake(&wlc_info.cur_step_ws);
			schedule_work(&wlc_info.cur_step_work);
		}
	}
#endif
//prize added by huarui, wireless charge soft start, 20190111-end

//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-start
#if defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
	if (info->chr_type == NONSTANDARD_CHARGER){
		MT5715_wlc_info.MT5715_chg_phase = 1;
		MT5715_wlc_info.MT5715_cur_step_count =0;
		MT5715_wlc_info.ret_ldo_status =0;
		MT5715_wlc_info.MT5715_up_status =0;
		MT5715_wlc_info.ret_ldo_status_last =0;
		MT5715_wlc_info.fast_vol_status =0;
		MT5715_wlc_info.MT5715_wait_cunt =0;
		MT5715_wlc_info.fake_sam_wait_cunt =0;
		__pm_stay_awake(&MT5715_wlc_info.cur_step_ws_wk);
		schedule_work(&MT5715_wlc_info.MT5715_cur_step_work);
	}
#endif
//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-end
/*prize-add by sunshuai for for Vietnam customer  20200106 start  */
#if defined(CONFIG_PRIZE_CHARGE_CTRL_VIETNAM)
			 info->step_info.current_step = STEP_INIT;
#endif
/*prize-add by sunshuai for for Vietnam customer  20200106 end	*/

	swchgalg->state = CHR_CC;
	info->polling_interval = CHARGING_INTERVAL;
	swchgalg->disable_charging = false;
	get_monotonic_boottime(&swchgalg->charging_begin_time);

	return 0;
}

static int mtk_switch_charging_plug_out(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->total_charging_time = 0;
	//prize added by huarui, wireless charge soft start, 20190111-start
#if defined(CONFIG_PRIZE_WLC_SOFT_START)
	wlc_info.chg_phase = 0;
	wlc_info.cur_step_count = 0;
	wlc_info.cur_limit_input = 0;
	wlc_info.cur_limit_output = 0;
	cancel_work_sync(&wlc_info.cur_step_work);
	__pm_relax(&wlc_info.cur_step_ws);
#endif
//prize added by huarui, wireless charge soft start, 20190111-end

//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-start
#if defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
    MT5715_wlc_info.MT5715_chg_phase = 0 ;
    MT5715_wlc_info.MT5715_cur_step_count = 0;
	MT5715_wlc_info.ret_ldo_status =0;
	MT5715_wlc_info.MT5715_up_status =0;
	MT5715_wlc_info.fast_vol_status =0;
	MT5715_wlc_info.ret_ldo_status_last =0;
	MT5715_wlc_info.MT5715_wait_cunt =0;
	MT5715_wlc_info.fake_sam_wait_cunt =0;
	set_is_samsung_charge(0);
	cancel_work_sync(&MT5715_wlc_info.MT5715_cur_step_work);
    __pm_relax(&MT5715_wlc_info.cur_step_ws_wk);
#endif
//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-end
/*prize-add by sunshuai for for Vietnam customer  20200106 start  */
#if defined(CONFIG_PRIZE_CHARGE_CTRL_VIETNAM)
		 info->step_info.current_step = STEP_INIT;
#endif
/*prize-add by sunshuai for for Vietnam customer  20200106 end	*/

	mtk_pe20_set_is_cable_out_occur(info, true);
	mtk_pe_set_is_cable_out_occur(info, true);
	mtk_pdc_plugout(info);
	mtk_pe40_plugout_reset(info);
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
	mtk_pe50_plugout_reset(info);
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
	return 0;
}

static int mtk_switch_charging_do_charging(struct charger_manager *info,
						bool en)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	chr_err("%s: en:%d %s\n", __func__, en, info->algorithm_name);
	if (en) {
		swchgalg->disable_charging = false;
		swchgalg->state = CHR_CC;
		get_monotonic_boottime(&swchgalg->charging_begin_time);
		charger_manager_notifier(info, CHARGER_NOTIFY_NORMAL);
		mtk_pe40_set_is_enable(info, en);
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
		mtk_pe50_set_is_enable(info, en);
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
	} else {
		/* disable charging might change state, so call it first */
		_disable_all_charging(info);
		swchgalg->disable_charging = true;
		swchgalg->state = CHR_ERROR;
		charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
	}

	return 0;
}

static int mtk_switch_chr_pe40_init(struct charger_manager *info)
{
	swchg_turn_on_charging(info);
	return mtk_pe40_init_state(info);
}

static int mtk_switch_chr_pe40_cc(struct charger_manager *info)
{
	swchg_turn_on_charging(info);
	return mtk_pe40_cc_state(info);
}
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
static int mtk_switch_chr_pe50_ready(struct charger_manager *info)
{
	int ret;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	ret = mtk_pe50_start(info);
	if (ret < 0) {
		info->pe5.online = false;
		swchgalg->state = CHR_CC;
	} else
		swchgalg->state = CHR_PE50_RUNNING;
	return 0;
}

static int mtk_switch_chr_pe50_running(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct charger_data *dvchg_data = &info->dvchg1_data;

	if (!mtk_pe50_is_running(info))
		goto stop;

	mtk_pe50_thermal_throttling(info,
				    dvchg_data->thermal_input_current_limit);
	if (info->enable_sw_jeita)
		mtk_pe50_set_jeita_vbat_cv(info, info->sw_jeita.cv);
	return 0;

stop:
	chr_info("%s PE5 stops\n", __func__);
	info->pe5.online = false;
	swchgalg->state = CHR_CC;
	/* Let charging algorithm run CHR_CC immediately */
	return -EINVAL;
}
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
/* return false if total charging time exceeds max_charging_time */
static bool mtk_switch_check_charging_time(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct timespec time_now;

	if (info->enable_sw_safety_timer) {
		get_monotonic_boottime(&time_now);
		chr_debug("%s: begin: %ld, now: %ld\n", __func__,
			swchgalg->charging_begin_time.tv_sec, time_now.tv_sec);

		if (swchgalg->total_charging_time >=
		    info->data.max_charging_time) {
			chr_err("%s: SW safety timeout: %d sec > %d sec\n",
				__func__, swchgalg->total_charging_time,
				info->data.max_charging_time);
			charger_dev_notify(info->chg1_dev,
					CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
			return false;
		}
	}

	return true;
}

static int mtk_switch_chr_cc(struct charger_manager *info)
{
	bool chg_done = false;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	struct timespec time_now, charging_time;

	/* check bif */
	if (IS_ENABLED(CONFIG_MTK_BIF_SUPPORT)) {
		if (pmic_is_bif_exist() != 1) {
			chr_err("CONFIG_MTK_BIF_SUPPORT but no bif , stop charging\n");
			swchgalg->state = CHR_ERROR;
			charger_manager_notifier(info, CHARGER_NOTIFY_ERROR);
		}
	}

	get_monotonic_boottime(&time_now);
	charging_time = timespec_sub(time_now, swchgalg->charging_begin_time);

	swchgalg->total_charging_time = charging_time.tv_sec;
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
	if (mtk_pe50_is_ready(info)) {
		chr_err("enter PE5.0\n");
		swchgalg->state = CHR_PE50_READY;
		info->pe5.online = true;
		return 1;
	}
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
	if (mtk_pe40_is_ready(info)) {
		chr_err("enter PE4.0!\n");
		swchgalg->state = CHR_PE40_INIT;
		info->pe4.is_connect = true;
		return 1;
	}

	swchg_turn_on_charging(info);

	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	if (chg_done) {
		swchgalg->state = CHR_BATFULL;
		charger_dev_do_event(info->chg1_dev, EVENT_EOC, 0);
		chr_err("battery full!\n");
	}

	/* If it is not disabled by throttling,
	 * enable PE+/PE+20, if it is disabled
	 */
	if (info->chg1_data.thermal_input_current_limit != -1 &&
		info->chg1_data.thermal_input_current_limit < 300)
		return 0;

	if (!mtk_pe20_get_is_enable(info)) {
		mtk_pe20_set_is_enable(info, true);
		mtk_pe20_set_to_check_chr_type(info, true);
	}

	if (!mtk_pe_get_is_enable(info)) {
		mtk_pe_set_is_enable(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
	}
	return 0;
}

int mtk_switch_chr_err(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	if (info->enable_sw_jeita) {
		if ((info->sw_jeita.sm == TEMP_BELOW_T0) ||
			(info->sw_jeita.sm == TEMP_ABOVE_T4))
			info->sw_jeita.error_recovery_flag = false;

		if ((info->sw_jeita.error_recovery_flag == false) &&
			(info->sw_jeita.sm != TEMP_BELOW_T0) &&
			(info->sw_jeita.sm != TEMP_ABOVE_T4)) {
			info->sw_jeita.error_recovery_flag = true;
			swchgalg->state = CHR_CC;
			get_monotonic_boottime(&swchgalg->charging_begin_time);
		}
	}

	swchgalg->total_charging_time = 0;

	_disable_all_charging(info);
	return 0;
}

int mtk_switch_chr_full(struct charger_manager *info)
{
	bool chg_done = false;
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;

	swchgalg->total_charging_time = 0;

	/* turn off LED */

	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
	swchg_select_cv(info);
	info->polling_interval = CHARGING_FULL_INTERVAL;
	charger_dev_is_charging_done(info->chg1_dev, &chg_done);
	if (!chg_done) {
		swchgalg->state = CHR_CC;
		charger_dev_do_event(info->chg1_dev, EVENT_RECHARGE, 0);
		mtk_pe20_set_to_check_chr_type(info, true);
		mtk_pe_set_to_check_chr_type(info, true);
		mtk_pe40_set_is_enable(info, true);
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
		mtk_pe50_set_is_enable(info, true);
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
		info->enable_dynamic_cv = true;
		get_monotonic_boottime(&swchgalg->charging_begin_time);
		chr_err("battery recharging!\n");
		info->polling_interval = CHARGING_INTERVAL;
	}

	return 0;
}

static int mtk_switch_charging_current(struct charger_manager *info)
{
	swchg_select_charging_current_limit(info);
	return 0;
}

static int mtk_switch_charging_run(struct charger_manager *info)
{
	struct switch_charging_alg_data *swchgalg = info->algorithm_data;
	int ret = 0;

	chr_err("%s [%d %d], timer=%d\n", __func__, swchgalg->state,
		info->pd_type,
		swchgalg->total_charging_time);

	if (mtk_pdc_check_charger(info) == false &&
	    mtk_is_TA_support_pd_pps(info) == false
		#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
		&&!info->pe5.online
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
		#endif
		) {
		mtk_pe20_check_charger(info);
		if (mtk_pe20_get_is_connect(info) == false)
			mtk_pe_check_charger(info);
	}
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
	if (mtk_pe40_get_is_connect(info)) {
		if (mtk_pe50_is_ready(info))
			mtk_pe40_end(info, 4, true);
	}
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
//prize added by huarui, eta6937 support, 20190111-start
#if defined(CONFIG_HL7005ALL_CHARGER_SUPPORT)
	charger_dev_kick_wdt(info->chg1_dev);	//PRIZE
#endif
//prize added by huarui, eta6937 support, 20190111-end
	do {
		switch (swchgalg->state) {
			chr_err("%s_2 [%d] %d\n", __func__, swchgalg->state,
				info->pd_type);
		case CHR_CC:
			ret = mtk_switch_chr_cc(info);
			break;

		case CHR_PE40_INIT:
			ret = mtk_switch_chr_pe40_init(info);
			break;

		case CHR_PE40_CC:
			ret = mtk_switch_chr_pe40_cc(info);
			break;
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
		case CHR_PE50_READY:
			ret = mtk_switch_chr_pe50_ready(info);
			break;

		case CHR_PE50_RUNNING:
			ret = mtk_switch_chr_pe50_running(info);
			break;
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
		case CHR_BATFULL:
			ret = mtk_switch_chr_full(info);
			break;

		case CHR_ERROR:
			ret = mtk_switch_chr_err(info);
			break;
		}
	} while (ret != 0);
	mtk_switch_check_charging_time(info);

	charger_dev_dump_registers(info->chg1_dev);
	return 0;
}

int charger_dev_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, chg1_nb);
	struct chgdev_notify *data = v;

	chr_info("%s %ld", __func__, event);

	switch (event) {
	case CHARGER_DEV_NOTIFY_EOC:
		charger_manager_notifier(info, CHARGER_NOTIFY_EOC);
		pr_info("%s: end of charge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_RECHG:
		charger_manager_notifier(info, CHARGER_NOTIFY_START_CHARGING);
		pr_info("%s: recharge\n", __func__);
		break;
	case CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT:
		info->safety_timeout = true;
		chr_err("%s: safety timer timeout\n", __func__);

		/* If sw safety timer timeout, do not wake up charger thread */
		if (info->enable_sw_safety_timer)
			return NOTIFY_DONE;
		break;
	case CHARGER_DEV_NOTIFY_VBUS_OVP:
		info->vbusov_stat = data->vbusov_stat;
		chr_err("%s: vbus ovp = %d\n", __func__, info->vbusov_stat);
		break;
	default:
		return NOTIFY_DONE;
	}

	if (info->chg1_dev->is_polling_mode == false)
		_wake_up_charger(info);

	return NOTIFY_DONE;
}

#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
static int dvchg1_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, dvchg1_nb);

	chr_info("%s %ld", __func__, event);

	return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event, data);
}

static int dvchg2_dev_event(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	struct charger_manager *info =
			container_of(nb, struct charger_manager, dvchg2_nb);

	chr_info("%s %ld", __func__, event);

	return mtk_pe50_notifier_call(info, MTK_PE50_NOTISRC_CHG, event, data);
}
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
int mtk_switch_charging_init(struct charger_manager *info)
{
	struct switch_charging_alg_data *swch_alg;

	swch_alg = devm_kzalloc(&info->pdev->dev,
				sizeof(*swch_alg), GFP_KERNEL);
	if (!swch_alg)
		return -ENOMEM;

	info->chg1_dev = get_charger_by_name("primary_chg");
	if (info->chg1_dev)
		chr_err("Found primary charger [%s]\n",
			info->chg1_dev->props.alias_name);
	else
		chr_err("*** Error : can't find primary charger ***\n");
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_50_SUPPORT
/*prize-huangjiwu-20200730, add for rt9759 pe50 start*/
	info->dvchg1_dev = get_charger_by_name("primary_divider_chg");
	if (info->dvchg1_dev) {
		chr_err("Found primary divider charger [%s]\n",
			info->dvchg1_dev->props.alias_name);
		info->dvchg1_nb.notifier_call = dvchg1_dev_event;
		register_charger_device_notifier(info->dvchg1_dev,
						 &info->dvchg1_nb);
	} else
		chr_err("*** Error : can't find primary divider charger ***\n");
	info->dvchg2_dev = get_charger_by_name("secondary_divider_chg");
	if (info->dvchg2_dev) {
		chr_err("Found secondary divider charger [%s]\n",
			info->dvchg2_dev->props.alias_name);
		info->dvchg2_nb.notifier_call = dvchg2_dev_event;
		register_charger_device_notifier(info->dvchg2_dev,
						 &info->dvchg2_nb);
	} else
		chr_err("*** Error : can't find secondary divider charger ***\n");
/*prize-huangjiwu-20200730, add for rt9759 pe50 end*/
#endif
	mutex_init(&swch_alg->ichg_aicr_access_mutex);
//prize added by huarui, wireless charge soft start, 20190111-start
#if defined(CONFIG_PRIZE_WLC_SOFT_START)
	wlc_init(info);
#endif
//prize added by huarui, wireless charge soft start, 20190111-end

//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-start
#if defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
    MT5715_init(info);
#endif
//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-end


	info->algorithm_data = swch_alg;
	info->do_algorithm = mtk_switch_charging_run;
	info->plug_in = mtk_switch_charging_plug_in;
	info->plug_out = mtk_switch_charging_plug_out;
	info->do_charging = mtk_switch_charging_do_charging;
	info->do_event = charger_dev_event;
	info->change_current_setting = mtk_switch_charging_current;

	return 0;
}
//prize added by huarui, wireless charge soft start, 20190111-start
#if defined(CONFIG_PRIZE_WLC_SOFT_START)
static enum hrtimer_restart wlc_cur_step_timeout(struct hrtimer *timer)
{
	schedule_work(&wlc_info.cur_step_work);

    return HRTIMER_NORESTART;
}

static void wlc_cur_step_work_func(struct work_struct *data)
{
	ktime_t ktime;

	printk("WLC:%s,cur_step_count%d,chg_phase%d\n",__func__,wlc_info.cur_step_count,wlc_info.chg_phase);

	if (wlc_info.chg_phase != 1){
		wlc_info.cur_step_count = 0;
		return;
	}

	wlc_info.cur_limit_input = *(wlc_info.cur_step_pattern_input + wlc_info.cur_step_count);//wlc_cur_limit_sel[wlc_cur_step_count][0];
	wlc_info.cur_limit_output = *(wlc_info.cur_step_pattern_output + wlc_info.cur_step_count);//wlc_cur_limit_sel[wlc_cur_step_count][1];
	swchg_select_charging_current_limit(wlc_info.cm_info);

#if defined(CONFIG_PRIZE_NE6153_SUPPORT)
	if (wlc_info.cur_step_count == 3){
		if (ne6153_get_tune_state() >= 2 ){
			wlc_info.cur_step_count++;
		}else{
			printk("ne6153 wlc wating\n");
		}
	}else{
		wlc_info.cur_step_count++;
	}
#else
	wlc_info.cur_step_count++;
#endif
	if (wlc_info.cur_step_count >= wlc_info.cur_step_pattern_num){
		wlc_info.chg_phase = 2;
		__pm_relax(&wlc_info.cur_step_ws);
	}

	if (wlc_info.chg_phase == 1){
		ktime = ktime_set(5, 0);
		hrtimer_start( &wlc_info.cur_step_timer, ktime, HRTIMER_MODE_REL );
	}
}

static int wlc_parse_dts(struct charger_manager *info){
	struct device_node *np = info->pdev->dev.of_node;
	int i;
	int *current_input;
	int *current_output;

	if (np != NULL){
		if (!of_property_read_u32(np, "wlc_cur_step_pattern_num", &wlc_info.cur_step_pattern_num)) {
			printk("WLC cur_step_pattern_num %d\n",wlc_info.cur_step_pattern_num);
		}else{
			wlc_info.cur_step_pattern_num = 0;
			printk("WLC get cur_step_pattern_num failed\n");
		}
		if ((wlc_info.cur_step_pattern_num == 0)||(wlc_info.cur_step_pattern_num > 10)){
			printk("WLC: invalid cur_step_pattern_num(%d)\n",wlc_info.cur_step_pattern_num);
			return -EINVAL;
		}

		wlc_info.cur_step_pattern_input = devm_kzalloc(&info->pdev->dev, sizeof(int)*wlc_info.cur_step_pattern_num, GFP_KERNEL);
		if (!wlc_info.cur_step_pattern_input){
			return -ENOMEM;
		}
		current_input = wlc_info.cur_step_pattern_input;
		for(i=0;i<wlc_info.cur_step_pattern_num;i++){
			if (!of_property_read_u32_index(np,"wlc_cur_step_pattern_input",i,current_input)){
				printk("WLC: input pattern %d %d\n",i,*current_input);
				current_input++;
			}else{
				printk("WLC get cur_step_pattern_input failed\n");
				break;
			}
		}

		wlc_info.cur_step_pattern_output = devm_kzalloc(&info->pdev->dev, sizeof(int)*wlc_info.cur_step_pattern_num, GFP_KERNEL);
		if (!wlc_info.cur_step_pattern_output){
			return -ENOMEM;
		}
		current_output = wlc_info.cur_step_pattern_output;
		for(i=0;i<wlc_info.cur_step_pattern_num;i++){
			if (!of_property_read_u32_index(np,"wlc_cur_step_pattern_output",i,current_output)){
				printk("WLC: output pattern %d %d\n",i,*current_output);
				current_output++;
			}else{
				printk("WLC get cur_step_pattern_output failed\n");
				break;
			}
		}
	}
	return 0;
}

static int wlc_init(struct charger_manager *info){
	wlc_parse_dts(info);
	hrtimer_init( &wlc_info.cur_step_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	wlc_info.cur_step_timer.function = wlc_cur_step_timeout;
	INIT_WORK(&wlc_info.cur_step_work, wlc_cur_step_work_func);
	wakeup_source_init(&wlc_info.cur_step_ws, "wlc wakelock");
	wlc_info.cm_info = info;
	return 0;
}
#endif
//prize added by huarui, wireless charge soft start, 20190111-end


//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-start
#if defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
static int MT5715_wlc_parse_dts(struct charger_manager *info){
	struct device_node *np = info->pdev->dev.of_node;
	int i;
	int *current_input;
	int *current_output;

    chr_err("MT5715_wlc_parse_dts\n");//prize modify by sunshuai, Optimized log 20190316

	if (np != NULL){
		if (!of_property_read_u32(np, "MT5715_wlc_cur_step_num", &MT5715_wlc_info.cur_step_pattern_num)) {
			chr_err("MT5715 WLC cur_step_pattern_num %d\n",MT5715_wlc_info.cur_step_pattern_num);
		}else{
			MT5715_wlc_info.cur_step_pattern_num = 0;
			chr_err("MT5715 WLC get cur_step_pattern_num failed\n");
		}

		if ((MT5715_wlc_info.cur_step_pattern_num == 0)||(MT5715_wlc_info.cur_step_pattern_num > 10)){
			chr_err("MT5715 WLC: invalid cur_step_pattern_num(%d)\n",MT5715_wlc_info.cur_step_pattern_num);
			return -EINVAL;
		}

		MT5715_wlc_info.mt5715_input_vol = devm_kzalloc(&info->pdev->dev, sizeof(int)*MT5715_wlc_info.cur_step_pattern_num, GFP_KERNEL);
		if (!MT5715_wlc_info.mt5715_input_vol){
			return -ENOMEM;
		}

		current_input = MT5715_wlc_info.mt5715_input_vol;
		for(i=0;i<MT5715_wlc_info.cur_step_pattern_num;i++){
			if (!of_property_read_u32_index(np,"MT5715_wlc_cur_step_input",i,current_input)){
				chr_err("MT5715 WLC: input pattern %d %d\n",i,*current_input);
				current_input++;
			}else{
				chr_err("MT5715 WLC get cur_step_pattern_input failed\n");
				break;
			}
		}

		MT5715_wlc_info.mt5715_vol = devm_kzalloc(&info->pdev->dev, sizeof(int)*MT5715_wlc_info.cur_step_pattern_num, GFP_KERNEL);
		if (!MT5715_wlc_info.mt5715_vol){
			return -ENOMEM;
		}
		current_output = MT5715_wlc_info.mt5715_vol;
		for(i=0;i<MT5715_wlc_info.cur_step_pattern_num;i++){
			if (!of_property_read_u32_index(np,"MT5715_wlc_cur_step_output",i,current_output)){
				chr_err("MT5715 WLC: output pattern %d %d\n",i,*current_output);
				current_output++;
			}else{
				chr_err("MT5715 WLC get cur_step_pattern_output failed\n");
				break;
			}
		}
	}
	return 0;
}


static enum hrtimer_restart MT5715_cur_step_timeout(struct hrtimer *timer)
{
	schedule_work(&MT5715_wlc_info.MT5715_cur_step_work);

    return HRTIMER_NORESTART;
}


static void MT5715_cur_step_work_func(struct work_struct *data)
{
	ktime_t ktime;

	chr_err("before chose %s,cur_step_count%d,chg_phase%d is_samsung_charge %d\n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_chg_phase,get_is_samsung_charge());

	if (MT5715_wlc_info.MT5715_chg_phase != 1){
		return;
	}

	MT5715_wlc_info.ret_ldo_status_last = MT5715_wlc_info.ret_ldo_status;

	MT5715_wlc_info.MT5715_up_status = get_MT5715on_status();
	MT5715_wlc_info.ret_ldo_status = get_lod_status()|confirm_MT5715_works();//prize added by sunshuai Update the way to judge whether the wireless charging chip 5715 is working,2019-06-21


	if(MT5715_wlc_info.ret_ldo_status == 0 ){
		chr_err("%s,MT5715 not up ,cur_step_count%d,chg_phase%d\n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_chg_phase);
		MT5715_wlc_info.MT5715_cur_step_count =0;
		MT5715_wlc_info.MT5715_wait_cunt = 0;
		MT5715_wlc_info.fake_sam_wait_cunt =0;
		return;
	}


	if((MT5715_wlc_info.MT5715_cur_step_count >= 7)){
		swchg_select_charging_current_limit(MT5715_wlc_info.mt5715_cm_info);
		MT5715_wlc_info.MT5715_chg_phase = 2;
		MT5715_wlc_info.MT5715_cur_step_count =7;
		__pm_relax(&MT5715_wlc_info.cur_step_ws_wk);
		chr_err("%s,current step sucess,cur_step_count%d,chg_phase%d\n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_chg_phase);
		return;
	}


	if(MT5715_wlc_info.MT5715_wait_cunt > 3){
		chr_err("%s,wait MT5715 ,cur_step_count%d,chg_phase%d wait_cunt=%d\n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_chg_phase,MT5715_wlc_info.MT5715_wait_cunt);
		if(get_is_samsung_charge()==0){
			if(MT5715_wlc_info.fake_sam_wait_cunt == 0){
				fast_sv_no_samsung(9000);
				MT5715_wlc_info.fake_sam_wait_cunt++;
				MT5715_wlc_info.MT5715_chg_phase = 1;
				MT5715_wlc_info.MT5715_wait_cunt =0;
			}
			else{//fake Samsung protocol wireless charging failure scenario
				MT5715_wlc_info.MT5715_chg_phase = 3;
		        __pm_relax(&MT5715_wlc_info.cur_step_ws_wk);
				chr_err("%s,fake Samsung protocol set VFC 9V fail\n",__func__);
			}
		}
		else{//Samsung protocol wireless charging failure scenario
			MT5715_wlc_info.MT5715_chg_phase = 3;
		    __pm_relax(&MT5715_wlc_info.cur_step_ws_wk);
			chr_err("%s,Samsung protocol set VFC 9V fail\n",__func__);
			return;
		}
	}

//prize added by sunshuai, After the fake Samsung protocol board failed to boost 9V, try again, 20190302-start
	if((MT5715_wlc_info.fake_sam_wait_cunt !=0) && (get_is_samsung_charge()==0)){
		 chr_err("%s MT5715_cur_step_count=%d  MT5715_wait_cunt =%d \n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_wait_cunt);
		 fast_sv_no_samsung(9000);
    }
//prize added by sunshuai, After the fake Samsung protocol board failed to boost 9V, try again, 20190302-end

    MT5715_wlc_info.fast_vol_status = fast_sv(9000);

	if(MT5715_wlc_info.MT5715_cur_step_count == 3){
       if(MT5715_wlc_info.ret_ldo_status == 1){
           MT5715_wlc_info.MT5715_cur_step_count++;
		   MT5715_wlc_info.MT5715_wait_cunt = 0;
	   }
	   else{
	   	   MT5715_wlc_info.MT5715_wait_cunt++;
           chr_err("%s,wait MT5715 ldo on, cur_step_count%d,chg_phase%d\n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_chg_phase);
	   }
	   swchg_select_charging_current_limit(MT5715_wlc_info.mt5715_cm_info);
	}
	else if(MT5715_wlc_info.MT5715_cur_step_count == 5){
       if(MT5715_wlc_info.ret_ldo_status == 1 && MT5715_wlc_info.fast_vol_status ==1){
           MT5715_wlc_info.MT5715_cur_step_count++;
		   MT5715_wlc_info.MT5715_wait_cunt = 0;
	   }
	   else{
	   	   MT5715_wlc_info.MT5715_wait_cunt++;
           chr_err("%s,wait MT5715 step up 9V, cur_step_count%d,chg_phase%d\n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_chg_phase);
	   }
	   swchg_select_charging_current_limit(MT5715_wlc_info.mt5715_cm_info);
	}else{
	     MT5715_wlc_info.MT5715_cur_step_count++;
	}

	chr_err("%s,ret_ldo_status%d,ret_ldo_status_last%d  fast_vol_status =%d\n",__func__,MT5715_wlc_info.ret_ldo_status,MT5715_wlc_info.ret_ldo_status_last,MT5715_wlc_info.fast_vol_status);

	chr_err("after chose %s,cur_step_count%d,chg_phase%d\n",__func__,MT5715_wlc_info.MT5715_cur_step_count,MT5715_wlc_info.MT5715_chg_phase);

    //swchg_select_charging_current_limit(MT5715_wlc_info.mt5715_cm_info);  //prize modify by sunshuai Change the frequency of the chip to increase the maximum current before trying 9V,2019-06-21

	if (MT5715_wlc_info.MT5715_chg_phase == 1) {
		if(MT5715_wlc_info.MT5715_cur_step_count < 3){
			ktime = ktime_set(3, 0);
		}
		else{
			ktime = ktime_set(2, 0);
		}
		hrtimer_start(&MT5715_wlc_info.MT5715_cur_step_timer, ktime, HRTIMER_MODE_REL );
	}
}


static int MT5715_init(struct charger_manager *info){

    MT5715_wlc_parse_dts(info);
    hrtimer_init( &MT5715_wlc_info.MT5715_cur_step_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	MT5715_wlc_info.MT5715_cur_step_timer.function = MT5715_cur_step_timeout;
	INIT_WORK(&MT5715_wlc_info.MT5715_cur_step_work, MT5715_cur_step_work_func);

	MT5715_wlc_info.MT5715_chg_phase = 0;  //0:idle 1:increase 2:cc
    MT5715_wlc_info.MT5715_cur_step_count = 0;
	MT5715_wlc_info.fast_vol_status = 0;
	MT5715_wlc_info.ret_ldo_status = 0;
	MT5715_wlc_info.MT5715_up_status =0;
	MT5715_wlc_info.MT5715_wait_cunt =0;
	MT5715_wlc_info.fake_sam_wait_cunt =0;
	MT5715_wlc_info.mt5715_cm_info = info;

	wakeup_source_init(&MT5715_wlc_info.cur_step_ws_wk, "MT5715 wakelock");
    return 0;
}
#endif
//prize added by sunshuai, wireless charge MT5715  soft start, 20190302-end


