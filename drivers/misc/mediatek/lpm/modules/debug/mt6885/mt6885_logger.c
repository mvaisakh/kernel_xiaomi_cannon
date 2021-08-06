// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 */


#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/rtc.h>
#include <linux/wakeup_reason.h>
#include <linux/syscore_ops.h>

#include <aee.h>
#include <mtk_lpm.h>

#include <mt6885_spm_comm.h>
#include <mt6885_spm_reg.h>
#include <mt6885_pwr_ctrl.h>
#include <mt6885_pcm_def.h>
#include <mtk_dbg_common_v1.h>
#include <mt-plat/mtk_ccci_common.h>
#include <mtk_lpm_timer.h>
#include <mtk_lpm_sysfs.h>

#define MT6885_LOG_MONITOR_STATE_NAME	"mcusysoff"
#define MT6885_LOG_DEFAULT_MS		5000
/**********MI ADD START*****************/
#define CONFIG_LPM_STATE_RECORDS
#ifdef CONFIG_LPM_STATE_RECORDS
#include <linux/proc_fs.h>
#include <linux/rtc.h>
#endif

#define WORLD_CLK_CNTCV_L        (0x10017008)
#define WORLD_CLK_CNTCV_H        (0x1001700C)

#define CONN_SLEEP_MASK          (0xe010)
#define MODEM_SLEEP_MASK         (0x110c)
#define SUBSYS_SLEEP_MASK        (0xff)

#ifdef CONFIG_LPM_STATE_RECORDS
#define MAX_LSR_RECORDS		1024
#define MAX_LWR_RECORDS		1024
#define MAX_LPM_SHORT_TIME	(30*32*1024)

static DEFINE_SPINLOCK(lsr_lock);
static DEFINE_SPINLOCK(lwr_lock);

struct lpm_state_records_buff {
	char msg[256];
	struct timespec key_time;
};

struct lpm_wakeup_records_buff {
	char msg[256];
	struct timespec key_time;
};

struct lpm_state_records_buff lsr_buff[MAX_LSR_RECORDS];
struct lpm_wakeup_records_buff lwr_buff[MAX_LWR_RECORDS];

static u32 lsr_num;
static u32 index_head;
static u32 index_tail;
static u32 lwr_num;
static u32 lwr_index_head;
static u32 lwr_index_tail;

static void lsr_record(const struct mt6885_spm_wake_status *wakesta, unsigned int spm_26M_off_pct);
static void lwr_record(char *buf);
#endif
/**********MI ADD END*****************/

static struct mt6885_spm_wake_status mt6885_wake;
void __iomem *mt6885_spm_base;

struct mt6885_log_helper {
	short cur;
	short prev;
	struct mt6885_spm_wake_status *wakesrc;
};
struct mt6885_log_helper mt6885_logger_help = {
	.wakesrc = &mt6885_wake,
	.cur = 0,
	.prev = 0,
};

const char *mt6885_wakesrc_str[32] = {
	[0] = " R12_PCM_TIMER",
	[1] = " R12_RESERVED_DEBUG_B",
	[2] = " R12_KP_IRQ_B",
	[3] = " R12_APWDT_EVENT_B",
	[4] = " R12_APXGPT1_EVENT_B",
	[5] = " R12_CONN2AP_SPM_WAKEUP_B",
	[6] = " R12_EINT_EVENT_B",
	[7] = " R12_CONN_WDT_IRQ_B",
	[8] = " R12_CCIF0_EVENT_B",
	[9] = " R12_LOWBATTERY_IRQ_B",
	[10] = " R12_SC_SSPM2SPM_WAKEUP_B",
	[11] = " R12_SC_SCP2SPM_WAKEUP_B",
	[12] = " R12_SC_ADSP2SPM_WAKEUP_B",
	[13] = " R12_PCM_WDT_WAKEUP_B",
	[14] = " R12_USB_CDSC_B",
	[15] = " R12_USB_POWERDWN_B",
	[16] = " R12_SYS_TIMER_EVENT_B",
	[17] = " R12_EINT_EVENT_SECURE_B",
	[18] = " R12_CCIF1_EVENT_B",
	[19] = " R12_UART0_IRQ_B",
	[20] = " R12_AFE_IRQ_MCU_B",
	[21] = " R12_THERM_CTRL_EVENT_B",
	[22] = " R12_SYS_CIRQ_IRQ_B",
	[23] = " R12_MD2AP_PEER_EVENT_B",
	[24] = " R12_CSYSPWREQ_B",
	[25] = " R12_MD1_WDT_B",
	[26] = " R12_AP2AP_PEER_WAKEUPEVENT_B",
	[27] = " R12_SEJ_EVENT_B",
	[28] = " R12_SPM_CPU_WAKEUPEVENT_B",
	[29] = " R12_APUSYS",
	[30] = " R12_NOT_USED1",
	[31] = " R12_NOT_USED2",
};

