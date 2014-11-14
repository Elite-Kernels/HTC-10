
#include <linux/sched.h>
#include <linux/sched/sysctl.h>
#include <linux/sched/rt.h>
#include <linux/sched/deadline.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/stop_machine.h>
#include <linux/tick.h>
#include <linux/slab.h>

#include "cpupri.h"
#include "cpudeadline.h"
#include "cpuacct.h"

struct rq;
struct cpuidle_state;

#define TASK_ON_RQ_QUEUED	1
#define TASK_ON_RQ_MIGRATING	2

extern __read_mostly int scheduler_running;

extern unsigned long calc_load_update;
extern atomic_long_t calc_load_tasks;

extern void calc_global_load_tick(struct rq *this_rq);
extern long calc_load_fold_active(struct rq *this_rq);

#ifdef CONFIG_SMP
extern void update_cpu_load_active(struct rq *this_rq);
#else
static inline void update_cpu_load_active(struct rq *this_rq) { }
#endif

#define NS_TO_JIFFIES(TIME)	((unsigned long)(TIME) / (NSEC_PER_SEC / HZ))

#if 0 
# define SCHED_LOAD_RESOLUTION	10
# define scale_load(w)		((w) << SCHED_LOAD_RESOLUTION)
# define scale_load_down(w)	((w) >> SCHED_LOAD_RESOLUTION)
#else
# define SCHED_LOAD_RESOLUTION	0
# define scale_load(w)		(w)
# define scale_load_down(w)	(w)
#endif

#define SCHED_LOAD_SHIFT	(10 + SCHED_LOAD_RESOLUTION)
#define SCHED_LOAD_SCALE	(1L << SCHED_LOAD_SHIFT)

#define NICE_0_LOAD		SCHED_LOAD_SCALE
#define NICE_0_SHIFT		SCHED_LOAD_SHIFT

#define SCHED_LOAD_WINDOW_SIZE  10

#define DL_SCALE (10)


#define RUNTIME_INF	((u64)~0ULL)

static inline int fair_policy(int policy)
{
	return policy == SCHED_NORMAL || policy == SCHED_BATCH;
}

static inline int rt_policy(int policy)
{
	return policy == SCHED_FIFO || policy == SCHED_RR;
}

static inline int dl_policy(int policy)
{
	return policy == SCHED_DEADLINE;
}

static inline int task_has_rt_policy(struct task_struct *p)
{
	return rt_policy(p->policy);
}

static inline int task_has_dl_policy(struct task_struct *p)
{
	return dl_policy(p->policy);
}

static inline bool dl_time_before(u64 a, u64 b)
{
	return (s64)(a - b) < 0;
}

static inline bool
dl_entity_preempt(struct sched_dl_entity *a, struct sched_dl_entity *b)
{
	return dl_time_before(a->deadline, b->deadline);
}

struct rt_prio_array {
	DECLARE_BITMAP(bitmap, MAX_RT_PRIO+1); 
	struct list_head queue[MAX_RT_PRIO];
};

struct rt_bandwidth {
	
	raw_spinlock_t		rt_runtime_lock;
	ktime_t			rt_period;
	u64			rt_runtime;
	struct hrtimer		rt_period_timer;
};

void __dl_clear_params(struct task_struct *p);

struct dl_bandwidth {
	raw_spinlock_t dl_runtime_lock;
	u64 dl_runtime;
	u64 dl_period;
};

static inline int dl_bandwidth_enabled(void)
{
	return sysctl_sched_rt_runtime >= 0;
}

extern struct dl_bw *dl_bw_of(int i);

struct dl_bw {
	raw_spinlock_t lock;
	u64 bw, total_bw;
};

extern struct mutex sched_domains_mutex;

#ifdef CONFIG_CGROUP_SCHED

#include <linux/cgroup.h>

struct cfs_rq;
struct rt_rq;

extern struct list_head task_groups;

struct cfs_bandwidth {
#ifdef CONFIG_CFS_BANDWIDTH
	raw_spinlock_t lock;
	ktime_t period;
	u64 quota, runtime;
	s64 hierarchical_quota;
	u64 runtime_expires;

	int idle, timer_active;
	struct hrtimer period_timer, slack_timer;
	struct list_head throttled_cfs_rq;

	
	int nr_periods, nr_throttled;
	u64 throttled_time;
#endif
};

struct task_group {
	struct cgroup_subsys_state css;

#ifdef CONFIG_FAIR_GROUP_SCHED
	
	struct sched_entity **se;
	
	struct cfs_rq **cfs_rq;
	unsigned long shares;

#ifdef	CONFIG_SMP
	atomic_long_t load_avg;
#endif
#endif

#ifdef CONFIG_RT_GROUP_SCHED
	struct sched_rt_entity **rt_se;
	struct rt_rq **rt_rq;

	struct rt_bandwidth rt_bandwidth;
#endif

