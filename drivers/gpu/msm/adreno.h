/* Copyright (c) 2008-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ADRENO_H
#define __ADRENO_H

#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "adreno_drawctxt.h"
#include "adreno_ringbuffer.h"
#include "adreno_profile.h"
#include "adreno_dispatch.h"
#include "kgsl_iommu.h"
#include "adreno_perfcounter.h"
#include <linux/stat.h>
#include <linux/delay.h>

#include "a4xx_reg.h"

#ifdef CONFIG_MSM_OCMEM
#include <soc/qcom/ocmem.h>
#endif

#define DEVICE_3D_NAME "kgsl-3d"
#define DEVICE_3D0_NAME "kgsl-3d0"

#define ADRENO_DEVICE(device) \
		container_of(device, struct adreno_device, dev)

#define KGSL_DEVICE(_dev) (&((_dev)->dev))

#define ADRENO_CONTEXT(context) \
		container_of(context, struct adreno_context, base)

#define ADRENO_GPU_DEVICE(_a) ((_a)->gpucore->gpudev)

#define ADRENO_CHIPID_CORE(_id) (((_id) >> 24) & 0xFF)
#define ADRENO_CHIPID_MAJOR(_id) (((_id) >> 16) & 0xFF)
#define ADRENO_CHIPID_MINOR(_id) (((_id) >> 8) & 0xFF)
#define ADRENO_CHIPID_PATCH(_id) ((_id) & 0xFF)

#define ADRENO_GPUREV(_a) ((_a)->gpucore->gpurev)

#define ADRENO_FEATURE(_dev, _bit) \
	((_dev)->gpucore->features & (_bit))

#define ADRENO_QUIRK(_dev, _bit) \
	((_dev)->quirks & (_bit))

#define ADRENO_PREEMPT_STYLE(flags) \
	((flags & KGSL_CONTEXT_PREEMPT_STYLE_MASK) >> \
		  KGSL_CONTEXT_PREEMPT_STYLE_SHIFT)

#define ADRENO_CMDBATCH_DISPATCH_CMDQUEUE(c)	\
	(&((ADRENO_CONTEXT(c->context))->rb->dispatch_q))

#define ADRENO_CMDBATCH_RB(c)			\
	((ADRENO_CONTEXT(c->context))->rb)

#define ADRENO_USES_OCMEM     BIT(0)
#define ADRENO_WARM_START     BIT(1)
#define ADRENO_USE_BOOTSTRAP  BIT(2)
#define ADRENO_SPTP_PC BIT(3)
#define ADRENO_PPD BIT(4)
#define ADRENO_CONTENT_PROTECTION BIT(5)
#define ADRENO_PREEMPTION BIT(6)
#define ADRENO_GPMU BIT(7)
#define ADRENO_LM BIT(8)
#define ADRENO_64BIT BIT(9)
#define ADRENO_CPZ_RETENTION BIT(10)


#define ADRENO_QUIRK_TWO_PASS_USE_WFI BIT(0)
#define ADRENO_QUIRK_IOMMU_SYNC BIT(1)
#define ADRENO_QUIRK_CRITICAL_PACKETS BIT(2)
#define ADRENO_QUIRK_FAULT_DETECT_MASK BIT(3)
#define ADRENO_QUIRK_DISABLE_RB_DP2CLOCKGATING BIT(4)

#define KGSL_CMD_FLAGS_NONE             0
#define KGSL_CMD_FLAGS_PMODE		BIT(0)
#define KGSL_CMD_FLAGS_INTERNAL_ISSUE   BIT(1)
#define KGSL_CMD_FLAGS_WFI              BIT(2)
#define KGSL_CMD_FLAGS_PROFILE		BIT(3)
#define KGSL_CMD_FLAGS_PWRON_FIXUP      BIT(4)

#define KGSL_CONTEXT_TO_MEM_IDENTIFIER	0x2EADBEEF
#define KGSL_CMD_IDENTIFIER		0x2EEDFACE
#define KGSL_CMD_INTERNAL_IDENTIFIER	0x2EEDD00D
#define KGSL_START_OF_IB_IDENTIFIER	0x2EADEABE
#define KGSL_END_OF_IB_IDENTIFIER	0x2ABEDEAD
#define KGSL_START_OF_PROFILE_IDENTIFIER	0x2DEFADE1
#define KGSL_END_OF_PROFILE_IDENTIFIER	0x2DEFADE2
#define KGSL_PWRON_FIXUP_IDENTIFIER	0x2AFAFAFA


#define ADRENO_IDLE_TIMEOUT (20 * 1000)

#define ADRENO_UCHE_GMEM_BASE	0x100000

enum adreno_gpurev {
	ADRENO_REV_UNKNOWN = 0,
	ADRENO_REV_A304 = 304,
	ADRENO_REV_A305 = 305,
	ADRENO_REV_A305C = 306,
	ADRENO_REV_A306 = 307,
	ADRENO_REV_A306A = 308,
	ADRENO_REV_A310 = 310,
	ADRENO_REV_A320 = 320,
	ADRENO_REV_A330 = 330,
	ADRENO_REV_A305B = 335,
	ADRENO_REV_A405 = 405,
	ADRENO_REV_A418 = 418,
	ADRENO_REV_A420 = 420,
	ADRENO_REV_A430 = 430,
	ADRENO_REV_A505 = 505,
	ADRENO_REV_A506 = 506,
	ADRENO_REV_A510 = 510,
	ADRENO_REV_A530 = 530,
	ADRENO_REV_A540 = 540,
};

#define ADRENO_START_WARM 0
#define ADRENO_START_COLD 1

#define ADRENO_SOFT_FAULT BIT(0)
#define ADRENO_HARD_FAULT BIT(1)
#define ADRENO_TIMEOUT_FAULT BIT(2)
#define ADRENO_IOMMU_PAGE_FAULT BIT(3)
#define ADRENO_PREEMPT_FAULT BIT(4)

#define ADRENO_SPTP_PC_CTRL 0
#define ADRENO_PPD_CTRL     1
#define ADRENO_LM_CTRL      2
#define ADRENO_HWCG_CTRL    3

#define ADRENO_GPMU_THROTTLE_COUNTERS 4
#define ADRENO_GPMU_THROTTLE_COUNTERS_BASE_REG 43

struct adreno_gpudev;

#define ADRENO_PREEMPT_TIMEOUT 10000

enum adreno_preempt_states {
	ADRENO_PREEMPT_NONE = 0,
	ADRENO_PREEMPT_START,
	ADRENO_PREEMPT_TRIGGERED,
	ADRENO_PREEMPT_FAULTED,
	ADRENO_PREEMPT_PENDING,
	ADRENO_PREEMPT_COMPLETE,
};

struct adreno_preemption {
	atomic_t state;
	struct kgsl_memdesc counters;
	struct timer_list timer;
	struct work_struct work;
	bool token_submit;
};


struct adreno_busy_data {
	unsigned int gpu_busy;
	unsigned int vbif_ram_cycles;
	unsigned int vbif_starved_ram;
	unsigned int throttle_cycles[ADRENO_GPMU_THROTTLE_COUNTERS];
};

struct adreno_gpu_core {
	enum adreno_gpurev gpurev;
	unsigned int core, major, minor, patchid;
	unsigned long features;
	const char *pm4fw_name;
	const char *pfpfw_name;
	const char *zap_name;
	struct adreno_gpudev *gpudev;
	size_t gmem_size;
	unsigned int pm4_jt_idx;
	unsigned int pm4_jt_addr;
	unsigned int pfp_jt_idx;
	unsigned int pfp_jt_addr;
	unsigned int pm4_bstrp_size;
	unsigned int pfp_bstrp_size;
	unsigned int pfp_bstrp_ver;
	unsigned long shader_offset;
	unsigned int shader_size;
	unsigned int num_protected_regs;
	const char *gpmufw_name;
	unsigned int gpmu_major;
	unsigned int gpmu_minor;
	unsigned int gpmu_features;
	unsigned int busy_mask;
	unsigned int lm_major, lm_minor;
	const char *regfw_name;
	unsigned int gpmu_tsens;
	unsigned int max_power;
};

struct adreno_device {
	struct kgsl_device dev;    
	unsigned long priv;
	unsigned int chipid;
	unsigned long gmem_base;
	unsigned long gmem_size;
	const struct adreno_gpu_core *gpucore;
	unsigned int *pfp_fw;
	size_t pfp_fw_size;
	unsigned int pfp_fw_version;
	struct kgsl_memdesc pfp;
	unsigned int *pm4_fw;
	size_t pm4_fw_size;
	unsigned int pm4_fw_version;
	struct kgsl_memdesc pm4;
	size_t gpmu_cmds_size;
	unsigned int *gpmu_cmds;
	struct adreno_ringbuffer ringbuffers[KGSL_PRIORITY_MAX_RB_LEVELS];
	int num_ringbuffers;
	struct adreno_ringbuffer *cur_rb;
	struct adreno_ringbuffer *next_rb;
	struct adreno_ringbuffer *prev_rb;
	unsigned int fast_hang_detect;
	unsigned long ft_policy;
	unsigned int long_ib_detect;
	unsigned long ft_pf_policy;
	struct ocmem_buf *ocmem_hdl;
	struct adreno_profile profile;
	struct adreno_dispatcher dispatcher;
	struct kgsl_memdesc pwron_fixup;
	unsigned int pwron_fixup_dwords;
	struct work_struct input_work;
	struct adreno_busy_data busy_data;
	unsigned int ram_cycles_lo;
	unsigned int starved_ram_lo;
	unsigned int perfctr_pwr_lo;
	atomic_t halt;
	struct dentry *ctx_d_debugfs;
	unsigned long pwrctrl_flag;

	struct kgsl_memdesc cmdbatch_profile_buffer;
	unsigned int cmdbatch_profile_index;
	uint64_t sp_local_gpuaddr;
	uint64_t sp_pvt_gpuaddr;
	const struct firmware *lm_fw;
	uint32_t *lm_sequence;
	uint32_t lm_size;
	struct adreno_preemption preempt;
	struct work_struct gpmu_work;
	uint32_t lm_leakage;
	uint32_t lm_limit;
	uint32_t lm_threshold_count;
	uint32_t lm_threshold_cross;

	unsigned int speed_bin;
	unsigned int quirks;

	struct coresight_device *csdev;
	uint32_t gpmu_throttle_counters[ADRENO_GPMU_THROTTLE_COUNTERS];
	struct work_struct irq_storm_work;

	struct list_head active_list;
	spinlock_t active_list_lock;
};

enum adreno_device_flags {
	ADRENO_DEVICE_PWRON = 0,
	ADRENO_DEVICE_PWRON_FIXUP = 1,
	ADRENO_DEVICE_INITIALIZED = 2,
	ADRENO_DEVICE_CORESIGHT = 3,
	ADRENO_DEVICE_HANG_INTR = 4,
	ADRENO_DEVICE_STARTED = 5,
	ADRENO_DEVICE_FAULT = 6,
	ADRENO_DEVICE_CMDBATCH_PROFILE = 7,
	ADRENO_DEVICE_GPU_REGULATOR_ENABLED = 8,
	ADRENO_DEVICE_PREEMPTION = 9,
	ADRENO_DEVICE_SOFT_FAULT_DETECT = 10,
	ADRENO_DEVICE_GPMU_INITIALIZED = 11,
	ADRENO_DEVICE_ISDB_ENABLED = 12,
	ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED = 13,
};

struct adreno_cmdbatch_profile_entry {
	uint64_t started;
	uint64_t retired;
};

#define ADRENO_CMDBATCH_PROFILE_COUNT \
	(PAGE_SIZE / sizeof(struct adreno_cmdbatch_profile_entry))

#define ADRENO_CMDBATCH_PROFILE_OFFSET(_index, _member) \
	 ((_index) * sizeof(struct adreno_cmdbatch_profile_entry) \
	  + offsetof(struct adreno_cmdbatch_profile_entry, _member))


enum adreno_regs {
	ADRENO_REG_CP_ME_RAM_WADDR,
	ADRENO_REG_CP_ME_RAM_DATA,
	ADRENO_REG_CP_PFP_UCODE_DATA,
	ADRENO_REG_CP_PFP_UCODE_ADDR,
	ADRENO_REG_CP_WFI_PEND_CTR,
	ADRENO_REG_CP_RB_BASE,
	ADRENO_REG_CP_RB_BASE_HI,
	ADRENO_REG_CP_RB_RPTR_ADDR_LO,
	ADRENO_REG_CP_RB_RPTR_ADDR_HI,
	ADRENO_REG_CP_RB_RPTR,
	ADRENO_REG_CP_RB_WPTR,
	ADRENO_REG_CP_CNTL,
	ADRENO_REG_CP_ME_CNTL,
	ADRENO_REG_CP_RB_CNTL,
	ADRENO_REG_CP_IB1_BASE,
	ADRENO_REG_CP_IB1_BASE_HI,
	ADRENO_REG_CP_IB1_BUFSZ,
	ADRENO_REG_CP_IB2_BASE,
	ADRENO_REG_CP_IB2_BASE_HI,
	ADRENO_REG_CP_IB2_BUFSZ,
	ADRENO_REG_CP_TIMESTAMP,
	ADRENO_REG_CP_SCRATCH_REG6,
	ADRENO_REG_CP_SCRATCH_REG7,
	ADRENO_REG_CP_ME_RAM_RADDR,
	ADRENO_REG_CP_ROQ_ADDR,
	ADRENO_REG_CP_ROQ_DATA,
	ADRENO_REG_CP_MERCIU_ADDR,
	ADRENO_REG_CP_MERCIU_DATA,
	ADRENO_REG_CP_MERCIU_DATA2,
	ADRENO_REG_CP_MEQ_ADDR,
	ADRENO_REG_CP_MEQ_DATA,
	ADRENO_REG_CP_HW_FAULT,
	ADRENO_REG_CP_PROTECT_STATUS,
	ADRENO_REG_CP_PREEMPT,
	ADRENO_REG_CP_PREEMPT_DEBUG,
	ADRENO_REG_CP_PREEMPT_DISABLE,
	ADRENO_REG_CP_PROTECT_REG_0,
	ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
	ADRENO_REG_RBBM_STATUS,
	ADRENO_REG_RBBM_STATUS3,
	ADRENO_REG_RBBM_PERFCTR_CTL,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD3,
	ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
	ADRENO_REG_RBBM_INT_0_MASK,
	ADRENO_REG_RBBM_INT_0_STATUS,
	ADRENO_REG_RBBM_PM_OVERRIDE2,
	ADRENO_REG_RBBM_INT_CLEAR_CMD,
	ADRENO_REG_RBBM_SW_RESET_CMD,
	ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD,
	ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD2,
	ADRENO_REG_RBBM_CLOCK_CTL,
	ADRENO_REG_VPC_DEBUG_RAM_SEL,
	ADRENO_REG_VPC_DEBUG_RAM_READ,
	ADRENO_REG_PA_SC_AA_CONFIG,
	ADRENO_REG_SQ_GPR_MANAGEMENT,
	ADRENO_REG_SQ_INST_STORE_MANAGMENT,
	ADRENO_REG_TP0_CHICKEN,
	ADRENO_REG_RBBM_RBBM_CTL,
	ADRENO_REG_UCHE_INVALIDATE0,
	ADRENO_REG_UCHE_INVALIDATE1,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
	ADRENO_REG_RBBM_SECVID_TRUST_CONTROL,
	ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
	ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI,
	ADRENO_REG_RBBM_SECVID_TRUST_CONFIG,
	ADRENO_REG_RBBM_SECVID_TSB_CONTROL,
	ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE,
	ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
	ADRENO_REG_RBBM_SECVID_TSB_TRUSTED_SIZE,
	ADRENO_REG_VBIF_XIN_HALT_CTRL0,
	ADRENO_REG_VBIF_XIN_HALT_CTRL1,
	ADRENO_REG_VBIF_VERSION,
	ADRENO_REG_REGISTER_MAX,
};

struct adreno_reg_offsets {
	unsigned int *const offsets;
	enum adreno_regs offset_0;
};

#define ADRENO_REG_UNUSED	0xFFFFFFFF
#define ADRENO_REG_SKIP	0xFFFFFFFE
#define ADRENO_REG_DEFINE(_offset, _reg) [_offset] = _reg

struct adreno_vbif_data {
	unsigned int reg;
	unsigned int val;
};

struct adreno_vbif_platform {
	int(*devfunc)(struct adreno_device *);
	const struct adreno_vbif_data *vbif;
};

struct adreno_vbif_snapshot_registers {
	const unsigned int version;
	const unsigned int mask;
	const unsigned int *registers;
	const int count;
};

struct adreno_coresight_register {
	unsigned int offset;
	unsigned int initial;
	unsigned int value;
};

struct adreno_coresight_attr {
	struct device_attribute attr;
	struct adreno_coresight_register *reg;
};

ssize_t adreno_coresight_show_register(struct device *device,
		struct device_attribute *attr, char *buf);

ssize_t adreno_coresight_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);

#define ADRENO_CORESIGHT_ATTR(_attrname, _reg) \
	struct adreno_coresight_attr coresight_attr_##_attrname  = { \
		__ATTR(_attrname, S_IRUGO | S_IWUSR, \
		adreno_coresight_show_register, \
		adreno_coresight_store_register), \
		(_reg), }

struct adreno_coresight {
	struct adreno_coresight_register *registers;
	unsigned int count;
	const struct attribute_group **groups;
};


struct adreno_irq_funcs {
	void (*func)(struct adreno_device *, int);
};
#define ADRENO_IRQ_CALLBACK(_c) { .func = _c }

struct adreno_irq {
	unsigned int mask;
	struct adreno_irq_funcs *funcs;
};

struct adreno_debugbus_block {
	unsigned int block_id;
	unsigned int dwords;
};

struct adreno_snapshot_sizes {
	int cp_pfp;
	int cp_me;
	int vpc_mem;
	int cp_meq;
	int shader_mem;
	int cp_merciu;
	int roq;
};

struct adreno_snapshot_data {
	struct adreno_snapshot_sizes *sect_sizes;
};

struct adreno_gpudev {
	const struct adreno_reg_offsets *reg_offsets;
	const struct adreno_ft_perf_counters *ft_perf_counters;
	unsigned int ft_perf_counters_count;

	struct adreno_perfcounters *perfcounters;
	const struct adreno_invalid_countables
			*invalid_countables;
	struct adreno_snapshot_data *snapshot_data;

	struct adreno_coresight *coresight;

	struct adreno_irq *irq;
	int num_prio_levels;
	unsigned int vbif_xin_halt_ctrl0_mask;
	
	void (*irq_trace)(struct adreno_device *, unsigned int status);
	void (*snapshot)(struct adreno_device *, struct kgsl_snapshot *);
	void (*platform_setup)(struct adreno_device *);
	void (*init)(struct adreno_device *);
	void (*remove)(struct adreno_device *);
	int (*rb_start)(struct adreno_device *, unsigned int start_type);
	int (*microcode_read)(struct adreno_device *);
	void (*perfcounter_init)(struct adreno_device *);
	void (*perfcounter_close)(struct adreno_device *);
	void (*start)(struct adreno_device *);
	bool (*is_sptp_idle)(struct adreno_device *);
	int (*regulator_enable)(struct adreno_device *);
	void (*regulator_disable)(struct adreno_device *);
	void (*pwrlevel_change_settings)(struct adreno_device *,
				unsigned int prelevel, unsigned int postlevel,
				bool post);
	unsigned int (*preemption_pre_ibsubmit)(struct adreno_device *,
				struct adreno_ringbuffer *rb,
				unsigned int *, struct kgsl_context *);
	int (*preemption_yield_enable)(unsigned int *);
	unsigned int (*preemption_post_ibsubmit)(struct adreno_device *,
				unsigned int *);
	int (*preemption_init)(struct adreno_device *);
	void (*preemption_schedule)(struct adreno_device *);
	void (*enable_64bit)(struct adreno_device *);
	void (*pre_reset)(struct adreno_device *);
};

enum kgsl_ft_policy_bits {
	KGSL_FT_OFF = 0,
	KGSL_FT_REPLAY = 1,
	KGSL_FT_SKIPIB = 2,
	KGSL_FT_SKIPFRAME = 3,
	KGSL_FT_DISABLE = 4,
	KGSL_FT_TEMP_DISABLE = 5,
	KGSL_FT_THROTTLE = 6,
	KGSL_FT_SKIPCMD = 7,
	
	KGSL_FT_MAX_BITS,
	
	
	KGSL_FT_SKIP_PMDUMP = 31,
};

#define KGSL_FT_POLICY_MASK GENMASK(KGSL_FT_MAX_BITS - 1, 0)

#define  KGSL_FT_DEFAULT_POLICY \
	(BIT(KGSL_FT_REPLAY) | \
	 BIT(KGSL_FT_SKIPCMD) | \
	 BIT(KGSL_FT_THROTTLE))

#define ADRENO_FT_TYPES \
	{ BIT(KGSL_FT_OFF), "off" }, \
	{ BIT(KGSL_FT_REPLAY), "replay" }, \
	{ BIT(KGSL_FT_SKIPIB), "skipib" }, \
	{ BIT(KGSL_FT_SKIPFRAME), "skipframe" }, \
	{ BIT(KGSL_FT_DISABLE), "disable" }, \
	{ BIT(KGSL_FT_TEMP_DISABLE), "temp" }, \
	{ BIT(KGSL_FT_THROTTLE), "throttle"}, \
	{ BIT(KGSL_FT_SKIPCMD), "skipcmd" }

enum {
	KGSL_FT_PAGEFAULT_INT_ENABLE = 0,
	KGSL_FT_PAGEFAULT_GPUHALT_ENABLE = 1,
	KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE = 2,
	KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT = 3,
	
	KGSL_FT_PAGEFAULT_MAX_BITS,
};

#define KGSL_FT_PAGEFAULT_MASK GENMASK(KGSL_FT_PAGEFAULT_MAX_BITS - 1, 0)

#define KGSL_FT_PAGEFAULT_DEFAULT_POLICY 0

#define FOR_EACH_RINGBUFFER(_dev, _rb, _i)			\
	for ((_i) = 0, (_rb) = &((_dev)->ringbuffers[0]);	\
		(_i) < (_dev)->num_ringbuffers;			\
		(_i)++, (_rb)++)

struct adreno_ft_perf_counters {
	unsigned int counter;
	unsigned int countable;
};

extern unsigned int *adreno_ft_regs;
extern unsigned int adreno_ft_regs_num;
extern unsigned int *adreno_ft_regs_val;

extern struct adreno_gpudev adreno_a3xx_gpudev;
extern struct adreno_gpudev adreno_a4xx_gpudev;
extern struct adreno_gpudev adreno_a5xx_gpudev;

extern int adreno_wake_nice;
extern unsigned int adreno_wake_timeout;

long adreno_ioctl(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg);

long adreno_ioctl_helper(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg,
		const struct kgsl_ioctl *cmds, int len);

int adreno_spin_idle(struct adreno_device *device, unsigned int timeout);
int adreno_idle(struct kgsl_device *device);
bool adreno_isidle(struct kgsl_device *device);

int adreno_set_constraint(struct kgsl_device *device,
				struct kgsl_context *context,
				struct kgsl_device_constraint *constraint);

void adreno_shadermem_regread(struct kgsl_device *device,
						unsigned int offsetwords,
						unsigned int *value);

void adreno_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		struct kgsl_context *context);

int adreno_reset(struct kgsl_device *device, int fault);

void adreno_fault_skipcmd_detached(struct adreno_device *adreno_dev,
					 struct adreno_context *drawctxt,
					 struct kgsl_cmdbatch *cmdbatch);

int adreno_coresight_init(struct adreno_device *adreno_dev);

void adreno_coresight_start(struct adreno_device *adreno_dev);
void adreno_coresight_stop(struct adreno_device *adreno_dev);

void adreno_coresight_remove(struct adreno_device *adreno_dev);

bool adreno_hw_isidle(struct adreno_device *adreno_dev);

void adreno_fault_detect_start(struct adreno_device *adreno_dev);
void adreno_fault_detect_stop(struct adreno_device *adreno_dev);

void adreno_hang_int_callback(struct adreno_device *adreno_dev, int bit);
void adreno_cp_callback(struct adreno_device *adreno_dev, int bit);

int adreno_sysfs_init(struct adreno_device *adreno_dev);
void adreno_sysfs_close(struct adreno_device *adreno_dev);

void adreno_irqctrl(struct adreno_device *adreno_dev, int state);

long adreno_ioctl_perfcounter_get(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data);

long adreno_ioctl_perfcounter_put(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data);

int adreno_efuse_map(struct adreno_device *adreno_dev);
int adreno_efuse_read_u32(struct adreno_device *adreno_dev, unsigned int offset,
		unsigned int *val);
void adreno_efuse_unmap(struct adreno_device *adreno_dev);

#define ADRENO_TARGET(_name, _id) \
static inline int adreno_is_##_name(struct adreno_device *adreno_dev) \
{ \
	return (ADRENO_GPUREV(adreno_dev) == (_id)); \
}

static inline int adreno_is_a3xx(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) >= 300) &&
		(ADRENO_GPUREV(adreno_dev) < 400));
}

ADRENO_TARGET(a304, ADRENO_REV_A304)
ADRENO_TARGET(a305, ADRENO_REV_A305)
ADRENO_TARGET(a305b, ADRENO_REV_A305B)
ADRENO_TARGET(a305c, ADRENO_REV_A305C)
ADRENO_TARGET(a306, ADRENO_REV_A306)
ADRENO_TARGET(a306a, ADRENO_REV_A306A)
ADRENO_TARGET(a310, ADRENO_REV_A310)
ADRENO_TARGET(a320, ADRENO_REV_A320)
ADRENO_TARGET(a330, ADRENO_REV_A330)

static inline int adreno_is_a330v2(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A330) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) > 0));
}

static inline int adreno_is_a330v21(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A330) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) > 0xF));
}

static inline int adreno_is_a4xx(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 400 &&
		ADRENO_GPUREV(adreno_dev) < 500;
}

ADRENO_TARGET(a405, ADRENO_REV_A405);

static inline int adreno_is_a405v2(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A405) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 0x10);
}

ADRENO_TARGET(a418, ADRENO_REV_A418)
ADRENO_TARGET(a420, ADRENO_REV_A420)
ADRENO_TARGET(a430, ADRENO_REV_A430)

static inline int adreno_is_a430v2(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A430) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 1));
}

static inline int adreno_is_a5xx(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 500 &&
			ADRENO_GPUREV(adreno_dev) < 600;
}

ADRENO_TARGET(a505, ADRENO_REV_A505)
ADRENO_TARGET(a506, ADRENO_REV_A506)
ADRENO_TARGET(a510, ADRENO_REV_A510)
ADRENO_TARGET(a530, ADRENO_REV_A530)
ADRENO_TARGET(a540, ADRENO_REV_A540)

static inline int adreno_is_a530v1(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A530) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 0);
}

static inline int adreno_is_a530v2(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A530) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 1);
}

static inline int adreno_is_a530v3(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A530) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 2);
}

static inline int adreno_is_a505_or_a506(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 505 &&
			ADRENO_GPUREV(adreno_dev) <= 506;
}

static inline int adreno_is_a540v1(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A540) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 0);
}

static inline bool adreno_checkreg_off(struct adreno_device *adreno_dev,
					enum adreno_regs offset_name)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (offset_name >= ADRENO_REG_REGISTER_MAX ||
		ADRENO_REG_UNUSED == gpudev->reg_offsets->offsets[offset_name])
			BUG();

	if (ADRENO_REG_SKIP == gpudev->reg_offsets->offsets[offset_name])
		return false;

	return true;
}

static inline void adreno_readreg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int *val)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regread(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets->offsets[offset_name], val);
	else
		*val = 0;
}

/*
 * adreno_writereg() - Write a register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be written
 * @val:		Value to write
 */