struct spm_wakesrc_irq_list mt6885_spm_wakesrc_irqs[] = {
	/* mtk-kpd */
	{ WAKE_SRC_STA1_KP_IRQ_B, "mediatek,kp", 0, 0},
	/* mt_wdt */
	{ WAKE_SRC_STA1_APWDT_EVENT_B, "mediatek,toprgu", 0, 0},
	/* BTCVSD_ISR_Handle */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mtk-btcvsd-snd", 0, 0},
	/* BTIF_WAKEUP_IRQ */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,bt", 0, 0},
	/* BGF_SW_IRQ */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,bt", 1, 0},
	/* wlan0 */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,wifi", 0, 0},
	/* wlan0 */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,wifi", 1, 0},
	/* fm */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,fm", 0, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 0, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 1, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 2, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 3, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 4, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 5, 0},
	/* gps */
	{ WAKE_SRC_STA1_CONN2AP_SPM_WAKEUP_B, "mediatek,mt6885-gps", 6, 0},
	/* CCIF_AP_DATA */
	{ WAKE_SRC_STA1_CCIF0_EVENT_B, "mediatek,ap_ccif0", 0, 0},
	/* SCP IPC0 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 0, 0},
	/* SCP IPC1 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 1, 0},
	/* MBOX_ISR0 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 2, 0},
	/* MBOX_ISR1 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 3, 0},
	/* MBOX_ISR2 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 4, 0},
	/* MBOX_ISR3 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 5, 0},
	/* MBOX_ISR4 */
	{ WAKE_SRC_STA1_SC_SCP2SPM_WAKEUP_B, "mediatek,scp", 6, 0},
	/* ADSP_A_AUD */
	{ WAKE_SRC_STA1_SC_ADSP2SPM_WAKEUP_B, "mediatek,adsp_core_0", 2, 0},
	/* ADSP_B_AUD */
	{ WAKE_SRC_STA1_SC_ADSP2SPM_WAKEUP_B, "mediatek,adsp_core_1", 2, 0},
	/* CCIF0_AP */
	{ WAKE_SRC_STA1_MD1_WDT_B, "mediatek,mddriver", 2, 0},
	/* DPMAIF_AP */
	{ WAKE_SRC_STA1_AP2AP_PEER_WAKEUPEVENT_B, "mediatek,dpmaif", 0, 0},
};

#define plat_mmio_read(offset)	__raw_readl(mt6885_spm_base + offset)

#define IRQ_NUMBER	\
(sizeof(mt6885_spm_wakesrc_irqs)/sizeof(struct spm_wakesrc_irq_list))
static void mt6885_get_spm_wakesrc_irq(void)
{
	int i;
	struct device_node *node;

	for (i = 0; i < IRQ_NUMBER; i++) {
		if (mt6885_spm_wakesrc_irqs[i].name == NULL)
			continue;

		node = of_find_compatible_node(NULL, NULL,
			mt6885_spm_wakesrc_irqs[i].name);
		if (!node) {
			pr_info("[name:spm&][SPM] find '%s' node failed\n",
				mt6885_spm_wakesrc_irqs[i].name);
			continue;
		}

		mt6885_spm_wakesrc_irqs[i].irq_no =
			irq_of_parse_and_map(node,
				mt6885_spm_wakesrc_irqs[i].order);

		if (!mt6885_spm_wakesrc_irqs[i].irq_no) {
			pr_info("[name:spm&][SPM] get '%s' failed\n",
				mt6885_spm_wakesrc_irqs[i].name);
		}
	}
}

struct mt6885_logger_timer {
	struct mt_lpm_timer tm;
	unsigned int fired;
};
struct mt6885_logger_fired_info {
	unsigned int fired;
	int state_index;
};

static struct mt6885_logger_timer mt6885_log_timer;
static struct mt6885_logger_fired_info mt6885_logger_fired;

u64 ap_pd_count;
u64 ap_slp_duration;
u64 spm_26M_off_count;
u64 spm_26M_off_duration;
u32 before_ap_slp_duration;

