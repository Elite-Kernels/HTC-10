/*
 * Copyright (C) 2015 HTC, Inc.
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

#ifndef __HTC_INFO__
#define __HTC_INFO__

#ifdef err
#undef err
#endif
#ifdef warn
#undef warn
#endif
#ifdef info
#undef info
#endif

#define USB_ERR(fmt, args...) \
	printk(KERN_ERR "[USB:ERR] " fmt, ## args)
#define USB_WARNING(fmt, args...) \
	printk(KERN_WARNING "[USB] " fmt, ## args)
#define USB_INFO(fmt, args...) \
	printk(KERN_INFO "[USB] " fmt, ## args)
#define USB_DEBUG(fmt, args...) \
	printk(KERN_DEBUG "[USB] " fmt, ## args)
/*
#ifdef pr_debug
#undef pr_debug
#endif
#define pr_debug(fmt, args...) \
	printk(KERN_INFO "[USB] " pr_fmt(fmt), ## args)
*/
#ifdef pr_warn
#undef pr_warn
#endif
#define pr_warn(fmt, args...) \
	printk(KERN_WARNING "[USB] " pr_fmt(fmt), ## args)

#ifdef pr_err
#undef pr_err
#endif
#define pr_err(fmt, args...) \
	printk(KERN_ERR "[USB] " pr_fmt(fmt), ## args)

#ifdef pr_info
#undef pr_info
#endif
#define pr_info(fmt, args...) \
	printk(KERN_INFO "[USB] " pr_fmt(fmt), ## args)

#if defined(CONFIG_ANALOGIX_7688)
enum anx7688_prop {
	ANX_DROLE = 0,
	ANX_PROLE,
	ANX_PMODE,
	ANX_PROLE_CHANGE,
	ANX_VCONN,
	ANX_START_HOST_FLAG,	// 7418 only
	ANX_EMARKER,			// 7418 only
	ANX_NON_STANDARD,		// 7418 only
	ANX_PD_CAP,
	ANX_FW_VERSION,			// 7418 only
};
#endif

void ufp_switch_usb_speed(int on);

#endif /* __HTC_INFO__ */