static inline void adreno_writereg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int val)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets->offsets[offset_name], val);
}

static inline unsigned int adreno_getreg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name)
{
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	if (!adreno_checkreg_off(adreno_dev, offset_name))
		return ADRENO_REG_REGISTER_MAX;
	return gpudev->reg_offsets->offsets[offset_name];
}

static inline unsigned int adreno_gpu_fault(struct adreno_device *adreno_dev)
{
	smp_rmb();
	return atomic_read(&adreno_dev->dispatcher.fault);
}

static inline void adreno_set_gpu_fault(struct adreno_device *adreno_dev,
	int state)
{
	
	atomic_add(state, &adreno_dev->dispatcher.fault);
	smp_wmb();
}



static inline void adreno_clear_gpu_fault(struct adreno_device *adreno_dev)
{
	atomic_set(&adreno_dev->dispatcher.fault, 0);
	smp_wmb();
}

static inline int adreno_gpu_halt(struct adreno_device *adreno_dev)
{
	smp_rmb();
	return atomic_read(&adreno_dev->halt);
}


static inline void adreno_clear_gpu_halt(struct adreno_device *adreno_dev)
{
	atomic_set(&adreno_dev->halt, 0);
	smp_wmb();
}

static inline void adreno_get_gpu_halt(struct adreno_device *adreno_dev)
{
	atomic_inc(&adreno_dev->halt);
}