	struct rcu_head rcu;
	struct list_head list;

	struct task_group *parent;
	struct list_head siblings;
	struct list_head children;

#ifdef CONFIG_SCHED_AUTOGROUP
	struct autogroup *autogroup;
#endif

	struct cfs_bandwidth cfs_bandwidth;
};

#ifdef CONFIG_FAIR_GROUP_SCHED
#define ROOT_TASK_GROUP_LOAD	NICE_0_LOAD

#define MIN_SHARES	(1UL <<  1)
#define MAX_SHARES	(1UL << 18)
#endif

typedef int (*tg_visitor)(struct task_group *, void *);

extern int walk_tg_tree_from(struct task_group *from,
			     tg_visitor down, tg_visitor up, void *data);

static inline int walk_tg_tree(tg_visitor down, tg_visitor up, void *data)
{
	return walk_tg_tree_from(&root_task_group, down, up, data);
}

extern int tg_nop(struct task_group *tg, void *data);

extern void free_fair_sched_group(struct task_group *tg);
extern int alloc_fair_sched_group(struct task_group *tg, struct task_group *parent);
extern void unregister_fair_sched_group(struct task_group *tg, int cpu);
extern void init_tg_cfs_entry(struct task_group *tg, struct cfs_rq *cfs_rq,
			struct sched_entity *se, int cpu,
			struct sched_entity *parent);
extern void init_cfs_bandwidth(struct cfs_bandwidth *cfs_b);
extern int sched_group_set_shares(struct task_group *tg, unsigned long shares);

extern void __refill_cfs_bandwidth_runtime(struct cfs_bandwidth *cfs_b);
extern void __start_cfs_bandwidth(struct cfs_bandwidth *cfs_b, bool force);
extern void unthrottle_cfs_rq(struct cfs_rq *cfs_rq);

extern void free_rt_sched_group(struct task_group *tg);
extern int alloc_rt_sched_group(struct task_group *tg, struct task_group *parent);
extern void init_tg_rt_entry(struct task_group *tg, struct rt_rq *rt_rq,
		struct sched_rt_entity *rt_se, int cpu,
		struct sched_rt_entity *parent);

extern struct task_group *sched_create_group(struct task_group *parent);
extern void sched_online_group(struct task_group *tg,
			       struct task_group *parent);
extern void sched_destroy_group(struct task_group *tg);
extern void sched_offline_group(struct task_group *tg);

extern void sched_move_task(struct task_struct *tsk);

#ifdef CONFIG_FAIR_GROUP_SCHED
extern int sched_group_set_shares(struct task_group *tg, unsigned long shares);
#endif

#else 

struct cfs_bandwidth { };

#endif	

/* CFS-related fields in a runqueue */
struct cfs_rq {
	struct load_weight load;
	unsigned int nr_running, h_nr_running;

	u64 exec_clock;
	u64 min_vruntime;
#ifndef CONFIG_64BIT
	u64 min_vruntime_copy;
#endif

	struct rb_root tasks_timeline;
	struct rb_node *rb_leftmost;

	struct sched_entity *curr, *next, *last, *skip;

#ifdef	CONFIG_SCHED_DEBUG
	unsigned int nr_spread_over;
#endif

#ifdef CONFIG_SMP
	/*
	 * CFS load tracking
	 */
	struct sched_avg avg;
	u64 runnable_load_sum;
	unsigned long runnable_load_avg;
#ifdef CONFIG_FAIR_GROUP_SCHED
	unsigned long tg_load_avg_contrib;
#endif
	atomic_long_t removed_load_avg, removed_util_avg;
#ifndef CONFIG_64BIT
	u64 load_last_update_time_copy;
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	/*
	 *   h_load = weight * f(tg)
	 *
	 * Where f(tg) is the recursive weight fraction assigned to
	 * this group.
	 */
	unsigned long h_load;
	u64 last_h_load_update;
	struct sched_entity *h_load_next;
#endif 
#endif 

#ifdef CONFIG_FAIR_GROUP_SCHED
	struct rq *rq;	

	int on_list;
	struct list_head leaf_cfs_rq_list;
	struct task_group *tg;	

#ifdef CONFIG_CFS_BANDWIDTH
	int runtime_enabled;
	u64 runtime_expires;
	s64 runtime_remaining;

	u64 throttled_clock, throttled_clock_task;
	u64 throttled_clock_task_time;
	int throttled, throttle_count;
	struct list_head throttled_list;
#endif 
#endif 
};

static inline int rt_bandwidth_enabled(void)
{
	return sysctl_sched_rt_runtime >= 0;
}

struct rt_rq {
	struct rt_prio_array active;
	unsigned int rt_nr_running;
#if defined CONFIG_SMP || defined CONFIG_RT_GROUP_SCHED
	struct {
		int curr; 
#ifdef CONFIG_SMP
		int next; 
#endif
	} highest_prio;
#endif
#ifdef CONFIG_SMP
	unsigned long rt_nr_migratory;
	unsigned long rt_nr_total;
	int overloaded;
	struct plist_head pushable_tasks;
#endif
	int rt_queued;