int mt6885_get_wakeup_status(struct mt6885_log_helper *help)
{
	if (!help->wakesrc || !mt6885_spm_base)
		return -EINVAL;

	help->wakesrc->r12 = plat_mmio_read(SPM_BK_WAKE_EVENT);
	help->wakesrc->r12_ext = plat_mmio_read(SPM_WAKEUP_STA);
	help->wakesrc->raw_sta = plat_mmio_read(SPM_WAKEUP_STA);
	help->wakesrc->raw_ext_sta = plat_mmio_read(SPM_WAKEUP_EXT_STA);
	help->wakesrc->md32pcm_wakeup_sta = plat_mmio_read(MD32PCM_WAKEUP_STA);
	help->wakesrc->md32pcm_event_sta = plat_mmio_read(MD32PCM_EVENT_STA);

	help->wakesrc->src_req = plat_mmio_read(SPM_SRC_REQ);

	/* backup of SPM_WAKEUP_MISC */
	help->wakesrc->wake_misc = plat_mmio_read(SPM_BK_WAKE_MISC);

	/* get sleep time */
	/* backup of PCM_TIMER_OUT */
	help->wakesrc->timer_out = plat_mmio_read(SPM_BK_PCM_TIMER);

	/* get other SYS and co-clock status */
	help->wakesrc->r13 = plat_mmio_read(PCM_REG13_DATA);
	help->wakesrc->idle_sta = plat_mmio_read(SUBSYS_IDLE_STA);
	help->wakesrc->req_sta0 = plat_mmio_read(SRC_REQ_STA_0);
	help->wakesrc->req_sta1 = plat_mmio_read(SRC_REQ_STA_1);
	help->wakesrc->req_sta2 = plat_mmio_read(SRC_REQ_STA_2);
	help->wakesrc->req_sta3 = plat_mmio_read(SRC_REQ_STA_3);
	help->wakesrc->req_sta4 = plat_mmio_read(SRC_REQ_STA_4);

	/* get HW CG check status */
	help->wakesrc->cg_check_sta = plat_mmio_read(SPM_CG_CHECK_STA);

	/* get debug flag for PCM execution check */
	help->wakesrc->debug_flag = plat_mmio_read(PCM_WDT_LATCH_SPARE_0);
	help->wakesrc->debug_flag1 = plat_mmio_read(PCM_WDT_LATCH_SPARE_1);

	/* get backup SW flag status */
	help->wakesrc->b_sw_flag0 = plat_mmio_read(SPM_SW_RSV_7);
	help->wakesrc->b_sw_flag1 = plat_mmio_read(SPM_SW_RSV_8);

	/* get ISR status */
	help->wakesrc->isr = plat_mmio_read(SPM_IRQ_STA);

	/* get SW flag status */
	help->wakesrc->sw_flag0 = plat_mmio_read(SPM_SW_FLAG_0);
	help->wakesrc->sw_flag1 = plat_mmio_read(SPM_SW_FLAG_1);

	/* get CLK SETTLE */
	help->wakesrc->clk_settle = plat_mmio_read(SPM_CLK_SETTLE);
	/* check abort */
	help->wakesrc->is_abort =
		help->wakesrc->debug_flag & DEBUG_ABORT_MASK;
	help->wakesrc->is_abort |=
		help->wakesrc->debug_flag1 & DEBUG_ABORT_MASK_1;

	help->cur += 1;
	return 0;
}

static void mt6885_save_sleep_info(void)
{
#define AVOID_OVERFLOW (0xFFFFFFFF00000000)
	u32 off_26M_duration;
	u32 slp_duration;

	slp_duration = plat_mmio_read(SPM_BK_PCM_TIMER);
	if (slp_duration == before_ap_slp_duration)
		return;

	/* Save ap off counter and duration */
	if (ap_pd_count >= AVOID_OVERFLOW)
		ap_pd_count = 0;
	else
		ap_pd_count++;

	if (ap_slp_duration >= AVOID_OVERFLOW)
		ap_slp_duration = 0;
	else {
		ap_slp_duration = ap_slp_duration + slp_duration;
		before_ap_slp_duration = slp_duration;
	}

	/* Save 26M's off counter and duration */
	if (spm_26M_off_duration >= AVOID_OVERFLOW)
		spm_26M_off_duration = 0;
	else {
		off_26M_duration = plat_mmio_read(SPM_BK_VTCXO_DUR);
		if (off_26M_duration == 0)
			return;

		spm_26M_off_duration = spm_26M_off_duration +
			off_26M_duration;
	}

	if (spm_26M_off_count >= AVOID_OVERFLOW)
		spm_26M_off_count = 0;
	else
		spm_26M_off_count = (plat_mmio_read(SPM_VTCXO_EVENT_COUNT_STA)
					& 0xffff)
			+ spm_26M_off_count;
}

