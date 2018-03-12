/******************************************************************************

Copyright (c) 2016, Analogix Semiconductor, Inc.

PKG Ver  : V2.1.11

Filename : anx7688_private_interface.h

Project  : ANX7688

Created  : 28 Nov. 2016

Devices  : ANX7688

Toolchain: Android

Description:

Revision History:

******************************************************************************/

#ifndef ANX7688_PRIVATE_INTERFACE_H
#define ANX7688_PRIVATE_INTERFACE_H
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/usb/class-dual-role.h>
#include <linux/usb/gadget.h>
#include <linux/usb/htc_info.h>
#include "anx7688_public_interface.h"

#ifdef pr_err
#undef pr_err
#endif
#define pr_err(fmt, args...) \
	printk(KERN_ERR "[ANX7688] " pr_fmt(fmt), ## args)

#ifdef pr_info
#undef pr_info
#endif
#define pr_info(fmt, args...) \
	printk(KERN_INFO "[ANX7688] " pr_fmt(fmt), ## args)

#ifdef pr_err_ratelimited
#undef pr_err_ratelimited
#endif
#define pr_err_ratelimited(fmt, ...) \
	printk_ratelimited(KERN_ERR "[ANX7688] " pr_fmt(fmt), ##__VA_ARGS__)

#ifdef pr_info_ratelimited
#undef pr_info_ratelimited
#endif
#define pr_info_ratelimited(fmt, ...) \
	printk_ratelimited(KERN_INFO "[ANX7688] " pr_fmt(fmt), ##__VA_ARGS__)

#ifdef BIT_IN_BYTE
#undef BIT_IN_BYTE
#endif
#define BIT_IN_BYTE(nr) ((u8)(1U) << (nr))

#define InterfaceSendBuf_Addr 0x30
#define InterfaceRecvBuf_Addr 0x51

#define OCM_LOADING_TIME 3200

#define CABLE_DET_PIN_HAS_GLITCH
#define OCM_DEBUG
//#define  PD_CTS_TEST
#define SUP_INT_VECTOR
//#define SUP_VBUS_CTL
#define AUTO_RDO_ENABLE
#define SUP_TRY_SRC_SINK

#define YES     1
#define NO      0
#define ERR_CABLE_UNPLUG -1
#define PD_ONE_DATA_OBJECT_SIZE  4
#define PD_MAX_DATA_OBJECT_NUM  7
#define VDO_SIZE (PD_ONE_DATA_OBJECT_SIZE * PD_MAX_DATA_OBJECT_NUM)

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP)

#define MV 1
#define MA 1
#define MW 1

/*5000mv voltage*/
#define PD_VOLTAGE_5V 5000

#define PD_MAX_VOLTAGE_20V 20000
#define PD_MAX_VOLTAGE_21V 21000

/*0.9A current */
#define PD_CURRENT_900MA   900
#define PD_CURRENT_1500MA 1500

#define PD_CURRENT_3A   3000

#define PD_POWER_15W  15000

#define PD_POWER_60W  60000

/* RDO : Request Data Object */
#define RDO_OBJ_POS(n)             (((u32)(n) & 0x7) << 28)
#define RDO_POS(rdo)               ((((32)rdo) >> 28) & 0x7)
#define RDO_GIVE_BACK              ((u32)1 << 27)
#define RDO_CAP_MISMATCH           ((u32)1 << 26)
#define RDO_COMM_CAP               ((u32)1 << 25)
#define RDO_NO_SUSPEND             ((u32)1 << 24)
#define RDO_FIXED_VAR_OP_CURR(ma)  (((((u32)ma) / 10) & 0x3FF) << 10)
#define RDO_FIXED_VAR_MAX_CURR(ma) (((((u32)ma) / 10) & 0x3FF) << 0)

#define RDO_BATT_OP_POWER(mw)      (((((u32)mw) / 250) & 0x3FF) << 10)
#define RDO_BATT_MAX_POWER(mw)     (((((u32)mw) / 250) & 0x3FF) << 10)

#define RDO_FIXED(n, op_ma, max_ma, flags)	\
	(RDO_OBJ_POS(n) | (flags) |		\
	RDO_FIXED_VAR_OP_CURR(op_ma) |		\
	RDO_FIXED_VAR_MAX_CURR(max_ma))

#define EXTERNALLY_POWERED  YES
/* Source Capabilities */
/* 1 to 5 */
#define SOURCE_PROFILE_NUMBER   1
/* 0 = Fixed, 1 = Battery, 2 = Variable */
#define SRC_PDO_SUPPLY_TYPE1    0
/* 0 to 3 */
#define SRC_PDO_PEAK_CURRENT1   0
/* 5000mV (5V) */
#define SRC_PDO_VOLTAGE1        5000
/* 500mA (0.5A) */
#define SRC_PDO_MAX_CURRENT1    500


#define IRQ_STATUS 0x53
#define IRQ_EXT_MASK_2 0x3d
#define IRQ_EXT_SOFT_RESET_BIT 0x04
#define IRQ_EXT_SOURCE_2 0x4F


#define INTERACE_TIMEOUT_MS 26


#define OCM_SLAVE_I2C_ADDR1 0x50
#define OCM_SLAVE_I2C_ADDR2 0x58
#define INTERFACE_INTR_MASK 0x17
#define RECEIVED_MSG_MASK 1
#define RECEIVED_ACK_MASK 2
#define VCONN_CHANGE_MASK 4
#define VBUS_CHANGE_MASK 8
#define CC_STATUS_CHANGE_MASK 16
#define DATA_ROLE_CHANGE_MASK 32

#define INTERFACE_CHANGE_INT 0x28
#define RECEIVED_MSG 0x01
#define RECEIVED_ACK 0x02
#define VCONN_CHANGE 0x04
#define VBUS_CHANGE 0x08
#define CC_STATUS_CHANGE 0x10
#define DATA_ROLE_CHANGE 0x20

#define SYSTEM_STSTUS 0x29
/*0: VCONN off; 1: VCONN on*/
#define VCONN_STATUS 0x04
/*0: vbus off; 1: vbus on*/
#define VBUS_STATUS 0x08
/*0: host; 1:device*/
#define DATA_ROLE 0x20

#define NEW_CC_STATUS 0x2A
#define INTP_CTRL 0x33

#define VCONN1_EN_OUT_OEN BIT_IN_BYTE(4)
#define VCONN2_EN_OUT_OEN BIT_IN_BYTE(5)

#define R_N_GPIO_CTRL_0 0x63
#define VCONN1_EN BIT_IN_BYTE(3)
#define VCONN2_EN BIT_IN_BYTE(5)

#if 0 // mis-420
#define INTR_MASK_SETTING /*0xf6*/0x80 /*MIS-420: Unmask all interrupt value is 0x80 */
#else
#define INTR_MASK_SETTING 0x80
#endif

/*
* Delay time for OCM
* bit4-bit7 Reserved
* bit0-bit3 Aelay time ocm disable vbus after receiving hardreset
*           According USB PD spec, this time shall be 25ms~35ms,
*           Real value: 30ms - (T_TIME_1 & 0x0F)
*/
#define T_TIME_1 0x6C
#define T_HARDREST_VBUS_OFF_MASK 0x0F

#define VBUS_DELAY_TIME 0x22/*0x69*/
#define TRY_UFP_TIMER 0x23/*0x6A*/

#define AUTO_PD_MODE 0x27/*0x6e*/
#define AUTO_PD_ENABLE 0x02
#define TRY_SNK_ENABLE BIT_IN_BYTE(3)

#define MAX_VOLTAGE_SETTING 0x1B/*0xd0*/
#define MAX_POWER_SETTING 0x1C/*0xd1*/
#define MIN_POWER_SETTING 0x1D/*0xd2*/

unsigned char ReadReg(unsigned char RegAddr);
void WriteReg(unsigned char RegAddr, unsigned char RegVal);

struct tagInterfaceHeader {
    unsigned char Indicator:1;	/* indicator */
    unsigned char Type:3;		/* data type */
    unsigned char Length:4;		/* data length */
};

struct tagInterfaceData {
    unsigned long SrcPDO[7];
    unsigned long SnkPDO[7];
    unsigned long RDO;
    unsigned long VDMHeader;
    unsigned long IDHeader;
    unsigned long CertStatVDO;
    unsigned long ProductVDO;
    unsigned long CableVDO;
    unsigned long AMM_VDO;
};

/*Comands status*/
enum interface_status { CMD_SUCCESS, CMD_REJECT, CMD_FAIL, CMD_BUSY,
                        CMD_STATUS
                      };

enum port_mode {
	MODE_UFP = 0,
	MODE_DFP,
	MODE_UNKNOWN,
};

enum power_role {
	PR_SOURCE = 0,
	PR_SINK,
	UNKNOWN_POWER_ROLE,
};

enum data_role {
	DR_HOST = 0,
	DR_DEVICE,
	UNKNOWN_DATA_ROLE,
};

enum vconn_supply {
	VCONN_SUPPLY_NO = 0,
	VCONN_SUPPLY_YES,
};

enum pr_change {
	PR_NOCHANGE = 0,
	SRC_TO_SNK,
	SNK_TO_SRC,
};

enum cc_orientation {
	CC1 = 0,
	CC2,
	CC_NONE,
};

typedef enum {
	utccNone = 0,
	utccDefault,
	utcc1p5A,
	utcc3p0A
} USBTypeCCurrent;

#define C1C2_LOW 0
#define C1C2_HIGH 1
#define C1C2_HIGH_Z 2

#define MAX_INTERFACE_COUNT 32
#define MAX_INTERFACE_MSG_LEN  32

#define INTERFACE_TIMEOUT 30
extern u8 pd_src_pdo_cnt;
extern u8 pd_src_pdo[];
extern u8 pd_snk_pdo_cnt;
extern u8 pd_snk_pdo[];
extern u8 pd_rdo[];
extern u8 DP_caps[];
extern u8 configure_DP_caps[];
extern u8 src_dp_caps[];
extern atomic_t anx7688_power_status;
extern unsigned char downstream_pd_cap;

/* check soft interrupt happens or not */
#define is_soft_reset_intr() \
	(ReadReg(IRQ_EXT_SOURCE_2) & IRQ_EXT_SOFT_RESET_BIT)

/* clear the anx7688's soft  interrupt bit */
#define clear_soft_interrupt()	\
	WriteReg(IRQ_EXT_SOURCE_2, IRQ_EXT_SOFT_RESET_BIT)
/*control cmd*/
#define interface_pr_swap() \
	interface_send_msg_timeout(TYPE_PSWAP_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_dr_swap() \
	interface_send_msg_timeout(TYPE_DSWAP_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_vconn_swap() \
	interface_send_msg_timeout(TYPE_VCONN_SWAP_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_get_dp_caps() \
	interface_send_msg_timeout(TYPE_GET_DP_SNK_CAP, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_gotomin() \
	interface_send_msg_timeout(TYPE_GOTO_MIN_REQ, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_soft_rst() \
	interface_send_msg_timeout(TYPE_SOFT_RST, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_hard_rst() \
	interface_send_msg_timeout(TYPE_HARD_RST, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_restart() \
	interface_send_msg_timeout(TYPE_RESTART, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_accept() \
	interface_send_msg_timeout(TYPE_ACCEPT, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_reject() \
	interface_send_msg_timeout(TYPE_REJECT, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_dp_enter() \
	interface_send_msg_timeout(TYPE_DP_ALT_ENTER, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_dp_exit() \
	interface_send_msg_timeout(TYPE_DP_ALT_EXIT, 0, 0, INTERFACE_TIMEOUT)
#define interface_send_src_cap() \
	interface_send_msg_timeout(TYPE_PWR_SRC_CAP, pd_src_pdo,\
	pd_src_pdo_cnt * 4, INTERFACE_TIMEOUT)
#define interface_send_snk_cap() \
	interface_send_msg_timeout(TYPE_PWR_SNK_CAP, pd_snk_pdo,\
	pd_snk_pdo_cnt * 4, INTERFACE_TIMEOUT)
#define interface_send_src_dp_cap() \
	interface_send_msg_timeout(TYPE_DP_SNK_IDENTITY, src_dp_caps,\
	4, INTERFACE_TIMEOUT)
#define interface_config_dp_caps() \
	interface_send_msg_timeout(TYPE_DP_SNK_CFG, configure_DP_caps,\
	4, INTERFACE_TIMEOUT)
#define interface_send_request() \
	interface_send_msg_timeout(TYPE_PWR_OBJ_REQ, pd_rdo,\
	4, INTERFACE_TIMEOUT)
#define interface_send_vdm_data(buf, len)	\
	interface_send_msg_timeout(TYPE_VDM, buf, len, INTERFACE_TIMEOUT)

void send_initialized_setting(void);
void interface_init(void);
void chip_register_init(void);
u8 polling_interface_msg(int timeout_ms);
u8 try_source(void);
u8 try_sink(void);
u8 get_otp_indicator_byte(void);
u8 send_dp_snk_cfg(const u8 *dp_snk_caps, u8 dp_snk_caps_size);
u8 send_dp_snk_identity(const u8 *, u8);
u8 send_vdm(const u8 *vdm, u8 size);
u8 send_svid(const u8 *svid, u8 size);
u8 recv_pd_pwr_object_req_default_callback(void *para, u8 para_len);
u8 recv_pd_dswap_default_callback(void *para, u8 para_len);
u8 recv_pd_pswap_default_callback(void *para, u8 para_len);
u8 recv_pd_sink_caps_default_callback(void *para, u8 para_len);
u8 recv_pd_source_caps_default_callback(void *para, u8 para_len);
u8 recv_pd_cmd_rsp_default_callback(void *para, u8 para_len);
u8 recv_pd_goto_min_default_callback(void *para, u8 para_len);
u8 recv_pd_accept_default_callback(void *para, u8 para_len);
u8 recv_pd_reject_default_callback(void *para, u8 para_len);
u8 recv_pd_hard_rst_default_callback(void *para, u8 para_len);
char *interface_to_str(unsigned char header_type);

int ReadBlockReg(u8 RegAddr, u8 len, u8 *dat);
int WriteBlockReg(u8 RegAddr, u8 len, const u8 *dat);
void anx7688_vbus_control(bool value);

void pd_vbus_control_default_func(bool on);
void pd_vconn_control_default_func(bool on);
void pd_cc_status_default_func(u8 cc_status);
void pd_drole_change_default_func(bool on);
//void handle_intr_vector(void);

/* 0, send interface msg timeout
  * 1 successfull */
u8 interface_send_msg_timeout(u8 type, u8 *pbuf, u8 len, int timeout_ms);
pd_callback_t get_pd_callback_fnc(PD_MSG_TYPE type);
void set_pd_callback_fnc(PD_MSG_TYPE type, pd_callback_t fnc);


/**
 * @desc:   The interface AP will set(fill) the source capability to anx7688
 *
 * @param:
 *	src_caps: PDO buffer pointer of source capability
 *		whose bit formats follow the rules:
 *		it's little-endian, defined in USB PD spec 5.5
 *		Transmitted Bit Ordering
 *		source capability's specific format defined in
 *		USB PD spec 6.4.1 Capabilities Message
 *              PDO refer to Table 6-4 Power Data Object
 *              Variable PDO : Table 6-8 Variable Supply (non-battery)
 *              Battery PDO : Table 6-9 Battery Supply PDO --source
 *              Fixed PDO refer to  Table 6-6 Fixed Supply PDO --source
 *	eg: default5Vsafe src_cap(5V, 0.9A fixed)
 *		PDO_FIXED(5000,900, PDO_FIXED_FLAGS)
 *
 *	src_caps_size: source capability's size
 *		if the source capability obtains one PDO object
 *		src_caps_size is 4, two PDO objects, src_caps_size is 8
 *
 * @return:  0: success.  1: fail
 *
 */
u8 send_src_cap(const u8 *src_caps, u8 src_caps_size);

/**
 * @desc:   The interface AP will set(fill) the sink capability to anx7688
 *
 * @param:
 *	snk_caps: PDO buffer pointer of sink capability
 *		whose bit formats follow the rules:
 *		it's little-endian, defined in USB PD spec 5.5
 *		Transmitted Bit Ordering
 *		source capability's specific format defined in
 *		USB PD spec 6.4.1 Capabilities Message
 *		PDO refer to Table 6-4 Power Data Object
 *		Variable PDO : Table 6-8 Variable Suppl
 *              Battery PDO : Table 6-9 Battery Supply PDO
 *		Fixed PDO refer to  Table 6-6 Fixed Supply PDO
 *	eg: default5Vsafe snk_cap(5V, 0.9A fixed) -->
 *		PDO_FIXED(5000,900, PDO_FIXED_FLAGS)
 *
 *	snk_caps_size: sink capability's size
 *		if the sink capability obtains
 *		one PDO object, snk_caps_size is 4
 *		two PDO objects, snk_caps_size is 8
 *
 * @return:  0: success.  1: fail
 *
 */
u8 send_snk_cap(const u8 *snk_caps, u8 snk_caps_size);
u8 send_rdo(const u8 *rdo, u8 size);
u8 send_data_swap(void);
u8 send_power_swap(void);
u8 send_accept(void);



/**
 * @desc:   The interface AP will get the anx7688's data role
 *
 * @param:  none
 *
 * @return:  data role , dfp 1 , ufp 0, other error: -1, not ready
 *
 */
s8 get_data_role(void);

/**
 * @desc:   The interface AP will get the anx7688's power role
 *
 * @param:  none
 *
 * @return:  data role , source 1 , sink 0, other error, -1, not ready
 *
 */
s8 get_power_role(void);

int ohio_set_data_value(int data_member, int val);
int ohio_get_data_value(int data_member);
int anx7688_set_prop(u8 prop, int val);
int anx7688_get_prop(u8 prop);

struct dual_role_phy_instance *ohio_get_dual_role_instance(void);

#endif