static inline void adreno_put_gpu_halt(struct adreno_device *adreno_dev)
{
	if (atomic_dec_return(&adreno_dev->halt) < 0)
		BUG();
}


static inline void adreno_vbif_start(struct adreno_device *adreno_dev,
			const struct adreno_vbif_platform *vbif_platforms,
			int num_platforms)
{
	int i;
	const struct adreno_vbif_data *vbif = NULL;

	for (i = 0; i < num_platforms; i++) {
		if (vbif_platforms[i].devfunc(adreno_dev)) {
			vbif = vbif_platforms[i].vbif;
			break;
		}
	}

	while ((vbif != NULL) && (vbif->reg != 0)) {
		kgsl_regwrite(KGSL_DEVICE(adreno_dev), vbif->reg, vbif->val);
		vbif++;
	}
}

static inline void adreno_set_protected_registers(
		struct adreno_device *adreno_dev, unsigned int *index,
		unsigned int reg, int mask_len)
{
	unsigned int val;
	unsigned int base =
		adreno_getreg(adreno_dev, ADRENO_REG_CP_PROTECT_REG_0);
	unsigned int offset = *index;

	if (adreno_dev->gpucore->num_protected_regs)
		BUG_ON(*index >= adreno_dev->gpucore->num_protected_regs);
	else
		BUG_ON(*index >= 16);


	if (adreno_is_a4xx(adreno_dev) && *index >= 0x10) {
		base = A4XX_CP_PROTECT_REG_10;
		offset = *index - 0x10;
	}

	val = 0x60000000 | ((mask_len & 0x1F) << 24) | ((reg << 2) & 0xFFFFF);

	kgsl_regwrite(KGSL_DEVICE(adreno_dev), base + offset, val);
	*index = *index + 1;
}

