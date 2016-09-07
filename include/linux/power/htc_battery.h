#include <linux/rtc.h>
#include <linux/alarmtimer.h>
#include <linux/wakelock.h>
#include <linux/power_supply.h>

#define BATT_LOG(x...) do { \
printk(KERN_INFO "[BATT] " x); \
} while (0)

#define CONFIG_HTC_BATT_WA_PCN0017 
#define CONFIG_HTC_BATT_WA_PCN0021 

#define BATT_ERR(x...) do { \
struct timespec ts; \
struct rtc_time tm; \
getnstimeofday(&ts); \
rtc_time_to_tm(ts.tv_sec, &tm); \
printk(KERN_ERR "[BATT] err:" x); \
printk(" at %lld (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n", \
ktime_to_ns(ktime_get()), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec); \
} while (0)

#define BATT_EMBEDDED(x...) do { \
struct timespec ts; \
struct rtc_time tm; \
getnstimeofday(&ts); \
rtc_time_to_tm(ts.tv_sec, &tm); \
printk(KERN_ERR "[BATT] " x); \
printk(" at %lld (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n", \
ktime_to_ns(ktime_get()), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec); \
} while (0)

#define POWER_MONITOR_BATT_CAPACITY	77
#define POWER_MONITOR_BATT_TEMP	330

#define STORE_MAGIC_NUM          0xDDAACC00
#define STORE_MAGIC_OFFSET       3104    
#define STORE_SOC_OFFSET         3108    
#define STORE_CURRTIME_OFFSET    3120    
#define STORE_TEMP_OFFSET		3140    

#define HTC_BATT_TOTAL_LEVELRAW		3144
#define HTC_BATT_OVERHEAT_MSEC		3148
#define HTC_BATT_FIRST_USE_TIME		3152
#define HTC_BATT_CYCLE_CHECKSUM		3156

#define HTC_EXT_UNKNOWN_USB_CHARGER		(1<<0)
#define HTC_EXT_CHG_UNDER_RATING		(1<<1)
#define HTC_EXT_CHG_SAFTY_TIMEOUT		(1<<2)
#define HTC_EXT_CHG_FULL_EOC_STOP		(1<<3)
#define HTC_EXT_BAD_CABLE_USED			(1<<4)
#define HTC_EXT_QUICK_CHARGER_USED		(1<<5)

#define BATT_SUSPEND_CHECK_TIME				(3600)
#define BATT_SUSPEND_HIGHFREQ_CHECK_TIME	(300)
#define BATT_TIMER_CHECK_TIME				(360)
#define BATT_TIMER_UPDATE_TIME				(60)
#define CHECH_TIME_TOLERANCE_MS	(1000)

#define SUSPEND_HIGHFREQ_CHECK_BIT_TALK		(1)
#define SUSPEND_HIGHFREQ_CHECK_BIT_SEARCH	(1<<1)
#define SUSPEND_HIGHFREQ_CHECK_BIT_MUSIC	(1<<3)

struct battery_info_reply {
	u32 batt_vol;
	u32 batt_id;
	s32 batt_temp;
	s32 batt_current;
	u32 charging_source;
	u32 level;
	u32 level_raw;
	u32 full_level;
	u32 status;
	u32 chg_src;
	u32 chg_en;
	u32 chg_batt_en;
	u32 full_level_dis_batt_chg;
	u32 overload;
	u32 over_vchg;
	u32 health;
	bool is_full;
#ifdef CONFIG_HTC_CHARGER
	bool is_htcchg_ext_mode;
#endif 
};

struct battery_info_previous {
	s32 batt_temp;
	u32 charging_source;
	u32 level;
	u32 level_raw;
};

struct htc_battery_store {
	u32 batt_stored_magic_num;
	u32 batt_stored_soc;
	u32 batt_stored_temperature;
	unsigned long batt_stored_update_time;
	u32 consistent_flag;
};

struct htc_charger {
	int (*dump_all)(void);
	int (*get_vbus)(void);
	int (*get_attr_text)(char *buf, int size);
	int (*is_battery_full_eoc_stop)(int *result);
};

struct htc_gauge {
	int (*get_attr_text)(char *buf, int size);
};