static void mt6885_suspend_show_detailed_wakeup_reason
	(struct mt6885_spm_wake_status *wakesta)
{
	int i;
	unsigned int irq_no;


#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_CCCI_DEVICES
	exec_ccci_kern_func_by_md_id(0, ID_DUMP_MD_SLEEP_MODE,
		NULL, 0);
#endif

#ifdef CONFIG_MTK_ECCCI_DRIVER
	if (wakesta->r12 & R12_CLDMA_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & R12_MD2AP_PEER_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & R12_CCIF0_EVENT_B)
		exec_ccci_kern_func_by_md_id(0, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
	if (wakesta->r12 & R12_CCIF1_EVENT_B)
		exec_ccci_kern_func_by_md_id(2, ID_GET_MD_WAKEUP_SRC,
		NULL, 0);
#endif
#endif
	/* Check if pending irq happen and log them */
	for (i = 0; i < IRQ_NUMBER; i++) {
		if (mt6885_spm_wakesrc_irqs[i].name == NULL ||
			!mt6885_spm_wakesrc_irqs[i].irq_no)
			continue;
		if (mt6885_spm_wakesrc_irqs[i].wakesrc & wakesta->r12) {
			irq_no = mt6885_spm_wakesrc_irqs[i].irq_no;
			if (mt_irq_get_pending(irq_no))
				log_irq_wakeup_reason(irq_no);
		}
	}
}

static void mt6885_suspend_spm_rsc_req_check
	(struct mt6885_spm_wake_status *wakesta)
{
#define LOG_BUF_SIZE		        256
#define IS_BLOCKED_OVER_TIMES		10
#undef AVOID_OVERFLOW
#define AVOID_OVERFLOW (0xF0000000)
static u32 is_blocked_cnt;
	char log_buf[LOG_BUF_SIZE] = { 0 };
	int log_size = 0;
	u32 is_no_blocked = 0;
	u32 req_sta_0, req_sta_1, req_sta_4;
	u32 src_req;

	if (is_blocked_cnt >= AVOID_OVERFLOW)
		is_blocked_cnt = 0;

	/* Check if ever enter deepest System LPM */
	is_no_blocked = wakesta->debug_flag & 0x2;

	/* Check if System LPM ever is blocked over 10 times */
	if (!is_no_blocked)
		is_blocked_cnt++;
	else
		is_blocked_cnt = 0;

	if (is_blocked_cnt < IS_BLOCKED_OVER_TIMES)
		return;

	/* Show who is blocking system LPM */
	log_size += scnprintf(log_buf + log_size,
		LOG_BUF_SIZE - log_size,
		"suspend warning:(OneShot) System LPM is blocked by ");

	req_sta_0 = plat_mmio_read(SRC_REQ_STA_0);
	if (req_sta_0 & 0xFFF)
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "md ");

	if (req_sta_0 & (0x3F << 12))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "conn ");

	if (req_sta_0 & (0x7 << 18))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "nfc ");

	if (req_sta_0 & (0xF << 26))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "disp ");

	req_sta_1 = plat_mmio_read(SRC_REQ_STA_1);
	if (req_sta_1 & 0x1F)
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "scp ");

	if (req_sta_1 & (0x1F << 5))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "adsp ");

	if (req_sta_1 & (0x1F << 10))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "ufs ");

	if (req_sta_1 & (0xF << 15))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "gce ");

	if (req_sta_1 & (0x3FF << 21))
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "msdc ");

	req_sta_4 = plat_mmio_read(SRC_REQ_STA_4);
	if (req_sta_4 & 0x1F)
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "apu ");

	src_req = plat_mmio_read(SPM_SRC_REQ);
	if (src_req & 0x9B)
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_SIZE - log_size, "spm ");

	WARN_ON(strlen(log_buf) >= LOG_BUF_SIZE);

	printk_deferred("[name:spm&][SPM] %s", log_buf);
}