	int rt_throttled;
	u64 rt_time;
	u64 rt_runtime;
	
	raw_spinlock_t rt_runtime_lock;

#ifdef CONFIG_RT_GROUP_SCHED
	unsigned long rt_nr_boosted;

	struct rq *rq;
	struct task_group *tg;
#endif
};

struct dl_rq {
	
	struct rb_root rb_root;
	struct rb_node *rb_leftmost;

	unsigned long dl_nr_running;

#ifdef CONFIG_SMP
	struct {
		u64 curr;
		u64 next;
	} earliest_dl;

	unsigned long dl_nr_migratory;
	int overloaded;

	struct rb_root pushable_dl_tasks_root;
	struct rb_node *pushable_dl_tasks_leftmost;
#else
	struct dl_bw dl_bw;
#endif
};

#ifdef CONFIG_SMP

struct root_domain {
	atomic_t refcount;
	atomic_t rto_count;
	struct rcu_head rcu;
	cpumask_var_t span;
	cpumask_var_t online;

	
	bool overload;

	cpumask_var_t dlo_mask;
	atomic_t dlo_count;
	struct dl_bw dl_bw;
	struct cpudl cpudl;

	cpumask_var_t rto_mask;
	struct cpupri cpupri;

	/* Maximum cpu capacity in the system. */
	unsigned long max_cpu_capacity;
};

extern struct root_domain def_root_domain;

#endif 

struct rq {
	
	raw_spinlock_t lock;

	unsigned int nr_running;
#ifdef CONFIG_NUMA_BALANCING
	unsigned int nr_numa_running;
	unsigned int nr_preferred_running;
#endif
	#define CPU_LOAD_IDX_MAX 5
	unsigned long cpu_load[CPU_LOAD_IDX_MAX];
	unsigned long last_load_update_tick;
#ifdef CONFIG_NO_HZ_COMMON
	u64 nohz_stamp;
	unsigned long nohz_flags;
#endif
#ifdef CONFIG_NO_HZ_FULL
	unsigned long last_sched_tick;
#endif
	int skip_clock_update;

	
	struct load_weight load;
	unsigned long nr_load_updates;
	u64 nr_switches;

	struct cfs_rq cfs;
	struct rt_rq rt;
	struct dl_rq dl;

#ifdef CONFIG_FAIR_GROUP_SCHED
	
	struct list_head leaf_cfs_rq_list;
#endif /* CONFIG_FAIR_GROUP_SCHED */

	unsigned long nr_uninterruptible;

	struct task_struct *curr, *idle, *stop;
	unsigned long next_balance;
	struct mm_struct *prev_mm;

	u64 clock;
	u64 clock_task;

	atomic_t nr_iowait;

#ifdef CONFIG_SMP
	struct root_domain *rd;
	struct sched_domain *sd;

	unsigned long cpu_capacity;
	unsigned long cpu_capacity_orig;

	unsigned char idle_balance;
	
	int post_schedule;
	int active_balance;
	int push_cpu;
	struct cpu_stop_work active_balance_work;
	
	int cpu;
	int online;

	struct list_head cfs_tasks;

	u64 rt_avg;
	u64 age_stamp;
	u64 idle_stamp;
	u64 avg_idle;
	int cstate, wakeup_latency, wakeup_energy;

	
	u64 max_idle_balance_cost;
#endif

#ifdef CONFIG_IRQ_TIME_ACCOUNTING
	u64 prev_irq_time;
#endif
#ifdef CONFIG_PARAVIRT
	u64 prev_steal_time;
#endif
#ifdef CONFIG_PARAVIRT_TIME_ACCOUNTING
	u64 prev_steal_time_rq;
#endif

	
	unsigned long calc_load_update;
	long calc_load_active;

#ifdef CONFIG_SCHED_HRTICK
#ifdef CONFIG_SMP
	int hrtick_csd_pending;
	struct call_single_data hrtick_csd;
#endif
	struct hrtimer hrtick_timer;
#endif

#ifdef CONFIG_SCHEDSTATS
	
	struct sched_info rq_sched_info;
	unsigned long long rq_cpu_time;
	

	
	unsigned int yld_count;

	
	unsigned int sched_count;
	unsigned int sched_goidle;

	
	unsigned int ttwu_count;
	unsigned int ttwu_local;
#endif

#ifdef CONFIG_SMP
	struct llist_head wake_list;
#endif

#ifdef CONFIG_CPU_IDLE
	
	struct cpuidle_state *idle_state;
#endif
};

static inline int cpu_of(struct rq *rq)
{
#ifdef CONFIG_SMP
	return rq->cpu;
#else
	return 0;
#endif
}

