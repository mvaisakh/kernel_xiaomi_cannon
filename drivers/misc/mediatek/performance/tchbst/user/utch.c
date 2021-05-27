/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "[usrtch]"fmt

#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <mt-plat/fpsgo_common.h>


#include "tchbst.h"
#include "fstb.h"
#include "mtk_perfmgr_internal.h"

static void notify_touch_up_timeout(struct work_struct *work);
static DECLARE_WORK(mt_touch_timeout_work, (void *) notify_touch_up_timeout);

static struct workqueue_struct *wq;
static struct mutex notify_lock;
static struct hrtimer hrt1;

static int usrtch_dbg;
static int  touch_boost_value;
static int touch_boost_opp; /* boost freq of touch boost */
static int *cluster_opp;
static struct ppm_limit_data *target_freq, *reset_freq;
static int touch_boost_duration;
static long long active_time;
static int time_to_last_touch;
static int deboost_when_render;
static int usrtch_debug;
static int touch_event;/*touch down:1 */
static ktime_t last_touch_time;

void switch_usrtch(int enable)
{
	mutex_lock(&notify_lock);
	usrtch_dbg = !enable;
	mutex_unlock(&notify_lock);
}

void switch_eas_boost(int boost_value)
{
	touch_boost_value = boost_value;
}

void switch_init_opp(int boost_opp)
{
	int i;

	touch_boost_opp = boost_opp;
	for (i = 0; i < perfmgr_clusters; i++)
		target_freq[i].min =
			mt_cpufreq_get_freq_by_idx(i, touch_boost_opp);
}

void switch_cluster_opp(int id, int boost_opp)
{
	if (id < 0 || id >= perfmgr_clusters || boost_opp < -2)
		return;

	cluster_opp[id] = boost_opp;

	if (boost_opp == -2) /* don't boost */
		target_freq[id].min = -1;
	else if (boost_opp == -1) /* use touch_boost_opp */
		target_freq[id].min = mt_cpufreq_get_freq_by_idx(id, touch_boost_opp);
	else /* use boost_opp */
		target_freq[id].min = mt_cpufreq_get_freq_by_idx(id, boost_opp);
}

void switch_init_duration(int duration)
{
	touch_boost_duration = duration;
}

void switch_active_time(int duration)
{
	active_time = duration;
}

void switch_time_to_last_touch(int duration)
{
	time_to_last_touch = duration;
}

void switch_deboost_when_render(int enable)
{
	deboost_when_render = !!enable;
}

/*--------------------TIMER------------------------*/
static void enable_touch_boost_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0, touch_boost_duration);
	hrtimer_start(&hrt1, ktime, HRTIMER_MODE_REL);
	if (usrtch_debug)
		pr_debug("touch_boost_duration:\t %d\n", touch_boost_duration);

}

static void disable_touch_boost_timer(void)
{
	hrtimer_cancel(&hrt1);
}


static enum hrtimer_restart mt_touch_timeout(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &mt_touch_timeout_work);

	return HRTIMER_NORESTART;
}

/*--------------------FRAME HINT OP------------------------*/
int notify_touch(int action)
{
	int ret = 0;
	int isact = 0;
	ktime_t now, delta;

	if (!deboost_when_render && action == 3)
		return ret;

	if (action != 3) {
		now = ktime_get();
		delta = ktime_sub(now, last_touch_time);
		last_touch_time = now;

		/* lock is mandatory*/
		WARN_ON(!mutex_is_locked(&notify_lock));
		isact = is_fstb_active(active_time);

		perfmgr_trace_count(isact, "isact");

		if ((isact && ktime_to_ms(delta) < time_to_last_touch) ||
				usrtch_dbg)
			return ret;
	}

	/*action 1: touch down 2: touch up*/
	/* -> 3: fpsgo active*/
	if (action == 1) {
		disable_touch_boost_timer();
		enable_touch_boost_timer();

		/* boost */
		update_eas_uclamp_min(EAS_UCLAMP_KIR_TOUCH,
				CGROUP_TA, touch_boost_value);
		update_userlimit_cpu_freq(CPU_KIR_TOUCH,
				perfmgr_clusters, target_freq);
		if (usrtch_debug)
			pr_debug("touch down\n");
		perfmgr_trace_count(1, "touch");
		touch_event = 1;
	} else if (touch_event == 1 && action == 3) {
		disable_touch_boost_timer();
		update_eas_uclamp_min(EAS_UCLAMP_KIR_TOUCH, CGROUP_TA, 0);
		update_userlimit_cpu_freq(CPU_KIR_TOUCH,
			perfmgr_clusters, reset_freq);
		perfmgr_trace_count(3, "touch");
		touch_event = 2;
		if (usrtch_debug)
			pr_debug("touch timeout\n");
	}

	return ret;
}


static void notify_touch_up_timeout(struct work_struct *work)
{
	mutex_lock(&notify_lock);

	update_eas_uclamp_min(EAS_UCLAMP_KIR_TOUCH, CGROUP_TA, 0);
	update_userlimit_cpu_freq(CPU_KIR_TOUCH, perfmgr_clusters, reset_freq);
	perfmgr_trace_count(0, "touch");
	touch_event = 2;
	if (usrtch_debug)
		pr_debug("touch timeout\n");

	mutex_unlock(&notify_lock);
}