#ifdef CONFIG_DEBUG_FS
void adreno_debugfs_init(struct adreno_device *adreno_dev);
void adreno_context_debugfs_init(struct adreno_device *,
				struct adreno_context *);
#else
static inline void adreno_debugfs_init(struct adreno_device *adreno_dev) { }
static inline void adreno_context_debugfs_init(struct adreno_device *device,
						struct adreno_context *context)
						{ }
#endif

static inline int adreno_compare_pm4_version(struct adreno_device *adreno_dev,
	unsigned int version)
{
	if (adreno_dev->pm4_fw_version == version)
		return 0;

	return (adreno_dev->pm4_fw_version > version) ? 1 : -1;
}

static inline int adreno_compare_pfp_version(struct adreno_device *adreno_dev,
	unsigned int version)
{
	if (adreno_dev->pfp_fw_version == version)
		return 0;

	return (adreno_dev->pfp_fw_version > version) ? 1 : -1;
}

static inline int adreno_bootstrap_ucode(struct adreno_device *adreno_dev)
{
	return (ADRENO_FEATURE(adreno_dev, ADRENO_USE_BOOTSTRAP) &&
		adreno_compare_pfp_version(adreno_dev,
			adreno_dev->gpucore->pfp_bstrp_ver) >= 0) ? 1 : 0;
}