static int mt6885_show_message(struct mt6885_spm_wake_status *wakesrc, int type,
					const char *prefix, void *data)
{
#undef LOG_BUF_SIZE
	#define LOG_BUF_SIZE		256
	#define LOG_BUF_OUT_SZ		768
	#define IS_WAKE_MISC(x)	(wakesrc->wake_misc & x)
	#define IS_LOGBUF(ptr, newstr) \
		((strlen(ptr) + strlen(newstr)) < LOG_BUF_SIZE)

	int i;
	unsigned int spm_26M_off_pct = 0;
	char buf[LOG_BUF_SIZE] = { 0 };
	char log_buf[LOG_BUF_OUT_SZ] = { 0 };
	char *local_ptr;
	int log_size = 0;
	unsigned int wr = WR_UNKNOWN;
	const char *scenario = prefix ?: "UNKNOWN";

	/* Disable rcu lock checking */
	if (type == MT_LPM_ISSUER_SUSPEND)
		rcu_irq_enter_irqson();

	if (wakesrc->is_abort != 0) {
		/* add size check for vcoredvfs */
		aee_sram_printk("SPM ABORT (%s), r13 = 0x%x, ",
			scenario, wakesrc->r13);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"[SPM] ABORT (%s), r13 = 0x%x, ",
			scenario, wakesrc->r13);

		aee_sram_printk(" debug_flag = 0x%x 0x%x",
			wakesrc->debug_flag, wakesrc->debug_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" debug_flag = 0x%x 0x%x",
			wakesrc->debug_flag, wakesrc->debug_flag1);

		aee_sram_printk(" sw_flag = 0x%x 0x%x",
			wakesrc->sw_flag0, wakesrc->sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" sw_flag = 0x%x 0x%x\n",
			wakesrc->sw_flag0, wakesrc->sw_flag1);

		aee_sram_printk(" b_sw_flag = 0x%x 0x%x",
			wakesrc->b_sw_flag0, wakesrc->b_sw_flag1);
		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			" b_sw_flag = 0x%x 0x%x",
			wakesrc->b_sw_flag0, wakesrc->b_sw_flag1);

		wr =  WR_ABORT;
	} else {
		if (wakesrc->r12 & R12_PCM_TIMER) {
			if (wakesrc->wake_misc & WAKE_MISC_PCM_TIMER_EVENT) {
				local_ptr = " PCM_TIMER";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PCM_TIMER;
			}
		}

		if (wakesrc->r12 & R12_TWAM_IRQ_B) {
			if (IS_WAKE_MISC(WAKE_MISC_DVFSRC_IRQ)) {
				local_ptr = " DVFSRC";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_DVFSRC;
			}
			if (IS_WAKE_MISC(WAKE_MISC_TWAM_IRQ_B)) {
				local_ptr = " TWAM";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_TWAM;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMSR_IRQ_B_SET0)) {
				local_ptr = " PMSR";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PMSR;
			}
			if (IS_WAKE_MISC(WAKE_MISC_PMSR_IRQ_B_SET1)) {
				local_ptr = " PMSR";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_PMSR;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_0)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_1)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_2)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_3)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
			if (IS_WAKE_MISC(WAKE_MISC_SPM_ACK_CHK_WAKEUP_ALL)) {
				local_ptr = " SPM_ACK_CHK";
				if (IS_LOGBUF(buf, local_ptr))
					strncat(buf, local_ptr,
						strlen(local_ptr));
				wr = WR_SPM_ACK_CHK;
			}
		}
		for (i = 1; i < 32; i++) {
			if (wakesrc->r12 & (1U << i)) {
				if (IS_LOGBUF(buf, mt6885_wakesrc_str[i]))
					strncat(buf, mt6885_wakesrc_str[i],
						strlen(mt6885_wakesrc_str[i]));

				wr = WR_WAKE_SRC;
			}
		}
		WARN_ON(strlen(buf) >= LOG_BUF_SIZE);

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"%s wake up by %s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x 0x%x, %s,%s,%s",
			scenario, buf, wakesrc->timer_out, wakesrc->r13,
			wakesrc->debug_flag, wakesrc->debug_flag1,
			(wakesrc->r13 & CONN_SLEEP_MASK) ? "connectivity not sleep" : "connectivity sleep",
			(wakesrc->r13 & MODEM_SLEEP_MASK) ? "modem not sleep" : "modem sleep",
			(wakesrc->debug_flag & SUBSYS_SLEEP_MASK) == SUBSYS_SLEEP_MASK ? "subsys sleep" : "subsys not sleep");

		log_size += scnprintf(log_buf + log_size,
			LOG_BUF_OUT_SZ - log_size,
			"r12 = 0x%x, r12_ext = 0x%x, raw_sta = 0x%x 0x%x 0x%x, idle_sta = 0x%x, ",
			wakesrc->r12, wakesrc->r12_ext,
			wakesrc->raw_sta,
			wakesrc->md32pcm_wakeup_sta,
			wakesrc->md32pcm_event_sta,
			wakesrc->idle_sta);

		log_size += scnprintf(log_buf + log_size,
			  LOG_BUF_OUT_SZ - log_size,
			  " req_sta =  0x%x 0x%x 0x%x 0x%x 0x%x, cg_check_sta =0x%x, isr = 0x%x, ",
			  wakesrc->req_sta0, wakesrc->req_sta1,
			  wakesrc->req_sta2, wakesrc->req_sta3,
			  wakesrc->req_sta4, wakesrc->cg_check_sta,
			  wakesrc->isr);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"raw_ext_sta = 0x%x, wake_misc = 0x%x, pcm_flag = 0x%x 0x%x 0x%x 0x%x, req = 0x%x, ",
				wakesrc->raw_ext_sta,
				wakesrc->wake_misc,
				wakesrc->sw_flag0,
				wakesrc->sw_flag1, wakesrc->b_sw_flag0,
				wakesrc->b_sw_flag0,
				wakesrc->src_req);

		log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				" clk_settle = 0x%x, ", wakesrc->clk_settle);

		if (type == MT_LPM_ISSUER_SUSPEND) {
			/* calculate 26M off percentage in suspend period */
			if (wakesrc->timer_out != 0) {
				spm_26M_off_pct =
					(100 * plat_mmio_read(SPM_BK_VTCXO_DUR))
							/ wakesrc->timer_out;
			}

			log_size += scnprintf(log_buf + log_size,
				LOG_BUF_OUT_SZ - log_size,
				"wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x, 26M_off_pct = %d\n",
				plat_mmio_read(SYS_TIMER_VALUE_L),
				plat_mmio_read(SYS_TIMER_VALUE_H),
				spm_26M_off_pct);
		}
	}
	WARN_ON(log_size >= LOG_BUF_OUT_SZ);

	if (type == MT_LPM_ISSUER_SUSPEND) {
		printk_deferred("[name:spm&][SPM] %s", log_buf);
		mt6885_suspend_show_detailed_wakeup_reason(wakesrc);
		mt6885_suspend_spm_rsc_req_check(wakesrc);

		printk_deferred("[name:spm&][SPM] Suspended for %d.%03d seconds",
			PCM_TICK_TO_SEC(wakesrc->timer_out),
			PCM_TICK_TO_SEC((wakesrc->timer_out %
				PCM_32K_TICKS_PER_SEC)
			* 1000));

		/* Eable rcu lock checking */
		rcu_irq_exit_irqson();
	} else
		pr_info("[name:spm&][SPM] %s", log_buf);

