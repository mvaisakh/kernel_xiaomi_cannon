/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_QOS_BOUND_H__
#define __MTK_QOS_BOUND_H__

#define QOS_BOUND_BUF_SIZE		64

#define QOS_BOUND_BW_FREE		0x1
#define QOS_BOUND_BW_CONGESTIVE		0x2
#define QOS_BOUND_BW_FULL		0x4

#define QOS_BOUND_BW_CONGESTIVE_PCT	70
#define QOS_BOUND_BW_FULL_PCT		95

enum qos_emibm_type {
	QOS_EMIBM_TOTAL,
	QOS_EMIBM_CPU,
	QOS_EMIBM_GPU,
	QOS_EMIBM_MM,
	QOS_EMIBM_MD,

	NR_QOS_EMIBM_TYPE
};
enum qos_smibm_type {
	QOS_SMIBM_VENC,
	QOS_SMIBM_CAM,
	QOS_SMIBM_IMG,
	QOS_SMIBM_MDP,
	QOS_SMIBM_GPU,
	QOS_SMIBM_APU,
	QOS_SMIBM_VPU0,
	QOS_SMIBM_VPU1,
	QOS_SMIBM_MDLA,

	NR_QOS_SMIBM_TYPE
};

enum qos_lat_type {
	QOS_LAT_CPU,
	QOS_LAT_VPU0,
	QOS_LAT_VPU1,
	QOS_LAT_MDLA,

	NR_QOS_LAT_TYPE
};

struct qos_bound_stat {
	unsigned short num;
	unsigned short event;
	u64 wallclock;
	unsigned short emibw_mon[NR_QOS_EMIBM_TYPE];
	unsigned short emibw_req[NR_QOS_EMIBM_TYPE];
	unsigned short smibw_mon[NR_QOS_SMIBM_TYPE];
	unsigned short smibw_req[NR_QOS_SMIBM_TYPE];
	unsigned short lat_mon[NR_QOS_LAT_TYPE];
};

struct qos_bound {
	unsigned short idx;
	unsigned short state;
	struct qos_bound_stat stats[QOS_BOUND_BUF_SIZE];
};

extern void qos_bound_init(void);
extern struct qos_bound *get_qos_bound(void);
extern int get_qos_bound_bw_threshold(int state);
extern unsigned short get_qos_bound_idx(void);
extern int register_qos_notifier(struct notifier_block *nb);
extern int unregister_qos_notifier(struct notifier_block *nb);
extern int qos_notifier_call_chain(unsigned long val, void *v);
extern int is_qos_bound_enabled(void);
extern void qos_bound_enable(int enable);
extern int is_qos_bound_stress_enabled(void);
extern void qos_bound_stress_enable(int enable);
extern int is_qos_bound_log_enabled(void);
extern void qos_bound_log_enable(int enable);
extern unsigned int get_qos_bound_count(void);
extern unsigned int *get_qos_bound_buf(void);

#endif