static inline bool adreno_in_preempt_state(struct adreno_device *adreno_dev,
			enum adreno_preempt_states state)
{
	return atomic_read(&adreno_dev->preempt.state) == state;
}
static inline void adreno_set_preempt_state(struct adreno_device *adreno_dev,
		enum adreno_preempt_states state)
{
	smp_wmb();
	atomic_set(&adreno_dev->preempt.state, state);

	
	smp_wmb();
}

static inline bool adreno_is_preemption_enabled(
				struct adreno_device *adreno_dev)
{
	return test_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
}
static inline struct adreno_ringbuffer *adreno_ctx_get_rb(
				struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt)
{
	struct kgsl_context *context;
	int level;
	if (!drawctxt)
		return NULL;

	context = &(drawctxt->base);


	if (!adreno_is_preemption_enabled(adreno_dev))
		return &(adreno_dev->ringbuffers[0]);

	level = context->priority / adreno_dev->num_ringbuffers;
	if (level < adreno_dev->num_ringbuffers)
		return &(adreno_dev->ringbuffers[level]);
	else
		return &(adreno_dev->ringbuffers[
				adreno_dev->num_ringbuffers - 1]);
}

static inline int adreno_compare_prio_level(int p1, int p2)
{
	return p2 - p1;
}

void adreno_readreg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t *val);