DECLARE_PER_CPU_SHARED_ALIGNED(struct rq, runqueues);

#define cpu_rq(cpu)		(&per_cpu(runqueues, (cpu)))
#define this_rq()		this_cpu_ptr(&runqueues)
#define task_rq(p)		cpu_rq(task_cpu(p))
#define cpu_curr(cpu)		(cpu_rq(cpu)->curr)
#define raw_rq()		raw_cpu_ptr(&runqueues)

static inline u64 __rq_clock_broken(struct rq *rq)
{
	return READ_ONCE(rq->clock);
}

static inline u64 rq_clock(struct rq *rq)
{
	lockdep_assert_held(&rq->lock);
	return rq->clock;
}

static inline u64 rq_clock_task(struct rq *rq)
{
	lockdep_assert_held(&rq->lock);
	return rq->clock_task;
}

#ifdef CONFIG_NUMA_BALANCING
extern void sched_setnuma(struct task_struct *p, int node);
extern int migrate_task_to(struct task_struct *p, int cpu);
extern int migrate_swap(struct task_struct *, struct task_struct *);
#endif 

#ifdef CONFIG_SMP

extern void sched_ttwu_pending(void);

#define rcu_dereference_check_sched_domain(p) \
	rcu_dereference_check((p), \
			      lockdep_is_held(&sched_domains_mutex))

#define for_each_domain(cpu, __sd) \
	for (__sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd); \
			__sd; __sd = __sd->parent)

#define for_each_lower_domain(sd) for (; sd; sd = sd->child)

static inline struct sched_domain *highest_flag_domain(int cpu, int flag)
{
	struct sched_domain *sd, *hsd = NULL;

	for_each_domain(cpu, sd) {
		if (!(sd->flags & flag))
			break;
		hsd = sd;
	}

	return hsd;
}

static inline struct sched_domain *lowest_flag_domain(int cpu, int flag)
{
	struct sched_domain *sd;

	for_each_domain(cpu, sd) {
		if (sd->flags & flag)
			break;
	}

	return sd;
}

DECLARE_PER_CPU(struct sched_domain *, sd_llc);
DECLARE_PER_CPU(int, sd_llc_size);
DECLARE_PER_CPU(int, sd_llc_id);
DECLARE_PER_CPU(struct sched_domain *, sd_numa);
DECLARE_PER_CPU(struct sched_domain *, sd_busy);
DECLARE_PER_CPU(struct sched_domain *, sd_asym);

struct sched_group_capacity {
	atomic_t ref;
	/*
	 * CPU capacity of this group, SCHED_LOAD_SCALE being max capacity
	 * for a single CPU.
	 */
	unsigned int capacity;
	unsigned long next_update;
	int imbalance; 
	atomic_t nr_busy_cpus;

	unsigned long cpumask[0]; 
};

struct sched_group {
	struct sched_group *next;	
	atomic_t ref;

	unsigned int group_weight;
	struct sched_group_capacity *sgc;
	const struct sched_group_energy const *sge;

	unsigned long cpumask[0];
};

static inline struct cpumask *sched_group_cpus(struct sched_group *sg)
{
	return to_cpumask(sg->cpumask);
}

static inline struct cpumask *sched_group_mask(struct sched_group *sg)
{
	return to_cpumask(sg->sgc->cpumask);
}

static inline unsigned int group_first_cpu(struct sched_group *group)
{
	return cpumask_first(sched_group_cpus(group));
}

extern int group_balance_cpu(struct sched_group *sg);

#else

static inline void sched_ttwu_pending(void) { }

#endif 

#include "stats.h"
#include "auto_group.h"

#ifdef CONFIG_CGROUP_SCHED

static inline struct task_group *task_group(struct task_struct *p)
{
	return p->sched_task_group;
}

/* Change a task's cfs_rq and parent entity if it moves across CPUs/groups */
static inline void set_task_rq(struct task_struct *p, unsigned int cpu)
{
#if defined(CONFIG_FAIR_GROUP_SCHED) || defined(CONFIG_RT_GROUP_SCHED)
	struct task_group *tg = task_group(p);
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	p->se.cfs_rq = tg->cfs_rq[cpu];
	p->se.parent = tg->se[cpu];
#endif

#ifdef CONFIG_RT_GROUP_SCHED
	p->rt.rt_rq  = tg->rt_rq[cpu];
	p->rt.parent = tg->rt_se[cpu];
#endif
}

#else 

static inline void set_task_rq(struct task_struct *p, unsigned int cpu) { }
static inline struct task_group *task_group(struct task_struct *p)
{
	return NULL;
}

#endif /* CONFIG_CGROUP_SCHED */

static inline void __set_task_cpu(struct task_struct *p, unsigned int cpu)
{
	set_task_rq(p, cpu);
#ifdef CONFIG_SMP
	smp_wmb();
	task_thread_info(p)->cpu = cpu;
	p->wake_cpu = cpu;
#endif
}