struct htc_battery_info {
	struct battery_info_reply rep;
	struct battery_info_previous prev;
	struct htc_battery_store store;
	struct htc_charger *icharger;
	struct htc_gauge *igauge;
	struct power_supply		*batt_psy;
	struct power_supply		*bms_psy;
	struct power_supply		*usb_psy;
	int critical_low_voltage_mv;
	int smooth_chg_full_delay_min;
	int decreased_batt_level_check;
	int batt_full_voltage_mv;
	int batt_full_current_ma;
	int overload_curr_thr_ma;
	struct wake_lock charger_exist_lock;
	struct delayed_work chg_full_check_work;
#ifdef CONFIG_HTC_CHARGER
	struct delayed_work is_usb_overheat_work;
#endif 
	struct delayed_work chk_unknown_chg_work;
	struct delayed_work cable_impedance_work;
	int state;
	int vbus;
	int k_debug_flag;
	int current_limit_reason;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
	struct workqueue_struct *batt_fb_wq;
	struct delayed_work work_fb;
#endif
	unsigned int htc_extension;	
};

struct htc_battery_timer {
	unsigned long batt_system_jiffies;
	unsigned long batt_suspend_ms;
	unsigned long total_time_ms;	
	struct work_struct batt_work;
	struct timer_list batt_timer;
	struct workqueue_struct *batt_wq;
	struct wake_lock battery_lock;
	unsigned int time_out;
	struct alarm batt_check_wakeup_alarm;
};

struct htc_battery_platform_data {
	struct htc_charger icharger;
};

struct htc_pd_data {
	int	pd_list[10][2];
};

struct htc_charging_statistics {
        unsigned long begin_chg_time;
        unsigned long end_chg_time;
        int begin_chg_batt_level;
        int end_chg_batt_level;
};

struct htc_statistics_category {
        unsigned long chg_time_sum;
        unsigned long dischg_time_sum;
        int sample_count;
};

enum charger_control_flag {
	STOP_CHARGER = 0,
	ENABLE_CHARGER,
	ENABLE_LIMIT_CHARGER,
	DISABLE_LIMIT_CHARGER,
	DISABLE_PWRSRC,
	ENABLE_PWRSRC,
	DISABLE_PWRSRC_FINGERPRINT,
	ENABLE_PWRSRC_FINGERPRINT,
	END_CHARGER
};

enum ftm_charger_control_flag {
	FTM_ENABLE_CHARGER = 0,
	FTM_STOP_CHARGER,
	FTM_FAST_CHARGE,
	FTM_SLOW_CHARGE,
	FTM_END_CHARGER
};

enum user_set_input_current {
	SLOW_CHARGE_CURR = 500000,
	FAST_CHARGE_CURR = 900000,
	WALL_CHARGE_CURR = 1500000,
	HVDCP_CHARGE_CURR = 1600000,
	HVDCP_3_CHARGE_CURR = 2500000,
};

enum htc_batt_probe {
	CHARGER_PROBE_DONE,
	GAUGE_PROBE_DONE,
	HTC_BATT_PROBE_DONE,
	BATT_PROBE_MAX,
};
enum htc_charger_request {
	CHARGER_ABILITY_DETECT_DONE,
	CHARGER_5V_2A_DETECT_DONE,
};

int htc_battery_create_attrs(struct device *dev);
void htc_battery_info_update(enum power_supply_property prop, int intval);
void htc_battery_probe_process(enum htc_batt_probe probe_type);
int htc_battery_level_adjust(void);
int htc_battery_charger_switch_internal(int enable);
int htc_battery_pd_charger_support(int size, struct htc_pd_data pd_data, int *max_mA);
bool htc_battery_get_pd_type(int *curr);

int request_charger_status(enum htc_charger_request mode, void *ret_buf);
void set_aicl_enable(bool bEnable);
void impedance_set_iusb_max (int current_ua, bool mode);
int charger_dump_all(void);
int pmi8994_get_usbin_voltage_now(void);
int pmi8994_charger_get_attr_text(char *buf, int size);
int pmi8994_is_batt_full_eoc_stop(int *result);
int pmi8994_set_float_voltage_comp (int vfloat_comp);
void pmi8994_set_iusb_max (int current_ua);
void pmi8994_set_batt_health_good(void);
void pmi8994_rerun_apsd(void);
bool is_otg_enabled(void);
int pmi8996_get_chgr_sts(void);
void force_dump_fg_sram(void);
#ifdef CONFIG_HTC_BATT_WA_PCN0021
void pmi8996_set_dcp_default(void);
#endif 

#ifdef CONFIG_HTC_CHARGER
int pm8996_get_usb_temp(void);
bool htc_battery_get_discharging_reason(void);
#endif 
bool get_ima_error_status(void);

bool usb_otg_pulse_skip_control(bool disable);