void adreno_writereg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t val);

unsigned int adreno_get_rptr(struct adreno_ringbuffer *rb);

static inline bool adreno_rb_empty(struct adreno_ringbuffer *rb)
{
	return (adreno_get_rptr(rb) == rb->wptr);
}

static inline bool adreno_soft_fault_detect(struct adreno_device *adreno_dev)
{
	return adreno_dev->fast_hang_detect &&
		!test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
}

static inline bool adreno_long_ib_detect(struct adreno_device *adreno_dev)
{
	return adreno_dev->long_ib_detect &&
		!test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
}

#if BITS_PER_LONG == 64
static inline bool adreno_support_64bit(struct adreno_device *adreno_dev)
{
	return ADRENO_FEATURE(adreno_dev, ADRENO_64BIT);
}
#else
static inline bool adreno_support_64bit(struct adreno_device *adreno_dev)
{
	return false;
}
#endif 

static inline void adreno_ringbuffer_set_global(
		struct adreno_device *adreno_dev, int name)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_sharedmem_writel(device,
		&adreno_dev->ringbuffers[0].pagetable_desc,
		PT_INFO_OFFSET(current_global_ptname), name);
}

static inline void adreno_ringbuffer_set_pagetable(struct adreno_ringbuffer *rb,
		struct kgsl_pagetable *pt)
{
	struct adreno_device *adreno_dev = ADRENO_RB_DEVICE(rb);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned long flags;

	spin_lock_irqsave(&rb->preempt_lock, flags);

	kgsl_sharedmem_writel(device, &rb->pagetable_desc,
		PT_INFO_OFFSET(current_rb_ptname), pt->name);

	kgsl_sharedmem_writeq(device, &rb->pagetable_desc,
		PT_INFO_OFFSET(ttbr0), kgsl_mmu_pagetable_get_ttbr0(pt));

	kgsl_sharedmem_writel(device, &rb->pagetable_desc,
		PT_INFO_OFFSET(contextidr),
		kgsl_mmu_pagetable_get_contextidr(pt));

	spin_unlock_irqrestore(&rb->preempt_lock, flags);
}

#endif 