#ifdef CONFIG_LPM_STATE_RECORDS
	if (type == MT_LPM_ISSUER_SUSPEND) {
		lsr_record(wakesrc, spm_26M_off_pct);
		lwr_record(buf);
	}
#endif

	return wr;
}

int mt6885_issuer_func(int type, const char *prefix, void *data)
{
	mt6885_get_wakeup_status(&mt6885_logger_help);
	return mt6885_show_message(mt6885_logger_help.wakesrc,
					type, prefix, data);
}

struct mtk_lpm_issuer mt6885_issuer = {
	.log = mt6885_issuer_func,
};

static int mt6885_idle_save_sleep_info_nb_func(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct mtk_lpm_nb_data *nb_data = (struct mtk_lpm_nb_data *)data;

	if (nb_data && (action == MTK_LPM_NB_BEFORE_REFLECT))
		mt6885_save_sleep_info();

	return NOTIFY_OK;
}

struct notifier_block mt6885_idle_save_sleep_info_nb = {
	.notifier_call = mt6885_idle_save_sleep_info_nb_func,
};

static void mt6885_suspend_save_sleep_info_func(void)
{
	mt6885_save_sleep_info();
}

static struct syscore_ops mt6885_suspend_save_sleep_info_syscore_ops = {
	.resume = mt6885_suspend_save_sleep_info_func,
};

static int mt6885_log_timer_func(unsigned long long dur, void *priv)
{
	struct mt6885_logger_timer *timer =
			(struct mt6885_logger_timer *)priv;
	struct mt6885_logger_fired_info *info = &mt6885_logger_fired;

	if (timer->fired != info->fired) {
		/* if the wake src had beed update before
		 * then won't do wake src update
		 */
		if (mt6885_logger_help.prev == mt6885_logger_help.cur)
			mt6885_get_wakeup_status(&mt6885_logger_help);
		mt6885_show_message(mt6885_logger_help.wakesrc,
					MT_LPM_ISSUER_CPUIDLE,
					"MCUSYSOFF", NULL);
		mt6885_logger_help.prev = mt6885_logger_help.cur;
	} else
		pr_info("[name:spm&][SPM] MCUSYSOFF Didn't enter low power scenario\n");

	timer->fired = info->fired;
	return 0;
}

static int mt6885_logger_nb_func(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct mtk_lpm_nb_data *nb_data = (struct mtk_lpm_nb_data *)data;
	struct mt6885_logger_fired_info *info = &mt6885_logger_fired;

	if (nb_data && (action == MTK_LPM_NB_BEFORE_REFLECT)
	    && (nb_data->index == info->state_index))
		info->fired++;

	return NOTIFY_OK;
}