#ifdef CONFIG_SCHED_DEBUG
# include <linux/static_key.h>
# define const_debug __read_mostly
#else
# define const_debug const
#endif

extern const_debug unsigned int sysctl_sched_features;

#define SCHED_FEAT(name, enabled)	\
	__SCHED_FEAT_##name ,

enum {
#include "features.h"
	__SCHED_FEAT_NR,
};

#undef SCHED_FEAT

#if defined(CONFIG_SCHED_DEBUG) && defined(HAVE_JUMP_LABEL)
#define SCHED_FEAT(name, enabled)					\
static __always_inline bool static_branch_##name(struct static_key *key) \
{									\
	return static_key_##enabled(key);				\
}

#include "features.h"

#undef SCHED_FEAT

extern struct static_key sched_feat_keys[__SCHED_FEAT_NR];
#define sched_feat(x) (static_branch_##x(&sched_feat_keys[__SCHED_FEAT_##x]))
#else 
#define sched_feat(x) (sysctl_sched_features & (1UL << __SCHED_FEAT_##x))
#endif 

#ifdef CONFIG_NUMA_BALANCING
#define sched_feat_numa(x) sched_feat(x)
#ifdef CONFIG_SCHED_DEBUG
#define numabalancing_enabled sched_feat_numa(NUMA)
#else
extern bool numabalancing_enabled;
#endif 
#else
#define sched_feat_numa(x) (0)
#define numabalancing_enabled (0)
#endif 

static inline u64 global_rt_period(void)
{
	return (u64)sysctl_sched_rt_period * NSEC_PER_USEC;
}

static inline u64 global_rt_runtime(void)
{
	if (sysctl_sched_rt_runtime < 0)
		return RUNTIME_INF;

	return (u64)sysctl_sched_rt_runtime * NSEC_PER_USEC;
}

static inline int task_current(struct rq *rq, struct task_struct *p)
{
	return rq->curr == p;
}

static inline int task_running(struct rq *rq, struct task_struct *p)
{
#ifdef CONFIG_SMP
	return p->on_cpu;
#else
	return task_current(rq, p);
#endif
}

static inline int task_on_rq_queued(struct task_struct *p)
{
	return p->on_rq == TASK_ON_RQ_QUEUED;
}

static inline int task_on_rq_migrating(struct task_struct *p)
{
	return p->on_rq == TASK_ON_RQ_MIGRATING;
}

#ifndef prepare_arch_switch
# define prepare_arch_switch(next)	do { } while (0)
#endif
#ifndef finish_arch_switch
# define finish_arch_switch(prev)	do { } while (0)
#endif
#ifndef finish_arch_post_lock_switch
# define finish_arch_post_lock_switch()	do { } while (0)
#endif

static inline void prepare_lock_switch(struct rq *rq, struct task_struct *next)
{
#ifdef CONFIG_SMP
	next->on_cpu = 1;
#endif
}

static inline void finish_lock_switch(struct rq *rq, struct task_struct *prev)
{
#ifdef CONFIG_SMP
	smp_wmb();
	prev->on_cpu = 0;
#endif
#ifdef CONFIG_DEBUG_SPINLOCK
	
	rq->lock.owner = current;
#endif
	spin_acquire(&rq->lock.dep_map, 0, 0, _THIS_IP_);

	raw_spin_unlock_irq(&rq->lock);
}

#define WF_SYNC		0x01		
#define WF_FORK		0x02		
#define WF_MIGRATED	0x4		
#define WF_NO_NOTIFIER	0x08		/* do not notify governor */

#define WEIGHT_IDLEPRIO                3
#define WMULT_IDLEPRIO         1431655765

static const int prio_to_weight[40] = {
      88761,     71755,     56483,     46273,     36291,
      29154,     23254,     18705,     14949,     11916,
       9548,      7620,      6100,      4904,      3906,
       3121,      2501,      1991,      1586,      1277,
       1024,       820,       655,       526,       423,
        335,       272,       215,       172,       137,
        110,        87,        70,        56,        45,
         36,        29,        23,        18,        15,
};

static const u32 prio_to_wmult[40] = {
      48388,     59856,     76040,     92818,    118348,
     147320,    184698,    229616,    287308,    360437,
     449829,    563644,    704093,    875809,   1099582,
    1376151,   1717300,   2157191,   2708050,   3363326,
    4194304,   5237765,   6557202,   8165337,  10153587,
   12820798,  15790321,  19976592,  24970740,  31350126,
   39045157,  49367440,  61356676,  76695844,  95443717,
  119304647, 148102320, 186737708, 238609294, 286331153,
};

#define ENQUEUE_WAKEUP		1
#define ENQUEUE_HEAD		2
#ifdef CONFIG_SMP
#define ENQUEUE_WAKING		4	
#else
#define ENQUEUE_WAKING		0
#endif
#define ENQUEUE_REPLENISH	8