/*--------------------DEV OP------------------------*/
static ssize_t device_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[32], cmd[32];
	int arg1, arg2;

	arg1 = 0;
	arg2 = -1;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%31s %d %d", cmd, &arg1, &arg2) < 2)
		return -EFAULT;

	if (strncmp(cmd, "enable", 6) == 0)
		switch_usrtch(arg1);
	else if (strncmp(cmd, "eas_boost", 4) == 0)
		switch_eas_boost(arg1);
	else if (strncmp(cmd, "touch_opp", 9) == 0) {
		if (arg1 >= 0 && arg1 <= 15)
			switch_init_opp(arg1);
	} else if (strncmp(cmd, "cluster_opp", 11) == 0) {
		if (arg1 >= 0 && arg1 < perfmgr_clusters && arg2 >= -2 && arg2 <= 15)
			switch_cluster_opp(arg1, arg2);
	} else if (strncmp(cmd, "duration", 8) == 0) {
		switch_init_duration(arg1);
	} else if (strncmp(cmd, "active_time", 11) == 0) {
		if (arg1 >= 0)
			switch_active_time(arg1);
	} else if (strncmp(cmd, "time_to_last_touch", 18) == 0) {
		if (arg1 >= 0)
			switch_time_to_last_touch(arg1);
	} else if (strncmp(cmd, "deboost_when_render", 19) == 0) {
		if (arg1 >= 0)
			switch_deboost_when_render(arg1);
	}
	return cnt;
}

static int device_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "-----------------------------------------------------\n");
	seq_printf(m, "enable:\t%d\n", !usrtch_dbg);
	seq_printf(m, "eas_boost:\t%d\n", touch_boost_value);
	seq_printf(m, "touch_opp:\t%d\n", touch_boost_opp);

	for (i = 0; i < perfmgr_clusters; i++)
		seq_printf(m, "cluster_opp[%d]:\t%d\n",
		i, cluster_opp[i]);

	seq_printf(m, "duration(ns):\t%d\n", touch_boost_duration);
	seq_printf(m, "active_time(us):\t%d\n", (int)active_time);
	seq_printf(m, "time_to_last_touch(ms):\t%d\n", time_to_last_touch);
	seq_printf(m, "deboost_when_render:\t%d\n", deboost_when_render);
	seq_printf(m, "touch_event:\t%d\n", touch_event);
	seq_puts(m, "-----------------------------------------------------\n");
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

long usrtch_ioctl(unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;

	mutex_lock(&notify_lock);

	switch (cmd) {
		/*receive touch info*/
	case FPSGO_TOUCH:
		ret = notify_touch(arg);
		break;

	default:
		pr_debug("non-game unknown cmd %u\n", cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	mutex_unlock(&notify_lock);
	return ret >= 0 ? ret : 0;
}

static const struct file_operations Fops = {
	.open = device_open,
	.write = device_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/*--------------------usrdebug OP------------------------*/
static ssize_t mt_usrdebug_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[32];
	int arg, ret, val;

	arg = 0;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';
	ret = kstrtoint(buf, 10, &val);

	if (ret < 0) {
		pr_debug("ddr_write ret < 0\n");
		return ret;
	}

	usrtch_debug = val;
	return cnt;
}

static int mt_usrdebug_show(struct seq_file *m, void *v)
{
	seq_printf(m, "usrdebug\t%d\n", usrtch_debug);
	return 0;
}

static int mt_usrdebug_open(struct inode *inode, struct file *file)
{
	return single_open(file, mt_usrdebug_show, inode->i_private);
}
static const struct file_operations fop = {
	.open = mt_usrdebug_open,
	.write = mt_usrdebug_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
/*--------------------INIT------------------------*/
int init_utch(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *usrtch, *usrdebug, *usrtch_root;
	int i;
	int ret_val = 0;

	pr_debug("Start to init usrtch  driver\n");

	/*create usr touch root procfs*/
	usrtch_root = proc_mkdir("user", parent);

	touch_boost_value = TOUCH_BOOST_EAS;

	touch_event = 2;
	touch_boost_opp = TOUCH_BOOST_OPP;
	touch_boost_duration = TOUCH_TIMEOUT_NSEC;
	active_time = TOUCH_FSTB_ACTIVE_US;
	time_to_last_touch = TOUCH_TIME_TO_LAST_TOUCH_MS;
	last_touch_time = ktime_get();

	target_freq = kcalloc(perfmgr_clusters,
			sizeof(struct ppm_limit_data), GFP_KERNEL);
	reset_freq = kcalloc(perfmgr_clusters,
			sizeof(struct ppm_limit_data), GFP_KERNEL);
	cluster_opp = kcalloc(perfmgr_clusters,
			sizeof(int), GFP_KERNEL);

	for (i = 0; i < perfmgr_clusters; i++) {
		target_freq[i].min =
			mt_cpufreq_get_freq_by_idx(i, touch_boost_opp);
		target_freq[i].max = reset_freq[i].min = reset_freq[i].max = -1;
		cluster_opp[i] = -1; /* depend on touch_boost_opp */
	}
	mutex_init(&notify_lock);

	wq = create_singlethread_workqueue("mt_usrtch__work");
	if (!wq) {
		pr_debug("work create fail\n");
		return -ENOMEM;
	}

	hrtimer_init(&hrt1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt1.function = &mt_touch_timeout;

	usrtch = proc_create("usrtch", 0664, usrtch_root, &Fops);
	if (!usrtch) {
		ret_val = -ENOMEM;
		goto out_chrdev;
	}
	usrdebug = proc_create("usrdebug", 0664, usrtch_root, &fop);
	if (!usrdebug) {
		ret_val = -ENOMEM;
		goto out_chrdev;
	}
	pr_debug("init usrtch  driver done\n");

	return 0;

out_chrdev:
	destroy_workqueue(wq);
	return ret_val;
}