struct notifier_block mt6885_logger_nb = {
	.notifier_call = mt6885_logger_nb_func,
};

static ssize_t mt6885_logger_debugfs_read(char *ToUserBuf,
					size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int len;

	if (priv == ((void *)&mt6885_log_timer)) {
		len = scnprintf(p, sz, "%lu\n",
			mtk_lpm_timer_interval(&mt6885_log_timer.tm));
		p += len;
		sz -= len;
	}

	return (p - ToUserBuf);
}

static ssize_t mt6885_logger_debugfs_write(char *FromUserBuf,
				   size_t sz, void *priv)
{
	if (priv == ((void *)&mt6885_log_timer)) {
		unsigned int val = 0;

		if (!kstrtouint(FromUserBuf, 10, &val)) {
			if (val == 0)
				mtk_lpm_timer_stop(&mt6885_log_timer.tm);
			else
				mtk_lpm_timer_interval_update(
						&mt6885_log_timer.tm, val);
		}
	}
	return sz;
}

struct MT6886_LOGGER_NODE {
	struct mtk_lp_sysfs_handle handle;
	struct mtk_lp_sysfs_op op;
};
#define MT6885_LOGGER_NODE_INIT(_n, _priv) ({\
	_n.op.fs_read = mt6885_logger_debugfs_read;\
	_n.op.fs_write = mt6885_logger_debugfs_write;\
	_n.op.priv = _priv; })\


struct mtk_lp_sysfs_handle mt6885_log_tm_node;
struct MT6886_LOGGER_NODE mt6885_log_tm_interval;

int mt6885_logger_timer_debugfs_init(void)
{
	mtk_lpm_sysfs_sub_entry_add("logger", 0644,
				NULL, &mt6885_log_tm_node);

	MT6885_LOGGER_NODE_INIT(mt6885_log_tm_interval,
				&mt6885_log_timer);
	mtk_lpm_sysfs_sub_entry_node_add("interval", 0644,
				&mt6885_log_tm_interval.op,
				&mt6885_log_tm_node,
				&mt6885_log_tm_interval.handle);
	return 0;
}

int __init mt6885_logger_init(void)
{
	struct device_node *node = NULL;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node) {
		mt6885_spm_base = of_iomap(node, 0);
		of_node_put(node);
	}

	if (mt6885_spm_base)
		mtk_lp_issuer_register(&mt6885_issuer);
	else
		pr_info("[name:mtk_lpm][P] - Don't register the issue by error! (%s:%d)\n",
			__func__, __LINE__);

	mt6885_get_spm_wakesrc_irq();
	mtk_lpm_sysfs_root_entry_create();
	mt6885_logger_timer_debugfs_init();

	dev = cpuidle_get_device();
	drv = cpuidle_get_cpu_driver(dev);
	mt6885_logger_fired.state_index = -1;

	if (drv) {
		int idx;

		for (idx = 0; idx < drv->state_count; ++idx) {
			if (!strcmp(MT6885_LOG_MONITOR_STATE_NAME,
					drv->states[idx].name)) {
				mt6885_logger_fired.state_index = idx;
				break;
			}
		}
	}

	mtk_lpm_notifier_register(&mt6885_logger_nb);
	mtk_lpm_notifier_register(&mt6885_idle_save_sleep_info_nb);

	mt6885_log_timer.tm.timeout = mt6885_log_timer_func;
	mt6885_log_timer.tm.priv = &mt6885_log_timer;
	mtk_lpm_timer_init(&mt6885_log_timer.tm, MTK_LPM_TIMER_REPEAT);
	mtk_lpm_timer_interval_update(&mt6885_log_timer.tm,
					MT6885_LOG_DEFAULT_MS);
	mtk_lpm_timer_start(&mt6885_log_timer.tm);

	register_syscore_ops(&mt6885_suspend_save_sleep_info_syscore_ops);

	return 0;
}
#ifdef CONFIG_LPM_STATE_RECORDS
static void lsr_record(const struct mt6885_spm_wake_status *wakesta, unsigned int spm_26M_off_pct)
{
	static int m;
	int conn_sleep = 0;
	int modem_sleep = 0;
	int subsys_sleep = 0;

	if (!wakesta)
		return;

	if (!spin_trylock(&lsr_lock))
		return;

	if (wakesta->timer_out > MAX_LPM_SHORT_TIME) {
		conn_sleep = (wakesta->r13 & CONN_SLEEP_MASK) ? 0 : 1;
		modem_sleep = (wakesta->r13 & MODEM_SLEEP_MASK) ? 0 : 1;
		subsys_sleep = (wakesta->debug_flag & SUBSYS_SLEEP_MASK) == SUBSYS_SLEEP_MASK ? 1 : 0;

		if (!conn_sleep || !modem_sleep || !subsys_sleep) {
			index_tail = m;
			getnstimeofday(&lsr_buff[m].key_time);
			sprintf(lsr_buff[m++].msg, "%u, %s, %s, %s, req_sta = 0x%x 0x%x 0x%x 0x%x 0x%x, %u", wakesta->timer_out,
					conn_sleep ? "connectivity sleep" : "connectivity not sleep",
					modem_sleep ? "modem sleep" : "modem not sleep",
					subsys_sleep ? "subsys sleep" : "subsys not sleep",
					wakesta->req_sta0, wakesta->req_sta1, wakesta->req_sta2,
					wakesta->req_sta3, wakesta->req_sta4, spm_26M_off_pct);

			if (m >= MAX_LSR_RECORDS)
				m = 0;

			lsr_num++;
			if (lsr_num >= MAX_LSR_RECORDS) {
				lsr_num = MAX_LSR_RECORDS;
				index_head = index_tail + 1;
				if (index_head >= MAX_LSR_RECORDS)
					index_head = 0;
			}
		}
	}

	spin_unlock(&lsr_lock);
}