#define DEQUEUE_SLEEP		1

#define RETRY_TASK		((void *)-1UL)

struct sched_class {
	const struct sched_class *next;

	void (*enqueue_task) (struct rq *rq, struct task_struct *p, int flags);
	void (*dequeue_task) (struct rq *rq, struct task_struct *p, int flags);
	void (*yield_task) (struct rq *rq);
	bool (*yield_to_task) (struct rq *rq, struct task_struct *p, bool preempt);

	void (*check_preempt_curr) (struct rq *rq, struct task_struct *p, int flags);

	struct task_struct * (*pick_next_task) (struct rq *rq,
						struct task_struct *prev);
	void (*put_prev_task) (struct rq *rq, struct task_struct *p);

#ifdef CONFIG_SMP
	int  (*select_task_rq)(struct task_struct *p, int task_cpu, int sd_flag, int flags);
	void (*migrate_task_rq)(struct task_struct *p, int next_cpu);

	void (*post_schedule) (struct rq *this_rq);
	void (*task_waking) (struct task_struct *task);
	void (*task_woken) (struct rq *this_rq, struct task_struct *task);

	void (*set_cpus_allowed)(struct task_struct *p,
				 const struct cpumask *newmask);

	void (*rq_online)(struct rq *rq);
	void (*rq_offline)(struct rq *rq);
#endif

	void (*set_curr_task) (struct rq *rq);
	void (*task_tick) (struct rq *rq, struct task_struct *p, int queued);
	void (*task_fork) (struct task_struct *p);
	void (*task_dead) (struct task_struct *p);

	/*
	 * The switched_from() call is allowed to drop rq->lock, therefore we
	 * cannot assume the switched_from/switched_to pair is serliazed by
	 * rq->lock. They are however serialized by p->pi_lock.
	 */
	void (*switched_from) (struct rq *this_rq, struct task_struct *task);
	void (*switched_to) (struct rq *this_rq, struct task_struct *task);
	void (*prio_changed) (struct rq *this_rq, struct task_struct *task,
			     int oldprio);

	unsigned int (*get_rr_interval) (struct rq *rq,
					 struct task_struct *task);

	void (*update_curr) (struct rq *rq);

#ifdef CONFIG_FAIR_GROUP_SCHED
	void (*task_move_group) (struct task_struct *p, int on_rq);
#endif
};

static inline void put_prev_task(struct rq *rq, struct task_struct *prev)
{
	prev->sched_class->put_prev_task(rq, prev);
}

#define sched_class_highest (&stop_sched_class)
#define for_each_class(class) \
   for (class = sched_class_highest; class; class = class->next)

extern const struct sched_class stop_sched_class;
extern const struct sched_class dl_sched_class;
extern const struct sched_class rt_sched_class;
extern const struct sched_class fair_sched_class;
extern const struct sched_class idle_sched_class;


#ifdef CONFIG_SMP

extern void update_group_capacity(struct sched_domain *sd, int cpu);

extern void trigger_load_balance(struct rq *rq);

extern void idle_enter_fair(struct rq *this_rq);
extern void idle_exit_fair(struct rq *this_rq);

#else

static inline void idle_enter_fair(struct rq *rq) { }
static inline void idle_exit_fair(struct rq *rq) { }

#endif

#ifdef CONFIG_CPU_IDLE
static inline void idle_set_state(struct rq *rq,
				  struct cpuidle_state *idle_state)
{
	rq->idle_state = idle_state;
}

static inline struct cpuidle_state *idle_get_state(struct rq *rq)
{
	WARN_ON(!rcu_read_lock_held());
	return rq->idle_state;
}
#else
static inline void idle_set_state(struct rq *rq,
				  struct cpuidle_state *idle_state)
{
}

static inline struct cpuidle_state *idle_get_state(struct rq *rq)
{
	return NULL;
}
#endif

extern void sysrq_sched_debug_show(void);
extern void sched_init_granularity(void);
extern void update_max_interval(void);

extern void init_sched_dl_class(void);
extern void init_sched_rt_class(void);
extern void init_sched_fair_class(void);
extern void init_sched_dl_class(void);

extern void resched_curr(struct rq *rq);
extern void resched_cpu(int cpu);

extern struct rt_bandwidth def_rt_bandwidth;
extern void init_rt_bandwidth(struct rt_bandwidth *rt_b, u64 period, u64 runtime);

extern struct dl_bandwidth def_dl_bandwidth;
extern void init_dl_bandwidth(struct dl_bandwidth *dl_b, u64 period, u64 runtime);
extern void init_dl_task_timer(struct sched_dl_entity *dl_se);

unsigned long to_ratio(u64 period, u64 runtime);

extern void init_entity_runnable_average(struct sched_entity *se);

static inline void add_nr_running(struct rq *rq, unsigned count)
{
	unsigned prev_nr = rq->nr_running;

	rq->nr_running = prev_nr + count;

	if (prev_nr < 2 && rq->nr_running >= 2) {
#ifdef CONFIG_SMP
		if (!rq->rd->overload)
			rq->rd->overload = true;
#endif

#ifdef CONFIG_NO_HZ_FULL
		if (tick_nohz_full_cpu(rq->cpu)) {
			tick_nohz_full_kick_cpu(rq->cpu);
		}
#endif
	}
}

static inline void sub_nr_running(struct rq *rq, unsigned count)
{
	rq->nr_running -= count;
}

static inline void rq_last_tick_reset(struct rq *rq)
{
#ifdef CONFIG_NO_HZ_FULL
	rq->last_sched_tick = jiffies;
#endif
}

extern void update_rq_clock(struct rq *rq);

extern void activate_task(struct rq *rq, struct task_struct *p, int flags);
extern void deactivate_task(struct rq *rq, struct task_struct *p, int flags);

extern void check_preempt_curr(struct rq *rq, struct task_struct *p, int flags);

extern const_debug unsigned int sysctl_sched_time_avg;
extern const_debug unsigned int sysctl_sched_nr_migrate;
extern const_debug unsigned int sysctl_sched_migration_cost;

static inline u64 sched_avg_period(void)
{
	return (u64)sysctl_sched_time_avg * NSEC_PER_MSEC / 2;
}

#ifdef CONFIG_SCHED_HRTICK

static inline int hrtick_enabled(struct rq *rq)
{
	if (!sched_feat(HRTICK))
		return 0;
	if (!cpu_active(cpu_of(rq)))
		return 0;
	return hrtimer_is_hres_active(&rq->hrtick_timer);
}

void hrtick_start(struct rq *rq, u64 delay);

#else

static inline int hrtick_enabled(struct rq *rq)
{
	return 0;
}

#endif 

#ifdef CONFIG_SMP
extern void sched_avg_update(struct rq *rq);

#ifndef arch_scale_freq_capacity
static __always_inline
unsigned long arch_scale_freq_capacity(struct sched_domain *sd, int cpu)
{
	return SCHED_CAPACITY_SCALE;
}
#endif

#ifndef arch_scale_cpu_capacity
static __always_inline
unsigned long arch_scale_cpu_capacity(struct sched_domain *sd, int cpu)
{
	if (sd && (sd->flags & SD_SHARE_CPUCAPACITY) && (sd->span_weight > 1))
		return sd->smt_gain / sd->span_weight;

	return SCHED_CAPACITY_SCALE;
}
#endif

static inline void sched_rt_avg_update(struct rq *rq, u64 rt_delta)
{
	rq->rt_avg += rt_delta * arch_scale_freq_capacity(NULL, cpu_of(rq));
	sched_avg_update(rq);
}
#else
static inline void sched_rt_avg_update(struct rq *rq, u64 rt_delta) { }
static inline void sched_avg_update(struct rq *rq) { }
#endif

extern void start_bandwidth_timer(struct hrtimer *period_timer, ktime_t period);

#ifdef CONFIG_SMP
#ifdef CONFIG_PREEMPT

static inline void double_rq_lock(struct rq *rq1, struct rq *rq2);

static inline int _double_lock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(this_rq->lock)
	__acquires(busiest->lock)
	__acquires(this_rq->lock)
{
	raw_spin_unlock(&this_rq->lock);
	double_rq_lock(this_rq, busiest);

	return 1;
}

#else
static inline int _double_lock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(this_rq->lock)
	__acquires(busiest->lock)
	__acquires(this_rq->lock)
{
	int ret = 0;

	if (unlikely(!raw_spin_trylock(&busiest->lock))) {
		if (busiest < this_rq) {
			raw_spin_unlock(&this_rq->lock);
			raw_spin_lock(&busiest->lock);
			raw_spin_lock_nested(&this_rq->lock,
					      SINGLE_DEPTH_NESTING);
			ret = 1;
		} else
			raw_spin_lock_nested(&busiest->lock,
					      SINGLE_DEPTH_NESTING);
	}
	return ret;
}

#endif 

static inline int double_lock_balance(struct rq *this_rq, struct rq *busiest)
{
	if (unlikely(!irqs_disabled())) {
		
		raw_spin_unlock(&this_rq->lock);
		BUG_ON(1);
	}

	return _double_lock_balance(this_rq, busiest);
}

static inline void double_unlock_balance(struct rq *this_rq, struct rq *busiest)
	__releases(busiest->lock)
{
	raw_spin_unlock(&busiest->lock);
	lock_set_subclass(&this_rq->lock.dep_map, 0, _RET_IP_);
}