static int lsr_seq_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&lsr_lock);
	if (lsr_num < MAX_LSR_RECORDS) {
		for (i = 0; i < lsr_num; i++) {
			rtc_time_to_tm(lsr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_state { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lsr_buff[i].msg);
		}
	} else {
		for (i = index_head; i < MAX_LSR_RECORDS; i++) {
			rtc_time_to_tm(lsr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_state { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lsr_buff[i].msg);
		}

		if (index_head > index_tail) {
			for (i = 0; i <= index_tail; i++) {
				rtc_time_to_tm(lsr_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_state { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						lsr_buff[i].msg);
			}
		}
	}
	spin_unlock(&lsr_lock);

	return 0;
}

static int lsr_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, lsr_seq_show, NULL);
}

static const struct file_operations lpm_state_records_ops = {
	.open           = lsr_record_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static void lwr_record(char *buf)
{
	static int m;

	if (!buf || (strlen(buf) > LOG_BUF_SIZE))
		return;

	if (!spin_trylock(&lwr_lock))
		return;

	lwr_index_tail = m;
	getnstimeofday(&lwr_buff[m].key_time);
	sprintf(lwr_buff[m++].msg, "%s", buf);

	if (m >= MAX_LWR_RECORDS)
		m = 0;

	lwr_num++;
	if (lwr_num >= MAX_LWR_RECORDS) {
		lwr_num = MAX_LWR_RECORDS;
		lwr_index_head = lwr_index_tail + 1;
		if (lwr_index_head >= MAX_LWR_RECORDS)
			lwr_index_head = 0;
	}

	spin_unlock(&lwr_lock);
}

static int lwr_seq_show(struct seq_file *seq, void *v)
{
	struct rtc_time tm;
	int i = 0;

	spin_lock(&lwr_lock);
	if (lwr_num < MAX_LWR_RECORDS) {
		for (i = 0; i < lwr_num; i++) {
			rtc_time_to_tm(lwr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_wakeup { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lwr_buff[i].msg);
		}
	} else {
		for (i = lwr_index_head; i < MAX_LWR_RECORDS; i++) {
			rtc_time_to_tm(lwr_buff[i].key_time.tv_sec, &tm);
			seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_wakeup { %s }\n",
					tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
					lwr_buff[i].msg);
		}

		if (lwr_index_head > lwr_index_tail) {
			for (i = 0; i <= lwr_index_tail; i++) {
				rtc_time_to_tm(lwr_buff[i].key_time.tv_sec, &tm);
				seq_printf(seq, "%d-%02d-%02d %02d:%02d:%02d UTC - lpm_wakeup { %s }\n",
						tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
						lwr_buff[i].msg);
			}
		}
	}
	spin_unlock(&lwr_lock);

	return 0;
}

static int lwr_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, lwr_seq_show, NULL);
}

static const struct file_operations lpm_wakeup_records_ops = {
	.open           = lwr_record_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

static int __init lpm_state_records(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *wp_entry;

	entry = proc_create("lpm_state_records", 0444, NULL, &lpm_state_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create proc lpm_state_records node failed\n", __func__);

	wp_entry = proc_create("lpm_wakeup_records", 0444, NULL, &lpm_wakeup_records_ops);
	if (!entry)
		printk(KERN_ERR "%s: create proc node lpm_wakeup_records failed\n", __func__);

	return 0;
}

late_initcall(lpm_state_records);
#endif

late_initcall_sync(mt6885_logger_init);