static inline void double_lock(spinlock_t *l1, spinlock_t *l2)
{
	if (l1 > l2)
		swap(l1, l2);

	spin_lock(l1);
	spin_lock_nested(l2, SINGLE_DEPTH_NESTING);
}

static inline void double_lock_irq(spinlock_t *l1, spinlock_t *l2)
{
	if (l1 > l2)
		swap(l1, l2);

	spin_lock_irq(l1);
	spin_lock_nested(l2, SINGLE_DEPTH_NESTING);
}

static inline void double_raw_lock(raw_spinlock_t *l1, raw_spinlock_t *l2)
{
	if (l1 > l2)
		swap(l1, l2);

	raw_spin_lock(l1);
	raw_spin_lock_nested(l2, SINGLE_DEPTH_NESTING);
}

static inline void double_rq_lock(struct rq *rq1, struct rq *rq2)
	__acquires(rq1->lock)
	__acquires(rq2->lock)
{
	BUG_ON(!irqs_disabled());
	if (rq1 == rq2) {
		raw_spin_lock(&rq1->lock);
		__acquire(rq2->lock);	
	} else {
		if (rq1 < rq2) {
			raw_spin_lock(&rq1->lock);
			raw_spin_lock_nested(&rq2->lock, SINGLE_DEPTH_NESTING);
		} else {
			raw_spin_lock(&rq2->lock);
			raw_spin_lock_nested(&rq1->lock, SINGLE_DEPTH_NESTING);
		}
	}
}

static inline void double_rq_unlock(struct rq *rq1, struct rq *rq2)
	__releases(rq1->lock)
	__releases(rq2->lock)
{
	raw_spin_unlock(&rq1->lock);
	if (rq1 != rq2)
		raw_spin_unlock(&rq2->lock);
	else
		__release(rq2->lock);
}

#else 

static inline void double_rq_lock(struct rq *rq1, struct rq *rq2)
	__acquires(rq1->lock)
	__acquires(rq2->lock)
{
	BUG_ON(!irqs_disabled());
	BUG_ON(rq1 != rq2);
	raw_spin_lock(&rq1->lock);
	__acquire(rq2->lock);	
}

static inline void double_rq_unlock(struct rq *rq1, struct rq *rq2)
	__releases(rq1->lock)
	__releases(rq2->lock)
{
	BUG_ON(rq1 != rq2);
	raw_spin_unlock(&rq1->lock);
	__release(rq2->lock);
}

#endif

extern struct sched_entity *__pick_first_entity(struct cfs_rq *cfs_rq);
extern struct sched_entity *__pick_last_entity(struct cfs_rq *cfs_rq);
extern void print_cfs_stats(struct seq_file *m, int cpu);
extern void print_rt_stats(struct seq_file *m, int cpu);

extern void init_cfs_rq(struct cfs_rq *cfs_rq);
extern void init_rt_rq(struct rt_rq *rt_rq, struct rq *rq);
extern void init_dl_rq(struct dl_rq *dl_rq, struct rq *rq);

extern void cfs_bandwidth_usage_inc(void);
extern void cfs_bandwidth_usage_dec(void);

#ifdef CONFIG_NO_HZ_COMMON
enum rq_nohz_flag_bits {
	NOHZ_TICK_STOPPED,
	NOHZ_BALANCE_KICK,
};

#define NOHZ_KICK_ANY 0
#define NOHZ_KICK_RESTRICT 1

#define nohz_flags(cpu)	(&cpu_rq(cpu)->nohz_flags)
#endif

#ifdef CONFIG_IRQ_TIME_ACCOUNTING

DECLARE_PER_CPU(u64, cpu_hardirq_time);
DECLARE_PER_CPU(u64, cpu_softirq_time);

#ifndef CONFIG_64BIT
DECLARE_PER_CPU(seqcount_t, irq_time_seq);

static inline void irq_time_write_begin(void)
{
	__this_cpu_inc(irq_time_seq.sequence);
	smp_wmb();
}

static inline void irq_time_write_end(void)
{
	smp_wmb();
	__this_cpu_inc(irq_time_seq.sequence);
}

static inline u64 irq_time_read(int cpu)
{
	u64 irq_time;
	unsigned seq;

	do {
		seq = read_seqcount_begin(&per_cpu(irq_time_seq, cpu));
		irq_time = per_cpu(cpu_softirq_time, cpu) +
			   per_cpu(cpu_hardirq_time, cpu);
	} while (read_seqcount_retry(&per_cpu(irq_time_seq, cpu), seq));

	return irq_time;
}
#else 
static inline void irq_time_write_begin(void)
{
}

static inline void irq_time_write_end(void)
{
}

static inline u64 irq_time_read(int cpu)
{
	return per_cpu(cpu_softirq_time, cpu) + per_cpu(cpu_hardirq_time, cpu);
}
#endif /* CONFIG_64BIT */
#endif /* CONFIG_IRQ_TIME_ACCOUNTING */
