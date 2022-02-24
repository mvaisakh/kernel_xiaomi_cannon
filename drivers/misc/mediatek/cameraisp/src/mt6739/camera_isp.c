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

/********************************************************************
 * camera_isp.c - MT6739 Linux ISP Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers ISP relative functions
 *
 **********************************************************************/
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
/* #include     <asm/io.h> */
/* #include     <asm/tcm.h>     */
#include <linux/proc_fs.h>      /* proc file use */
/*      */
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include     <linux/io.h> */
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
/* #include     <mach/mt6593_pll.h>     */
#include "inc/camera_isp.h"
/*#include <mach/irqs.h>*/
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/*#include <mach/mt_clkmgr.h>*/
#include <mt-plat/sync_write.h> /* For mt65xx_reg_sync_writel(). */
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <m4u.h>
/* #define EP_NO_CLKMGR */
#include <cmdq_core.h>

#include <smi_public.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*      */
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif

/* #define ISP_DEBUG */

#define LOG_CONSTRAINT_ADJ              (1)
#if (LOG_CONSTRAINT_ADJ == 1)
/* for kernel log reduction */
#include <linux/printk.h>
#endif
#include <aee.h>

#define CAMSV_DBG
#ifdef CAMSV_DBG
#define CAM_TAG "CAM:"
#define CAMSV_TAG "SV1:"
#define CAMSV2_TAG "SV2:"
#else
#define CAMSV_TAG ""
#define CAMSV2_TAG ""
#define CAM_TAG ""
#endif

#ifndef MTRUE
#define MTRUE                           1
#endif
#ifndef MFALSE
#define MFALSE                          0
#endif
/* --------------------------------------------------------------- */

#define MyTag "[ISP]"
#define IRQTag "KEEPER"


#ifdef ISP_DEBUG
#define LOG_DBG(format, args...)    pr_debug(MyTag format, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...)         pr_info(MyTag format,  ##args)
#define LOG_NOTICE(format, args...)      pr_notice(MyTag format,  ##args)
#define LOG_PR_WARN(format, args...)     pr_warn(MyTag format,  ##args)
#define LOG_PR_ERR(format, args...)      pr_err(MyTag format,  ##args)
#define LOG_PR_ALERT(format, args...)    pr_alert(MyTag format, ##args)

/* --------------------------------------------------------------- */
#define camera_isp_aee(key, string) \
	do { \
		LOG_PR_ERR(string); \
		aee_kernel_exception("Camera-ISP", "\nCRDISPATCH_KEY:" key "\n" string); \
	} while (0)

/*******************************************************************
*
********************************************************************/
/* For other projects.*/
/* #define ISP_WR32(addr, data) iowrite32(data, addr)*/
/* For 89 Only.   NEED_TUNING_BY_PROJECT */
#define ISP_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define ISP_RD32(addr)                  ioread32((void *)addr)
#define ISP_SET_BIT(reg, bit)\
	((*(unsigned int *)(reg)) |= (unsigned int)(1 << (bit)))
#define ISP_CLR_BIT(reg, bit)\
	((*(unsigned int *)(reg)) &= ~((unsigned int)(1 << (bit))))
/********************************************************************
*
********************************************************************/
#define ISP_DEV_NAME                            "camera-isp"

/* ///////////////////////////////////////////////////////////////// */
/* for restricting range in     mmap function */
/* isp driver */
#define ISP_RTBUF_REG_RANGE      0x10000
#define IMGSYS_BASE_ADDR         0x15000000
/* 0x10000,the same with the value in isp_reg.h and page-aligned */
#define ISP_REG_RANGE            (0x10000)
/* seninf driver */
/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define SENINF_BASE_ADDR         0x15008000
/* 0x4000,the same with the value in seninf_reg.h and page-aligned */
#define SENINF_REG_RANGE        (0x4000)
 /* the same with the value in seninf_drv.cpp(chip-dependent) */
#define PLL_BASE_ADDR            0x10000000
/* 0x200,the same with the value in seninf_drv.cpp and page-aligned */
#define PLL_RANGE               (0x1000)
/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define MIPIRX_CONFIG_ADDR       0x1500C000
 /* 0x100,the same with the value in seninf_drv.cpp and page-aligned */
#define MIPIRX_CONFIG_RANGE     (0x1000)
 /* the same with the value in seninf_drv.cpp(chip-dependent) */
#define MIPIRX_ANALOG_ADDR       0x11c10000
#define MIPIRX_ANALOG_RANGE     (0x20000)
/* the same with the value in seninf_drv.cpp(chip-dependent) */
#define GPIO_BASE_ADDR           0x10002000
#define GPIO_RANGE               (0x1000)
/* security     concern */
#define ISP_RANGE                 (0x10000)
/* ///////////////////////////////////////////////////////////////// */

/*******************************************************************
*
********************************************************************/
#define ISP_DBG_INT                             (0x00000001)
#define ISP_DBG_HOLD_REG                        (0x00000002)
#define ISP_DBG_READ_REG                        (0x00000004)
#define ISP_DBG_WRITE_REG                       (0x00000008)
#define ISP_DBG_CLK                             (0x00000010)
#define ISP_DBG_TASKLET                         (0x00000020)
#define ISP_DBG_SCHEDULE_WORK			(0x00000040)
#define ISP_DBG_BUF_WRITE                       (0x00000080)
#define ISP_DBG_BUF_CTRL                        (0x00000100)
#define ISP_DBG_REF_CNT_CTRL			(0x00000200)
#define ISP_DBG_INT_2                           (0x00000400)
#define ISP_DBG_INT_3                           (0x00000800)

/*******************************************************************
*
********************************************************************/

#ifdef CONFIG_OF

enum ISP_CAM_IRQ_ENUM {
	ISP_CAM0_IRQ_IDX = 0,
	ISP_CAM1_IRQ_IDX,
	ISP_CAM2_IRQ_IDX,
	ISP_CAMSV0_IRQ_IDX,
	ISP_CAMSV1_IRQ_IDX,
	ISP_CAM_IRQ_IDX_NUM
};

enum ISP_CAM_BASEADDR_ENUM {
	ISP_BASE_ADDR = 0,
	ISP_INNER_BASE_ADDR,
	ISP_IMGSYS_CONFIG_BASE_ADDR,
	ISP_MIPI_ANA_BASE_ADDR,
	ISP_GPIO_BASE_ADDR,
	ISP_CAM_BASEADDR_NUM
};

#if (!defined CONFIG_MTK_CLKMGR) && (!defined EP_NO_CLKMGR) /*CCF*/
#include <linux/clk.h>
struct ISP_CLK_STRUCT {
	struct clk *CG_SCP_SYS_MM0;
	struct clk *CG_SCP_SYS_ISP;
	struct clk *CG_IMG_CAM_SMI;
	struct clk *CG_IMG_CAM_CAM;
	struct clk *CG_IMG_SEN_TG;
	struct clk *CG_IMG_SEN_CAM;
	struct clk *CG_IMG_CAM_SV;
};
struct ISP_CLK_STRUCT isp_clk;
#endif

static unsigned long gISPSYS_Irq[ISP_CAM_IRQ_IDX_NUM];
static unsigned long gISPSYS_Reg[ISP_CAM_BASEADDR_NUM];

#ifdef CONFIG_PM_SLEEP
struct wakeup_source isp_wake_lock;
#endif

static int g_bWaitLock;

/*static void __iomem *g_isp_base_dase;*/
/*static void __iomem *g_isp_inner_base_dase;*/
/*static void __iomem *g_imgsys_config_base_dase;*/

#if (LOG_CONSTRAINT_ADJ == 1)
static unsigned int g_log_def_constraint;
#endif


#define ISP_ADDR            (gISPSYS_Reg[ISP_BASE_ADDR])
#define ISP_IMGSYS_BASE     (gISPSYS_Reg[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define ISP_ADDR_CAMINF     (gISPSYS_Reg[ISP_IMGSYS_CONFIG_BASE_ADDR])
#define ISP_MIPI_ANA_ADDR   (gISPSYS_Reg[ISP_MIPI_ANA_BASE_ADDR])
#define ISP_GPIO_ADDR       (gISPSYS_Reg[ISP_GPIO_BASE_ADDR])
#define ISP_IMGSYS_BASE_PHY  0x15000000
#else
#define ISP_ADDR            (IMGSYS_BASE + 0x4000)
#define ISP_IMGSYS_BASE     IMGSYS_BASE
#define ISP_ADDR_CAMINF     IMGSYS_BASE
#define ISP_MIPI_ANA_ADDR   0x10215000
#define ISP_GPIO_ADDR       GPIO_BASE

#endif


#define ISP_REG_ADDR_EN1                        (ISP_ADDR +     0x4)
#define ISP_REG_ADDR_INT_P1_ST                  (ISP_ADDR +     0x4C)
#define ISP_REG_ADDR_INT_P1_ST2                 (ISP_ADDR +     0x54)
#define ISP_REG_ADDR_INT_P1_ST_D                (ISP_ADDR +     0x5C)
#define ISP_REG_ADDR_INT_P1_ST2_D               (ISP_ADDR +     0x64)
#define ISP_REG_ADDR_INT_P2_ST                  (ISP_ADDR +     0x6C)
#define ISP_REG_ADDR_INT_STATUSX                (ISP_ADDR +     0x70)
#define ISP_REG_ADDR_INT_STATUS2X               (ISP_ADDR +     0x74)
#define ISP_REG_ADDR_INT_STATUS3X               (ISP_ADDR +     0x78)
#define ISP_REG_ADDR_CAM_SW_CTL                 (ISP_ADDR +     0x8C)
#define ISP_REG_ADDR_IMGO_FBC                   (ISP_ADDR +     0xF0)
#define ISP_REG_ADDR_RRZO_FBC                   (ISP_ADDR +     0xF4)
#define ISP_REG_ADDR_IMGO_D_FBC                 (ISP_ADDR +     0xF8)
#define ISP_REG_ADDR_RRZO_D_FBC                 (ISP_ADDR +     0xFC)
#define ISP_REG_ADDR_TG_VF_CON                  (ISP_ADDR +     0x414)
#define ISP_REG_ADDR_TG_INTER_ST                (ISP_ADDR +     0x44C)
#define ISP_REG_ADDR_TG2_VF_CON                 (ISP_ADDR +     0x2414)
#define ISP_REG_ADDR_TG2_INTER_ST               (ISP_ADDR +     0x244C)
#define ISP_REG_ADDR_IMGO_BASE_ADDR             (ISP_ADDR +     0x3300)
#define ISP_REG_ADDR_RRZO_BASE_ADDR             (ISP_ADDR +     0x3320)
#if (ISP_RAW_D_SUPPORT == 1)
#define ISP_REG_ADDR_IMGO_D_BASE_ADDR           (ISP_ADDR +     0x34D4)
#define ISP_REG_ADDR_RRZO_D_BASE_ADDR           (ISP_ADDR +     0x34F4)
#endif
#define ISP_REG_ADDR_SENINF1_INT                (ISP_ADDR +     0x4128)
#define ISP_REG_ADDR_SENINF2_INT                (ISP_ADDR +     0x4528)
#define ISP_REG_ADDR_SENINF3_INT                (ISP_ADDR +     0x4928)
#define ISP_REG_ADDR_SENINF4_INT                (ISP_ADDR +     0x4D28)
#define ISP_REG_ADDR_CAMSV_FMT_SEL              (ISP_ADDR +     0x5004)
#define ISP_REG_ADDR_CAMSV_INT                  (ISP_ADDR +     0x500C)
#define ISP_REG_ADDR_CAMSV_SW_CTL               (ISP_ADDR +     0x5010)
#define ISP_REG_ADDR_CAMSV_TG_INTER_ST          (ISP_ADDR +     0x544C)
#define ISP_REG_ADDR_CAMSV2_FMT_SEL             (ISP_ADDR +     0x5804)
#define ISP_REG_ADDR_CAMSV2_INT                 (ISP_ADDR +     0x580C)
#define ISP_REG_ADDR_CAMSV2_SW_CTL              (ISP_ADDR +     0x5810)
#define ISP_REG_ADDR_CAMSV_TG2_INTER_ST         (ISP_ADDR +     0x5C4C)
#define ISP_REG_ADDR_CAMSV_IMGO_FBC             (ISP_ADDR +     0x501C)
#define ISP_REG_ADDR_CAMSV2_IMGO_FBC            (ISP_ADDR +     0x581C)
#define ISP_REG_ADDR_IMGO_SV_BASE_ADDR          (ISP_ADDR +     0x5208)
#define ISP_REG_ADDR_IMGO_SV_XSIZE              (ISP_ADDR +     0x5210)
#define ISP_REG_ADDR_IMGO_SV_YSIZE              (ISP_ADDR +     0x5214)
#define ISP_REG_ADDR_IMGO_SV_STRIDE             (ISP_ADDR +     0x5218)
#define ISP_REG_ADDR_IMGO_SV_CROP               (ISP_ADDR +     0x5224)
#define ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR        (ISP_ADDR +     0x5228)
#define ISP_REG_ADDR_IMGO_SV_D_XSIZE            (ISP_ADDR +     0x5230)
#define ISP_REG_ADDR_IMGO_SV_D_YSIZE            (ISP_ADDR +     0x5234)
#define ISP_REG_ADDR_IMGO_SV_D_STRIDE           (ISP_ADDR +     0x5238)
#define ISP_REG_ADDR_IMGO_SV_D_CROP             (ISP_ADDR +     0x5244)
#define TG_REG_ADDR_GRAB_W                      (ISP_ADDR +     0x418)
#define TG2_REG_ADDR_GRAB_W                     (ISP_ADDR +     0x2418)
#define TG_REG_ADDR_GRAB_H                      (ISP_ADDR +     0x41C)
#define TG2_REG_ADDR_GRAB_H                     (ISP_ADDR +     0x241C)

#define ISP_REG_ADDR_CAMSV_TG_VF_CON            (ISP_ADDR +     0x5414)
#define ISP_REG_ADDR_CAMSV_TG2_VF_CON           (ISP_ADDR +     0x5C14)
#define CAMSV_TG_REG_ADDR_GRAB_W                (ISP_ADDR +     0x5418)
#define CAMSV_TG2_REG_ADDR_GRAB_W               (ISP_ADDR +     0x5C18)
#define CAMSV_TG_REG_ADDR_GRAB_H                (ISP_ADDR +     0x541C)
#define CAMSV_TG2_REG_ADDR_GRAB_H               (ISP_ADDR +     0x5C1C)

/* spare register */
/* #define ISP_REG_ADDR_TG_MAGIC_0                 (ISP_ADDR + 0x94) */
/* #define ISP_REG_ADDR_TG_MAGIC_1                 (ISP_ADDR + 0x9C) */
/* New define by 20131114 */
#define ISP_REG_ADDR_TG_MAGIC_0	    (ISP_IMGSYS_BASE + 0x75DC)/* 0088 */

#define ISP_REG_ADDR_TG2_MAGIC_0    (ISP_IMGSYS_BASE + 0x75E4)/* 0090 */

/* for rrz input crop size */
#define ISP_REG_ADDR_TG_RRZ_CROP_IN	(ISP_IMGSYS_BASE + 0x75E0)
#define ISP_REG_ADDR_TG_RRZ_CROP_IN_D    (ISP_IMGSYS_BASE + 0x75E8)

/* MT6739 top registers */
#define IMGSYS_REG_CG_SET        (ISP_IMGSYS_BASE + 0x4)
#define IMGSYS_REG_CG_CLR        (ISP_IMGSYS_BASE + 0x8)

/* for rrz destination width  */
/* (in twin mode,     ISP_INNER_REG_ADDR_RRZO_XSIZE < RRZO width)*/
#define ISP_REG_ADDR_RRZ_W          (ISP_ADDR_CAMINF     + 0x4094)
#define ISP_REG_ADDR_RRZ_W_D        (ISP_ADDR_CAMINF     + 0x409C)

/*CAM_REG_CTL_SPARE1             CAM_CTL_SPARE1;	//4094*/
/*CAM_REG_CTL_SPARE2             CAM_CTL_SPARE2;  //409C*/
/*CAM_REG_CTL_SPARE3             CAM_CTL_SPARE3;	//4100*/
/*CAM_REG_AE_SPARE               CAM_AE_SPARE;	//4694*/
/*CAM_REG_DM_O_SPARE             CAM_DM_O_SPARE;	//48F0*/
/*CAM_REG_MIX1_SPARE             CAM_MIX1_SPARE;	//4C98*/
/*CAM_REG_MIX2_SPARE             CAM_MIX2_SPARE;	//4CA8*/
/*CAM_REG_MIX3_SPARE             CAM_MIX3_SPARE;	//4CB8*/
/*CAM_REG_NR3D_SPARE0            CAM_NR3D_SPARE0;	//4D04*/
/*CAM_REG_AWB_D_SPARE            CAM_AWB_D_SPARE;	//663C*/
/*CAM_REG_AE_D_SPARE             CAM_AE_D_SPARE;	//6694*/
/*CAMSV_REG_CAMSV_SPARE0         CAMSV_CAMSV_SPARE0;	//9014*/
/*CAMSV_REG_CAMSV_SPARE1         CAMSV_CAMSV_SPARE1;	//9018*/
/*CAMSV_REG_CAMSV2_SPARE0	       CAMSV_CAMSV2_SPARE0;	//9814*/
/*CAMSV_REG_CAMSV2_SPARE1        CAMSV_CAMSV2_SPARE1;	//9818*/


/* inner register */
/* 1500_d000 ==> 1500_4000 */
/* 1500_e000 ==> 1500_6000 */
/* 1500_f000 ==> 1500_7000 */
#define ISP_INNER_REG_ADDR_FMT_SEL_P1           (ISP_ADDR +     0x0028)
#define ISP_INNER_REG_ADDR_FMT_SEL_P1_D         (ISP_ADDR +     0x002C)
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1    (ISP_ADDR_CAMINF + 0xD028)*/
/* #define ISP_INNER_REG_ADDR_FMT_SEL_P1_D  (ISP_ADDR_CAMINF + 0xD02C)*/
#define ISP_INNER_REG_ADDR_CAM_CTRL_SEL_P1      (ISP_ADDR + 0x0034)
#define ISP_INNER_REG_ADDR_CAM_CTRL_SEL_P1_D    (ISP_ADDR + 0x0038)
#define ISP_INNER_REG_ADDR_IMGO_XSIZE           (ISP_ADDR_CAMINF + 0xF308)
#define ISP_INNER_REG_ADDR_IMGO_YSIZE           (ISP_ADDR_CAMINF + 0xF30C)
#define ISP_INNER_REG_ADDR_IMGO_STRIDE          (ISP_ADDR_CAMINF + 0xF310)
#define ISP_INNER_REG_ADDR_IMGO_CROP            (ISP_ADDR_CAMINF + 0xF31C)
#define ISP_INNER_REG_ADDR_RRZO_XSIZE           (ISP_ADDR_CAMINF + 0xF328)
#define ISP_INNER_REG_ADDR_RRZO_YSIZE           (ISP_ADDR_CAMINF + 0xF32C)
#define ISP_INNER_REG_ADDR_RRZO_STRIDE          (ISP_ADDR_CAMINF + 0xF330)
#define ISP_INNER_REG_ADDR_RRZO_CROP            (ISP_ADDR_CAMINF + 0xF33C)
#if (ISP_RAW_D_SUPPORT == 1)
#define ISP_INNER_REG_ADDR_IMGO_D_XSIZE         (ISP_ADDR_CAMINF + 0xF4DC)
#define ISP_INNER_REG_ADDR_IMGO_D_YSIZE         (ISP_ADDR_CAMINF + 0xF4E0)
#define ISP_INNER_REG_ADDR_IMGO_D_STRIDE        (ISP_ADDR_CAMINF + 0xF4E4)
#define ISP_INNER_REG_ADDR_IMGO_D_CROP          (ISP_ADDR_CAMINF + 0xF4F0)
#define ISP_INNER_REG_ADDR_RRZO_D_XSIZE         (ISP_ADDR_CAMINF + 0xF4FC)
#define ISP_INNER_REG_ADDR_RRZO_D_YSIZE         (ISP_ADDR_CAMINF + 0xF500)
#define ISP_INNER_REG_ADDR_RRZO_D_STRIDE        (ISP_ADDR_CAMINF + 0xF504)
#define ISP_INNER_REG_ADDR_RRZO_D_CROP          (ISP_ADDR_CAMINF + 0xF510)
#endif
#define ISP_INNER_REG_ADDR_RRZ_HORI_INT_OFST (ISP_ADDR_CAMINF + 0xD7B4)
#define ISP_INNER_REG_ADDR_RRZ_VERT_INT_OFST (ISP_ADDR_CAMINF + 0xD7BC)
#define ISP_INNER_REG_ADDR_RRZ_IN_IMG           (ISP_ADDR_CAMINF + 0xD7A4)
#define ISP_INNER_REG_ADDR_RRZ_OUT_IMG          (ISP_ADDR_CAMINF + 0xD7A8)

#define ISP_INNER_REG_ADDR_RRZ_D_HORI_INT_OFST (ISP_ADDR_CAMINF + 0xE7B4)
#define ISP_INNER_REG_ADDR_RRZ_D_VERT_INT_OFST (ISP_ADDR_CAMINF + 0xE7BC)
#define ISP_INNER_REG_ADDR_RRZ_D_IN_IMG         (ISP_ADDR_CAMINF + 0xE7A4)
#define ISP_INNER_REG_ADDR_RRZ_D_OUT_IMG        (ISP_ADDR_CAMINF + 0xE7A8)


/* camsv hw     no inner address to     read */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_XSIZE        (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_YSIZE      (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_STRIDE (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_D_XSIZE    (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_D_YSIZE    (0) */
/* #define ISP_INNER_REG_ADDR_IMGO_SV_D_STRIDE   (0) */
/* #define ISP_INNER_REG_ADDR_CAMSV_FMT_SEL      (0) */
/* #define ISP_INNER_REG_ADDR_CAMSV2_FMT_SEL (0) */

#define ISP_TPIPE_ADDR                  (0x15004000)

/* CAM_CTL_SW_CTL */
#define ISP_REG_SW_CTL_SW_RST_P1_MASK   (0x00000007)
#define ISP_REG_SW_CTL_SW_RST_TRIG      (0x00000001)
#define ISP_REG_SW_CTL_SW_RST_STATUS    (0x00000002)
#define ISP_REG_SW_CTL_HW_RST           (0x00000004)
#define ISP_REG_SW_CTL_SW_RST_P2_MASK   (0x00000070)
#define ISP_REG_SW_CTL_SW_RST_P2_TRIG   (0x00000010)
#define ISP_REG_SW_CTL_SW_RST_P2_STATUS (0x00000020)
#define ISP_REG_SW_CTL_HW_RST_P2        (0x00000040)
/* CAMSV_SW_CTL */
#define ISP_REG_CAMSV_SW_CTL_IMGO_RST_TRIG   (0x00000001)
#define ISP_REG_CAMSV_SW_CTL_IMGO_RST_ST     (0x00000002)
#define ISP_REG_CAMSV_SW_CTL_SW_RST          (0x00000004)

#define ISP_REG_SW_CTL_RST_CAM_P1       (1)
#define ISP_REG_SW_CTL_RST_CAM_P2       (2)
#define ISP_REG_SW_CTL_RST_CAMSV        (3)
#define ISP_REG_SW_CTL_RST_CAMSV2       (4)

/* CAM_CTL_INT_P1_STATUS */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST     (ISP_IRQ_P1_STATUS_VS1_INT_ST | \
				    ISP_IRQ_P1_STATUS_TG1_INT1_ST | \
				    ISP_IRQ_P1_STATUS_TG1_INT2_ST | \
				    ISP_IRQ_P1_STATUS_EXPDON1_ST | \
				    ISP_IRQ_P1_STATUS_PASS1_DON_ST | \
				    ISP_IRQ_P1_STATUS_SOF1_INT_ST | \
				    ISP_IRQ_P1_STATUS_AF_DON_ST | \
				    ISP_IRQ_P1_STATUS_FLK_DON_ST | \
				    ISP_IRQ_P1_STATUS_FBC_RRZO_DON_ST | \
				    ISP_IRQ_P1_STATUS_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST_ERR  (ISP_IRQ_P1_STATUS_TG1_ERR_ST | \
				 ISP_IRQ_P1_STATUS_TG1_GBERR | \
				     ISP_IRQ_P1_STATUS_CQ0_ERR | \
				     ISP_IRQ_P1_STATUS_CQ0_VS_ERR_ST | \
				ISP_IRQ_P1_STATUS_IMGO_DROP_FRAME_ST | \
				ISP_IRQ_P1_STATUS_RRZO_DROP_FRAME_ST | \
				     ISP_IRQ_P1_STATUS_IMGO_ERR_ST | \
				     ISP_IRQ_P1_STATUS_AAO_ERR_ST | \
				     ISP_IRQ_P1_STATUS_LCSO_ERR_ST | \
				     ISP_IRQ_P1_STATUS_RRZO_ERR_ST | \
				     ISP_IRQ_P1_STATUS_ESFKO_ERR_ST | \
				     ISP_IRQ_P1_STATUS_FLK_ERR_ST | \
				     ISP_IRQ_P1_STATUS_LSC_ERR_ST | \
				     ISP_IRQ_P1_STATUS_DMA_ERR_ST)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_P1_ST_WAITQ    (ISP_IRQ_P1_STATUS_VS1_INT_ST | \
					 ISP_IRQ_P1_STATUS_PASS1_DON_ST | \
					 ISP_IRQ_P1_STATUS_SOF1_INT_ST|\
					 ISP_IRQ_P1_STATUS_AF_DON_ST)

/* CAM_CTL_INT_P1_STATUS2 */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST2     (ISP_IRQ_P1_STATUS2_IMGO_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_UFEO_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_RRZO_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_ESFKO_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_LCSO_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_AAO_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_BPCI_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_LSCI_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_AF_TAR_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_AF_FLO1_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_AF_FLO2_DONE_ST | \
				     ISP_IRQ_P1_STATUS2_AF_FLO3_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST2_ERR             (0x0)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_P1_ST2_WAITQ		(0x0)


/* CAM_CTL_INT_P1_STATUS_D */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST_D    (ISP_IRQ_P1_STATUS_D_VS1_INT_ST | \
				     ISP_IRQ_P1_STATUS_D_TG1_INT1_ST | \
				     ISP_IRQ_P1_STATUS_D_TG1_INT2_ST | \
				     ISP_IRQ_P1_STATUS_D_EXPDON1_ST | \
				     ISP_IRQ_P1_STATUS_D_PASS1_DON_ST | \
				     ISP_IRQ_P1_STATUS_D_SOF1_INT_ST | \
				     ISP_IRQ_P1_STATUS_D_AF_DON_ST | \
				ISP_IRQ_P1_STATUS_D_FBC_RRZO_DON_ST | \
				     ISP_IRQ_P1_STATUS_D_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST_D_ERR (ISP_IRQ_P1_STATUS_D_TG1_ERR_ST | \
				      ISP_IRQ_P1_STATUS_D_TG1_GBERR | \
				      ISP_IRQ_P1_STATUS_D_CQ0_ERR | \
				      ISP_IRQ_P1_STATUS_D_CQ0_VS_ERR_ST | \
				 ISP_IRQ_P1_STATUS_D_IMGO_DROP_FRAME_ST | \
				 ISP_IRQ_P1_STATUS_D_RRZO_DROP_FRAME_ST | \
				      ISP_IRQ_P1_STATUS_D_IMGO_ERR_ST | \
				      ISP_IRQ_P1_STATUS_D_AAO_ERR_ST | \
				      ISP_IRQ_P1_STATUS_D_LCSO_ERR_ST | \
				      ISP_IRQ_P1_STATUS_D_RRZO_ERR_ST | \
				      ISP_IRQ_P1_STATUS_D_AFO_ERR_ST | \
				      ISP_IRQ_P1_STATUS_D_LSC_ERR_ST | \
				      ISP_IRQ_P1_STATUS_D_DMA_ERR_ST)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_P1_ST_D_WAITQ (ISP_IRQ_P1_STATUS_D_VS1_INT_ST | \
				  ISP_IRQ_P1_STATUS_D_PASS1_DON_ST | \
					ISP_IRQ_P1_STATUS_D_SOF1_INT_ST|\
					ISP_IRQ_P1_STATUS_D_AF_DON_ST)

/* CAM_CTL_INT_P1_STATUS2_D     */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P1_ST2_D  (ISP_IRQ_P1_STATUS2_D_IMGO_D_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_RRZO_D_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_AFO_D_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_LCSO_D_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_AAO_D_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_BPCI_D_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_LSCI_D_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_AF_TAR_DONE_ST | \
				    ISP_IRQ_P1_STATUS2_D_AF_FLO1_DONE_ST|\
				    ISP_IRQ_P1_STATUS2_D_AF_FLO2_DONE_ST|\
				    ISP_IRQ_P1_STATUS2_D_AF_FLO3_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P1_ST2_D_ERR   (0x0)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_P1_ST2_D_WAITQ  (0x0)

/* CAM_CTL_INT_P2_STATUS */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_P2_ST       (ISP_IRQ_P2_STATUS_PASS2_DON_ST | \
				      ISP_IRQ_P2_STATUS_TILE_DON_ST | \
				      ISP_IRQ_P2_STATUS_CQ_DON_ST | \
				      ISP_IRQ_P2_STATUS_PASS2A_DON_ST | \
				      ISP_IRQ_P2_STATUS_PASS2B_DON_ST | \
				      ISP_IRQ_P2_STATUS_PASS2C_DON_ST | \
				      ISP_IRQ_P2_STATUS_CQ1_DONE_ST | \
				      ISP_IRQ_P2_STATUS_CQ2_DONE_ST | \
				      ISP_IRQ_P2_STATUS_CQ3_DONE_ST | \
				      ISP_IRQ_P2_STATUS_IMGI_DONE_ST | \
				      ISP_IRQ_P2_STATUS_UFDI_DONE_ST | \
				      ISP_IRQ_P2_STATUS_VIPI_DONE_ST | \
				      ISP_IRQ_P2_STATUS_VIP2I_DONE_ST | \
				      ISP_IRQ_P2_STATUS_VIP3I_DONE_ST | \
				      ISP_IRQ_P2_STATUS_LCEI_DONE_ST | \
				      ISP_IRQ_P2_STATUS_MFBO_DONE_ST | \
				      ISP_IRQ_P2_STATUS_IMG2O_DONE_ST | \
				      ISP_IRQ_P2_STATUS_IMG3O_DONE_ST | \
				      ISP_IRQ_P2_STATUS_IMG3BO_DONE_ST | \
				      ISP_IRQ_P2_STATUS_IMG3CO_DONE_ST | \
				      ISP_IRQ_P2_STATUS_FEO_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_P2_ST_ERR   (ISP_IRQ_P2_STATUS_CQ_ERR_ST | \
				      ISP_IRQ_P2_STATUS_TDR_ERR_ST | \
				  ISP_IRQ_P2_STATUS_PASS2A_ERR_TRIG_ST | \
				  ISP_IRQ_P2_STATUS_PASS2B_ERR_TRIG_ST | \
				  ISP_IRQ_P2_STATUS_PASS2C_ERR_TRIG_ST)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_P2_ST_WAITQ   (ISP_IRQ_P2_STATUS_PASS2_DON_ST | \
					ISP_IRQ_P2_STATUS_PASS2A_DON_ST | \
					ISP_IRQ_P2_STATUS_PASS2B_DON_ST | \
					ISP_IRQ_P2_STATUS_PASS2C_DON_ST)
/* CAM_CTL_INT_STATUSX */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_STATUSX       (ISP_IRQ_STATUSX_VS1_INT_ST | \
					ISP_IRQ_STATUSX_TG1_INT1_ST | \
					ISP_IRQ_STATUSX_TG1_INT2_ST | \
					ISP_IRQ_STATUSX_EXPDON1_ST | \
					ISP_IRQ_STATUSX_PASS1_DON_ST | \
					ISP_IRQ_STATUSX_SOF1_INT_ST | \
					ISP_IRQ_STATUSX_PASS2_DON_ST | \
					ISP_IRQ_STATUSX_TILE_DON_ST | \
					ISP_IRQ_STATUSX_AF_DON_ST | \
					ISP_IRQ_STATUSX_FLK_DON_ST | \
					ISP_IRQ_STATUSX_CQ_DON_ST | \
					ISP_IRQ_STATUSX_FBC_RRZO_DON_ST | \
					ISP_IRQ_STATUSX_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_STATUSX_ERR    (ISP_IRQ_STATUSX_TG1_ERR_ST | \
					 ISP_IRQ_STATUSX_TG1_GBERR | \
					 ISP_IRQ_STATUSX_CQ0_ERR | \
					 ISP_IRQ_STATUSX_CQ0_VS_ERR_ST|\
				   ISP_IRQ_STATUSX_IMGO_DROP_FRAME_ST | \
				   ISP_IRQ_STATUSX_RRZO_DROP_FRAME_ST | \
					 ISP_IRQ_STATUSX_CQ_ERR_ST | \
					 ISP_IRQ_STATUSX_IMGO_ERR_ST | \
					 ISP_IRQ_STATUSX_AAO_ERR_ST | \
					 ISP_IRQ_STATUSX_LCSO_ERR_ST | \
					 ISP_IRQ_STATUSX_RRZO_ERR_ST | \
					 ISP_IRQ_STATUSX_ESFKO_ERR_ST | \
					 ISP_IRQ_STATUSX_FLK_ERR_ST | \
					 ISP_IRQ_STATUSX_LSC_ERR_ST | \
					 ISP_IRQ_STATUSX_DMA_ERR_ST)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_STATUSX_WAITQ  (0x0)

/* CAM_CTL_INT_STATUS2X */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_STATUS2X       (ISP_IRQ_STATUS2X_VS1_INT_ST | \
					 ISP_IRQ_STATUS2X_TG1_INT1_ST | \
					 ISP_IRQ_STATUS2X_TG1_INT2_ST | \
					 ISP_IRQ_STATUS2X_EXPDON1_ST | \
					 ISP_IRQ_STATUS2X_PASS1_DON_ST|\
					 ISP_IRQ_STATUS2X_SOF1_INT_ST | \
					 ISP_IRQ_STATUS2X_AF_DON_ST | \
					 ISP_IRQ_STATUS2X_FBC_RRZO_DON_ST | \
				     ISP_IRQ_STATUS2X_FBC_IMGO_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_STATUS2X_ERR   (ISP_IRQ_STATUS2X_TG1_ERR_ST | \
					 ISP_IRQ_STATUS2X_TG1_GBERR | \
					 ISP_IRQ_STATUS2X_CQ0_ERR | \
					 ISP_IRQ_STATUS2X_CQ0_VS_ERR_ST | \
				    ISP_IRQ_STATUS2X_IMGO_DROP_FRAME_ST | \
				    ISP_IRQ_STATUS2X_RRZO_DROP_FRAME_ST | \
					 ISP_IRQ_STATUS2X_IMGO_ERR_ST|\
					 ISP_IRQ_STATUS2X_AAO_ERR_ST | \
					 ISP_IRQ_STATUS2X_LCSO_ERR_ST|\
					 ISP_IRQ_STATUS2X_RRZO_ERR_ST|\
					 ISP_IRQ_STATUS2X_AFO_ERR_ST | \
					 ISP_IRQ_STATUS2X_LSC_ERR_ST | \
					 ISP_IRQ_STATUS2X_DMA_ERR_ST)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_STATUS2X_WAITQ  (0x0)

/* CAM_CTL_INT_STATUS3X */
/* IRQ  Mask */
#define ISP_REG_MASK_INT_STATUS3X    (ISP_IRQ_STATUS3X_IMGO_DONE_ST | \
				      ISP_IRQ_STATUS3X_UFEO_DONE_ST | \
				      ISP_IRQ_STATUS3X_RRZO_DONE_ST | \
				      ISP_IRQ_STATUS3X_ESFKO_DONE_ST | \
				      ISP_IRQ_STATUS3X_LCSO_DONE_ST | \
				      ISP_IRQ_STATUS3X_AAO_DONE_ST | \
				      ISP_IRQ_STATUS3X_BPCI_DONE_ST | \
				      ISP_IRQ_STATUS3X_LSCI_DONE_ST | \
				      ISP_IRQ_STATUS3X_IMGO_D_DONE_ST | \
				      ISP_IRQ_STATUS3X_RRZO_D_DONE_ST | \
				      ISP_IRQ_STATUS3X_AFO_D_DONE_ST | \
				      ISP_IRQ_STATUS3X_LCSO_D_DONE_ST | \
				      ISP_IRQ_STATUS3X_AAO_D_DONE_ST | \
				      ISP_IRQ_STATUS3X_BPCI_D_DONE_ST | \
				      ISP_IRQ_STATUS3X_LCSI_D_DONE_ST | \
				      ISP_IRQ_STATUS3X_IMGI_DONE_ST | \
				      ISP_IRQ_STATUS3X_UFDI_DONE_ST | \
				      ISP_IRQ_STATUS3X_VIPI_DONE_ST | \
				      ISP_IRQ_STATUS3X_VIP2I_DONE_ST | \
				      ISP_IRQ_STATUS3X_VIP3I_DONE_ST | \
				      ISP_IRQ_STATUS3X_LCEI_DONE_ST | \
				      ISP_IRQ_STATUS3X_MFBO_DONE_ST | \
				      ISP_IRQ_STATUS3X_IMG2O_DONE_ST | \
				      ISP_IRQ_STATUS3X_IMG3O_DONE_ST | \
				      ISP_IRQ_STATUS3X_IMG3BO_DONE_ST | \
				      ISP_IRQ_STATUS3X_IMG3CO_DONE_ST | \
				      ISP_IRQ_STATUS3X_FEO_DONE_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_INT_STATUS3X_ERR   (0X0)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_STATUS3X_WAITQ  (0x0)


/* SENINF1_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF1           (0X0)
#define ISP_REG_MASK_INT_SENINF1_ERR   (SENINF1_IRQ_OVERRUN_IRQ_STA | \
					SENINF1_IRQ_CRCERR_IRQ_STA | \
					SENINF1_IRQ_FSMERR_IRQ_STA | \
					SENINF1_IRQ_VSIZEERR_IRQ_STA | \
					SENINF1_IRQ_HSIZEERR_IRQ_STA | \
				 SENINF1_IRQ_SENSOR_VSIZEERR_IRQ_STA | \
				 SENINF1_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_SENINF1_WAITQ  (0x0)

/* SENINF2_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF2           (0X0)
#define ISP_REG_MASK_INT_SENINF2_ERR   (SENINF2_IRQ_OVERRUN_IRQ_STA | \
					SENINF1_IRQ_CRCERR_IRQ_STA | \
					SENINF2_IRQ_FSMERR_IRQ_STA | \
					SENINF2_IRQ_VSIZEERR_IRQ_STA | \
					SENINF2_IRQ_HSIZEERR_IRQ_STA | \
				 SENINF2_IRQ_SENSOR_VSIZEERR_IRQ_STA | \
				 SENINF2_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_SENINF2_WAITQ  (0x0)

/* SENINF3_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF3           (0X0)
#define ISP_REG_MASK_INT_SENINF3_ERR   (SENINF3_IRQ_OVERRUN_IRQ_STA | \
					SENINF3_IRQ_CRCERR_IRQ_STA | \
					SENINF3_IRQ_FSMERR_IRQ_STA | \
					SENINF3_IRQ_VSIZEERR_IRQ_STA | \
					SENINF3_IRQ_HSIZEERR_IRQ_STA | \
				SENINF3_IRQ_SENSOR_VSIZEERR_IRQ_STA | \
				SENINF3_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_SENINF3_WAITQ  (0x0)

/* SENINF4_IRQ_INTSTA */
#define ISP_REG_MASK_INT_SENINF4           (0X0)
#define ISP_REG_MASK_INT_SENINF4_ERR   (SENINF4_IRQ_OVERRUN_IRQ_STA | \
					SENINF4_IRQ_CRCERR_IRQ_STA | \
					SENINF4_IRQ_FSMERR_IRQ_STA | \
					SENINF4_IRQ_VSIZEERR_IRQ_STA | \
					SENINF4_IRQ_HSIZEERR_IRQ_STA | \
				 SENINF4_IRQ_SENSOR_VSIZEERR_IRQ_STA | \
				 SENINF4_IRQ_SENSOR_HSIZEERR_IRQ_STA)
/* if we service wait queue     or not */
#define ISP_REG_MASK_INT_SENINF4_WAITQ  (0x0)

#define ISP_REG_MASK_CAMSV_ST              (ISP_IRQ_CAMSV_STATUS_VS1_ST | \
					    ISP_IRQ_CAMSV_STATUS_TG_ST1 | \
					    ISP_IRQ_CAMSV_STATUS_TG_ST2 | \
					    ISP_IRQ_CAMSV_STATUS_EXPDON1_ST | \
					ISP_IRQ_CAMSV_STATUS_TG_SOF1_ST | \
					ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_CAMSV_ST_ERR  (ISP_IRQ_CAMSV_STATUS_TG_ERR_ST | \
				    ISP_IRQ_CAMSV_STATUS_TG_GBERR_ST | \
				    ISP_IRQ_CAMSV_STATUS_TG_DROP_ST | \
				    ISP_IRQ_CAMSV_STATUS_IMGO_ERR_ST | \
				    ISP_IRQ_CAMSV_STATUS_IMGO_OVERR_ST | \
				    ISP_IRQ_CAMSV_STATUS_IMGO_DROP_ST)
/* if we service wait queue     or not */
#define ISP_REG_MASK_CAMSV_ST_WAITQ   (ISP_IRQ_CAMSV_STATUS_VS1_ST | \
				   ISP_IRQ_CAMSV_STATUS_TG_SOF1_ST | \
				       ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST)

#define ISP_REG_MASK_CAMSV2_ST        (ISP_IRQ_CAMSV2_STATUS_VS1_ST | \
				       ISP_IRQ_CAMSV2_STATUS_TG_ST1 | \
				       ISP_IRQ_CAMSV2_STATUS_TG_ST2 | \
				       ISP_IRQ_CAMSV2_STATUS_EXPDON1_ST | \
				       ISP_IRQ_CAMSV2_STATUS_TG_SOF1_ST | \
				       ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST)
/* IRQ Error Mask */
#define ISP_REG_MASK_CAMSV2_ST_ERR    (ISP_IRQ_CAMSV2_STATUS_TG_ERR_ST | \
				       ISP_IRQ_CAMSV2_STATUS_TG_GBERR_ST|\
				       ISP_IRQ_CAMSV2_STATUS_TG_DROP_ST | \
				       ISP_IRQ_CAMSV2_STATUS_IMGO_ERR_ST|\
				     ISP_IRQ_CAMSV2_STATUS_IMGO_OVERR_ST|\
				       ISP_IRQ_CAMSV2_STATUS_IMGO_DROP_ST)
/* if we service wait queue     or not */
#define ISP_REG_MASK_CAMSV2_ST_WAITQ  (ISP_IRQ_CAMSV2_STATUS_VS1_ST | \
				       ISP_IRQ_CAMSV2_STATUS_TG_SOF1_ST | \
				       ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST)

static signed int gEismetaRIdx;
static signed int gEismetaWIdx;
static signed int gEismetaInSOF;
static signed int gEismetaRIdx_D;
static signed int gEismetaWIdx_D;
static signed int gEismetaInSOF_D;
#define EISMETA_RINGSIZE 4

/* record remain node count(success/fail) excludes head */
/* when enque/deque control */
static signed int EDBufQueRemainNodeCnt;

static /*volatile*/ wait_queue_head_t WaitQueueHead_EDBuf_WaitDeque;
static /*volatile*/ wait_queue_head_t WaitQueueHead_EDBuf_WaitFrame;
static spinlock_t SpinLockEDBufQueList;
#define _MAX_SUPPORT_P2_FRAME_NUM_ 512
#define _MAX_SUPPORT_P2_BURSTQ_NUM_     4
static signed int P2_Support_BurstQNum = 2;
#define _MAX_SUPPORT_P2_PACKAGE_NUM_ \
	(_MAX_SUPPORT_P2_FRAME_NUM_/_MAX_SUPPORT_P2_BURSTQ_NUM_)
#define P2_EDBUF_MLIST_TAG 1
#define P2_EDBUF_RLIST_TAG 2
struct ISP_EDBUF_STRUCT {
	unsigned int processID;     /* caller process ID */
	unsigned long callerID;      /* caller thread     ID */
	/* p2 duplicate CQ index(for recognize belong to which package) */
	signed int p2dupCQIdx;
	enum ISP_ED_BUF_STATE_ENUM bufSts;  /* buffer status */
	signed int p2Scenario;
};
static signed int P2_EDBUF_RList_FirstBufIdx;
static signed int P2_EDBUF_RList_CurBufIdx;
static signed int P2_EDBUF_RList_LastBufIdx;
static struct ISP_EDBUF_STRUCT P2_EDBUF_RingList[_MAX_SUPPORT_P2_FRAME_NUM_];

struct ISP_EDBUF_MGR_STRUCT {
	unsigned int processID;     /* caller process ID */
	unsigned long callerID;      /* caller thread  ID */
	/* p2 duplicate CQ index(for recognize belong to     which package) */
	signed int p2dupCQIdx;
	signed int frameNum;
	 /* number of dequed buffer no matter deque success or fail */
	signed int dequedNum;
	signed int p2Scenario;
};

static signed int compareRingBufNode(struct ISP_EDBUF_STRUCT ringBufNode,
	struct ISP_ED_BUFQUE_STRUCT param)
{
	if ((ringBufNode.processID == param.processID) &&
		 (ringBufNode.callerID == param.callerID) &&
		 (ringBufNode.p2Scenario == param.p2Scenario)) {
		return 1;
	}
	return 0;
}

static signed int compareMgrNode(struct ISP_EDBUF_MGR_STRUCT mgrNode,
	struct ISP_ED_BUFQUE_STRUCT param)
{
	if ((mgrNode.processID == param.processID) &&
		 (mgrNode.callerID == param.callerID) &&
		 (mgrNode.p2dupCQIdx == param.p2dupCQIdx) &&
		 (mgrNode.p2Scenario == param.p2Scenario) &&
		 (mgrNode.dequedNum < mgrNode.frameNum)) {
		return 1;
	}
	return 0;
}

static signed int compareMgrNodeLossely(struct ISP_EDBUF_MGR_STRUCT mgrNode,
	struct ISP_ED_BUFQUE_STRUCT param)
{
	if ((mgrNode.processID == param.processID) &&
		(mgrNode.callerID == param.callerID) &&
		(mgrNode.p2Scenario == param.p2Scenario)) {
		return 1;
	}
	return 0;
}

static signed int P2_EDBUF_MList_FirstBufIdx;
/* static volatile signed int P2_EDBUF_MList_CurBufIdx=0;*/
static signed int P2_EDBUF_MList_LastBufIdx;
static struct ISP_EDBUF_MGR_STRUCT P2_EDBUF_MgrList[_MAX_SUPPORT_P2_PACKAGE_NUM_];

static unsigned int g_regScen = 0xa5a5a5a5;
static spinlock_t SpinLockRegScen;

enum CAMSV_ENUM {
	_camsv_1 = 0,
	_camsv_2,
	_camsv_max
};
static unsigned int g_firstCnt[_camsv_max];
static spinlock_t SpinLockMagNum;
/* queue camsv magnum */
static unsigned int g_magQue[_camsv_max][ISP_RT_BUF_SIZE];
/* g_magQIdx[][0]: write index, g_magQIdx[][1]: read index*/
static unsigned int g_magQIdx[_camsv_max][2];
/*MFALSE:HAL1 enque, configFrame, MTRUE:HAL3 immed enque, DmaCfg */
static bool enqueImd[_camsv_max] = {MFALSE, MFALSE};
/* maximum number for supporting user to do     interrupt operation  */
/* index 0 is for all the user that     do not do register irq first */
#define IRQ_USER_NUM_MAX 32
static spinlock_t SpinLock_UserKey;

/*m4u_callback_ret_t \ */
/* ISP_M4U_TranslationFault_callback \*/
/* (int port, unsigned int mva, void *data);*/

/*********************************************************************
*
**********************************************************************/
/* internal     data */
/* pointer to the kmalloc'd area, rounded up to a page boundary */
static int *pTbl_RTBuf;
/* original pointer for kmalloc'd area as returned by kmalloc */
static void *pBuf_kmalloc;
/*      */
static struct ISP_RT_BUF_STRUCT *pstRTBuf;

/* static struct ISP_DEQUE_BUF_INFO_STRUCT g_deque_buf = {0,{}}; */
/* Marked to remove     build warning. */
unsigned long g_Flash_SpinLock;


static unsigned int G_u4EnableClockCount;


/******************************************************************
*
********************************************************************/
struct ISP_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};

/********************************************************************
*
*********************************************************************/
#define ISP_BUF_SIZE           (4096)
#define ISP_BUF_SIZE_WRITE      1024
#define ISP_BUF_WRITE_AMOUNT    6

enum ISP_BUF_STATUS_ENUM {
	ISP_BUF_STATUS_EMPTY,
	ISP_BUF_STATUS_HOLD,
	ISP_BUF_STATUS_READY
};

struct ISP_BUF_STRUCT {
	enum ISP_BUF_STATUS_ENUM Status;
	unsigned int Size;
	unsigned char *pData;
};

struct ISP_BUF_INFO_STRUCT {
	struct ISP_BUF_STRUCT Read;
	struct ISP_BUF_STRUCT Write[ISP_BUF_WRITE_AMOUNT];
};


/**********************************************************************
*
***********************************************************************/
struct ISP_HOLD_INFO_STRUCT {
	atomic_t HoldEnable;
	atomic_t WriteEnable;
	enum ISP_HOLD_TIME_ENUM Time;
};

static signed int FirstUnusedIrqUserKey = 1;
#define USERKEY_STR_LEN 128

struct UserKeyInfo {
	char userName[USERKEY_STR_LEN]; /* name for the user that register a userKey */
	int userKey;            /* the user key for that user */
};
/* array for recording the user name for a specific user key */
static struct UserKeyInfo IrqUserKey_UserInfo[IRQ_USER_NUM_MAX];

static unsigned int IrqFlush[ISP_IRQ_USER_MAX];
static unsigned int IrqFlush_v3[IRQ_USER_NUM_MAX];


struct ISP_IRQ_INFO_STRUCT {
	unsigned int Status[IRQ_USER_NUM_MAX][ISP_IRQ_TYPE_AMOUNT];
	unsigned int Mask[ISP_IRQ_TYPE_AMOUNT];
	unsigned int ErrMask[ISP_IRQ_TYPE_AMOUNT];

	/* flag for indicating that user do mark for a interrupt or not */
	unsigned int MarkedFlag[IRQ_USER_NUM_MAX][ISP_IRQ_TYPE_AMOUNT];
	/* time for marking a specific interrupt */
	unsigned int MarkedTime_sec[IRQ_USER_NUM_MAX][ISP_IRQ_TYPE_AMOUNT][32];
	/* time for     marking a specific interrupt */
	unsigned int MarkedTime_usec[IRQ_USER_NUM_MAX][ISP_IRQ_TYPE_AMOUNT][32];
	/* number of a specific signal that     passed by */
	signed int PassedBySigCnt[IRQ_USER_NUM_MAX][ISP_IRQ_TYPE_AMOUNT][32];
	/* latest occurring time for each interrupt */
	unsigned int LastestSigTime_sec[ISP_IRQ_TYPE_AMOUNT][32];
	/* latest occurring time for each     interrupt */
	unsigned int LastestSigTime_usec[ISP_IRQ_TYPE_AMOUNT][32];
	/*     eis     meta only for p1 and p1_d */
	struct ISP_EIS_META_STRUCT Eismeta[ISP_IRQ_TYPE_INT_STATUSX][EISMETA_RINGSIZE];
};

struct ISP_TIME_LOG_STRUCT {
	unsigned int Vd;
	unsigned int Expdone;
	unsigned int WorkQueueVd;
	unsigned int WorkQueueExpdone;
	unsigned int TaskletVd;
	unsigned int TaskletExpdone;
};

enum eChannel {
	_PASS1 = 0,
	_PASS1_D = 1,
	_CAMSV = 2,
	_CAMSV_D = 3,
	_PASS2 = 4,
	_ChannelMax = 5,
};

/**********************************************************************/
#define my_get_pow_idx(value)\
({\
	int i = 0, cnt = 0;\
	for (i = 0; i < 32; i++) { \
		if ((value>>i) & (0x00000001)) {\
			break; \
		} else { \
			cnt++;\
		} \
	} \
	cnt;\
})

#define DMA_TRANS(dma, Out)     \
	do { \
		if (dma == _imgo_ || dma ==     _rrzo_) {\
			Out     = _PASS1;\
		} else if (dma == _imgo_d_ || dma == _rrzo_d_) { \
			Out     = _PASS1_D;\
		} else if (dma == _camsv_imgo_) {\
			Out     = _CAMSV;\
		} else if (dma == _camsv2_imgo_) {\
			Out     = _CAMSV_D;\
		} else {\
		} \
	} while (0)

/* basically , should separate into     p1/p1_d/p2/camsv/camsv_d, */
/* currently, only use camsv/camsv_d/others     */
enum eISPIrq {
	_IRQ = 0,
	_IRQ_D = 1,
	_CAMSV_IRQ = 2,
	_CAMSV_D_IRQ = 3,
	_IRQ_MAX = 4,
};

enum eLOG_TYPE {
	_LOG_DBG = 0,           /* currently, only used at ipl_buf_ctrl. to     protect critical section */
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
};

enum eLOG_OP {
	_LOG_INIT = 0,
	_LOG_RST = 1,
	_LOG_ADD = 2,
	_LOG_PRT = 3,
	_LOG_GETCNT = 4,
	_LOG_OP_MAX = 5
};

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/* #define SV_LOG_STR_LEN NORMAL_STR_LEN */

#define LOG_PPNUM 2
/*volatile static unsigned int m_CurrentPPB;*/ /* ERROR */
static unsigned int m_CurrentPPB;
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
}; /* PSV_LOG_STR; */

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[_IRQ_MAX];
/* static struct SV_LOG_STR gSvLog_IRQ = {0};*/
/* static struct SV_LOG_STR gSvLog_CAMSV_IRQ= {0};*/
/* static struct SV_LOG_STR gSvLog_CAMSV_D_IRQ= {0};*/
static bool g_bDmaERR_p1 = MFALSE;
static bool g_bDmaERR_p1_d = MFALSE;
#if 0
static bool g_bDmaERR_p2 = MFALSE;
#endif
static bool g_bDmaERR_deepDump = MFALSE;
static unsigned int g_ISPIntErr[_IRQ_MAX] = { 0 };

#define nDMA_ERR_P1     (11)
#define nDMA_ERR_P1_D   (7)
#define nDMA_ERR        (nDMA_ERR_P1 + nDMA_ERR_P1_D)
static unsigned int g_DmaErr_p1[nDMA_ERR] = { 0 };

/*	for     irq     used,keep log until     IRQ_LOG_PRINTER being involked,*/
/*	limited:*/
/*	each log must shorter than 512 bytes*/
/*	total log length in     each irq/logtype can't over     1024 bytes*/

#define IRQ_LOG_KEEPER_T(sec, usec)     {\
		ktime_t time;              \
		time = ktime_get();        \
		sec     = time.tv64;       \
		do_div(sec,     1000);   \
		usec = do_div(sec, 1000000);\
	}
#if     1
/* snprintf: avaLen, 1 for null termination*/
#define IRQ_LOG_KEEPER(irq,     ppb, logT, fmt, ...) do {\
		char *ptr;\
		char *pDes;\
		signed int avaLen;\
		unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
		unsigned int str_leng;\
		if (logT == _LOG_ERR) {\
			str_leng = NORMAL_STR_LEN*ERR_PAGE;\
		} else if (logT == _LOG_DBG) {\
			str_leng = NORMAL_STR_LEN*DBG_PAGE;\
		} else if (logT == _LOG_INF) {\
			str_leng = NORMAL_STR_LEN*INF_PAGE;\
		} else {\
			str_leng = 0;\
		} \
		ptr     = pDes = (char *)&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);   \
		avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT];\
		if (avaLen > 1) {\
			snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);  \
			if ('\0' !=     gSvLog[irq]._str[ppb][logT][str_leng - 1]) { \
				LOG_INF("(%d)(%d)log str over flow", irq, logT);\
			} \
			while (*ptr++ != '\0') {\
				(*ptr2)++;\
			} \
		} else { \
			LOG_INF("(%d)(%d)log str avalible=0", irq, logT);\
		} \
	} while (0)
/*for keep IRQ err log*/
#define IRQ_LOG_KEEPER_PR_ERR(irq,     ppb, logT, fmt, ...) do {\
		char *ptr;\
		char *pDes;\
		signed int avaLen;\
		unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
		unsigned int str_leng;\
		if (logT == _LOG_ERR) {\
			str_leng = NORMAL_STR_LEN*ERR_PAGE;\
		} else {\
			str_leng = 0;\
		} \
		ptr     = pDes = (char *)&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);   \
		avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT];\
		if (avaLen > 1) {\
			snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);  \
			if ('\0' !=     gSvLog[irq]._str[ppb][logT][str_leng - 1]) { \
				LOG_PR_ERR("(%d)(%d)log str over flow", irq, logT);\
			} \
			while (*ptr++ != '\0') {\
				(*ptr2)++;\
			} \
		} else { \
			LOG_INF("(%d)(%d)log str avalible=0", irq, logT);\
		} \
	} while (0)
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, args...)    pr_debug(IRQTag fmt,  ##args)
#define IRQ_LOG_KEEPER_PR_ERR(irq, ppb, logT, fmt, args...)    pr_err(IRQTag fmt,  ##args)
#endif

#if     1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
		struct SV_LOG_STR *pSrc = &gSvLog[irq];\
		char *ptr;\
		unsigned int i;\
		signed int ppb = 0;\
		signed int logT     = 0;\
		if (ppb_in > 1) {\
			ppb     = 1;\
		} else{\
			ppb     = ppb_in;\
		} \
		if (logT_in     > _LOG_ERR)     {\
			logT = _LOG_ERR;\
		} else{\
			logT = logT_in;\
		} \
		ptr     = pSrc->_str[ppb][logT];\
		if (pSrc->_cnt[ppb][logT] != 0) {\
			if (logT == _LOG_DBG) {\
				for     (i = 0; i <     DBG_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
						LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else if (logT == _LOG_INF) {\
				for     (i = 0; i <     INF_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
						LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else if (logT == _LOG_ERR) {\
				for     (i = 0; i <     ERR_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
						LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else {\
				LOG_INF("N.S.%d", logT);\
			} \
			ptr[0] = '\0';\
			pSrc->_cnt[ppb][logT] = 0;\
		} \
	} while (0)
/*for print IRQ err log*/
#define IRQ_LOG_PRINTER_PR_ERR(irq, ppb_in, logT_in) do {\
		struct SV_LOG_STR *pSrc = &gSvLog[irq];\
		char *ptr;\
		unsigned int i;\
		signed int ppb = 0;\
		signed int logT     = 0;\
		if (ppb_in > 1) {\
			ppb     = 1;\
		} else{\
			ppb     = ppb_in;\
		} \
		if (logT_in     > _LOG_ERR)     {\
			logT = _LOG_ERR;\
		} else{\
			logT = logT_in;\
		} \
		ptr     = pSrc->_str[ppb][logT];\
		if (pSrc->_cnt[ppb][logT] != 0) {\
			if (logT == _LOG_ERR) {\
				for     (i = 0; i <     ERR_PAGE; i++) {\
					if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
						ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
						LOG_PR_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
					} else{\
						LOG_PR_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
						break;\
					} \
				} \
			} \
			else {\
				LOG_PR_ERR("N.S.%d", logT);\
			} \
			ptr[0] = '\0';\
			pSrc->_cnt[ppb][logT] = 0;\
		} \
	} while (0)
#else
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#define IRQ_LOG_PRINTER_PR_ERR(irq, ppb, logT)
#endif

#define SUPPORT_MAX_IRQ 32
struct ISP_INFO_STRUCT {
	spinlock_t SpinLockIspRef;
	spinlock_t SpinLockIsp;
	/* currently,     IRQ     and     IRQ_D share     the     same ISR , so share     the     same key,IRQ. */
	spinlock_t SpinLockIrq[_IRQ_MAX];
	spinlock_t SpinLockHold;
	spinlock_t SpinLockRTBC;
	spinlock_t SpinLockClock;
	wait_queue_head_t WaitQueueHead;
	/* wait_queue_head_t*                      WaitQHeadList; */
	wait_queue_head_t WaitQHeadList[SUPPORT_MAX_IRQ];
	struct work_struct ScheduleWorkVD;
	struct work_struct ScheduleWorkEXPDONE;
	unsigned int UserCount;
	unsigned int DebugMask;
	signed int IrqNum;
	struct ISP_IRQ_INFO_STRUCT IrqInfo;
	struct ISP_HOLD_INFO_STRUCT HoldInfo;
	struct ISP_BUF_INFO_STRUCT BufInfo;
	struct ISP_TIME_LOG_STRUCT TimeLog;
	struct ISP_CALLBACK_STRUCT Callback[ISP_CALLBACK_AMOUNT];
};

static struct tasklet_struct isp_tasklet;

static bool bSlowMotion = MFALSE;
static bool bRawEn = MFALSE;
static bool bRawDEn = MFALSE;
static bool bCamSVEn = MFALSE;
static bool bCamSVDEn = MFALSE;

static struct ISP_INFO_STRUCT IspInfo;

unsigned int PrvAddr[_ChannelMax] = { 0 };

/*MCLK counter*/
static signed int mMclk1User;
static signed int mMclk2User;
static signed int mMclk3User;
/**********************************************
************************************************/
#ifdef T_STAMP_2_0
#define SlowMotion      100
struct T_STAMP {
	unsigned long long T_ns;       /* 1st     frame start     time, accurency in us,unit in ns */
	unsigned long interval_us;      /* unit in us */
	unsigned long compensation_us;
	unsigned int fps;
	unsigned int fcnt;
};

static struct T_STAMP m_T_STAMP = { 0 };
#endif

/*******************************************************************************
*
********************************************************************************/
/* test flag */
#define ISP_KERNEL_MOTIFY_SIGNAL_TEST
#ifdef ISP_KERNEL_MOTIFY_SIGNAL_TEST
/*** Linux signal test ***/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/siginfo.h>        /* siginfo */
#include <linux/rcupdate.h>     /* rcu_read_lock */
#include <linux/sched.h>        /* find_task_by_pid_type */
#include <linux/debugfs.h>
#include <linux/uaccess.h>

/* js_test */
#define __tcmfunc


#define SIG_TEST 44 /* we choose 44 as our signal number (real-time signals are     in the range of 33 to 64) */

struct siginfo info;
struct task_struct *t;


int getTaskInfo(pid_t pid)
{
	/* send the     signal */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_TEST;
	info.si_code = SI_QUEUE; /* this is bit of a trickery: SI_QUEUE is normally used by */
	/* sigqueue from user space,     */
	/* and kernel space     should use SI_KERNEL. But if SI_KERNEL is used the real_time data */
	/* is not delivered     to the user     space signal handler function. */
	info.si_int = 1234;     /* real time signals may     have 32 bits of data. */

	rcu_read_lock();

	t = find_task_by_vpid(pid);
	/* t = find_task_by_pid_type(PIDTYPE_PID, g_pid); //find the task_struct associated     with this pid */
	if (t == NULL) {
		LOG_DBG("no	such pid");
		rcu_read_unlock();
		return -ENODEV;
	}
	rcu_read_unlock();

	return 0;
}

int sendSignal(void)
{
	int ret = 0;

	ret = send_sig_info(SIG_TEST, &info, t);        /* send the signal */
	if (ret < 0) {
		LOG_DBG("error sending signal");
		return ret;
	}
	return ret;
}

/*** Linux signal test ***/

#endif                          /* ISP_KERNEL_MOTIFY_SIGNAL_TEST */

/*******************************************************************************
*
********************************************************************************/
static inline unsigned int ISP_MsToJiffies(unsigned int Ms)
{
	unsigned int ret;

	ret = (Ms * HZ + 512) >> 10;
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static inline unsigned int ISP_UsToJiffies(unsigned int Us)
{
	unsigned int ret;

	ret = ((Us / 1000) * HZ + 512) >> 10;
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static inline unsigned int ISP_GetIRQState(enum eISPIrq eIrq, unsigned int type,
					   unsigned int userNumber, unsigned int stus)
{
	unsigned int ret;
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */

	/*      */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	ret = (IspInfo.IrqInfo.Status[userNumber][type] & stus);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	/*      */
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static inline unsigned int ISP_GetEDBufQueWaitDequeState(signed int idx)
{
	unsigned int ret = MFALSE;
	/*      */
	spin_lock(&(SpinLockEDBufQueList));
	if (P2_EDBUF_RingList[idx].bufSts == ISP_ED_BUF_STATE_RUNNING)
		ret = MTRUE;

	spin_unlock(&(SpinLockEDBufQueList));
	/*      */
	return ret;
}

static inline unsigned int ISP_GetEDBufQueWaitFrameState(signed int idx)
{
	unsigned int ret = MFALSE;
	/*      */
	spin_lock(&(SpinLockEDBufQueList));
	if (P2_EDBUF_MgrList[idx].dequedNum == P2_EDBUF_MgrList[idx].frameNum)
		ret = MTRUE;

	spin_unlock(&(SpinLockEDBufQueList));
	/*      */
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static inline unsigned int ISP_JiffiesToMs(unsigned int Jiffies)
{
	unsigned int ret;

	ret = (Jiffies * 1000) / HZ;
	return ret;
}


/*******************************************************************************
*
********************************************************************************/
static unsigned int ISP_DumpDmaDeepDbg(void)
{
	if (g_bDmaERR_p1) {
		g_DmaErr_p1[0] = (unsigned int) ISP_RD32(ISP_ADDR + 0x356c);
		g_DmaErr_p1[1] = (unsigned int) ISP_RD32(ISP_ADDR + 0x3570);
		g_DmaErr_p1[2] = (unsigned int) ISP_RD32(ISP_ADDR + 0x3574);
		g_DmaErr_p1[3] = (unsigned int) ISP_RD32(ISP_ADDR + 0x3578);
		g_DmaErr_p1[4] = (unsigned int) ISP_RD32(ISP_ADDR + 0x357C);
		g_DmaErr_p1[5] = (unsigned int) ISP_RD32(ISP_ADDR + 0x358c);
		g_DmaErr_p1[6] = (unsigned int) ISP_RD32(ISP_ADDR + 0x3590);
		g_DmaErr_p1[7] = (unsigned int) ISP_RD32(ISP_ADDR + 0x3594);
		g_DmaErr_p1[8] = (unsigned int) ISP_RD32(ISP_ADDR + 0x3598);
		g_DmaErr_p1[9] = (unsigned int) ISP_RD32(ISP_ADDR + 0x359c);
		g_DmaErr_p1[10] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35a0);
		LOG_PR_ERR
		("IMGI:0x%x,BPCI:0x%x,LSCI=0x%x,UFDI=0x%x,LCEI=0x%x,imgo=0x%x,",
		 g_DmaErr_p1[0], g_DmaErr_p1[1], g_DmaErr_p1[2], g_DmaErr_p1[3], g_DmaErr_p1[4],
		 g_DmaErr_p1[5]);
		LOG_PR_ERR
		("rrzo:0x%x,lcso:0x%x,esfko:0x%x,aao:0x%x,ufeo:0x%x",
		 g_DmaErr_p1[6], g_DmaErr_p1[7], g_DmaErr_p1[8], g_DmaErr_p1[9],
		 g_DmaErr_p1[10]);
		g_bDmaERR_p1 = MFALSE;
	}
	if (g_bDmaERR_p1_d) {
		g_DmaErr_p1[11] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35bc);
		g_DmaErr_p1[12] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35c0);
		g_DmaErr_p1[13] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35c4);
		g_DmaErr_p1[14] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35c8);
		g_DmaErr_p1[15] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35cc);
		g_DmaErr_p1[16] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35d0);
		g_DmaErr_p1[17] = (unsigned int) ISP_RD32(ISP_ADDR + 0x35d4);
		LOG_PR_ERR
		("BPCI_D:0x%x,LSCI_D:0x%x,IMGO_D=0x%x,RRZO_D=0x%x,LSCO_D=0x%x,AFO_D=0x%x,AAO_D:0x%x",
		 g_DmaErr_p1[11], g_DmaErr_p1[12], g_DmaErr_p1[13], g_DmaErr_p1[14],
		 g_DmaErr_p1[15], g_DmaErr_p1[16], g_DmaErr_p1[17]);
		g_bDmaERR_p1_d = MFALSE;
	}
#if     0
	if (g_bDmaERR_p2) {
		LOG_PR_ERR
		("vipi:0x%x,VIPI:0x%x,VIP2I=0x%x,VIP3I=0x%x,MFBO=0x%x,",
		 ISP_RD32(ISP_ADDR + 0x3574), ISP_RD32(ISP_ADDR + 0x3580),
		 ISP_RD32(ISP_ADDR + 0x3584), ISP_RD32(ISP_ADDR + 0x3588),
		 ISP_RD32(ISP_ADDR + 0x35a4));
		LOG_PR_ERR
		("IMG3BO=0x%x,IMG3CO:0x%x,IMG2O:0x%x,IMG3O:0x%x,FEO:0x%x",
		 ISP_RD32(ISP_ADDR + 0x35a8),
		 ISP_RD32(ISP_ADDR + 0x35ac), ISP_RD32(ISP_ADDR + 0x35b0),
		 ISP_RD32(ISP_ADDR + 0x35b4), ISP_RD32(ISP_ADDR + 0x35b8));
		g_bDmaERR_p2 = MFALSE;
	}

	if (g_bDmaERR_deepDump) {
		ISP_WR32((ISP_ADDR + 0x160), 0x0);
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1E);
		LOG_PR_ERR("imgi_debug_0 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x11E);
		LOG_PR_ERR("imgi_debug_1 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x21E);
		LOG_PR_ERR("imgi_debug_2 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x31E);
		LOG_PR_ERR("imgi_debug_3 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* vipi */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x41E);
		LOG_PR_ERR("vipi_debug_0 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x51E);
		LOG_PR_ERR("vipi_debug_1 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x61E);
		LOG_PR_ERR("vipi_debug_2 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x71E);
		LOG_PR_ERR("vipi_debug_3 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* imgo */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x81E);
		LOG_PR_ERR("imgo_debug_0 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x91E);
		LOG_PR_ERR("imgo_debug_1 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xa1E);
		LOG_PR_ERR("imgo_debug_2 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xb1E);
		LOG_PR_ERR("imgo_debug_3 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* imgo_d */
		ISP_WR32((ISP_ADDR + 0x35f4), 0xc1E);
		LOG_PR_ERR("imgo_d_debug_0	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xd1E);
		LOG_PR_ERR("imgo_d_debug_1	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xe1E);
		LOG_PR_ERR("imgo_d_debug_2	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0xf1E);
		LOG_PR_ERR("imgo_d_debug_3	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* rrzo */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x101E);
		LOG_PR_ERR("rrzo_debug_0 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x111E);
		LOG_PR_ERR("rrzo_debug_1 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x121E);
		LOG_PR_ERR("rrzo_debug_2 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x131E);
		LOG_PR_ERR("rrzo_debug_3 =	0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* rrzo_d */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x151E);
		LOG_PR_ERR("rrzo_d_debug_0	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x161E);
		LOG_PR_ERR("rrzo_d_debug_1	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x171E);
		LOG_PR_ERR("rrzo_d_debug_2	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x181E);
		LOG_PR_ERR("rrzo_d_debug_3	= 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* img3o */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x181E);
		LOG_PR_ERR("img3o_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x191E);
		LOG_PR_ERR("img3o_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1a1E);
		LOG_PR_ERR("img3o_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1b1E);
		LOG_PR_ERR("img3o_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		/* img2o */
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1c1E);
		LOG_PR_ERR("img3o_debug_0 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1d1E);
		LOG_PR_ERR("img3o_debug_1 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1e1E);
		LOG_PR_ERR("img3o_debug_2 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		ISP_WR32((ISP_ADDR + 0x35f4), 0x1f1E);
		LOG_PR_ERR("img3o_debug_3 = 0x%x\n", ISP_RD32(ISP_ADDR + 0x164));
		g_bDmaERR_deepDump = MFALSE;
	}
#endif

	return 0;
}

#define RegDump(start, end) {\
		unsigned int i;\
		for (i = start; i <= end; i += 0x10) {\
			LOG_PR_ERR("[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]\n", \
				(unsigned int)(ISP_TPIPE_ADDR + i), (unsigned int)ISP_RD32(ISP_ADDR + i), \
				(unsigned int)(ISP_TPIPE_ADDR + i+0x4), (unsigned int)ISP_RD32(ISP_ADDR + i+0x4), \
				(unsigned int)(ISP_TPIPE_ADDR + i+0x8), (unsigned int)ISP_RD32(ISP_ADDR + i+0x8), \
				(unsigned int)(ISP_TPIPE_ADDR + i+0xc), (unsigned int)ISP_RD32(ISP_ADDR + i+0xc));\
		} \
	}

bool ISP_chkModuleSetting(void)
{
	/*check the setting; */
	unsigned int cam_ctrl_en_p1; /*4004 */
	unsigned int cam_ctrl_sel_pl;        /*4034 */
	unsigned int cam_bmx_crop;     /*4E14 */
	unsigned int cam_bmx_d_crop;   /*6E14 */
	unsigned int cam_bmx_vsize;  /*4E18 */
	unsigned int cam_tg1_vf_con; /*4414 */
	unsigned int cam_tg2_vf_con; /*6414 */

	unsigned int grab_width;
	unsigned int grab_height;

	unsigned int cam_tg1_sen_grab_pxl;   /*4418 */
	unsigned int cam_tg1_sen_grab_lin;   /*441C */

	unsigned int cam_tg2_sen_grab_pxl;   /*6418 */
	unsigned int cam_tg2_sen_grab_lin;   /*641C */


	unsigned int bmx_width;
	unsigned int bmx_d_width;
	unsigned int bmx_height;

	unsigned int sgg_sel;
	unsigned int eis_sel;
	bool bmx_enable;
	bool hbin2_en;
	bool sgg3_en, flk_en;
	unsigned int i;

	cam_ctrl_en_p1 = ISP_RD32(ISP_ADDR + 0x4);
	bmx_enable = (cam_ctrl_en_p1 >> 11) & 0x01;
	hbin2_en = (cam_ctrl_en_p1 >> 18) & 0x01;
	sgg3_en = (cam_ctrl_en_p1 >> 27) & 0x01;
	flk_en = (cam_ctrl_en_p1 >> 17) & 0x01;
	cam_ctrl_sel_pl = ISP_RD32(ISP_ADDR + 0x34);
	sgg_sel = (cam_ctrl_sel_pl >> 1) & 0x03;
	eis_sel = (cam_ctrl_sel_pl >> 10) & 0x03;

	cam_bmx_crop = ISP_RD32(ISP_ADDR + 0xE14);
	cam_bmx_d_crop = ISP_RD32(ISP_ADDR + 0x2E14);
	cam_bmx_vsize = ISP_RD32(ISP_ADDR + 0xE18);
	bmx_width = ((cam_bmx_crop >> 16) & 0x1fff) - (cam_bmx_crop & 0x1fff) + 1;
	bmx_d_width = ((cam_bmx_d_crop >> 16) & 0x1fff) - (cam_bmx_d_crop & 0x1fff) + 1;
	bmx_height = (cam_bmx_vsize & 0x1fff);

	cam_tg1_sen_grab_pxl = ISP_RD32(ISP_ADDR + 0x418);
	cam_tg1_sen_grab_lin = ISP_RD32(ISP_ADDR + 0x41C);

	cam_tg2_sen_grab_pxl = ISP_RD32(ISP_ADDR + 0x2418);
	cam_tg2_sen_grab_lin = ISP_RD32(ISP_ADDR + 0x241C);
	cam_tg1_vf_con = ISP_RD32(ISP_ADDR + 0x414);
	cam_tg2_vf_con = ISP_RD32(ISP_ADDR + 0x2414);


	if (cam_tg1_vf_con & 0x01) {
		/*Check FLK setting */
		unsigned int cam_flk_con;    /*4770 */
		unsigned int cam_flk_ofst;   /*4774 */
		unsigned int cam_flk_size;   /*4778 */
		unsigned int cam_flk_num;    /*477C */

		unsigned int cam_esfko_xsize;        /*7370 */

		unsigned int FLK_OFST_X;
		unsigned int FLK_OFST_Y;
		unsigned int FLK_SIZE_X;
		unsigned int FLK_SIZE_Y;
		unsigned int FLK_NUM_X;
		unsigned int FLK_NUM_Y;
		unsigned int sggmux_h_size;
		unsigned int sggmux_v_size;

		unsigned int esfko_xsize;



		unsigned int cam_aao_xsize;  /*7390 */
		unsigned int cam_aao_ysize;  /*7394 */
		unsigned int cam_awb_win_num;        /*45BC */
		unsigned int cam_ae_hst_ctl; /*4650 */

		unsigned int AAO_XSIZE;
		unsigned int AWB_W_HNUM;
		unsigned int AWB_W_VNUM;
		unsigned int histogramen_num;

		unsigned int cam_awb_win_org;        /*45B0 */
		unsigned int cam_awb_win_pit;        /*45B8 */

		unsigned int AAO_InWidth;
		unsigned int AAO_InHeight;
		unsigned int AWB_W_HPIT;
		unsigned int AWB_W_VPIT;
		unsigned int AWB_W_HORG;
		unsigned int AWB_W_VORG;

		unsigned int tmp, rst;
		unsigned int h_size;
		unsigned int v_size;
		unsigned int xsize, ysize;
		unsigned int af_v_avg_lvl, af_v_gonly;
		unsigned int af_h_gonly;
		unsigned int af_ext_stat_en, af_blk_sz;
		unsigned int afo_d_xsize, afo_d_ysize;
		unsigned int af_sat_th0, af_sat_th1, af_sat_th2, af_sat_th3;
		unsigned int TG_W;
		unsigned int TG_H;
		unsigned int AF_EN, AFO_D_EN;
		unsigned int SGG1_EN, SGG5_EN;
		unsigned int cam_ctrl_en_p1_dma_d;  /*4014*/
		unsigned int cam_tg_sen_mode, dbl_data_bus;
		unsigned int tg_w_pxl_e, tg_w_pxl_s;
		unsigned int tg_h_lin_e, tg_h_lin_s;
		unsigned int cam_af_con;
		unsigned int cam_af_th_2;
		unsigned int cam_af_size, af_image_wd;
		unsigned int cam_af_vld, af_vld_ystart, af_vld_xstart;
		unsigned int cam_af_blk_0, af_blk_ysize, af_blk_xsize;
		unsigned int cam_af_blk_1, af_blk_ynum, af_blk_xnum;
		unsigned int cam_bmx_crop, bmx_end_x, bmx_str_x;
		unsigned int cam_rmx_crop, rmx_end_x, rmx_str_x;

		unsigned int rrz_out_width;
		unsigned int rrz_out_height;
		unsigned int rrz_d_out_width;
		unsigned int rrz_d_out_height;
		unsigned int scenario;

		unsigned int cam_rrz_out_img;        /*47A8 */
		unsigned int cam_rrz_d_out_img;      /*67A8 */
		unsigned int cam_ctl_scenario;       /*4024 */

		unsigned int EIS_RP_VOFST;
		unsigned int EIS_RP_HOFST;
		unsigned int EIS_WIN_VSIZE;
		unsigned int EIS_WIN_HSIZE;

		unsigned int EIS_OP_HORI;
		unsigned int EIS_OP_VERT;

		unsigned int EIS_SUBG_EN;
		unsigned int EIS_NUM_HRP;
		unsigned int EIS_NUM_VRP;
		unsigned int EIS_NUM_HWIN;
		unsigned int EIS_NUM_VWIN;

		unsigned int EIS_IMG_WIDTH;
		unsigned int EIS_IMG_HEIGHT;
		unsigned int EISO_XSIZE;

		bool bError;

		unsigned int CAM_EIS_PREP_ME_CTRL1;  /*4DC0 */
		unsigned int CAM_EIS_MB_OFFSET;      /*4DD0 */
		unsigned int CAM_EIS_MB_INTERVAL;    /*4DD4 */
		unsigned int CAM_EIS_IMAGE_CTRL;     /*4DE0 */

		LOG_INF("ISP chk TG1");

		grab_width =
			((cam_tg1_sen_grab_pxl >> 16) & 0x7fff) - (cam_tg1_sen_grab_pxl & 0x7fff);
		grab_height =
			((cam_tg1_sen_grab_lin >> 16) & 0x1fff) - (cam_tg1_sen_grab_lin & 0x1fff);

		cam_esfko_xsize = ISP_RD32(ISP_ADDR + 0x3370);
		esfko_xsize = cam_esfko_xsize & 0xffff;

		cam_flk_con = ISP_RD32(ISP_ADDR + 0x770);
		cam_flk_ofst = ISP_RD32(ISP_ADDR + 0x774);
		cam_flk_size = ISP_RD32(ISP_ADDR + 0x778);
		cam_flk_num = ISP_RD32(ISP_ADDR + 0x77C);
		FLK_OFST_X = cam_flk_ofst & 0xFFF;
		FLK_OFST_Y = (cam_flk_ofst >> 16) & 0xFFF;
		FLK_SIZE_X = cam_flk_size & 0xFFF;
		FLK_SIZE_Y = (cam_flk_size >> 16) & 0xFFF;
		FLK_NUM_X = cam_flk_num & 0x7;
		FLK_NUM_Y = (cam_flk_num >> 4) & 0x7;
		if ((flk_en == 1) && (sgg3_en == 0))
			LOG_INF("HwRWCtrl:: Flicker Error: SGG3_EN should be 1 when FLK_EN = 1");
		/*1. The window size must be multiples of 2 */
		/*2. horizontally and vertically */
		/*5. CAM_FLK_SIZE.FLK_SIZE_X.value can't be 0 */
		/*6. CAM_FLK_SIZE.FLK_SIZE_Y.value can't be 0 */
		if ((FLK_SIZE_X % 2 != 0) || (FLK_SIZE_Y % 2 != 0)
			|| (FLK_SIZE_X == 0) || (FLK_SIZE_Y == 0)) {
			/* Error */
			LOG_INF
			("HwRWCtrl:: Flicker Error: The window size must be multiples of 2.");
			LOG_INF
			("horizontally and vertically!!");
			LOG_INF
			("HwRWCtrl:: Flicker Error: CAM_FLK_SIZE.FLK_SIZE_X(%d)	and",
				FLK_SIZE_X);
			LOG_INF
			(" CAM_FLK_SIZE.FLK_SIZE_Y(%d) value can't be 0!!",
				FLK_SIZE_Y);
		}
		if (bmx_enable == MTRUE) {
			sggmux_h_size = bmx_width + bmx_d_width;
			sggmux_v_size = bmx_height;
		} else {
			sggmux_h_size = grab_width;
			sggmux_v_size = grab_height;
		}
		/*(CAM_FLK_NUM.FLK_NUM_X.value * CAM_FLK_SIZE.FLK_SIZE_X.value) + */
		/*	CAM_FLK_OFST.FLK_OFST_X.value <= sggmux_h_size.value */
		/*(CAM_FLK_NUM.FLK_NUM_Y.value * CAM_FLK_SIZE.FLK_SIZE_Y.value) + */
		/*	CAM_FLK_OFST.FLK_OFST_Y.value <= sggmux_v_size.value */
		if ((((FLK_NUM_X * FLK_SIZE_X) + FLK_OFST_X) > sggmux_h_size) ||
			(((FLK_NUM_Y * FLK_SIZE_Y) + FLK_OFST_Y) > sggmux_v_size)) {
			/*Error */
			LOG_INF
			("HwRWCtrl:: Flicker Error: bmx_enable(%d), sgg_sel(%d), grab_width(%d),",
				bmx_enable, sgg_sel, grab_width);
			LOG_INF
			(" grab_height(%d), bmx_width(%d), bmx_height(%d)!!",
				grab_height, bmx_width, bmx_height);
			LOG_INF
			("HwRWCtrl:: Flicker Error: (CAM_FLK_NUM.FLK_NUM_X.value(%d) *",
				FLK_NUM_X);
			LOG_INF
			(" CAM_FLK_SIZE.FLK_SIZE_X.value(%d)) + CAM_FLK_OFST.FLK_OFST_X.value(%d)",
				FLK_SIZE_X, FLK_OFST_X);
			LOG_INF
			(" <= sggmux_h_size.value(%d)!!",
				sggmux_h_size);
			LOG_INF
			("HwRWCtrl:: Flicker Error: (CAM_FLK_NUM.FLK_NUM_Y.value(%d) *",
				FLK_NUM_Y);
			LOG_INF
			(" CAM_FLK_SIZE.FLK_SIZE_Y.value(%d)) + CAM_FLK_OFST.FLK_OFST_Y.value(%d)",
				FLK_SIZE_Y, FLK_OFST_Y);
			LOG_INF
			(" <= sggmux_v_size.value(%d)!!",
				sggmux_v_size);
		}

		/*4. flko_xs == ((CAM_FLK_NUM.FLK_NUM_X.value * CAM_FLK_NUM.FLK_NUM_Y.value *  */
			/*CAM_FLK_SIZE.FLK_SIZE_Y.value * 2) - 1) */
		if (esfko_xsize != ((FLK_NUM_X * FLK_NUM_Y * FLK_SIZE_Y * 2) - 1)) {
			/*Error */
			LOG_INF
			("HwRWCtrl:: Flicker Error: flko_xs(%d) must be equal ((CAM_FLK_NUM.FLK_NUM_X.value(%d) *",
				esfko_xsize, FLK_NUM_X);
			LOG_INF
			(" CAM_FLK_NUM.FLK_NUM_Y.value(%d) * CAM_FLK_SIZE.FLK_SIZE_Y.value(%d) * 2) - 1)!!",
				FLK_NUM_Y, FLK_SIZE_Y);
		}
	/*Check AF setting */

	/* under twin case, sgg_sel won't be 0 , so , don't need to take into consideration at twin case */
	cam_ctrl_en_p1_dma_d = ISP_RD32(ISP_ADDR + 0x14);
	cam_af_con = ISP_RD32(ISP_ADDR + 0x6B0);
	tmp = 0;
	rst = 0;
	cam_tg_sen_mode = ISP_RD32(ISP_ADDR + 0x410);
	cam_bmx_crop = ISP_RD32(ISP_ADDR + 0xE14);
	cam_rmx_crop = ISP_RD32(ISP_ADDR + 0xE24);
	TG_W = ISP_RD32(ISP_ADDR + 0x418);
	TG_H = ISP_RD32(ISP_ADDR + 0x41C);
	cam_af_size = ISP_RD32(ISP_ADDR + 0x6E0);
	cam_af_vld = ISP_RD32(ISP_ADDR + 0x6E4);
	cam_af_blk_0 = ISP_RD32(ISP_ADDR + 0x6E8);
	cam_af_blk_1 = ISP_RD32(ISP_ADDR + 0x6EC);
	cam_af_th_2 = ISP_RD32(ISP_ADDR + 0x6F0);
	afo_d_xsize = ISP_RD32(ISP_ADDR + 0x3534);
	afo_d_ysize = ISP_RD32(ISP_ADDR + 0x353C);

	AF_EN = (cam_ctrl_en_p1 >> 16) & 0x1;
	AFO_D_EN = (cam_ctrl_en_p1_dma_d >> 3) & 0x1;
	SGG1_EN = (cam_ctrl_en_p1 >> 15) & 0x1;
	SGG5_EN = (cam_ctrl_en_p1 >> 28) & 0x1;
	/*  */
	if (AF_EN == 0) {
		if (AFO_D_EN == 1) {
			LOG_INF("DO NOT enable AFO_D without enable AF\n");
			rst = 2; /* 2: check module en */
			goto AF_EXIT;
		} else {
			rst = 0; /* PASS */
			goto AF_EXIT;
		}
	}

	/*  */
	tg_w_pxl_e = (TG_W >> 16) & 0x7fff;
	tg_w_pxl_s = TG_W & 0x7fff;
	tg_h_lin_e = (TG_H >> 16) & 0x7fff;
	tg_h_lin_s = TG_H & 0x7fff;
	if (tg_w_pxl_e - tg_w_pxl_s < 32) {
		LOG_INF("tg width < 32, can't enable AF:0x%x\n", (tg_w_pxl_e - tg_w_pxl_s));
		rst = 2;  /* 2: check module en */
		goto AF_EXIT;
	}

	/* AFO and AF relaterd module enable check */
	if ((AFO_D_EN == 0) || (SGG1_EN == 0)) {
		LOG_INF("AF is enabled, MUST enable AFO/SGG1:0x%x_0x%x\n",
		AFO_D_EN, SGG1_EN);
		rst = 2; /* 2: check module en */
		goto AF_EXIT;
	}

	/*  */
	af_v_avg_lvl = (cam_af_con >> 20) & 0x3;
	af_v_gonly = (cam_af_con >> 17) & 0x1;
	dbl_data_bus = (cam_tg_sen_mode >> 1) & 0x1;
	bmx_end_x = (cam_bmx_crop >> 16) & 0x1fff;
	bmx_str_x = cam_bmx_crop & 0x1fff;
	rmx_end_x = (cam_rmx_crop >> 16) & 0x1fff;
	rmx_str_x = cam_rmx_crop & 0x1fff;
	/* AF image wd */
	switch (sgg_sel) {
	case 0:
		h_size = tg_w_pxl_e - tg_w_pxl_s;
		v_size = tg_h_lin_e - tg_h_lin_s;
		break;
	case 1:
		h_size = bmx_end_x - bmx_str_x + 1;
		v_size = tg_h_lin_e - tg_h_lin_s;
		break;
	case 2:
		h_size = rmx_end_x - rmx_str_x + 1;
		v_size = tg_h_lin_e - tg_h_lin_s;
		break;
	default:
		LOG_INF("unsupported sgg_sel:0x%x\n", sgg_sel);
		return MFALSE;
	}
	af_image_wd = cam_af_size & 0x3fff;
	if (h_size != af_image_wd) {
		LOG_INF("AF input size mismatch:0x%x_0x%x\n", af_image_wd, h_size);
		rst = 3; /* 3: check image window */
		goto AF_EXIT;
	}

	/* ofset */
	af_vld_ystart = (cam_af_vld >> 16) & 0x3fff;
	af_vld_xstart = cam_af_vld & 0x3fff;
	if ((af_vld_xstart&0x1) || (af_vld_ystart&0x1)) {
		LOG_INF("AF vld start must be even:0x%x_0x%x\n", af_vld_xstart, af_vld_ystart);
		rst = 4; /* 4: check alignment */
		goto AF_EXIT;
	}

	/* window num */
	af_blk_xnum = cam_af_blk_1 & 0x1ff;
	af_blk_ynum = (cam_af_blk_1 >> 16) & 0x1ff;
	/* win_num_x = CAM_READ_BITS(this->m_pDrv->getPhyObj(),CAM_AF_BLK_1,AF_BLK_XNUM); */
	/* win_num_y = CAM_READ_BITS(this->m_pDrv->getPhyObj(),CAM_AF_BLK_1,AF_BLK_YNUM); */
	if ((af_blk_xnum == 0) || (af_blk_xnum > 128)) {
		rst = 3; /* 3: check image window */
		LOG_INF("AF af_blk_xnum :0x%x[1~128]\n", af_blk_xnum);
		goto AF_EXIT;
	}
	if ((af_blk_ynum == 0) || (af_blk_ynum > 128)) {
		rst = 3; /* 3: check image window */
		LOG_INF("AF af_blk_ynum :0x%x[1~128]\n", af_blk_ynum);
		goto AF_EXIT;
	}

	/* win size */
	af_blk_xsize = cam_af_blk_0 & 0xff;
	af_blk_ysize = (cam_af_blk_0 >> 16) & 0xff;
	/* max */
	if (af_blk_xsize > 254) {
		rst = 3; /* 3: check image window */
		LOG_INF("af max h win size:254 cur:0x%x\n", af_blk_xsize);
		goto AF_EXIT;
	}
	/* min constraint */
	if ((af_v_avg_lvl == 3) && (af_v_gonly == 1))
		tmp = 32;
	else if ((af_v_avg_lvl == 3) && (af_v_gonly == 0))
		tmp = 16;
	else if ((af_v_avg_lvl == 2) && (af_v_gonly == 1))
		tmp = 16;
	else
		tmp = 8;
	if (af_blk_xsize < tmp) {
		LOG_INF("af min h win size::0x%x cur:0x%x [0x%x_0x%x]\n", tmp, af_blk_xsize, af_v_avg_lvl, af_v_gonly);
		rst = 3; /* 3: check image window */
		goto AF_EXIT;
	}

	if (af_v_gonly == 1) {
		if (af_blk_xsize & 0x3) {
			LOG_INF("af min h win size must 4 alighment:0x%x\n", af_blk_xsize);
			rst = 4; /* 4: check alignment */
			goto AF_EXIT;
		}
	} else {
		if (af_blk_xsize & 0x1) {
			LOG_INF("af min h win size must 2 alighment:0x%x\n", af_blk_xsize);
			rst = 4; /* 4: check alignment */
			goto AF_EXIT;
		}
	}

	if (af_blk_ysize > 255) {
		rst = 3; /* 3: check image window */
		LOG_INF("af max v win size:255 cur:0x%x\n", af_blk_ysize);
		goto AF_EXIT;
	}
	/* min constraint */
	if (af_blk_xsize < 1) {
		LOG_INF("af min v win size:1, cur:0x%x\n", af_blk_xsize);
		rst = 3; /* 3: check image window */
		goto AF_EXIT;
	}

	af_ext_stat_en = (cam_af_con >> 22) & 0x1;
	af_blk_sz = ((af_ext_stat_en == MTRUE)?32:16);
	af_h_gonly = (cam_af_con >> 16) & 0x1;
	if (af_ext_stat_en == 1) {
		if (af_blk_xsize < 8) {
			LOG_INF("AF_EXT_STAT_EN=1, af min h win size::8 cur:0x%x\n", af_blk_xsize);
			rst = 3; /* 3: check image window */
			goto AF_EXIT;
		}
		if ((SGG5_EN == 0) || (af_h_gonly != 0)) {
			LOG_INF("AF_EXT_STAT_EN=1, MUST enable sgg5 & disable AF_H_GONLY:0x%x_0x%x\n",
				SGG5_EN, af_h_gonly);
			rst = 2; /* 2: check module en */
			goto AF_EXIT;
		}
	} else {
		if (SGG5_EN == 1) {
			LOG_INF("AF_EXT_STAT_EN=0, sgg5 must be disabled:0x%x\n", SGG5_EN);
			rst = 2; /* 2: check module en */
			goto AF_EXIT;
		}
	}

	/* check max afo size, 128*128*af_blk_sz */
	afo_d_xsize = afo_d_xsize & 0x3fff;
	afo_d_ysize = afo_d_ysize & 0x1fff;
	if (afo_d_xsize * afo_d_ysize > 128*128*af_blk_sz) {
		rst = 5; /* 5: check AFO size */
		LOG_INF("afo max size out of range:0x%x_0x%x\n", afo_d_xsize * afo_d_ysize, 128*128*af_blk_sz);
		goto AF_EXIT;
	}

	/* xsize/ysize */
	xsize = af_blk_xnum*af_blk_sz;
	if (afo_d_xsize != (xsize - 1)) {
		LOG_INF("afo xsize mismatch:0x%x_0x%x\n", afo_d_xsize, (xsize - 1));
		rst = 1; /* 1: check driver */
		goto AF_EXIT;
	}
	ysize = af_blk_ynum;
	if (afo_d_ysize != (ysize - 1)) {
		LOG_INF("afo ysize mismatch:0x%x_0x%x\n", afo_d_ysize, (ysize - 1));
		rst = 1; /* 1: check driver */
		goto AF_EXIT;
	}

	if ((af_vld_xstart + af_blk_xsize*af_blk_xnum) > h_size) {
		rst = 3; /* 3: check image window */
		LOG_INF("af h window out of range:0x%x_0x%x\n", (af_vld_xstart + af_blk_xsize*af_blk_xnum), h_size);
		goto AF_EXIT;
	}
	if ((af_vld_ystart + af_blk_ysize*af_blk_ynum) > v_size) {
		rst = 3; /* 3: check image window */
		LOG_INF("af v window out of range:0x%x_0x%x\n", (af_vld_ystart + af_blk_ysize*af_blk_ynum), v_size);
		goto AF_EXIT;
	}

	/* AF_TH */
	af_sat_th0 = cam_af_th_2 & 0xff;
	af_sat_th1 = (cam_af_th_2 >> 8) & 0xff;
	af_sat_th2 = (cam_af_th_2 >> 16) & 0xff;
	af_sat_th3 = (cam_af_th_2 >> 24) & 0xff;
	if ((af_sat_th0 > af_sat_th1) || (af_sat_th1 > af_sat_th2) || (af_sat_th2 > af_sat_th3)) {
		LOG_INF("af sat th, MUST th3 >= th2 >= th1 >= th0:0x%x_0x%x_0x%x_0x%x\n",
			af_sat_th3, af_sat_th2, af_sat_th1, af_sat_th0);
		rst = 6; /* 6: check TH */
		goto AF_EXIT;
	}

AF_EXIT:
	/* 0: PASS, 1:  check driver, 2: check module en, 3: check image window
	 * 4: check alignment, 5: check AFO size, 6: check TH
	*/
	if (rst == 0)
		LOG_INF("af check pass\n");
	else if (rst == 2)
		camera_isp_aee("AF data error", "Error: AF module enable is over HW constraint.");
	else if (rst == 3)
		camera_isp_aee("AF data error", "Error: AF image window is over HW constraint.");
	else if (rst == 4)
		camera_isp_aee("AF data error", "Error: AF alignment is over HW constraint.");
	else if (rst == 5)
		camera_isp_aee("AF data error", "Error: AFO size is over HW constraint.");
	else if (rst == 6)
		camera_isp_aee("AF data error", "Error: AF TH is over HW constraint.");
	else
		LOG_INF("af check fail: check driver\n");


		/*First Check AWB setting */
		rst = 0;
		cam_awb_win_num = ISP_RD32(ISP_ADDR + 0x5BC);
		cam_ae_hst_ctl = ISP_RD32(ISP_ADDR + 0x650);
		cam_aao_xsize = ISP_RD32(ISP_ADDR + 0x3390);
		cam_aao_ysize = ISP_RD32(ISP_ADDR + 0x3394);

		AAO_XSIZE = cam_aao_xsize & 0x1ffff;
		AWB_W_HNUM = cam_awb_win_num & 0xff;
		AWB_W_VNUM = (cam_awb_win_num >> 16) & 0xff;

		cam_awb_win_num = ISP_RD32(ISP_ADDR + 0x5BC);
		cam_awb_win_pit = ISP_RD32(ISP_ADDR + 0x5B8);
		cam_awb_win_org = ISP_RD32(ISP_ADDR + 0x5B0);

		if (hbin2_en == MTRUE) {
			/*hbin_enable should be true under Twin mode. */
			AAO_InWidth = (bmx_width + bmx_d_width) / 2;
			AAO_InHeight = bmx_height;
		} else {
			AAO_InWidth = grab_width;
			AAO_InHeight = grab_height;
		}
		if ((AAO_InWidth == 0) || (AAO_InHeight == 0)) {
			LOG_INF("Error HwRWCtrl: AAO input size == 0!!");
			rst = 1; /* 1: check driver */
			goto AA_EXIT;
		}
		AWB_W_HNUM = (cam_awb_win_num & 0xff);
		AWB_W_VNUM = ((cam_awb_win_num >> 16) & 0xff);

		AWB_W_HPIT = (cam_awb_win_pit & 0x1fff);
		AWB_W_VPIT = ((cam_awb_win_pit >> 16) & 0x1fff);

		AWB_W_HORG = (cam_awb_win_org & 0x1fff);
		AWB_W_VORG = ((cam_awb_win_org >> 16) & 0x1fff);
		if (AAO_InWidth < (AWB_W_HNUM * AWB_W_HPIT + AWB_W_HORG)) {
			/*Error */
			LOG_INF
			("Error HwRWCtrl:: bmx_enable(%d), bmx_width(%d),",
				bmx_enable, bmx_width);
			LOG_INF
			(" bmx_height(%d), grab_width(%d), grab_height(%d)!!",
				bmx_height, grab_width, grab_height);
			LOG_INF
			("Error HwRWCtrl:: input frame width(%d) >= AWB_W_HNUM(%d)",
				 AAO_InWidth, AWB_W_HNUM);
			LOG_INF
			(" * AWB_W_HPIT(%d) + AWB_W_HORG(%d) !!",
				 AWB_W_HPIT, AWB_W_HORG);
			rst = 2; /* 2: check AWB size */
			goto AA_EXIT;
		}
		if (AAO_InHeight < (AWB_W_VNUM * AWB_W_VPIT + AWB_W_VORG)) {
			/*Error */
			LOG_INF
			("Error HwRWCtrl:: bmx_enable(%d), bmx_width(%d),",
				bmx_enable, bmx_width);
			LOG_INF
			(" bmx_height(%d), grab_width(%d), grab_height(%d)!!",
				bmx_height, grab_width, grab_height);
			LOG_INF
			("Error HwRWCtrl:: input frame height(%d) >=",
				AAO_InHeight);
			LOG_INF
			(" AWB_W_VNUM(%d)	* AWB_W_VPIT(%d) + AWB_W_VORG(%d) !!",
				AWB_W_VNUM, AWB_W_VPIT, AWB_W_VORG);
			rst = 2; /* 2: check AWB size */
			goto AA_EXIT;
		}

		/* Then, check AE setting */

		histogramen_num = 0;
		for (i = 0; i < 4; i++) {
			if ((cam_ae_hst_ctl >> i) & 0x1)
				histogramen_num += 1;
		}

		if ((cam_aao_ysize + 1) != 1) {
			LOG_INF("Error HwRWCtrl::AAO_YSIZE(%d) must be equal 1 !!",
				cam_aao_ysize);
			rst = 3;
			goto AA_EXIT;
		}
		if ((AAO_XSIZE + 1) !=
			(AWB_W_HNUM * AWB_W_VNUM * 5 + (histogramen_num << 8))) {
			LOG_INF
			("Error HwRWCtrl::AAO_XSIZE(%d) = AWB_W_HNUM(%d)*AWB_W_VNUM(%d)*5 +",
				AAO_XSIZE, AWB_W_HNUM, AWB_W_VNUM);
			LOG_INF
			(" (how many histogram enable(%d)(AE_HST0/1/2/3_EN))*2*128 !!",
				histogramen_num);
			rst = 3;
			goto AA_EXIT;
		}
AA_EXIT:
	/* 0: PASS, 1:  check driver, 2: check AWB size, 3: check AE size */
	if (rst == 0)
		LOG_INF("aa check pass\n");
	else if (rst == 2)
		camera_isp_aee("AWB data error", "Error: AWB size is over HW constraint");
	else if (rst == 3)
		camera_isp_aee("AE data error", "Error: AAO size is over HW constraint");
	else
		LOG_INF("aa check fail, check driver\n");

		/*Check EIS Setting */
#if 0
		unsigned int rrz_out_width;
		unsigned int rrz_out_height;
		unsigned int scenario;

		unsigned int cam_rrz_out_img;        /*47A8 */
		unsigned int cam_ctl_scenario;       /*4024 */

		unsigned int EIS_RP_VOFST;
		unsigned int EIS_RP_HOFST;
		unsigned int EIS_WIN_VSIZE;
		unsigned int EIS_WIN_HSIZE;

		unsigned int EIS_OP_HORI;
		unsigned int EIS_OP_VERT;

		unsigned int EIS_NUM_HRP;
		unsigned int EIS_NUM_VRP;
		unsigned int EIS_NUM_HWIN;
		unsigned int EIS_NUM_VWIN;

		unsigned int EIS_IMG_WIDTH;
		unsigned int EIS_IMG_HEIGHT;
		unsigned int EISO_XSIZE;

		bool bError = MFALSE;
#else
		bError = MFALSE;
#endif
		cam_ctl_scenario = ISP_RD32(ISP_ADDR + 0x24);
		scenario = cam_ctl_scenario & 0x7;

		cam_rrz_out_img = ISP_RD32(ISP_ADDR + 0x7A8);
		cam_rrz_d_out_img = ISP_RD32(ISP_ADDR + 0x27A8);
		rrz_out_width = cam_rrz_out_img & 0xffff;
		rrz_out_height = (cam_rrz_out_img >> 16) & 0xffff;
		rrz_d_out_width = cam_rrz_d_out_img & 0xffff;
		rrz_d_out_height = (cam_rrz_d_out_img >> 16) & 0xffff;

		EISO_XSIZE = (ISP_RD32(ISP_ADDR + 0x3360)) & 0x3ff;

#if 0
		unsigned int CAM_EIS_PREP_ME_CTRL1;  /*4DC0 */
		unsigned int CAM_EIS_MB_OFFSET;      /*4DD0 */
		unsigned int CAM_EIS_MB_INTERVAL;    /*4DD4 */
		unsigned int CAM_EIS_IMAGE_CTRL;     /*4DE0 */
#endif
		CAM_EIS_PREP_ME_CTRL1 = ISP_RD32(ISP_ADDR + 0xDC0);
		CAM_EIS_MB_OFFSET = ISP_RD32(ISP_ADDR + 0xDD0);
		CAM_EIS_MB_INTERVAL = ISP_RD32(ISP_ADDR + 0xDD4);
		CAM_EIS_IMAGE_CTRL = ISP_RD32(ISP_ADDR + 0xDE0);

		EIS_SUBG_EN = (CAM_EIS_PREP_ME_CTRL1 >> 6) & 0x1;
		EIS_RP_VOFST = (CAM_EIS_MB_OFFSET) & 0xfff;
		EIS_RP_HOFST = (CAM_EIS_MB_OFFSET >> 16) & 0xfff;
		EIS_WIN_VSIZE = (CAM_EIS_MB_INTERVAL) & 0xfff;
		EIS_WIN_HSIZE = (CAM_EIS_MB_INTERVAL >> 16) & 0xfff;

		EIS_OP_HORI = CAM_EIS_PREP_ME_CTRL1 & 0x7;
		EIS_OP_VERT = (CAM_EIS_PREP_ME_CTRL1 >> 3) & 0x7;

		EIS_NUM_HRP = (CAM_EIS_PREP_ME_CTRL1 >> 8) & 0x1f;
		EIS_NUM_VRP = (CAM_EIS_PREP_ME_CTRL1 >> 21) & 0xf;
		EIS_NUM_HWIN = (CAM_EIS_PREP_ME_CTRL1 >> 25) & 0x7;
		EIS_NUM_VWIN = (CAM_EIS_PREP_ME_CTRL1 >> 28) & 0xf;

		EIS_IMG_WIDTH = (CAM_EIS_IMAGE_CTRL >> 16) & 0x1fff;
		EIS_IMG_HEIGHT = CAM_EIS_IMAGE_CTRL & 0x1fff;
		hbin2_en = (cam_ctrl_en_p1 >> 18) & 0x01;

		if (EISO_XSIZE != 255)
			LOG_INF("EIS Error, EISO_XISZE must be 255 !!!");

		/*1. The max horizontal window size is 4 */
		/*2. The max vertical window size is 8 */
		/*3. EIS_MF_OFFSET.EIS_RP_VOFST/EIS_RP_HOFST  > 16 */
		/*6. EIS_PREP_ME_CTRL1.EIS_NUM_HRP <= 16 */
		/*7. EIS_PREP_ME_CTRL1.EIS_NUM_VRP <= 8 */
		if ((EIS_NUM_VWIN > 8) || (EIS_NUM_HWIN > 4) ||
		    (EIS_NUM_VRP > 8) || (EIS_NUM_HRP > 16) || (EIS_RP_VOFST < 16)
		    || (EIS_RP_HOFST <= 16)) {
			/*Error */
			LOG_INF
			("EIS Error, 1. The max horizontal window size is 4, EIS_NUM_HWIN(%d)!!",
				EIS_NUM_HWIN);
			LOG_INF
			("EIS Error, 2. The max vertical window size is 8, EIS_NUM_VWIN(%d)!!",
				EIS_NUM_VWIN);
			LOG_INF
			("EIS Error, 3. EIS_MF_OFFSET.EIS_RP_VOFST or EIS_RP_HOFST  > 16!!,");
			LOG_INF
			(" EIS_RP_VOFST(%d), EIS_RP_HOFST(%d)",
				 EIS_RP_VOFST, EIS_RP_HOFST);
			LOG_INF
			("EIS Error, 6. EIS_PREP_ME_CTRL1.EIS_NUM_HRP <= 16, EIS_NUM_HRP(%d)!!",
				EIS_NUM_HRP);
			LOG_INF
			("EIS Error, 7. EIS_PREP_ME_CTRL1.EIS_NUM_VRP <= 8, EIS_NUM_VRP(%d)!!",
				EIS_NUM_VRP);
		}
		/* It's special changing HW constraint limitation for EIS */
		/*8. EIS_MB_INTERVAL.EIS_WIN_HSIZE >= (EIS_PREP_ME_CTRL1.EIS_NUM_HRP+1)*16+1 */
		/*9. EIS_MB_INTERVAL.EIS_WIN_VSIZE >= (EIS_PREP_ME_CTRL1.EIS_NUM_VRP+1)*16+1 */
		if ((EIS_WIN_HSIZE < (((EIS_NUM_HRP + 1) << 4) + 1)) ||
		    (EIS_WIN_VSIZE < (((EIS_NUM_VRP + 1) << 4) + 1))) {
			/*Error */
			LOG_INF
			("EIS Error, 8. EIS_MB_INTERVAL.EIS_WIN_HSIZE >= (EIS_PREP_ME_CTRL1.EIS_NUM_HRP+1)*");
			LOG_INF
			("16+1!!, EIS_WIN_HSIZE:%d, EIS_NUM_HRP:%d",
				EIS_WIN_HSIZE, EIS_NUM_HRP);
			LOG_INF
			("EIS Error, 9. EIS_MB_INTERVAL.EIS_WIN_VSIZE >= (EIS_PREP_ME_CTRL1.EIS_NUM_VRP+1)*");
			LOG_INF
			("16+1!!, EIS_WIN_VSIZE:%d, EIS_NUM_VRP:%d",
				EIS_WIN_VSIZE, EIS_NUM_VRP);
		}
		/*10. (EIS_MB_OFFSET.EIS_RP_HOFST + ((EIS_MB_INTERVAL.EIS_WIN_HSIZE-1)* */
		/*	EIS_PREP_ME_CTRL1.EIS_NUM_HWIN)+EIS_PREP_ME_CTRL1.EIS_NUM_HRP*16)* */
		/*	EIS_PREP_ME_CTRL1.EIS_OP_HORI < EIS_IMAGE_CTRL.WIDTH */
		/*10.( EIS_MB_OFFSET.EIS_RP_VOFST + ((EIS_MB_INTERVAL.EIS_WIN_VSIZE-1)* */
		/*	EIS_PREP_ME_CTRL1.EIS_NUM_VWIN)+EIS_PREP_ME_CTRL1.EIS_NUM_VRP*16)* */
		/*	EIS_PREP_ME_CTRL1.EIS_OP_VERT < EIS_IMAGE_CTRL.HEIGHT */
		if ((((EIS_RP_HOFST + ((EIS_WIN_HSIZE - 1) * (EIS_NUM_HWIN - 1)) +
		       (EIS_NUM_HRP << 4)) * EIS_OP_HORI) >= EIS_IMG_WIDTH)
		    ||
		    (((EIS_RP_VOFST + ((EIS_WIN_VSIZE - 1) * (EIS_NUM_VWIN - 1)) +
		       (EIS_NUM_VRP << 4)) * EIS_OP_VERT) >= EIS_IMG_HEIGHT)) {
			/*Error */
			LOG_INF
			("EIS Error, 10. (EIS_MB_OFFSET.EIS_RP_HOFST(%d) + ((EIS_MB_INTERVAL.EIS_WIN_HSIZE(%d)-1)*",
				EIS_RP_HOFST, EIS_WIN_HSIZE);
			LOG_INF
			("(EIS_PREP_ME_CTRL1.EIS_NUM_HWIN(%d)-1))+EIS_PREP_ME_CTRL1.EIS_NUM_HRP(%d)*16)*",
				EIS_NUM_HWIN, EIS_NUM_HRP);
			LOG_INF
			("EIS_PREP_ME_CTRL1.EIS_OP_HORI(%d) < EIS_IMAGE_CTRL.WIDTH(%d)!!",
				EIS_OP_HORI, EIS_IMG_WIDTH);
			LOG_INF
			("EIS Error, 10. (EIS_MB_OFFSET.EIS_RP_VOFST(%d) + ((EIS_MB_INTERVAL.EIS_WIN_VSIZE(%d)-1)*",
				EIS_RP_VOFST, EIS_WIN_VSIZE);
			LOG_INF
			("(EIS_PREP_ME_CTRL1.EIS_NUM_VWIN(%d)-1))+EIS_PREP_ME_CTRL1.EIS_NUM_VRP(%d)*16)*",
				EIS_NUM_VWIN, EIS_NUM_VRP);
			LOG_INF
			("EIS_PREP_ME_CTRL1.EIS_OP_VERT(%d) < EIS_IMG_HEIGHT.WIDTH(%d)!!",
				EIS_OP_VERT, EIS_IMG_HEIGHT);
		}

		/*11. EISO_XISZE = 255 (after 82, EISO_XISZE = 407 in 89) */
		/*4. EIS_IMAGE_CTRL.WIDTH = EIS input image width but if */
		/*   (two_pix mode) EIS_IMAGE_CTRL.WIDTH = input image width/2 */
		/*5. EIS_IMAGE_CTRL.HEIGHT = EIS input image height */
		switch (eis_sel) {
		case 0:
			if ((scenario == 1) && (sgg_sel == 0x0)) {
				if ((grab_width != EIS_IMG_WIDTH)
				    || (grab_height != EIS_IMG_HEIGHT)) {
					bError = MTRUE;
				}
				/*Error */
			} else {
				/*Error in non-yuv sensor */
				LOG_INF("EIS Error, Non-Yuv Sensor!!");
			}
			break;
		case 1:
			if (hbin2_en == MTRUE) {
				/*bmx_width */
				if (((bmx_width + bmx_d_width)/2  != EIS_IMG_WIDTH)
				    || (bmx_height != EIS_IMG_HEIGHT)) {
					bError = MTRUE;
				}
				/*Error */
			} else {
				if (((bmx_width + bmx_d_width) != EIS_IMG_WIDTH)
				    || (grab_height != EIS_IMG_HEIGHT)) {
					bError = MTRUE;
				}
				/*Error */
			}
			break;
		case 2:
			if (bmx_enable == MTRUE) {
				if (((EIS_SUBG_EN == 1) && (EIS_IMG_WIDTH != (rrz_out_width + rrz_d_out_width) / 2)) ||
				((EIS_SUBG_EN == 0) && (EIS_IMG_WIDTH != (rrz_out_width + rrz_d_out_width))) ||
				    (rrz_out_height != EIS_IMG_HEIGHT)) {
					bError = MTRUE;
				}
				/*Error */

			} else {
				if ((rrz_out_width != EIS_IMG_WIDTH)
				    || (rrz_out_height != EIS_IMG_HEIGHT)) {
					bError = MTRUE;
				}
				/*Error */
			}
			break;
		default:
			/*Error */
			break;
		}
		if (bError == MTRUE) {
			LOG_INF
			("EIS Error, 4. EIS_IMAGE_CTRL.WIDTH = EIS input image width but if");
			LOG_INF
			("(two_pix mode) EIS_IMAGE_CTRL.WIDTH = input image width/2!!\n");
			LOG_INF
			("EIS Error, 5. EIS_IMAGE_CTRL.HEIGHT != EIS input image height!!\n");
			LOG_INF("eis_sel:%d, scenario:%d, sgg_sel:%d, twin_mode:%d", eis_sel,
				scenario, sgg_sel, bmx_enable);
			LOG_INF
			("EIS_IMG_WIDTH:%d, EIS_IMG_HEIGHT:%d, rrz_out_width:%d, rrz_out_height:%d, hbin2_en:%d",
			 EIS_IMG_WIDTH, EIS_IMG_HEIGHT, rrz_out_width, rrz_out_height, hbin2_en);
			LOG_INF("grab_width:%d, grab_height:%d, bmx_width:%d, bmx_height:%d",
				grab_width, grab_height, bmx_width, bmx_height);
		}


	}
#if (ISP_RAW_D_SUPPORT == 1)
	if (cam_tg2_vf_con & 0x01) {
		unsigned int cam_af_con;     /*46B0 */
		unsigned int cam_af_winx_1;  /*46B4 */
		unsigned int cam_af_winx_2;  /*46B8 */
		unsigned int cam_af_winx_3;  /*46BC */
		unsigned int cam_af_winy_1;  /*46C0 */
		unsigned int cam_af_winy_2;  /*46C4 */
		unsigned int cam_af_winy_3;  /*46C8 */
		unsigned int cam_af_size;    /*46CC */
		unsigned int cam_af_flt_1;   /*46D4 */
		unsigned int cam_af_flt_2;   /*46D8 */
		unsigned int cam_af_flt_3;   /*46DC */
		unsigned int cam_af_th;      /*46E0 */
		unsigned int cam_af_flo_win_1;       /*46E4 */
		unsigned int cam_af_flo_size_1;      /*46E8 */
		unsigned int cam_af_flo_win_2;       /*46EC */
		unsigned int cam_af_flo_size_2;      /*46F0 */
		unsigned int cam_af_flo_win_3;       /*46F4 */
		unsigned int cam_af_flo_size_3;      /*46F8 */
		unsigned int cam_af_flo_th;  /*46FC */
		unsigned int cam_af_image_size;      /*4700 */
		unsigned int AF_WIN_WD;
		unsigned int AF_WIN_HT;
		unsigned int AF_WINX_0;
		unsigned int AF_WINX_1;
		unsigned int AF_WINX_2;
		unsigned int AF_WINX_3;
		unsigned int AF_WINX_4;
		unsigned int AF_WINX_5;
		/*              unsigned int AF_WINX_6;*/

		unsigned int AF_WINY_0;
		unsigned int AF_WINY_1;
		unsigned int AF_WINY_2;
		unsigned int AF_WINY_3;
		unsigned int AF_WINY_4;
		unsigned int AF_WINY_5;
		/*              unsigned int AF_WINY_6;*/

		unsigned int AF_FLO_WINX_1;
		unsigned int AF_FLO_WINY_1;
		unsigned int AF_FLO_WINX_2;
		unsigned int AF_FLO_WINY_2;
		unsigned int AF_FLO_WINX_3;
		unsigned int AF_FLO_WINY_3;

		unsigned int AF_FLO_WD_1;
		unsigned int AF_FLO_HT_1;
		unsigned int AF_FLO_WD_2;
		unsigned int AF_FLO_HT_2;
		unsigned int AF_FLO_WD_3;
		unsigned int AF_FLO_HT_3;

		unsigned int AF_IMAGE_WD;
		unsigned int AF_DECI;
		unsigned int AF_InputWidth;

		unsigned int AF_LeftOverWidth;

		unsigned int cam_aao_xsize;  /*7554 */
		unsigned int cam_aao_ysize;  /*7558 */
		unsigned int cam_awb_win_num;        /*65BC */
		unsigned int cam_ae_hst_ctl; /*6650 */

		unsigned int AAO_XSIZE;
		unsigned int AWB_W_HNUM;
		unsigned int AWB_W_VNUM;
		unsigned int histogramen_num;

		unsigned int cam_awb_win_org;        /*65B0 */
		unsigned int cam_awb_win_pit;        /*65B8 */

		unsigned int AAO_InWidth;
		unsigned int AAO_InHeight;
		unsigned int AWB_W_HPIT;
		unsigned int AWB_W_VPIT;
		unsigned int AWB_W_HORG;
		unsigned int AWB_W_VORG;
		/*Check ISP_D AF_D. */
		LOG_INF("ISP chk TG2");

		grab_width =
			((cam_tg2_sen_grab_pxl >> 16) & 0x7fff) - (cam_tg2_sen_grab_pxl & 0x7fff);
		grab_height =
			((cam_tg2_sen_grab_lin >> 16) & 0x1fff) - (cam_tg2_sen_grab_lin & 0x1fff);

		cam_af_con = ISP_RD32(ISP_ADDR + 0x6B0);
		cam_af_winx_1 = ISP_RD32(ISP_ADDR + 0x6B4);
		cam_af_winx_2 = ISP_RD32(ISP_ADDR + 0x6B8);
		cam_af_winx_3 = ISP_RD32(ISP_ADDR + 0x6BC);
		cam_af_winy_1 = ISP_RD32(ISP_ADDR + 0x6C0);
		cam_af_winy_2 = ISP_RD32(ISP_ADDR + 0x6C4);
		cam_af_winy_3 = ISP_RD32(ISP_ADDR + 0x6C8);
		cam_af_size = ISP_RD32(ISP_ADDR + 0x6CC);

		cam_af_flt_1 = ISP_RD32(ISP_ADDR + 0x26D4);     /*66D4 */
		cam_af_flt_2 = ISP_RD32(ISP_ADDR + 0x26D8);     /*66D8 */
		cam_af_flt_3 = ISP_RD32(ISP_ADDR + 0x26DC);     /*66DC */
		cam_af_th = ISP_RD32(ISP_ADDR + 0x26E0);        /*66E0 */
		cam_af_flo_win_1 = ISP_RD32(ISP_ADDR + 0x26E4); /*66E4 */
		cam_af_flo_size_1 = ISP_RD32(ISP_ADDR + 0x26E8);        /*66E8 */
		cam_af_flo_win_2 = ISP_RD32(ISP_ADDR + 0x26EC); /*66EC */
		cam_af_flo_size_2 = ISP_RD32(ISP_ADDR + 0x26F0);        /*66F0 */
		cam_af_flo_win_3 = ISP_RD32(ISP_ADDR + 0x26F4); /*66F4 */
		cam_af_flo_size_3 = ISP_RD32(ISP_ADDR + 0x26F8);        /*66F8 */
		cam_af_flo_th = ISP_RD32(ISP_ADDR + 0x26FC);    /*66FC */
		cam_af_image_size = ISP_RD32(ISP_ADDR + 0x2700);        /*6700 */

		AF_WINX_0 = (cam_af_winx_1 & 0x1fff);
		AF_WINX_1 = ((cam_af_winx_1 >> 16) & 0x1fff);
		AF_WINX_2 = (cam_af_winx_2 & 0x1fff);
		AF_WINX_3 = ((cam_af_winx_2 >> 16) & 0x1fff);
		AF_WINX_4 = (cam_af_winx_3 & 0x1fff);
		AF_WINX_5 = ((cam_af_winx_3 >> 16) & 0x1fff);

		AF_WINY_0 = (cam_af_winy_1 & 0x1fff);
		AF_WINY_1 = ((cam_af_winy_1 >> 16) & 0x1fff);
		AF_WINY_2 = (cam_af_winy_2 & 0x1fff);
		AF_WINY_3 = ((cam_af_winy_2 >> 16) & 0x1fff);
		AF_WINY_4 = (cam_af_winy_3 & 0x1fff);
		AF_WINY_5 = ((cam_af_winy_3 >> 16) & 0x1fff);

		AF_FLO_WINX_1 = (cam_af_flo_win_1 & 0x1fff);
		AF_FLO_WINY_1 = ((cam_af_flo_win_1 >> 16) & 0x1fff);
		AF_FLO_WINX_2 = (cam_af_flo_win_2 & 0x1fff);
		AF_FLO_WINY_2 = ((cam_af_flo_win_2 >> 16) & 0x1fff);
		AF_FLO_WINX_3 = (cam_af_flo_win_3 & 0x1fff);
		AF_FLO_WINY_3 = ((cam_af_flo_win_3 >> 16) & 0x1fff);

		AF_FLO_WD_1 = (cam_af_flo_size_1 & 0xfff);
		AF_FLO_HT_1 = ((cam_af_flo_size_1 >> 16) & 0xfff);
		AF_FLO_WD_2 = (cam_af_flo_size_2 & 0xfff);
		AF_FLO_HT_2 = ((cam_af_flo_size_2 >> 16) & 0xfff);
		AF_FLO_WD_3 = (cam_af_flo_size_3 & 0xfff);
		AF_FLO_HT_3 = ((cam_af_flo_size_3 >> 16) & 0xfff);

		AF_IMAGE_WD = cam_af_image_size & 0x1fff;

		AF_DECI = cam_af_con & 0x03;

		/*1. The min horizontal window size is 8. (AF_WIN_WD/AF_FLO_WD_x)  */
		/*2. The min vertical window size is 4. (AF_WIN_HT/AF_FLO_HT_x) */
		AF_WIN_WD = (cam_af_size & 0x7ff);
		AF_WIN_HT = ((cam_af_size >> 16) & 0x7ff);
		if ((AF_WIN_WD < 8) || (AF_FLO_WD_1 < 8) || (AF_FLO_WD_2 < 8) || (AF_FLO_WD_3 < 8)
		    || (AF_WIN_HT < 4) || (AF_FLO_HT_1 < 4) || (AF_FLO_HT_2 < 4)
		    || (AF_FLO_HT_3 < 4)) {
			LOG_INF
			("HwRWCtrl:: AF_D Error: The min horizontal window size is 8.");
			LOG_INF
			("(AF_WIN_WD(%d), AF_FLO_WD_1(%d), AF_FLO_WD_2(%d), AF_FLO_WD_3(%d))!!",
				AF_WIN_WD, AF_FLO_WD_1, AF_FLO_WD_2, AF_FLO_WD_3);
			LOG_INF
			("HwRWCtrl:: AF_D Error: The min vertical window size is 4.");
			LOG_INF
			("(AF_WIN_HT(%d), AF_FLO_HT_1(%d), AF_FLO_HT_2(%d), AF_FLO_HT_3(%d))!!",
				AF_WIN_HT, AF_FLO_HT_1, AF_FLO_HT_2, AF_FLO_HT_3);
		}
		/*3. The horizontal window start point and size must be multiples of 2*/
		/*	(AF_WINX_x/AF_FLO_WINX_x/AF_WIN_WD/AF_FLO_WD_x) */
		if (((AF_WINX_0 % 2) != 0) || ((AF_WINX_1 % 2) != 0) || ((AF_WINX_2 % 2) != 0)
		    || ((AF_WINX_3 % 2) != 0) || ((AF_WINX_4 % 2) != 0) || ((AF_WINX_5 % 2) != 0)
		    || ((AF_WINY_0 % 2) != 0) || ((AF_WINY_1 % 2) != 0) || ((AF_WINY_2 % 2) != 0)
		    || ((AF_WINY_3 % 2) != 0) || ((AF_WINY_4 % 2) != 0) || ((AF_WINY_5 % 2) != 0)) {
			LOG_INF
			("HwRWCtrl:: AF_D Error: The horizontal window start point and size must be multiples of 2 !!");
			LOG_INF
			("HwRWCtrl:: AF_D Error: AF_WINX_0(%d)/AF_WINX_1(%d)/AF_WINX_2(%d)/",
				AF_WINX_0, AF_WINX_1, AF_WINX_2);
			LOG_INF
			("AF_WINX_3(%d)/AF_WINX_4(%d)/AF_WINX_5(%d)!!",
				AF_WINX_3, AF_WINX_4, AF_WINX_5);
			LOG_INF
			("HwRWCtrl:: AF_D Error: AF_WINY_0(%d)/AF_WINY_1(%d)/AF_WINY_2(%d)/",
				AF_WINY_0, AF_WINY_1, AF_WINY_2);
			LOG_INF
			("AF_WINY_3(%d)/AF_WINY_4(%d)/AF_WINY_5(%d)!!",
				AF_WINY_3, AF_WINY_4, AF_WINY_5);

		}
		/*10. Horizontal start position of floating window must be multiples of 2 */
		/*11. Horizontal size of floating window must be multiples of 2 */
		if (((AF_FLO_WINX_1 % 2) != 0) || ((AF_FLO_WINX_2 % 2) != 0)
		    || ((AF_FLO_WINX_3 % 2) != 0)
		    || ((AF_FLO_WINY_1 % 2) != 0) || ((AF_FLO_WINY_2 % 2) != 0)
		    || ((AF_FLO_WINY_3 % 2) != 0)
		    || ((AF_FLO_WD_1 % 2) != 0) || ((AF_FLO_WD_2 % 2) != 0)
		    || ((AF_FLO_WD_3 % 2) != 0)
		    || ((AF_FLO_HT_1 % 2) != 0) || ((AF_FLO_HT_2 % 2) != 0)
		    || ((AF_FLO_HT_3 % 2) != 0)) {
			LOG_INF
			("HwRWCtrl:: AF_D Error: Horizontal start position of floating window");
			LOG_INF
			(" must be multiples of 2!!");
			LOG_INF
			("HwRWCtrl:: AF_D Error: Horizontal size of floating window must be multiples of 2!!");
			LOG_INF
			("HwRWCtrl:: AF_D Error: AF_FLO_WINX_1(%d)/AF_FLO_WINX_2(%d)/AF_FLO_WINX_3(%d)!!",
			 AF_FLO_WINX_1, AF_FLO_WINX_2, AF_FLO_WINX_3);
			LOG_INF
			("HwRWCtrl:: AF_D Error: AF_FLO_WD_1(%d)/AF_FLO_WD_2(%d)/AF_FLO_WD_3(%d)!!",
			 AF_FLO_WD_1, AF_FLO_WD_2, AF_FLO_WD_3);
			LOG_INF
			("HwRWCtrl:: AF_D Error: AF_FLO_HT_1(%d)/AF_FLO_HT_2(%d)/AF_FLO_HT_3(%d)!!",
			 AF_FLO_HT_1, AF_FLO_HT_2, AF_FLO_HT_3);
		}
		/*5. Horizontal size of each normal window is limited to 510*2 (AF_WIN_WD) */
		/*6. Vertical size of each normal window is limited to 511*2 (AF_WIN_HT) */
		if ((AF_WIN_WD > 1020) || (AF_WIN_HT > 1022)) {
			LOG_INF
			("HwRWCtrl:: AF_D Error: Horizontal size of each normal window is");
			LOG_INF
			(" limited to 510*2 (AF_WIN_WD(%d))!!", AF_WIN_WD);
			LOG_INF
			("HwRWCtrl:: AF_D Error: Vertical size of each normal window is");
			LOG_INF
			(" limited to 511*2 (AF_WIN_HT(%d))!!", AF_WIN_HT);
		}

		/*12. Maximum floating widow size is 4094x4095 */
		if ((AF_FLO_WD_1 > 4094) || (AF_FLO_WD_2 > 4094) || (AF_FLO_WD_3 > 4094) ||
		    (AF_FLO_HT_1 > 4095) || (AF_FLO_HT_2 > 4095) || (AF_FLO_HT_3 > 4095))
			LOG_INF
			("HwRWCtrl:: AF_D Error: Maximum floating widow size is 4094x4095!!");

		if (bmx_enable == MTRUE) {
			if (sgg_sel == 1)
				AF_InputWidth = bmx_width;
			else
				AF_InputWidth = grab_width;
			/*18. If two_pixel mode, AF_DECI >= 1 */
			if (AF_DECI != 0)
				LOG_INF("HwRWCtrl:: AF_D Error: If two_pixel mode, AF_DECI >= 1!!");
			/*Error */
		} else {
			AF_InputWidth = grab_width;
		}

		/*15. AF_IMAGE_WD must be the same as input frame width */
		if (AF_InputWidth != AF_IMAGE_WD) {
			/*Error */
			LOG_INF
			("HwRWCtrl:: AF_D Error: bmx_enable(%d), sgg_sel(%d), bmx_width(%d), grab_width(%d)!!",
			 bmx_enable, sgg_sel, bmx_width, grab_width);
			LOG_INF
			("HwRWCtrl:: AF_D Error: AF_IMAGE_WD(%d) must be the same as input frame width(%d)!!",
			 AF_IMAGE_WD, AF_InputWidth);
		}
		/*16. "CAM_AF_WINX_x.AF_WINX_x + CAM_AF_SIZE.AF_WIN_WD <= input frame width" for a valid window */
		/*17. "CAM_AF_WINY_x.AF_WINY_x + CAM_AF_SIZE.AF_WIN_HT <= input frame height" for a valid window */

		/*14. If AF window is aligned to image left/right boundary, minimum horizontal size should be*/
		/*	larger than 24*(1<<AF_DECI_1) for non-zero 24-tap FIR statistics result */
		AF_LeftOverWidth = AF_IMAGE_WD - (AF_WINX_0 + 6 * AF_WIN_WD);
		/*if ( (AF_WINX_0 < (24*(1<<AF_DECI))) || ((AF_LeftOverWidth) < (24*(1<<AF_DECI)))) */
		if ((AF_WINX_0 < 16) || (AF_LeftOverWidth < 16)) {
			if (AF_WIN_WD < (24 * (1 << AF_DECI))) {
				LOG_INF
				("HwRWCtrl:: AF_D Error: minimum horizontal size should be larger than");
				LOG_INF
				("24*(1<<AF_DECI_1) for non-zero 24-tap FIR statistics result!!");
				LOG_INF
				("HwRWCtrl:: AF_D Error: AF_WINX_0(%d), AF_DECI(%d),",
					AF_WINX_0, AF_DECI);
				LOG_INF
				(" AF_LeftOverWidth(%d), AF_IMAGE_WD(%d), AF_WIN_WD(%d)!!",
					AF_LeftOverWidth, AF_IMAGE_WD, AF_WIN_WD);
			}
		}

		/*Chek AE_D setting */
#if 0
		unsigned int cam_aao_xsize;  /*7554 */
		unsigned int cam_aao_ysize;  /*7558 */
		unsigned int cam_awb_win_num;        /*65BC */
		unsigned int cam_ae_hst_ctl; /*6650 */

		unsigned int AAO_XSIZE;
		unsigned int AWB_W_HNUM;
		unsigned int AWB_W_VNUM;
		unsigned int histogramen_num;
#endif
		{

			cam_awb_win_num = ISP_RD32(ISP_ADDR + 0x25BC);
			cam_ae_hst_ctl = ISP_RD32(ISP_ADDR + 0x2650);
			cam_aao_xsize = ISP_RD32(ISP_ADDR + 0x3554);
			cam_aao_ysize = ISP_RD32(ISP_ADDR + 0x3558);

			AAO_XSIZE = cam_aao_xsize & 0x1ffff;
			AWB_W_HNUM = cam_awb_win_num & 0xff;
			AWB_W_VNUM = (cam_awb_win_num >> 16) & 0xff;
			histogramen_num = 0;
			for (i = 0; i < 4; i++) {
				if ((cam_ae_hst_ctl >> i) & 0x1)
					histogramen_num += 1;
			}

			if ((cam_aao_ysize + 1) != 1)
				LOG_INF("Error HwRWCtrl::AAO_D_YSIZE(%d) must be equal 1 !!",
					cam_aao_ysize);
			if ((AAO_XSIZE + 1) !=
			    (AWB_W_HNUM * AWB_W_VNUM * 5 + (histogramen_num << 8)))
				LOG_INF
				("Error HwRWCtrl::AAO_D_XSIZE(%d) = AWB_W_HNUM(%d)*AWB_W_VNUM(%d)*5 +",
					AAO_XSIZE, AWB_W_HNUM, AWB_W_VNUM);
				LOG_INF
				(" (how many histogram enable(%d)(AE_HST0/1/2/3_EN))*2*128 !!",
					histogramen_num);
		}
		/*Chek AWB_D setting */
#if 0
		unsigned int cam_awb_win_org;        /*65B0 */
		unsigned int cam_awb_win_pit;        /*65B8 */

		unsigned int AAO_InWidth;
		unsigned int AAO_InHeight;
		unsigned int AWB_W_HPIT;
		unsigned int AWB_W_VPIT;
		unsigned int AWB_W_HORG;
		unsigned int AWB_W_VORG;
#endif
		cam_awb_win_num = ISP_RD32(ISP_ADDR + 0x25BC);
		cam_awb_win_pit = ISP_RD32(ISP_ADDR + 0x25B8);
		cam_awb_win_org = ISP_RD32(ISP_ADDR + 0x25B0);

		if (bmx_enable == MTRUE) {      /*hbin_enable should be true under Twin mode. */
			AAO_InWidth = bmx_width / 2;
			AAO_InHeight = bmx_height;
		} else {
			AAO_InWidth = grab_width;
			AAO_InHeight = grab_height;
		}
		AWB_W_HNUM = (cam_awb_win_num & 0xff);
		AWB_W_VNUM = ((cam_awb_win_num >> 16) & 0xff);

		AWB_W_HPIT = (cam_awb_win_pit & 0x1fff);
		AWB_W_VPIT = ((cam_awb_win_pit >> 16) & 0x1fff);

		AWB_W_HORG = (cam_awb_win_org & 0x1fff);
		AWB_W_VORG = ((cam_awb_win_org >> 16) & 0x1fff);
		if (AAO_InWidth < (AWB_W_HNUM * AWB_W_HPIT + AWB_W_HORG)) {
			/*Error */
			LOG_INF
			("Error HwRWCtrl:: AWB_D bmx_enable(%d), bmx_width(%d),",
				bmx_enable, bmx_width);
			LOG_INF
			("	bmx_height(%d), grab_width(%d), grab_height(%d)!!",
				bmx_height, grab_width, grab_height);
			LOG_INF
			("Error HwRWCtrl:: AWB_D input frame width(%d) >= ",
				AAO_InWidth);
			LOG_INF
			("AWB_W_HNUM(%d) * AWB_W_HPIT(%d) + AWB_W_HORG(%d) !!",
				AWB_W_HNUM, AWB_W_HPIT, AWB_W_HORG);
		}
		if (AAO_InHeight < (AWB_W_VNUM * AWB_W_VPIT + AWB_W_VORG)) {
			/*Error */
			LOG_INF
			("Error HwRWCtrl:: AWB_D bmx_enable(%d), bmx_width(%d),",
				bmx_enable, bmx_width);
			LOG_INF
			("	bmx_height(%d), grab_width(%d), grab_height(%d)!!",
				bmx_height, grab_width, grab_height);
			LOG_INF
			("Error HwRWCtrl:: AWB_D input frame height(%d) >=",
				AAO_InHeight);
			LOG_INF
			("AWB_W_VNUM(%d) * AWB_W_VPIT(%d) + AWB_W_VORG(%d) !!",
				AWB_W_VNUM, AWB_W_VPIT, AWB_W_VORG);
		}
	}
#endif
	return MTRUE;
}

static signed int ISP_DumpReg(void)
{
	signed int Ret = 0;
	/*      */
	LOG_PR_ERR("- E.");
	/*      */
	/* spin_lock_irqsave(&(IspInfo.SpinLock), flags); */

	/* tile tool parse range */
	/* Joseph Hung (xa)#define ISP_ADDR_START  0x15004000 */
	/* #define ISP_ADDR_END    0x15006000 */
	/*      */
	/* N3D control */
	ISP_WR32((ISP_ADDR + 0x40c0), 0x746);
	LOG_PR_ERR("[0x%08X %08X] [0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x40c0), (unsigned int)ISP_RD32(ISP_ADDR + 0x40c0),
		(unsigned int)(ISP_TPIPE_ADDR + 0x40d8), (unsigned int)ISP_RD32(ISP_ADDR + 0x40d8));
	ISP_WR32((ISP_ADDR + 0x40c0), 0x946);
	LOG_PR_ERR("[0x%08X %08X] [0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x40c0), (unsigned int)ISP_RD32(ISP_ADDR + 0x40c0),
		(unsigned int)(ISP_TPIPE_ADDR + 0x40d8), (unsigned int)ISP_RD32(ISP_ADDR + 0x40d8));

	/* isp top */
	RegDump(0x0, 0x200);
	/* dump p1 dma reg */
	RegDump(0x3200, 0x3570);
	/* dump all     isp     dma     reg     */
	RegDump(0x3300, 0x3400);
	/* dump all     isp     dma     err     reg     */
	RegDump(0x3560, 0x35e0);
#if     0
	g_bDmaERR_p1 = g_bDmaERR_p1_d = g_bDmaERR_p2 = g_bDmaERR_deepDump = MTRUE;
	ISP_DumpDmaDeepDbg();
#endif
	/* TG1 */
	RegDump(0x410, 0x4a0);
	/* TG2 */
	RegDump(0x2410, 0x2450);
	/* hbin */
	LOG_PR_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4f0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x534), (unsigned int)(ISP_TPIPE_ADDR + 0x4f4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x538));
	/* LSC */
	RegDump(0x530, 0x550);
	/* awb win */
	RegDump(0x5b0, 0x5d0);
	/* ae win */
	RegDump(0x650, 0x690);
	/* af win */
	RegDump(0x6b0, 0x700);
	/* flk */
	RegDump(0x770, 0x780);
	/* rrz */
	RegDump(0x7a0, 0x7d0);
	/* eis */
	RegDump(0xdc0, 0xdf0);
	/* dmx/rmx/bmx */
	RegDump(0xe00, 0xe30);
	/* Mipi source */
	LOG_PR_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10215830),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0x830), (unsigned int)(0x10215830),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0x830));
	LOG_PR_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(0x10215c30),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0xc30), (unsigned int)(0x10215c30),
		(unsigned int)ISP_RD32(ISP_MIPI_ANA_ADDR + 0xc30));

	/* NSCI2 1 debug */
	ISP_WR32((ISP_ADDR + 0x43B8), 0x02);
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43B8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x43B8));
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43BC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x43BC));
	ISP_WR32((ISP_ADDR + 0x43B8), 0x12);
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43B8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x43B8));
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43BC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x43BC));
	/* NSCI2 2 debug */
	ISP_WR32((ISP_ADDR + 0x47B8), 0x02);
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x47B8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x47B8));
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x47BC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x47BC));
	ISP_WR32((ISP_ADDR + 0x47B8), 0x12);
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x47B8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x47B8));
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x47BC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x47BC));
	/* NSCI2 3 debug */
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x02);
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x43BC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x12);
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x43B8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_PR_ERR("[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));

	/* seninf1 */
	LOG_PR_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4008),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4008), (unsigned int)(ISP_TPIPE_ADDR + 0x4100),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4100));
	RegDump(0x4120, 0x4160);
	RegDump(0x4360, 0x43f0);
	/* seninf2 */
	LOG_PR_ERR("[0x%08X %08X],[0x%08X %08X]", (unsigned int)(ISP_TPIPE_ADDR + 0x4008),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4008),
		(unsigned int)(ISP_TPIPE_ADDR + 0x4100),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4100));
	RegDump(0x4520, 0x4560);
	RegDump(0x4600, 0x4610);
	RegDump(0x4760, 0x47f0);
	RegDump(0x5000, 0x503C); /*CAMSV_MODULE*/
	RegDump(0x5800, 0x583C); /*CAMSV2_MODULE*/
	RegDump(0x5410, 0x541C); /*CAMSV_TG*/
	RegDump(0x5C10, 0x5C1C); /*CAMSV_TG2*/
	LOG_PR_ERR("[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",
		(unsigned int)(ISP_TPIPE_ADDR + 0x5448),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x5448),
		(unsigned int)(ISP_TPIPE_ADDR + 0x544C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x544C),
		(unsigned int)(ISP_TPIPE_ADDR + 0x5C48),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x5C48),
		(unsigned int)(ISP_TPIPE_ADDR + 0x5C4C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x5C4C));

	RegDump(0x5208, 0x5254); /*CAMSV_IMGO_SV, CAMSV_IMGO_SV_D*/
	/* LSC_D */
	RegDump(0x2530, 0x2550);
	/* awb_d */
	RegDump(0x25b0, 0x25d0);
	/* ae_d */
	RegDump(0x2650, 0x2690);
	/* af_d */
	RegDump(0x26b0, 0x2700);
	/* rrz_d */
	RegDump(0x27a0, 0x27d0);
	/* rmx_d/bmx_d/dmx_d */
	RegDump(0x2e00, 0x2e30);

	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x800),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x800));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x880),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x880));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x884),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x884));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x888),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x888));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x8A0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x8A0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x920),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x920));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x924),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x924));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x928),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x928));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x92C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x92C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x930),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x930));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x934),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x934));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x938),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x938));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x93C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x93C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x960),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x960));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9C4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9C4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9E4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9E4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9E8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9E8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x9EC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x9EC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA00),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA00));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA04),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA04));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA08),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA08));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA0C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA0C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA10),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA10));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA14),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA14));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xA20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xA20));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xAA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xAA0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xACC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xACC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB00),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB00));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB04),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB04));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB08),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB08));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB0C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB0C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB10),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB10));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB14),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB14));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB18),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB18));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB1C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB1C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB20));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB44),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB44));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB48),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB48));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB4C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB4C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB50),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB50));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB54),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB54));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB58),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB58));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB5C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB5C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xB60),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xB60));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBA0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBA4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBA8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBAC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBB0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBB4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBB8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBBC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xBC0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xBC0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xC20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xC20));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCC0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCC0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCE4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCE4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCE8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCE8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCEC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCEC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCF0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCF4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCF8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCF8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xCFC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xCFC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD24),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD24));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD28),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD28));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD2C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD2c));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD40),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD40));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD64),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD64));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD68),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD68));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD6C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD6c));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD70),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD70));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD74),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD74));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD78),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD78));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xD7C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xD7C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xDA4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xDA8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0xDAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xDAC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2410),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2410));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2414),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2414));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2418),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2418));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x241C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x241C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2420),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2420));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x243C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x243C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2440),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2440));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2444),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2444));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x2448),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x2448));

	/* seninf3 */
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4900),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4900));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4920),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4920));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4924),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4924));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4928),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4928));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x492C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x492C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4930),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4930));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4934),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4934));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4938),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4938));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BA8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BAC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB4));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x10);
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x11);
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	ISP_WR32((ISP_ADDR + 0x4BB8), 0x12);
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BB8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4BBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4BBC));
	/* seninf4 */
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D00),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D00));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D20));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D24),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D24));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D28),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D28));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D2C),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D2C));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D30),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D30));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D34),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D34));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4D38),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4D38));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA4));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FA8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FA8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FAC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FAC));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB0),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB0));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB4),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB4));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x10);
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x11);
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));
	ISP_WR32((ISP_ADDR + 0x4FB8), 0x12);
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FB8),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FB8));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x4FBC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x4FBC));

	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x35FC),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x35FC));
	LOG_PR_ERR("end MT6593");

	/*      */
	LOG_PR_ERR("0x%08X	%08X ", (unsigned int)ISP_ADDR_CAMINF,
		(unsigned int)ISP_RD32(ISP_ADDR_CAMINF));
	LOG_PR_ERR("0x%08X	%08X ", (unsigned int)(ISP_TPIPE_ADDR + 0x150),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x150));
	/*      */
	/* debug msg for direct link */


	/* mdp crop     */
	LOG_PR_ERR("MDPCROP Related");
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_ADDR + 0xd10),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xd10));
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_ADDR + 0xd20),
		(unsigned int)ISP_RD32(ISP_ADDR + 0xd20));
	/* cq */
	LOG_PR_ERR("CQ	Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x6000);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=6000)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x7000);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=7000)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x8000);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=8000)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	/* imgi_debug */
	LOG_PR_ERR("IMGI_DEBUG	Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x001e);
	LOG_PR_ERR("0x%08X	%08X (0x150075f4=001e)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x011e);
	LOG_PR_ERR("0x%08X	%08X (0x150075f4=011e)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x021e);
	LOG_PR_ERR("0x%08X	%08X (0x150075f4=021e)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x75f4, 0x031e);
	LOG_PR_ERR("0x%08X	%08X (0x150075f4=031e)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	/* yuv */
	LOG_PR_ERR("yuv-mdp crop Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x3014);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=3014)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	LOG_PR_ERR("yuv-c24b out Related");
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x301e);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=301e)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x301f);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=301f)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x3020);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=3020)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x3021);
	LOG_PR_ERR("0x%08X	%08X (0x15004160=3021)", (unsigned int)(ISP_IMGSYS_BASE + 0x4164),
		(unsigned int)ISP_RD32(ISP_IMGSYS_BASE + 0x4164));
	/* ANR1_LceLink */
	LOG_PR_ERR("ANR1_LceLink");
	LOG_PR_ERR("0x%08X	%08X", (unsigned int)(ISP_ADDR + 0x3A00),
		(unsigned int)ISP_RD32(ISP_ADDR + 0x3A00));

	ISP_chkModuleSetting();

#if     0                       /* _mt6593fpga_dvt_use_ */
	{
		int tpipePA = ISP_RD32(ISP_ADDR + 0x204);
		int ctlStart = ISP_RD32(ISP_ADDR + 0x000);
		int ctlTcm = ISP_RD32(ISP_ADDR + 0x054);
		int map_va = 0, map_size;
		int i;
		int *pMapVa;
#define TPIPE_DUMP_SIZE    200

		if ((ctlStart & 0x01) && (tpipePA) && (ctlTcm & 0x80000000)) {
			map_va = 0;
			m4u_mva_map_kernel(tpipePA, TPIPE_DUMP_SIZE, 0, &map_va, &map_size);
			pMapVa = map_va;
			LOG_DBG("pMapVa(0x%x),map_size(0x%x)", pMapVa, map_size);
			LOG_DBG("ctlStart(0x%x),tpipePA(0x%x),ctlTcm(0x%x)", ctlStart, tpipePA,
				ctlTcm);
			if (pMapVa) {
				for (i = 0; i < TPIPE_DUMP_SIZE; i += 10) {
					LOG_DBG
					("[idx(%d)]%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X-%08X",
					 i, pMapVa[i], pMapVa[i + 1], pMapVa[i + 2],
					 pMapVa[i + 3], pMapVa[i + 4], pMapVa[i + 5],
					 pMapVa[i + 6], pMapVa[i + 7], pMapVa[i + 8],
					 pMapVa[i + 9]);
				}
			}
			m4u_mva_unmap_kernel(tpipePA, map_size, map_va);
		}
	}
#endif
	/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
	/*      */
	LOG_PR_ERR("- X.");
	/*      */
	return Ret;
}

#if (!defined CONFIG_MTK_CLKMGR) && (!defined EP_NO_CLKMGR) /*CCF*/

static inline void Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* must keep this clk open order: CG_SCP_SYS_MM0-> CG_SCP_SYS_ISP -> ISP clk */
	/* enable through smi API : CG_IMG_LARB2_SMI, CG_MM_SMI_COMMON*/
	smi_bus_prepare_enable(SMI_LARB2, ISP_DEV_NAME);

	ret = clk_prepare_enable(isp_clk.CG_SCP_SYS_MM0);
	if (ret)
		LOG_PR_ERR("cannot get CG_SCP_SYS_MM0 clock\n");

	ret = clk_prepare_enable(isp_clk.CG_SCP_SYS_ISP);
	if (ret)
		LOG_PR_ERR("cannot get CG_SCP_SYS_ISP clock\n");

	ret = clk_prepare_enable(isp_clk.CG_IMG_CAM_SMI);
	if (ret)
		LOG_PR_ERR("cannot get CG_IMG_CAM_SMI clock\n");

	ret = clk_prepare_enable(isp_clk.CG_IMG_CAM_CAM);
	if (ret)
		LOG_PR_ERR("cannot get CG_IMG_CAM_CAM clock\n");

	ret = clk_prepare_enable(isp_clk.CG_IMG_SEN_TG);
	if (ret)
		LOG_PR_ERR("cannot get CG_IMG_SEN_TG clock\n");

	ret = clk_prepare_enable(isp_clk.CG_IMG_SEN_CAM);
	if (ret)
		LOG_PR_ERR("cannot get CG_IMG_SEN_CAM clock\n");

	ret = clk_prepare_enable(isp_clk.CG_IMG_CAM_SV);
	if (ret)
		LOG_PR_ERR("cannot get CG_IMG_CAM_SV clock\n");

}


static inline void Disable_Unprepare_ccf_clock(void)
{
	/* must keep this clk close order: ISP clk -> CG_SCP_SYS_ISP -> CG_SCP_SYS_MM0 */
	clk_disable_unprepare(isp_clk.CG_IMG_CAM_SMI);
	clk_disable_unprepare(isp_clk.CG_IMG_CAM_CAM);
	clk_disable_unprepare(isp_clk.CG_IMG_SEN_TG);
	clk_disable_unprepare(isp_clk.CG_IMG_SEN_CAM);
	clk_disable_unprepare(isp_clk.CG_IMG_CAM_SV);
	clk_disable_unprepare(isp_clk.CG_SCP_SYS_ISP);
	clk_disable_unprepare(isp_clk.CG_SCP_SYS_MM0);
	/* disable through smi API : CG_IMG_LARB2_SMI, CG_MM_SMI_COMMON*/
	smi_bus_disable_unprepare(SMI_LARB2, ISP_DEV_NAME);
}



#endif
/*******************************************************************************
*
********************************************************************************/
static void ISP_EnableClock(bool En)
{
	/*   if (G_u4EnableClockCount ==  1) {*/
	/*   LOG_DBG("- E. En: %d. G_u4EnableClockCount: %d.", En, G_u4EnableClockCount);*/
	/*   }*/
#if defined(EP_NO_CLKMGR)
	unsigned int setReg;
#endif

	if (En) {
		/* from SY yang,,*IMG_CG_CLR = 0xffffffff;*/
		/* *MMSYS_CG_CLR0 = 0x00000003;*CLK_CFG_7 = *CLK_CFG_7 | 0x02000000;*CAM_CTL_CLK_EN = 0x00000009; */
		/* address map, MMSYS_CG_CLR0:0x14000108,CLK_CFG_7:0x100000b0 */
#if defined(CONFIG_MTK_CLKMGR) || defined(EP_NO_CLKMGR)

		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock enbled. G_u4EnableClockCount: %d.", G_u4EnableClockCount); */
		switch (G_u4EnableClockCount) {
		case 0:
#ifdef EP_NO_CLKMGR /* FPGA test */
			/* Enable clock by hardcode:
			* 1. IMG_CG_CLR (0x15000008) = 0xffffffff;
			*/
			setReg = 0xFFFFFFFF;
			ISP_WR32(IMGSYS_REG_CG_CLR, setReg);
#else
			/* LOG_INF("MTK_LEGACY:enable clk"); */
			enable_clock(MT_CG_MM_SMI_COMMON, "CAMERA");
			enable_clock(MT_CG_IMG_CAM_SMI, "CAMERA");
			enable_clock(MT_CG_IMG_CAM_CAM, "CAMERA");
			enable_clock(MT_CG_IMG_SEN_TG, "CAMERA");
			enable_clock(MT_CG_IMG_SEN_CAM, "CAMERA");
			enable_clock(MT_CG_IMG_CAM_SV, "CAMERA");
			/* enable_clock(MT_CG_IMG_FD, "CAMERA"); */
			enable_clock(MT_CG_IMG_LARB2_SMI, "CAMERA");
#endif
			break;
		default:
			break;
		}
		G_u4EnableClockCount++;
		spin_unlock(&(IspInfo.SpinLockClock));

#else
		/*LOG_INF("CCF:prepare_enable clk"); */
		spin_lock(&(IspInfo.SpinLockClock));
		G_u4EnableClockCount++;
		spin_unlock(&(IspInfo.SpinLockClock));
		Prepare_Enable_ccf_clock();
#endif
	} else {                /* Disable clock. */
#if defined(CONFIG_MTK_CLKMGR) || defined(EP_NO_CLKMGR)
		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock disabled. G_u4EnableClockCount: %d.", G_u4EnableClockCount); */
		G_u4EnableClockCount--;
		switch (G_u4EnableClockCount) {
		case 0:
#ifdef EP_NO_CLKMGR /* FPGA test */
			/* Disable clock by hardcode:
			* 1. IMG_CG_SET (0x15000004) = 0xffffffff;
			*/
			setReg = 0xFFFFFFFF;
			ISP_WR32(IMGSYS_REG_CG_SET, setReg);
#else
			/*LOG_INF("MTK_LEGACY:disable clk"); */
			/* do disable clock     */
			disable_clock(MT_CG_IMG_CAM_SMI, "CAMERA");
			disable_clock(MT_CG_IMG_CAM_CAM, "CAMERA");
			disable_clock(MT_CG_IMG_SEN_TG, "CAMERA");
			disable_clock(MT_CG_IMG_SEN_CAM, "CAMERA");
			disable_clock(MT_CG_IMG_CAM_SV, "CAMERA");
			/* disable_clock(MT_CG_IMG_FD, "CAMERA"); */
			disable_clock(MT_CG_IMG_LARB2_SMI, "CAMERA");
			disable_clock(MT_CG_MM_SMI_COMMON, "CAMERA");
#endif
			break;
		default:
			break;
		}
		spin_unlock(&(IspInfo.SpinLockClock));

#else
		/*LOG_INF("CCF:disable_unprepare clk\n"); */
		spin_lock(&(IspInfo.SpinLockClock));
		G_u4EnableClockCount--;
		spin_unlock(&(IspInfo.SpinLockClock));
		Disable_Unprepare_ccf_clock();
#endif
	}
}

/*******************************************************************************
*
********************************************************************************/
static inline void ISP_Reset(signed int rst_path)
{
	/* ensure the view finder is disabe. 0: take_picture */
	/* ISP_CLR_BIT(ISP_REG_ADDR_EN1, 0); */
	unsigned int Reg;
	unsigned int setReg;
	unsigned int LoopCnt = 5, i;
	/* unsigned int i, flags; */
	/*      */
	LOG_DBG("- E.");

	LOG_DBG("isp gate clk(0x%x),rst_path(%d)", ISP_RD32(ISP_ADDR_CAMINF), rst_path);


	if (rst_path == ISP_REG_SW_CTL_RST_CAM_P1) {
		/* ISP Soft     SW reset process */
#if     1
		Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		setReg =
			(Reg & (~ISP_REG_SW_CTL_SW_RST_P1_MASK)) | (ISP_REG_SW_CTL_SW_RST_TRIG &
								    ISP_REG_SW_CTL_SW_RST_P1_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);
		/* ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0); */
		i = LoopCnt;
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
			if (Reg & ISP_REG_SW_CTL_SW_RST_STATUS)
				break;
			udelay(100);
		} while (--i);

		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST);

		setReg =
			(Reg & (~ISP_REG_SW_CTL_SW_RST_P1_MASK)) | (0x00 &
								    ISP_REG_SW_CTL_SW_RST_P1_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);
#else
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_SW_RST_TRIG);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
#endif
	} else if (rst_path == ISP_REG_SW_CTL_RST_CAM_P2) {
#if     1
		Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		setReg =
			(Reg & (~ISP_REG_SW_CTL_SW_RST_P2_MASK)) | (ISP_REG_SW_CTL_SW_RST_P2_TRIG &
								    ISP_REG_SW_CTL_SW_RST_P2_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);
		/* ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0); */
		i = LoopCnt;
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
			if (Reg & ISP_REG_SW_CTL_SW_RST_P2_STATUS)
				break;
			udelay(100);
		} while (--i);

		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST_P2);

		setReg =
			(Reg & (~ISP_REG_SW_CTL_SW_RST_P2_MASK)) | (0x00 &
								    ISP_REG_SW_CTL_SW_RST_P2_MASK);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, setReg);

#else
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_SW_RST_P2_TRIG);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAM_SW_CTL);
		} while ((!Reg) & ISP_REG_SW_CTL_SW_RST_P2_STATUS);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, ISP_REG_SW_CTL_HW_RST_P2);
		ISP_WR32(ISP_REG_ADDR_CAM_SW_CTL, 0);
#endif
	} else if (rst_path == ISP_REG_SW_CTL_RST_CAMSV) {
		ISP_WR32(ISP_REG_ADDR_CAMSV_SW_CTL, ISP_REG_CAMSV_SW_CTL_IMGO_RST_TRIG);
		i = LoopCnt;
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAMSV_SW_CTL);
			if (Reg & ISP_REG_CAMSV_SW_CTL_IMGO_RST_ST)
				break;
			udelay(100);
		} while (--i);

		ISP_WR32(ISP_REG_ADDR_CAMSV_SW_CTL, ISP_REG_CAMSV_SW_CTL_SW_RST);
		ISP_WR32(ISP_REG_ADDR_CAMSV_SW_CTL, 0);
	} else if (rst_path == ISP_REG_SW_CTL_RST_CAMSV2) {
		ISP_WR32(ISP_REG_ADDR_CAMSV2_SW_CTL, ISP_REG_CAMSV_SW_CTL_IMGO_RST_TRIG);
		i = LoopCnt;
		do {
			Reg = ISP_RD32(ISP_REG_ADDR_CAMSV2_SW_CTL);
			if (Reg & ISP_REG_CAMSV_SW_CTL_IMGO_RST_ST)
				break;
			udelay(100);
		} while (--i);
		ISP_WR32(ISP_REG_ADDR_CAMSV2_SW_CTL, ISP_REG_CAMSV_SW_CTL_SW_RST);
		ISP_WR32(ISP_REG_ADDR_CAMSV2_SW_CTL, 0);
	}
#if     0
	/* need modify here     */
	for (i = 0; i < _IRQ_MAX; i++)
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[i]), flags);

#endif
#if     0
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_IRQ]), flags);
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_IRQ_D]), flags);
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]), flags);
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]), flags);
	for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++)
		IspInfo.IrqInfo.Status[i] = 0;

	for (i = 0; i < _ChannelMax; i++)
		PrvAddr[i] = 0;

#if     0
	for (i = 0; i < _IRQ_MAX; i++)
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[i]), flags);

#endif
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]), flags);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]), flags);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_IRQ_D]), flags);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_IRQ]), flags);
	/*      */
#endif


	/*      */
	LOG_DBG("- X.");
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_ReadReg(struct ISP_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;
	/*      */
	struct ISP_REG_STRUCT *pData = NULL;
	struct ISP_REG_STRUCT *pDataArray = NULL;

	if ((pRegIo->Count > (PAGE_SIZE/sizeof(unsigned int))) || (pRegIo->Count == 0)
		|| (pRegIo->pData == NULL)) {
		LOG_PR_ERR("ERROR ISP_ReadReg pRegIo->pData is NULL or pRegIo->Count error:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pDataArray = kmalloc((pRegIo->Count) * sizeof(struct ISP_REG_STRUCT), GFP_KERNEL);
	if (pDataArray == NULL) {
		LOG_PR_ERR("ERROR ISP_ReadReg kmalloc failed, cnt:%d\n", pRegIo->Count);
		Ret = -ENOMEM;
		goto EXIT;
	}
	if (copy_from_user(pDataArray, (void *)pRegIo->pData,
		(pRegIo->Count)*sizeof(struct ISP_REG_STRUCT)) != 0) {
		LOG_PR_ERR("ERROR ISP_ReadReg copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = pDataArray;
	for (i = 0; i < pRegIo->Count; i++) {
		/*      */
		if ((ISP_ADDR_CAMINF + pData->Addr >= ISP_ADDR_CAMINF)
			&& (ISP_ADDR_CAMINF + pData->Addr < (ISP_ADDR_CAMINF + ISP_RANGE))) {
			pData->Val = ISP_RD32(ISP_ADDR_CAMINF + pData->Addr);
		} else {
			LOG_PR_ERR("ERROR ISP_ReadReg wrong address(0x%x)\n",
				(unsigned int)(ISP_ADDR_CAMINF + pData->Addr));
			pData->Val = 0;
		}
		/*      */
		pData++;
	}
	if (copy_to_user((void *)pRegIo->pData, pDataArray,
		(pRegIo->Count)*sizeof(struct ISP_REG_STRUCT)) != 0) {
		LOG_PR_ERR("ERROR ISP_ReadReg copy_to_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*      */
EXIT:
	if (pDataArray != NULL) {
		kfree(pDataArray);
		pDataArray = NULL;
	}
	return Ret;
}


/*******************************************************************************
*
********************************************************************************/
static signed int ISP_WriteRegToHw(struct ISP_REG_STRUCT *pReg, unsigned int Count)
{
	signed int Ret = 0;
	unsigned int i;
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_WRITE_REG)
		LOG_DBG("- E.");

	/*      */
	spin_lock(&(IspInfo.SpinLockIsp));
	for (i = 0; i < Count; i++) {
		if (IspInfo.DebugMask & ISP_DBG_WRITE_REG)
			LOG_DBG("Addr(0x%08X), Val(0x%08X)",
				(unsigned int)(ISP_ADDR_CAMINF + pReg[i].Addr),
				(unsigned int)(pReg[i].Val));

		if (((ISP_ADDR_CAMINF + pReg[i].Addr) >= ISP_ADDR_CAMINF)
		    && ((ISP_ADDR_CAMINF + pReg[i].Addr) < (ISP_ADDR_CAMINF + ISP_RANGE))) {
			ISP_WR32(ISP_ADDR_CAMINF + pReg[i].Addr, pReg[i].Val);
		} else
			LOG_PR_ERR("wrong address(0x%x)",
				(unsigned int)(ISP_ADDR_CAMINF + pReg[i].Addr));

	}
	spin_unlock(&(IspInfo.SpinLockIsp));
	/*      */
	return Ret;
}

/*******************************************************************************
*
********************************************************************************
//Vent@20121106: Marked to remove build warning: 'ISP_BufWrite_Init' defined but not used [-Wunused-function]
static void     ISP_BufWrite_Init(void)
{
	unsigned int i;
	//
	if(IspInfo.DebugMask & ISP_DBG_BUF_WRITE) {
	LOG_DBG("- E.");
	}
	//
	for(i=0; i<ISP_BUF_WRITE_AMOUNT; i++) {
	IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
	IspInfo.BufInfo.Write[i].Size = 0;
	IspInfo.BufInfo.Write[i].pData = NULL;
	}
}

*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_Dump(void)
{
	unsigned int i;
	/*      */
	LOG_DBG("- E.");
	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		LOG_DBG("i=%d, Status=%d, Size=%d", i, IspInfo.BufInfo.Write[i].Status,
			IspInfo.BufInfo.Write[i].Size);
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
		IspInfo.BufInfo.Write[i].pData = NULL;
	}
}


/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_Free(void)
{
	unsigned int i;
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
		LOG_DBG("- E.");
	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
		spin_lock(&(IspInfo.SpinLockIspRef));
		if (IspInfo.BufInfo.Write[i].pData != NULL) {
			kfree(IspInfo.BufInfo.Write[i].pData);
			IspInfo.BufInfo.Write[i].pData = NULL;
		}
		spin_unlock(&(IspInfo.SpinLockIspRef));
	}
}

/*******************************************************************************
*
********************************************************************************/
static bool ISP_BufWrite_Alloc(void)
{
	unsigned int i;
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
		LOG_DBG("- E.");

	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		spin_lock(&(IspInfo.SpinLockIspRef));
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
		IspInfo.BufInfo.Write[i].pData =
			kmalloc(ISP_BUF_SIZE_WRITE * sizeof(unsigned char), GFP_ATOMIC);
		if (IspInfo.BufInfo.Write[i].pData != NULL)
			spin_unlock(&(IspInfo.SpinLockIspRef));
		else {
			spin_unlock(&(IspInfo.SpinLockIspRef));
			LOG_DBG("ERROR:	i = %d, pData is NULL", i);
			ISP_BufWrite_Free();
			return false;
		}
	}
	return true;
}

/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_Reset(void)
{
	unsigned int i;
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
		LOG_DBG("- E.");
	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_EMPTY;
		IspInfo.BufInfo.Write[i].Size = 0;
	}
}

/*******************************************************************************
*
********************************************************************************/
static inline unsigned int ISP_BufWrite_GetAmount(void)
{
	unsigned int i, Count = 0;
	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_READY)
			Count++;
	}
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
		LOG_DBG("Count = %d", Count);
	return Count;
}

/*******************************************************************************
*
********************************************************************************/
static bool ISP_BufWrite_Add(unsigned int Size,
			      /* unsigned char*         pData) */
			      struct ISP_REG_STRUCT *pData)
{
	unsigned int i;
	/*      */
	/* LOG_DBG("- E."); */
	/*      */
	if (Size > ISP_BUF_SIZE_WRITE) {
		LOG_PR_ERR("ERROR ISP_BufWrite_Add Size(%d) > %d", Size, ISP_BUF_SIZE_WRITE);
		return false;
	}
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_HOLD) {
			if ((IspInfo.BufInfo.Write[i].Size + Size) > ISP_BUF_SIZE_WRITE) {
				LOG_PR_ERR("i = %d, BufWriteSize(%d)+Size(%d) > %d", i,
					IspInfo.BufInfo.Write[i].Size, Size, ISP_BUF_SIZE_WRITE);
				return false;
			}
			/*      */
			if (IspInfo.BufInfo.Write[i].Size > ISP_BUF_SIZE_WRITE) {
				LOG_PR_ERR("ERROR ISP_BufWrite_Add pData buffer size(%d) > %d\n",
					IspInfo.BufInfo.Write[i].Size, ISP_BUF_SIZE_WRITE);
				return false;
			}
			if (copy_from_user
			    ((unsigned char *)(IspInfo.BufInfo.Write[i].pData +
					IspInfo.BufInfo.Write[i].Size), (unsigned char *) pData,
			     Size) != 0) {
				LOG_PR_ERR("copy_from_user	failed");
				return false;
			}
			/*      */
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
				LOG_DBG("i = %d, BufSize = %d, Size	= %d", i,
					IspInfo.BufInfo.Write[i].Size, Size);
			/*      */
			IspInfo.BufInfo.Write[i].Size += Size;
			return true;
		}
	}
	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_EMPTY) {
			if (Size > ISP_BUF_SIZE_WRITE) {
				LOG_PR_ERR("i = %d, Size(%d) > %d", i, Size, ISP_BUF_SIZE_WRITE);
				return false;
			}
			/*      */
			if (copy_from_user
			    ((unsigned char *)(IspInfo.BufInfo.Write[i].pData), (unsigned char *) pData,
			     Size) != 0) {
				LOG_PR_ERR("copy_from_user	failed");
				return false;
			}
			/*      */
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
				LOG_DBG("i = %d, Size =	%d", i, Size);
			/*      */
			IspInfo.BufInfo.Write[i].Size = Size;
			/*      */
			IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_HOLD;
			return true;
		}
	}

	/*      */
	LOG_PR_ERR("All write buffer are full of data!");
	return false;

}

/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_SetReady(void)
{
	unsigned int i;
	/*      */
	/* LOG_DBG("- E."); */
	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_HOLD) {
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
				LOG_DBG("i = %d, Size =	%d", i, IspInfo.BufInfo.Write[i].Size);
			IspInfo.BufInfo.Write[i].Status = ISP_BUF_STATUS_READY;
		}
	}
}

/*******************************************************************************
*
********************************************************************************/
static bool ISP_BufWrite_Get(unsigned int *pIndex, unsigned int *pSize, unsigned char **ppData)
{
	unsigned int i;
	/*      */
	/* LOG_DBG("- E."); */
	/*      */
	for (i = 0; i < ISP_BUF_WRITE_AMOUNT; i++) {
		if (IspInfo.BufInfo.Write[i].Status == ISP_BUF_STATUS_READY) {
			if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
				LOG_DBG("i = %d, Size =	%d", i, IspInfo.BufInfo.Write[i].Size);

			*pIndex = i;
			*pSize = IspInfo.BufInfo.Write[i].Size;
			*ppData = IspInfo.BufInfo.Write[i].pData;
			return true;
		}
	}
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
		LOG_DBG("No buf	is ready!");
	return false;
}

/*******************************************************************************
*
********************************************************************************/
static bool ISP_BufWrite_Clear(unsigned int Index)
{
	/*      */
	/* LOG_DBG("- E."); */
	/*      */
	if (IspInfo.BufInfo.Write[Index].Status == ISP_BUF_STATUS_READY) {
		if (IspInfo.DebugMask & ISP_DBG_BUF_WRITE)
			LOG_DBG("Index = %d, Size = %d", Index, IspInfo.BufInfo.Write[Index].Size);

		IspInfo.BufInfo.Write[Index].Size = 0;
		IspInfo.BufInfo.Write[Index].Status = ISP_BUF_STATUS_EMPTY;
		return true;
	}
		LOG_DBG("WARNING: Index(%d) is not ready! Status = %d", Index,
			IspInfo.BufInfo.Write[Index].Status);
		return false;
}

/*******************************************************************************
*
********************************************************************************/
static void ISP_BufWrite_WriteToHw(void)
{
	unsigned char *pBuf;
	unsigned int Index, BufSize;
	/*      */
	spin_lock(&(IspInfo.SpinLockHold));
	/*      */
	LOG_DBG("- E.");
	/*      */
	while (ISP_BufWrite_Get(&Index, &BufSize, &pBuf)) {

		if (IspInfo.DebugMask & ISP_DBG_TASKLET)
			LOG_DBG("Index = %d, BufSize = %d ", Index, BufSize);

		ISP_WriteRegToHw((struct ISP_REG_STRUCT *) pBuf, BufSize / sizeof(struct ISP_REG_STRUCT));
		ISP_BufWrite_Clear(Index);
	}
	/* LOG_DBG("No more     buf."); */
	atomic_set(&(IspInfo.HoldInfo.WriteEnable), 0);
	wake_up_interruptible_all(&(IspInfo.WaitQueueHead));
	/*      */
	spin_unlock(&(IspInfo.SpinLockHold));
}


/*******************************************************************************
*
********************************************************************************/
void ISP_ScheduleWork_VD(struct work_struct *data)
{
	if (IspInfo.DebugMask & ISP_DBG_SCHEDULE_WORK)
		LOG_DBG("- E.");
	/*      */
	IspInfo.TimeLog.WorkQueueVd = ISP_JiffiesToMs(jiffies);
	/*      */
	if (IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_VD].Func != NULL)
		IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_VD].Func();
}

/*******************************************************************************
*
********************************************************************************/
void ISP_ScheduleWork_EXPDONE(struct work_struct *data)
{
	if (IspInfo.DebugMask & ISP_DBG_SCHEDULE_WORK)
		LOG_DBG("- E.");
	/*      */
	IspInfo.TimeLog.WorkQueueExpdone = ISP_JiffiesToMs(jiffies);
	/*      */
	if (IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_EXPDONE].Func != NULL)
		IspInfo.Callback[ISP_CALLBACK_WORKQUEUE_EXPDONE].Func();
}

/*******************************************************************************
*
********************************************************************************/
void ISP_Tasklet_VD(unsigned long Param)
{
	if (IspInfo.DebugMask & ISP_DBG_TASKLET)
		LOG_DBG("- E.");
	/*      */
	IspInfo.TimeLog.TaskletVd = ISP_JiffiesToMs(jiffies);
	/*      */
	if (IspInfo.Callback[ISP_CALLBACK_TASKLET_VD].Func != NULL)
		IspInfo.Callback[ISP_CALLBACK_TASKLET_VD].Func();
	/*      */
	if (IspInfo.HoldInfo.Time == ISP_HOLD_TIME_VD)
		ISP_BufWrite_WriteToHw();
}

DECLARE_TASKLET(IspTaskletVD, ISP_Tasklet_VD, 0);

/*******************************************************************************
*
********************************************************************************/
void ISP_Tasklet_EXPDONE(unsigned long Param)
{
	if (IspInfo.DebugMask & ISP_DBG_TASKLET)
		LOG_DBG("- E.");
	/*      */
	IspInfo.TimeLog.TaskletExpdone = ISP_JiffiesToMs(jiffies);
	/*      */
	if (IspInfo.Callback[ISP_CALLBACK_TASKLET_EXPDONE].Func != NULL)
		IspInfo.Callback[ISP_CALLBACK_TASKLET_EXPDONE].Func();

	/*      */
	if (IspInfo.HoldInfo.Time == ISP_HOLD_TIME_EXPDONE)
		ISP_BufWrite_WriteToHw();

}

DECLARE_TASKLET(IspTaskletEXPDONE, ISP_Tasklet_EXPDONE, 0);


/*******************************************************************************
*
********************************************************************************/
static signed int ISP_WriteReg(struct ISP_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;
	signed int TimeVd = 0;
	signed int TimeExpdone = 0;
	signed int TimeTasklet = 0;
	/* unsigned char* pData = NULL; */
	struct ISP_REG_STRUCT *pData = NULL;

	if (pRegIo->Count > (PAGE_SIZE / sizeof(unsigned int))) {
		LOG_PR_ERR("pRegIo->Count error");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*      */
	if ((pRegIo->Count > (PAGE_SIZE/sizeof(unsigned int))) || (pRegIo->Count == 0)
		|| (pRegIo->pData == NULL)) {
		LOG_PR_ERR("ERROR ISP_WriteReg pRegIo->pData is NULL or pRegIo->Count error:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	if (IspInfo.DebugMask & ISP_DBG_WRITE_REG) {
	/* LOG_DBG("Data(0x%08X), Count(%d)", (unsigned int)(pRegIo->pData), (unsigned int)(pRegIo->Count)); */
		LOG_DBG("Data(0x%p), Count(%d)", (pRegIo->pData), (pRegIo->Count));
	}
	/*      */
	if (atomic_read(&(IspInfo.HoldInfo.HoldEnable))) {
	/* if(ISP_BufWrite_Add((pRegIo->Count)*sizeof(struct ISP_REG_STRUCT), (unsigned char*)(pRegIo->Data))) */
		if (ISP_BufWrite_Add((pRegIo->Count) * sizeof(struct ISP_REG_STRUCT), pRegIo->pData)) {
			/* LOG_DBG("Add write buffer OK"); */
		} else {
			LOG_PR_ERR("Add write buffer fail");
			TimeVd = ISP_JiffiesToMs(jiffies) - IspInfo.TimeLog.Vd;
			TimeExpdone = ISP_JiffiesToMs(jiffies) - IspInfo.TimeLog.Expdone;
			TimeTasklet = ISP_JiffiesToMs(jiffies) - IspInfo.TimeLog.TaskletExpdone;
			LOG_PR_ERR("HoldTime(%d), VD(%d ms), Expdone(%d ms), Tasklet(%d ms)",
				IspInfo.HoldInfo.Time, TimeVd, TimeExpdone, TimeTasklet);
			ISP_BufWrite_Dump();
			/* ISP_DumpReg(); */
			Ret = -EFAULT;
			goto EXIT;
		}
	} else {
		/* pData = (unsigned char*)kmalloc((pRegIo->Count)*sizeof(struct ISP_REG_STRUCT),     GFP_ATOMIC); */
		pData =
			kmalloc((pRegIo->Count) * sizeof(struct ISP_REG_STRUCT),
						   GFP_ATOMIC);
		if (pData == NULL) {
			LOG_DBG("ERROR:	kmalloc	failed,	(process, pid, tgid)=(%s, %d, %d)",
				current->comm, current->pid, current->tgid);
			Ret = -ENOMEM;
			goto EXIT;
		}
		/*      */
		if (copy_from_user
		    (pData, (void __user *)(pRegIo->pData),
		     pRegIo->Count * sizeof(struct ISP_REG_STRUCT)) != 0) {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
			goto EXIT;
		}
		/*      */
		Ret = ISP_WriteRegToHw(
			      /* (struct ISP_REG_STRUCT*)pData, */
			      pData, pRegIo->Count);
	}
	/*      */
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_SetHoldTime(enum ISP_HOLD_TIME_ENUM HoldTime)
{
	LOG_DBG("HoldTime(%d)", HoldTime);
	IspInfo.HoldInfo.Time = HoldTime;
	/*      */
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_ResetBuf(void)
{
	LOG_DBG("- E. hold_reg(%d),	BufAmount(%d)", atomic_read(&(IspInfo.HoldInfo.HoldEnable)),
		ISP_BufWrite_GetAmount());
	/*      */
	ISP_BufWrite_Reset();
	atomic_set(&(IspInfo.HoldInfo.HoldEnable), 0);
	atomic_set(&(IspInfo.HoldInfo.WriteEnable), 0);
	LOG_DBG("- X.");
	return 0;
}


/*******************************************************************************
*
********************************************************************************/
static signed int ISP_EnableHoldReg(bool En)
{
	signed int Ret = 0;
	unsigned int BufAmount = 0;
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_HOLD_REG)
		LOG_DBG("En(%d), HoldEnable(%d)", En, atomic_read(&(IspInfo.HoldInfo.HoldEnable)));

	/*      */
	if (!spin_trylock_bh(&(IspInfo.SpinLockHold))) {
		/* Should wait until tasklet done. */
		signed int Timeout;
		signed int IsLock = 0;
		/*      */
		if (IspInfo.DebugMask & ISP_DBG_TASKLET)
			LOG_DBG("Start wait	...	");

		/*      */
		Timeout = wait_event_interruptible_timeout(IspInfo.WaitQueueHead,
							   (IsLock =
								    spin_trylock_bh(&
										    (IspInfo.
										     SpinLockHold))),
							   ISP_MsToJiffies(500));
		if (Timeout == 0)
			LOG_DBG("[%s] wait timeout 500", __func__);
		/*      */
		if (IspInfo.DebugMask & ISP_DBG_TASKLET) {
			LOG_DBG("End wait ");
		}

		/*      */
		if (IsLock == 0) {
			LOG_PR_ERR("Should	not	happen,	Timeout	& IsLock is	0");
			Ret = -EFAULT;
			goto EXIT;
		}
	}
	/* Here we get the lock. */
	if (En == MFALSE) {
		ISP_BufWrite_SetReady();
		BufAmount = ISP_BufWrite_GetAmount();
		/*      */
		if (BufAmount)
			atomic_set(&(IspInfo.HoldInfo.WriteEnable), 1);

	}
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_HOLD_REG)
		LOG_DBG("En(%d), HoldEnable(%d), BufAmount(%d)", En,
			atomic_read(&(IspInfo.HoldInfo.HoldEnable)), BufAmount);

	/*      */
	atomic_set(&(IspInfo.HoldInfo.HoldEnable), En);
	/*      */
	spin_unlock_bh(&(IspInfo.SpinLockHold));
	/*      */
EXIT:
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static atomic_t g_imem_ref_cnt[ISP_REF_CNT_ID_MAX];
/*      */
/* static long ISP_REF_CNT_CTRL_FUNC(unsigned int Param)     */
static long ISP_REF_CNT_CTRL_FUNC(unsigned long Param)
{
	signed int Ret = 0;
	struct ISP_REF_CNT_CTRL_STRUCT ref_cnt_ctrl;
	signed int imem_ref_cnt = 0;

	LOG_DBG("[rc]+ QQ");    /* for memory corruption check */


	/* //////////////////---add     lock here */
	/* spin_lock_irq(&(IspInfo.SpinLock));*/
	/* ////////////////// */
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
		LOG_DBG("[rc]+");

	/*      */
	if (NULL == (void __user *)Param) {
		LOG_PR_ERR("[rc]NULL Param");
		/* //////////////////---add     unlock here     */
		/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags); */
		/* ////////////////// */
		return -EFAULT;
	}
	/*      */
	if (copy_from_user(&ref_cnt_ctrl, (void __user *)Param, sizeof(struct ISP_REF_CNT_CTRL_STRUCT)) ==
	    0) {
		if ((ref_cnt_ctrl.id < 0) || (ref_cnt_ctrl.id >= ISP_REF_CNT_ID_MAX)) {
			LOG_PR_ERR("[rc] invalid ref_cnt_ctrl.id %d\n", ref_cnt_ctrl.id);
			return -EFAULT;
		}
		if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
			LOG_DBG("[rc]ctrl(%d),id(%d)", ref_cnt_ctrl.ctrl, ref_cnt_ctrl.id);

		/*      */
		if (ref_cnt_ctrl.id < ISP_REF_CNT_ID_MAX) {
			/* //////////////////---add     lock here */
			spin_lock(&(IspInfo.SpinLockIspRef));
			/* ////////////////// */
			/*      */
			switch (ref_cnt_ctrl.ctrl) {
			case ISP_REF_CNT_GET:
				break;
			case ISP_REF_CNT_INC:
				atomic_inc(&g_imem_ref_cnt[ref_cnt_ctrl.id]);
				/* g_imem_ref_cnt++; */
				break;
			case ISP_REF_CNT_DEC:
			case ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE:
			case ISP_REF_CNT_DEC_AND_RESET_P1_IF_LAST_ONE:
			case ISP_REF_CNT_DEC_AND_RESET_P2_IF_LAST_ONE:
				atomic_dec(&g_imem_ref_cnt[ref_cnt_ctrl.id]);
				/* g_imem_ref_cnt--; */
				break;
			default:
			case ISP_REF_CNT_MAX:
				/* Add this     to remove build warning. */
				/* Do nothing. */
				break;
			}
			/*      */
			imem_ref_cnt = (signed int) atomic_read(&g_imem_ref_cnt[ref_cnt_ctrl.id]);

			if (imem_ref_cnt == 0) {
				/* No user left and     ctrl is RESET_IF_LAST_ONE, do ISP reset. */
				if (ref_cnt_ctrl.ctrl == ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE
				    || ref_cnt_ctrl.ctrl ==
				    ISP_REF_CNT_DEC_AND_RESET_P1_IF_LAST_ONE) {
					ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P1);
					LOG_DBG("Reset P1\n");
				}

				if (ref_cnt_ctrl.ctrl == ISP_REF_CNT_DEC_AND_RESET_P1_P2_IF_LAST_ONE
				    || ref_cnt_ctrl.ctrl ==
				    ISP_REF_CNT_DEC_AND_RESET_P2_IF_LAST_ONE) {
					ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);
				}

			}
			/* //////////////////---add     unlock here     */
			spin_unlock(&(IspInfo.SpinLockIspRef));
			/* ////////////////// */

			/*      */
			if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
				LOG_DBG("[rc]ref_cnt(%d)", imem_ref_cnt);

			/*      */
			if (copy_to_user
			    ((void *)ref_cnt_ctrl.data_ptr, &imem_ref_cnt, sizeof(signed int)) != 0) {
				LOG_PR_ERR("[rc][GET]:copy_to_user	failed");
				Ret = -EFAULT;
			}
		} else {
			LOG_PR_ERR("[rc]:id(%d) exceed", ref_cnt_ctrl.id);
			Ret = -EFAULT;
		}

	} else {
		LOG_PR_ERR("[rc]copy_from_user	failed");
		Ret = -EFAULT;
	}

	/*      */
	if (IspInfo.DebugMask & ISP_DBG_REF_CNT_CTRL)
		LOG_DBG("[rc]-");


	LOG_DBG("[rc]QQ	return value:(%d)", Ret);
	/*      */
	/* //////////////////---add     unlock here     */
	/* spin_unlock_irqrestore(&(IspInfo.SpinLock), flags);*/
	/* ////////////////// */
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
/* js_test */
/*      */
#ifndef _rtbc_use_cq0c_
static unsigned int bEnqBuf;
static unsigned int bDeqBuf;
static signed int rtbc_enq_dma = _rt_dma_max_;
static signed int rtbc_deq_dma = _rt_dma_max_;
#endif

static unsigned int prv_tstamp_s[_rt_dma_max_] = { 0 };
static unsigned int prv_tstamp_us[_rt_dma_max_] = { 0 };

static unsigned int sof_count[_ChannelMax] = { 0, 0, 0, 0 };
static unsigned int start_time[_ChannelMax] = { 0, 0, 0, 0 };
static unsigned int avg_frame_time[_ChannelMax] = { 0, 0, 0, 0 };


/* record lost p1_done or not, 1 for lost p1_done. 0 for normal , 2 for last working buffer. */
static int sof_pass1done[2] = { 0, 0 };

static unsigned int lost_pass1_done_cnt;
#if (ISP_RAW_D_SUPPORT == 1)
static unsigned int lost_pass1_d_done_cnt;
#endif
/* record lost p1_done or not, 1 for lost p1_done. 0 for normal , 2 for last working buffer. */
static unsigned int gSof_camsvdone[2] = { 0, 0 };
static unsigned int lost_camsv_done_cnt;

static unsigned int lost_camsv2_done_cnt;


static bool g1stSof[4] = { MTRUE, MTRUE };

#ifdef _rtbc_buf_que_2_0_
struct FW_RCNT_CTRL {
	unsigned int INC[_IRQ_MAX][ISP_RT_BUF_SIZE]; /* rcnt_in */
	unsigned int DMA_IDX[_rt_dma_max_];  /* enque cnt */
	unsigned int rdIdx[_IRQ_MAX];        /* enque read cnt */
	unsigned int curIdx[_IRQ_MAX];       /* record avail rcnt pair */
	unsigned int bLoadBaseAddr[_IRQ_MAX];
};
static struct FW_RCNT_CTRL mFwRcnt = { {{0} }, {0}, {0}, {0}, {0} };
static unsigned char dma_en_recorder[_rt_dma_max_][ISP_RT_BUF_SIZE] = { {0} };
#endif
/*      */
static signed int ISP_RTBC_ENQUE(signed int dma, struct ISP_RT_BUF_INFO_STRUCT *prt_buf_info)
{
	signed int Ret = 0;
	signed int rt_dma = dma;
	unsigned int buffer_exist = 0;
	unsigned int i = 0;
	unsigned int index = 0;

	/* check max */
	if (pstRTBuf->ring_buf[rt_dma].total_count == ISP_RT_BUF_SIZE) {
		LOG_INF("[rtbc][ENQUE]:real	time buffer	number FULL:rt_dma(%d)", rt_dma);
		Ret = -EFAULT;
		/* break; */
	}

	/*      */
	/* spin_lock_irqsave(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
	/* check if     buffer exist */
	for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
		if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr == prt_buf_info->base_pAddr) {
			buffer_exist = 1;
			break;
		}
		/*      */
		if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr == 0)
			break;

	}
	/*      */
	if (buffer_exist) {
		/*      */
		if (pstRTBuf->ring_buf[rt_dma].data[i].bFilled != ISP_RTBC_BUF_EMPTY) {
			pstRTBuf->ring_buf[rt_dma].data[i].bFilled = ISP_RTBC_BUF_EMPTY;
			pstRTBuf->ring_buf[rt_dma].empty_count++;
			index = i;
		}
		/*      */
		/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL)     { */
		LOG_DBG("[rtbc][ENQUE]::buffer_exist(%d)/i(%d)/PA(0x%x)/bFilled(%d)/empty(%d)",
			buffer_exist,
			i,
			prt_buf_info->base_pAddr,
			pstRTBuf->ring_buf[rt_dma].data[i].bFilled,
			pstRTBuf->ring_buf[rt_dma].empty_count);
		/* } */

	} else {
		/* overwrite oldest     element if buffer is full */
		if (pstRTBuf->ring_buf[rt_dma].total_count == ISP_RT_BUF_SIZE) {
			LOG_INF("[ENQUE]:[rtbc]:buffer full(%d)",
				pstRTBuf->ring_buf[rt_dma].total_count);
		} else {
			/* first time add */
			index = pstRTBuf->ring_buf[rt_dma].total_count % ISP_RT_BUF_SIZE;
			/*      */
			pstRTBuf->ring_buf[rt_dma].data[index].memID = prt_buf_info->memID;
			pstRTBuf->ring_buf[rt_dma].data[index].size = prt_buf_info->size;
			pstRTBuf->ring_buf[rt_dma].data[index].base_vAddr =
				prt_buf_info->base_vAddr;
			pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr =
				prt_buf_info->base_pAddr;
			pstRTBuf->ring_buf[rt_dma].data[index].bFilled = ISP_RTBC_BUF_EMPTY;
			pstRTBuf->ring_buf[rt_dma].data[index].bufIdx = prt_buf_info->bufIdx;
			/*      */
			pstRTBuf->ring_buf[rt_dma].total_count++;
			pstRTBuf->ring_buf[rt_dma].empty_count++;
			/*      */
			/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL)     { */
			LOG_DBG
			("[rtbc][ENQUE]:dma(%d),index(%d),bufIdx(0x%x),PA(0x%x)/empty(%d)/total(%d)",
			 rt_dma, index, prt_buf_info->bufIdx,
			 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr,
			 pstRTBuf->ring_buf[rt_dma].empty_count,
			 pstRTBuf->ring_buf[rt_dma].total_count);
			/* } */
		}
	}
	/*      */

	/* count ==1 means DMA stalled already or NOT start     yet     */
	if (pstRTBuf->ring_buf[rt_dma].empty_count == 1) {
		if (_imgo_ == rt_dma) {
			/* set base_addr at     beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_IMGO_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		} else if (_rrzo_ == rt_dma) {
			/* set base_addr at     beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_RRZO_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		}
#if (ISP_RAW_D_SUPPORT == 1)
		else if (_imgo_d_ == rt_dma) {
			/* set base_addr at     beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_IMGO_D_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		} else if (_rrzo_d_ == rt_dma) {
			/* set base_addr at     beginning before VF_EN */
			ISP_WR32(ISP_REG_ADDR_RRZO_D_BASE_ADDR,
				 pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		}
#endif
		else if (_camsv_imgo_ == rt_dma) {
			ISP_WR32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR,
				pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);

		} else if (_camsv2_imgo_ == rt_dma) {
			ISP_WR32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR,
				pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
		}

		/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL)     { */
#if (ISP_RAW_D_SUPPORT == 1)
		LOG_DBG
		("[rtbc][ENQUE]:dma(%d),base_pAddr(0x%x)/imgo(0x%x)/rrzo(0x%x)/",
		 rt_dma, pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr,
		 ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR), ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR);
		LOG_DBG
		("imgo_d(0x%x)/rrzo_d(0x%x)/empty_count(%d)",
		 ISP_RD32(ISP_REG_ADDR_IMGO_D_BASE_ADDR),
		 ISP_RD32(ISP_REG_ADDR_RRZO_D_BASE_ADDR),
		 pstRTBuf->ring_buf[rt_dma].empty_count);
#else
		LOG_DBG
		("[rtbc][ENQUE]:dma(%d),base_pAddr(0x%x)/imgo(0x%x)/rrzo(0x%x)/camsv_imgo(0x%x)/empty_count(%d)",
		 rt_dma, pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr,
		 ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR), ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR),
		 ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR), pstRTBuf->ring_buf[rt_dma].empty_count);
#endif

		/* } */

#if     defined(_rtbc_use_cq0c_)
		/* Do nothing */
#else
		unsigned int reg_val = 0;

		/* disable FBC control to go on download */
		if (_imgo_ == rt_dma) {
			reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
			reg_val &= ~0x4000;
			ISP_WR32(ISP_REG_ADDR_IMGO_FBC, reg_val);
		} else {
			reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
			reg_val &= ~0x4000;
			ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC, reg_val);
		}

		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
			LOG_DBG("[rtbc][ENQUE]:dma(%d),disable fbc:IMGO(0x%x),IMG2O(0x%x)", rt_dma,
				ISP_RD32(ISP_REG_ADDR_IMGO_FBC), ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC));

#endif
		pstRTBuf->ring_buf[rt_dma].pre_empty_count = pstRTBuf->ring_buf[rt_dma].empty_count;

	}

	/*      */
	/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
	/*      */
	/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL)     { */
	LOG_DBG("[rtbc][ENQUE]:dma:(%d),start(%d),index(%d),empty_count(%d),base_pAddr(0x%x)",
		rt_dma,
		pstRTBuf->ring_buf[rt_dma].start,
		index,
		pstRTBuf->ring_buf[rt_dma].empty_count,
		pstRTBuf->ring_buf[rt_dma].data[index].base_pAddr);
	/* } */
	/*      */
	return Ret;
}

static void ISP_FBC_DUMP(unsigned int dma_id, unsigned int VF_1, unsigned int VF_2,
			    unsigned int VF_3, unsigned int VF_4)
{
	unsigned int z;
	char str[128];
	signed int strLeng = sizeof(str) - 1;
	char str2[_rt_dma_max_];
	unsigned int dma;

	LOG_INF("================================\n");
	LOG_INF("pass1 timeout log(timeout port:%d)", dma_id);
	LOG_INF("================================\n");
	str[0] = '\0';
	LOG_INF("current activated dmaport");
	for (z = 0; z < _rt_dma_max_; z++) {
		sprintf(str2, "%d_", pstRTBuf->ring_buf[z].active);
		strncat(str, str2, strLeng - strlen(str));
	}
	LOG_INF("%s", str);
	LOG_INF("================================\n");
	if (VF_1) {
		LOG_INF("imgo:");
		dma = _imgo_;
		str[0] = '\0';
		LOG_INF("current fillled buffer(%d):\n", pstRTBuf->ring_buf[dma].total_count);
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
		LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
		LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", dma_en_recorder[dma][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:inc record\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", mFwRcnt.INC[_IRQ][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("RCNT_RECORD: dma idx =	%d\n", mFwRcnt.DMA_IDX[dma]);
		LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ]);
		LOG_INF("RCNT_RECORD: cur idx =	%d\n", mFwRcnt.curIdx[_IRQ]);
		LOG_INF("RCNT_RECORD: bLoad	= %d\n", mFwRcnt.bLoadBaseAddr[_IRQ]);
		LOG_INF("================================\n");
		LOG_INF("rrzo:");
		dma = _rrzo_;
		str[0] = '\0';
		LOG_INF("current fillled buffer(%d):\n", pstRTBuf->ring_buf[dma].total_count);
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
		LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
		LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", dma_en_recorder[dma][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:inc record\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", mFwRcnt.INC[_IRQ][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("RCNT_RECORD: dma idx =	%d\n", mFwRcnt.DMA_IDX[dma]);
		LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ]);
		LOG_INF("RCNT_RECORD: cur idx =	%d\n", mFwRcnt.curIdx[_IRQ]);
		LOG_INF("RCNT_RECORD: bLoad	= %d\n", mFwRcnt.bLoadBaseAddr[_IRQ]);
		LOG_INF("================================\n");
	}

	if (VF_2) {
		LOG_INF("imgo_d:");
		dma = _imgo_d_;
		str[0] = '\0';
		LOG_INF("current fillled buffer(%d):\n", pstRTBuf->ring_buf[dma].total_count);
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
		LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
		LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", dma_en_recorder[dma][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:inc record\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", mFwRcnt.INC[_IRQ_D][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("RCNT_RECORD: dma idx =	%d\n", mFwRcnt.DMA_IDX[dma]);
		LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ_D]);
		LOG_INF("RCNT_RECORD: cur idx =	%d\n", mFwRcnt.curIdx[_IRQ_D]);
		LOG_INF("RCNT_RECORD: bLoad	= %d\n", mFwRcnt.bLoadBaseAddr[_IRQ_D]);
		LOG_INF("================================\n");
		LOG_INF("rrzo_d:");
		dma = _rrzo_d_;
		str[0] = '\0';
		LOG_INF("current fillled buffer(%d):\n", pstRTBuf->ring_buf[dma].total_count);
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
		LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
		LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", dma_en_recorder[dma][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:inc record\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", mFwRcnt.INC[_IRQ_D][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("RCNT_RECORD: dma idx =	%d\n", mFwRcnt.DMA_IDX[dma]);
		LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_IRQ_D]);
		LOG_INF("RCNT_RECORD: cur idx =	%d\n", mFwRcnt.curIdx[_IRQ_D]);
		LOG_INF("RCNT_RECORD: bLoad	= %d\n", mFwRcnt.bLoadBaseAddr[_IRQ_D]);
		LOG_INF("================================\n");
	}

	if (VF_3) {
		LOG_INF("camsv_imgo:");
		dma = _camsv_imgo_;
		{
			str[0] = '\0';
			LOG_INF("current fillled buffer(%d):\n",
				pstRTBuf->ring_buf[dma].total_count);
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
				strncat(str, str2, strLeng - strlen(str));
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
			LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
			LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", dma_en_recorder[dma][z]);
				strncat(str, str2, strLeng - strlen(str));
				LOG_INF("magic queue=0x%x", g_magQue[_camsv_1][z]);
			}
			LOG_INF("%s", str);
			LOG_INF("================================\n");
			LOG_INF("RCNT_RECORD:inc record\n");
			str[0] = '\0';
			for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
				sprintf(str2, "%d_", mFwRcnt.INC[_CAMSV_IRQ][z]);
				strncat(str, str2, strLeng - strlen(str));
			}
			LOG_INF("%s", str);
			LOG_INF("RCNT_RECORD: dma idx =	%d\n", mFwRcnt.DMA_IDX[dma]);
			LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_CAMSV_IRQ]);
			LOG_INF("RCNT_RECORD: cur idx =	%d\n", mFwRcnt.curIdx[_CAMSV_IRQ]);
			LOG_INF("RCNT_RECORD: bLoad	= %d\n", mFwRcnt.bLoadBaseAddr[_CAMSV_IRQ]);
			LOG_INF("================================\n");
		}
	}

	if (VF_4) {
		LOG_INF("camsv2_imgo:");
		dma = _camsv2_imgo_;
		str[0] = '\0';
		LOG_INF("current fillled buffer(%d):\n", pstRTBuf->ring_buf[dma].total_count);
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", pstRTBuf->ring_buf[dma].data[z].bFilled);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("cur_start_idx:%d", pstRTBuf->ring_buf[dma].start);
		LOG_INF("cur_read_idx=%d", pstRTBuf->ring_buf[dma].read_idx);
		LOG_INF("cur_empty_cnt:%d", pstRTBuf->ring_buf[dma].empty_count);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:cur dma_en_recorder\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", dma_en_recorder[dma][z]);
			strncat(str, str2, strLeng - strlen(str));
			LOG_INF("magic queue=0x%x", g_magQue[_camsv_2][z]);
		}
		LOG_INF("%s", str);
		LOG_INF("================================\n");
		LOG_INF("RCNT_RECORD:inc record\n");
		str[0] = '\0';
		for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
			sprintf(str2, "%d_", mFwRcnt.INC[_CAMSV_D_IRQ][z]);
			strncat(str, str2, strLeng - strlen(str));
		}
		LOG_INF("%s", str);
		LOG_INF("RCNT_RECORD: dma idx =	%d\n", mFwRcnt.DMA_IDX[dma]);
		LOG_INF("RCNT_RECORD: read idx = %d\n", mFwRcnt.rdIdx[_CAMSV_D_IRQ]);
		LOG_INF("RCNT_RECORD: cur idx =	%d\n", mFwRcnt.curIdx[_CAMSV_D_IRQ]);
		LOG_INF("RCNT_RECORD: bLoad	= %d\n", mFwRcnt.bLoadBaseAddr[_CAMSV_D_IRQ]);
		LOG_INF("================================\n");
	}
}

static signed int ISP_RTBC_DEQUE(signed int dma, struct ISP_DEQUE_BUF_INFO_STRUCT *pdeque_buf)
{
	signed int Ret = 0;
	signed int rt_dma = dma;
	unsigned int i = 0;
	unsigned int index = 0, out = 0;

	/* spin_lock_irqsave(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
		LOG_DBG("[rtbc][DEQUE]+");

	/*      */
	pdeque_buf->count = 0;
	pdeque_buf->img_cnt = 0;

	DMA_TRANS(dma, out);
	pdeque_buf->sof_cnt = sof_count[out];
	/* in SOF, "start" is next buffer index */
	for (i = 0; i < pstRTBuf->ring_buf[rt_dma].total_count; i++) {
		index =
			(pstRTBuf->ring_buf[rt_dma].start + i) % pstRTBuf->ring_buf[rt_dma].total_count;

		if (pstRTBuf->ring_buf[rt_dma].data[index].bFilled == ISP_RTBC_BUF_FILLED) {
			pstRTBuf->ring_buf[rt_dma].data[index].bFilled = ISP_RTBC_BUF_LOCKED;
			pdeque_buf->count = P1_DEQUE_CNT;
			break;
		}
	}
	/*      */
	if (pdeque_buf->count == 0) {
		/* queue buffer status */
		LOG_DBG
		("[rtbc][DEQUE]:dma(%d),start(%d),total(%d),empty(%d), pdeque_buf->count(%d)	",
		 rt_dma, pstRTBuf->ring_buf[rt_dma].start,
		 pstRTBuf->ring_buf[rt_dma].total_count,
		 pstRTBuf->ring_buf[rt_dma].empty_count, pdeque_buf->count);
		/*      */
		for (i = 0; i <= pstRTBuf->ring_buf[rt_dma].total_count - 1; i++) {
			LOG_DBG("[rtbc][DEQUE]Buf List:%d/%d/0x%x/0x%llx/0x%x/%d/  ",
				i,
				pstRTBuf->ring_buf[rt_dma].data[i].memID,
				pstRTBuf->ring_buf[rt_dma].data[i].size,
				pstRTBuf->ring_buf[rt_dma].data[i].base_vAddr,
				pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr,
				pstRTBuf->ring_buf[rt_dma].data[i].bFilled);
		}
	}
	/*      */
	if (pdeque_buf->count) {
		/* Fill buffer head     */
		/* "start" is current working index     */
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
			LOG_DBG("[rtbc][DEQUE]:rt_dma(%d)/index(%d)/empty(%d)/total(%d)",
				rt_dma,
				index,
				pstRTBuf->ring_buf[rt_dma].empty_count,
				pstRTBuf->ring_buf[rt_dma].total_count);
		}
		/*      */
		for (i = 0; i < pdeque_buf->count; i++) {
			pdeque_buf->data[i].memID =
				pstRTBuf->ring_buf[rt_dma].data[index + i].memID;
			pdeque_buf->data[i].size = pstRTBuf->ring_buf[rt_dma].data[index + i].size;
			pdeque_buf->data[i].base_vAddr =
				pstRTBuf->ring_buf[rt_dma].data[index + i].base_vAddr;
			pdeque_buf->data[i].base_pAddr =
				pstRTBuf->ring_buf[rt_dma].data[index + i].base_pAddr;
			pdeque_buf->data[i].timeStampS =
				pstRTBuf->ring_buf[rt_dma].data[index + i].timeStampS;
			pdeque_buf->data[i].timeStampUs =
				pstRTBuf->ring_buf[rt_dma].data[index + i].timeStampUs;
			/*      */
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
				LOG_DBG
				("[rtbc][DEQUE]:index(%d)/PA(0x%x)/memID(%d)/size(0x%x)/VA(0x%llx)",
				 index + i, pdeque_buf->data[i].base_pAddr,
				 pdeque_buf->data[i].memID, pdeque_buf->data[i].size,
				 pdeque_buf->data[i].base_vAddr);
			}
		}
		/*      */
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
			LOG_DBG("[rtbc][DEQUE]-");

		/*      */
		/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
		/*      */
	} else {
		/*      */
		/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
		LOG_PR_ERR("[rtbc][DEQUE]:no filled buffer");
		Ret = -EFAULT;
	}

	return Ret;
}

#ifdef _MAGIC_NUM_ERR_HANDLING_
#define _INVALID_FRM_CNT_ 0xFFFF
#define _MAX_FRM_CNT_ 0xFF

#define _UNCERTAIN_MAGIC_NUM_FLAG_ 0x40000000
#define _DUMMY_MAGIC_                      0x20000000
static unsigned int m_LastMNum[_rt_dma_max_] = { 0 };        /* imgo/rrzo */

#endif
/* static long ISP_Buf_CTRL_FUNC(unsigned int Param) */
static long ISP_Buf_CTRL_FUNC(unsigned long Param)
{
	signed int Ret = 0;
	signed int rt_dma;
	unsigned int reg_val = 0;
	unsigned int reg_val2 = 0;
	unsigned int camsv_reg_cal[2] = { 0, 0 };
	unsigned int i = 0;
	unsigned int x = 0;
	unsigned int iBuf = 0;
	unsigned int size = 0;
	unsigned int bWaitBufRdy = 0;
	struct ISP_BUFFER_CTRL_STRUCT rt_buf_ctrl;
	bool _bFlag = MTRUE;
	/* unsigned int buffer_exist = 0; */
	union CQ_RTBC_FBC p1_fbc[_rt_dma_max_];
	/* unsigned int p1_fbc_reg[_rt_dma_max_]; */
	unsigned long p1_fbc_reg[_rt_dma_max_];
	/* unsigned int p1_dma_addr_reg[_rt_dma_max_]; */
	unsigned long p1_dma_addr_reg[_rt_dma_max_];
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */
	struct ISP_RT_BUF_INFO_STRUCT rt_buf_info;
	struct ISP_DEQUE_BUF_INFO_STRUCT deque_buf;
	enum eISPIrq irqT = _IRQ_MAX;
	enum eISPIrq irqT_Lock = _IRQ_MAX;
	bool CurVF_En = MFALSE;

	for (i = 0; i < _rt_dma_max_; i++)
		p1_dma_addr_reg[i] = 0;
	/*      */
	if (pstRTBuf == NULL) {
		LOG_PR_ERR("[rtbc]NULL	pstRTBuf");
		return -EFAULT;
	}
	/*      */
	if (copy_from_user(&rt_buf_ctrl, (void __user *)Param, sizeof(struct ISP_BUFFER_CTRL_STRUCT)) == 0) {
		rt_dma = rt_buf_ctrl.buf_id;
		/*      */
		/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL)     { */
		/* LOG_DBG("[rtbc]ctrl(0x%x)/buf_id(0x%x)/data_ptr(0x%x)/ex_data_ptr(0x%x)\n", */
		/* rt_buf_ctrl.ctrl, \ */
		/* rt_buf_ctrl.buf_id, \ */
		/* rt_buf_ctrl.data_ptr, \ */
		/* rt_buf_ctrl.ex_data_ptr); */
		/* } */
		/*      */
		if (_imgo_ == rt_dma || _rrzo_ == rt_dma || _imgo_d_ == rt_dma || _rrzo_d_ == rt_dma
		    || _camsv_imgo_ == rt_dma || _camsv2_imgo_ == rt_dma) {

#if     defined(_rtbc_use_cq0c_)
			/* do nothing */
#else                           /* for camsv */
			if ((_camsv_imgo_ == rt_dma) || (_camsv2_imgo_ == rt_dma))
				_bFlag = MTRUE;
			else
				_bFlag = MFALSE;
#endif

			if (_bFlag == MTRUE) {
				if ((rt_buf_ctrl.ctrl == ISP_RT_BUF_CTRL_ENQUE) ||
				    (rt_buf_ctrl.ctrl == ISP_RT_BUF_CTRL_DEQUE) ||
				    (rt_buf_ctrl.ctrl == ISP_RT_BUF_CTRL_IS_RDY) ||
				    (rt_buf_ctrl.ctrl == ISP_RT_BUF_CTRL_ENQUE_IMD)) {
					/*      */
					p1_fbc[_imgo_].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
					p1_fbc[_rrzo_].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);
					p1_fbc[_imgo_d_].Reg_val =
						ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
					p1_fbc[_rrzo_d_].Reg_val =
						ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);

					p1_fbc_reg[_imgo_] = ISP_REG_ADDR_IMGO_FBC;
					p1_fbc_reg[_rrzo_] = ISP_REG_ADDR_RRZO_FBC;
					p1_fbc_reg[_imgo_d_] = ISP_REG_ADDR_IMGO_D_FBC;
					p1_fbc_reg[_rrzo_d_] = ISP_REG_ADDR_RRZO_D_FBC;

					p1_dma_addr_reg[_imgo_] = ISP_REG_ADDR_IMGO_BASE_ADDR;
					p1_dma_addr_reg[_rrzo_] = ISP_REG_ADDR_RRZO_BASE_ADDR;
#if (ISP_RAW_D_SUPPORT == 1)
					p1_dma_addr_reg[_imgo_d_] = ISP_REG_ADDR_IMGO_D_BASE_ADDR;
					p1_dma_addr_reg[_rrzo_d_] = ISP_REG_ADDR_RRZO_D_BASE_ADDR;
#endif
					p1_fbc[_camsv_imgo_].Reg_val =
						ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC);
					p1_fbc[_camsv2_imgo_].Reg_val =
						ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC);

					p1_fbc_reg[_camsv_imgo_] = ISP_REG_ADDR_CAMSV_IMGO_FBC;
					p1_fbc_reg[_camsv2_imgo_] = ISP_REG_ADDR_CAMSV2_IMGO_FBC;

					p1_dma_addr_reg[_camsv_imgo_] =
						ISP_REG_ADDR_IMGO_SV_BASE_ADDR;
					p1_dma_addr_reg[_camsv2_imgo_] =
						ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR;
					/*      */
#if     0
					/**/    if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
						LOG_DBG
						("[rtbc]:ctrl(%d),o(0x%x),zo(0x%x),camsv(0x%x/0x%x)",
						 rt_buf_ctrl.ctrl, p1_fbc[_imgo_].Reg_val,
						 p1_fbc[_rrzo_].Reg_val,
						 p1_fbc[_camsv_imgo_].Reg_val,
						 p1_fbc[_camsv2_imgo_].Reg_val);
					}
#endif
				}
			}
		} else {
#ifdef _rtbc_buf_que_2_0_
			if (rt_buf_ctrl.ctrl != ISP_RT_BUF_CTRL_DMA_EN)
#endif
			{
				LOG_PR_ERR("[rtbc]invalid dma channel(%d)", rt_dma);
				return -EFAULT;
			}
		}
		/*      */
		switch (rt_buf_ctrl.ctrl) {
		/* warning: enumeration value 'ISP_RT_BUF_CTRL_EXCHANGE_ENQUE' not handled in switch [-Wswitch] */
		case ISP_RT_BUF_CTRL_EXCHANGE_ENQUE:
			break;

		/* make sure rct_inc will be pulled     at the same     vd.     */
		case ISP_RT_BUF_CTRL_ENQUE:
		case ISP_RT_BUF_CTRL_ENQUE_IMD:
			/*      */
			if (copy_from_user
			    (&rt_buf_info, (void __user *)rt_buf_ctrl.data_ptr,
			     sizeof(struct ISP_RT_BUF_INFO_STRUCT)) == 0) {
				reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
				reg_val2 = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON);
				camsv_reg_cal[0] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG_VF_CON);
				camsv_reg_cal[1] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_VF_CON);
				/* VF start     already */
				/* bool CurVF_En =     MFALSE; */
				if ((_imgo_ == rt_dma) || (_rrzo_ == rt_dma)) {
					if (reg_val & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				} else if ((_imgo_d_ == rt_dma) || (_rrzo_d_ == rt_dma)) {
					if (reg_val2 & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				} else if (_camsv_imgo_ == rt_dma) {
					if (camsv_reg_cal[0] & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				} else if (_camsv2_imgo_ == rt_dma) {
					if (camsv_reg_cal[1] & 0x1)
						CurVF_En = MTRUE;
					else
						CurVF_En = MFALSE;
				}

				if (CurVF_En) {
					if (_bFlag == MTRUE) {
						unsigned int ch_imgo = 0, ch_rrzo = 0;
						unsigned int z;
						/*      */
					switch (rt_dma) {
					case _imgo_:
						/**/
					case _rrzo_:
						irqT = _IRQ;
						ch_imgo = _imgo_;
						ch_rrzo = _rrzo_;
						irqT_Lock = _IRQ;
						break;
					case _imgo_d_:
						/**/
					case _rrzo_d_:
						irqT = _IRQ_D;
						ch_imgo = _imgo_d_;
						ch_rrzo = _rrzo_d_;
						irqT_Lock = _IRQ;
						break;
					case _camsv_imgo_:
						irqT_Lock = _CAMSV_IRQ;
						irqT = _CAMSV_IRQ;
						break;
					case _camsv2_imgo_:
						irqT_Lock = _CAMSV_D_IRQ;
						irqT = _CAMSV_D_IRQ;
						break;
					default:
						irqT_Lock = _IRQ;
						irqT = _IRQ;
						LOG_PR_ERR("[rtbc]N.S.(%d)\n", rt_dma);
						break;
					}
#if     0
						static unsigned int RTBC_DBG_test;

					if (RTBC_DBG_test++ > 3) {
						RTBC_DBG_test -= 3;
						ISP_FBC_DUMP(rt_dma, 1, 0, 0, 0);
					}
#endif
						/*      copy_from_user()/copy_to_user() might sleep when page fault,*/
						/*   it can't use in atomic context, e.g. spin_lock_irqsave()      */
						/* spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags); */
			if (rt_buf_ctrl.ex_data_ptr != 0) {
						/* borrow deque_buf.data memory , in order to shirnk memory*/
						/*	required,avoid compile err */
				if (copy_from_user(&deque_buf.data[0],
					(void __user *)rt_buf_ctrl.ex_data_ptr,
					sizeof(struct ISP_RT_BUF_INFO_STRUCT)) == 0) {
					spin_lock_irqsave(&(IspInfo.SpinLockIrq
											[irqT_Lock]), flags);
								/*      */
							i = 0;
			if (deque_buf.data[0].bufIdx !=
							0xFFFF) {
								/* replace the specific buffer with the same bufIdx*/
								/* LOG_PR_ERR("[rtbc][replace2]Search By Idx"); */
				for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
					if (pstRTBuf->ring_buf[rt_dma].data[i].bufIdx ==
									deque_buf.data[0].bufIdx) {
						break;
								}
							}
									/**/
					} else {
								/*      */
								/* LOG_PR_ERR("[rtbc][replace2]Search By Addr+"); */
			for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
				if (pstRTBuf->ring_buf[rt_dma].
						data[i].base_pAddr ==
						rt_buf_info.base_pAddr) {
									/* LOG_PR_ERR("[rtbc][replace2]*/
									/*Search By   Addr i[%d]", i); */
					break;
						}
					}
					}

					if (i == ISP_RT_BUF_SIZE) {
									/* error: can't search the buffer... */
						LOG_PR_ERR
						("[rtbc][replace2]error Can't get the idx-(0x%x)/Addr(0x%x) buf\n",
									 deque_buf.data[0].
									 bufIdx,
									 rt_buf_info.
									 base_pAddr);
									spin_unlock_irqrestore(&
											       (IspInfo.
												SpinLockIrq
												[irqT_Lock]),
											       flags);
									IRQ_LOG_PRINTER(irqT, 0,
											_LOG_DBG);

					for (i = 0; i < ISP_RT_BUF_SIZE; i += 4) {
						LOG_PR_ERR
						("[rtbc][replace2]error idx-(0x%x/0x%x/0x%x/0x%x)\n",
										 pstRTBuf->ring_buf[rt_dma].
										 data[i + 0].bufIdx,
										 pstRTBuf->ring_buf[rt_dma].
										 data[i + 1].bufIdx,
										 pstRTBuf->ring_buf[rt_dma].
										 data[i + 2].bufIdx,
										 pstRTBuf->ring_buf[rt_dma].
										 data[i + 3].bufIdx);
					}
					return -EFAULT;
					}
					LOG_DBG(
					"[rtbc]replace2]dma(%d),old(%d)	PA(0x%x) VA(0x%llx)",
									rt_dma, i, pstRTBuf->ring_buf[rt_dma].data[i].
									base_pAddr, pstRTBuf->ring_buf[rt_dma].data[i].
									base_vAddr);
					/* IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,*/
					/*"[rtbc][replace2]dma(%d),idx(%d) PA(0x%x_0x%x)\n",*/
					/*rt_dma, i, pstRTBuf->ring_buf[rt_dma].*/
					/*data[i].base_pAddr,*/
					/*deque_buf.data[0].base_pAddr);*/
					/* spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT]),*/
					/*flags); */
								pstRTBuf->ring_buf[rt_dma].data[i].
								memID = deque_buf.data[0].memID;
								pstRTBuf->ring_buf[rt_dma].data[i].
								size = deque_buf.data[0].size;
								pstRTBuf->ring_buf[rt_dma].data[i].
								base_pAddr =
									deque_buf.data[0].base_pAddr;
								pstRTBuf->ring_buf[rt_dma].data[i].
								base_vAddr =
									deque_buf.data[0].base_vAddr;
								pstRTBuf->ring_buf[rt_dma].data[i].
								bFilled = ISP_RTBC_BUF_EMPTY;
								pstRTBuf->ring_buf[rt_dma].data[i].
								image.frm_cnt =
									_INVALID_FRM_CNT_;

#ifdef _rtbc_buf_que_2_0_
				if (pstRTBuf->ring_buf[rt_dma].empty_count <
								    pstRTBuf->ring_buf[rt_dma].total_count)
					pstRTBuf->ring_buf[rt_dma].empty_count++;
				else {
					spin_unlock_irqrestore(&
											       (IspInfo.
												SpinLockIrq
												[irqT_Lock]),
											       flags);
									IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
									LOG_PR_ERR
									("[rtbc]dma(%d),PA(0x%x),over enque_1",
									 rt_dma, pstRTBuf->ring_buf[rt_dma].
									 data[i].base_pAddr);
									return -EFAULT;
					}
							/* LOG_INF("RTBC_DBG7 e_dma_%d:%d %d %d\n",*/
							/*   rt_dma,pstRTBuf->ring_buf[rt_dma].data[0].bFilled,*/
							/*   pstRTBuf->ring_buf[rt_dma].data[1].bFilled,*/
							/*   pstRTBuf->ring_buf[rt_dma].data[2].bFilled); */
#else
								pstRTBuf->ring_buf[rt_dma].
								empty_count++;
#endif
								/* spin_unlock_irqrestore(*/
								/*&(IspInfo.SpinLockIrq[irqT]), flags); */
								/**/    /**/
					} else {
						/* spin_unlock_irqrestore(*/
						/*&(IspInfo.SpinLockIrq[irqT_Lock]), flags); */
						LOG_PR_ERR
						("[rtbc][ENQUE_ext]:copy_from_user fail");
								/* LOG_PR_ERR("[rtbc][ENQUE_ext]:copy_from_user fail,*/
								/*dst_buf(0x%lx), user_buf(0x%lx)",*/
								/*   &deque_buf.data[0],  rt_buf_ctrl.ex_data_ptr); */
								return -EAGAIN;
					}
							/**/    /**/
				} else {        /*     this case for camsv     & pass1 fw rtbc */
					spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
				for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
					if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr ==
						rt_buf_info.base_pAddr) {
									/* LOG_DBG("[rtbc]dma(%d),old(%d) PA(0x%x)*/
									/*  VA(0x%x)",*/
									/*   rt_dma,i,pstRTBuf->ring_buf[rt_dma].*/
									/*   data[i].base_pAddr,*/
									/*   pstRTBuf->ring_buf[rt_dma].data[i].*/
									/*   base_vAddr); */
									/* spin_lock_irqsave(*/
									/*&(IspInfo.SpinLockIrq[irqT]), flags); */
#if (HAL3_IPBASE == 1)
				/* update vAddr for deciding which buffer had been changed to dummy frame. */
					pstRTBuf->ring_buf[rt_dma].data[i].base_vAddr = rt_buf_info.base_vAddr;
#endif
						pstRTBuf->ring_buf[rt_dma].data[i].bFilled =
										ISP_RTBC_BUF_EMPTY;
						pstRTBuf->ring_buf[rt_dma].
										data[i].image.frm_cnt =
										_INVALID_FRM_CNT_;
#ifdef _rtbc_buf_que_2_0_
					if (pstRTBuf->ring_buf[rt_dma].empty_count <
						pstRTBuf->ring_buf[rt_dma].total_count)
						pstRTBuf->ring_buf[rt_dma].empty_count++;
					else {
						spin_unlock_irqrestore(&(IspInfo.SpinLockIrq
							[irqT_Lock]), flags);
							IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
							LOG_PR_ERR("[rtbc]error:dma(%d),PA(0x%x),over enque_2", rt_dma,
							pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr);
						return -EFAULT;
					}

									/* double check */
					if (1) {
					if (rt_buf_info.bufIdx !=
						pstRTBuf->ring_buf[rt_dma].data[i].bufIdx)
						LOG_INF("[rtbc][replace2]error:	BufIdx MisMatch. 0x%x/0x%x",
						rt_buf_info.bufIdx,
						pstRTBuf->ring_buf[rt_dma].data[i].bufIdx);
					}
#else
					pstRTBuf->ring_buf[rt_dma].empty_count++;
#endif
						/* spin_unlock_irqrestore(&(IspInfo.*/
						/*	SpinLockIrq[irqT]), flags); */
						/* LOG_DBG("[rtbc]dma(%d),new(%d)*/
						/*	PA(0x%x) VA(0x%x)",*/
						/*	rt_dma,i,pstRTBuf->ring_buf[rt_dma].*/
						/*	data[i].base_pAddr,pstRTBuf->*/
						/*	ring_buf[rt_dma].data[i].base_vAddr);*/
					break;
					}
				}
				if (i == ISP_RT_BUF_SIZE) {
					for (x = 0; x < ISP_RT_BUF_SIZE; x++)
						LOG_DBG("[rtbc]dma(%d),idx(%d) PA(0x%x) VA(0x%llx)",
						rt_dma, x, pstRTBuf->ring_buf[rt_dma].data[x].base_pAddr,
						pstRTBuf->ring_buf[rt_dma].data[x].base_vAddr);

					LOG_INF("[rtbc][replace3]can't find thespecified Addr(0x%x)\n",
					rt_buf_info.base_pAddr);
					return -EFAULT;
					}
				}
						/* set RCN_INC = 1; */
						/* RCNT++ */
						/* FBC_CNT-- */


						/* RCNT_INC++ */
#ifdef _rtbc_buf_que_2_0_

		if (rt_buf_ctrl.ctrl == ISP_RT_BUF_CTRL_ENQUE) {
						/* make sure rct_inc will be pulled     at the same     vd.     */
			if ((irqT == _IRQ) || (irqT == _IRQ_D)) {
				if ((pstRTBuf->ring_buf[ch_imgo].active == MTRUE)
									&& (pstRTBuf->ring_buf[ch_rrzo].
									active == MTRUE)) {
				if (rt_buf_ctrl.ex_data_ptr != 0) {
					if ((p1_fbc[rt_dma].Bits.FB_NUM ==
										p1_fbc[rt_dma].Bits.FBC_CNT)
										|| ((p1_fbc[rt_dma].Bits.FB_NUM - 1) ==
										p1_fbc[rt_dma].Bits.FBC_CNT)) {
						mFwRcnt.bLoadBaseAddr
											[irqT] = MTRUE;
							}
						}
								dma_en_recorder[rt_dma][mFwRcnt.
									 DMA_IDX[rt_dma]] = MTRUE;
								mFwRcnt.DMA_IDX[rt_dma] =
										(++mFwRcnt.DMA_IDX[rt_dma] >=
										 ISP_RT_BUF_SIZE)
										? (mFwRcnt.DMA_IDX[rt_dma] -
										   ISP_RT_BUF_SIZE)
										: (mFwRcnt.DMA_IDX[rt_dma]);
									/* LOG_INF("RTBC_DBG1:%d %d     %d\n",*/
									/*   rt_dma,dma_en_recorder[rt_dma]*/
									/*[mFwRcnt.DMA_IDX[rt_dma]],*/
									/*mFwRcnt.DMA_IDX[rt_dma]); */
				for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
					if (dma_en_recorder[ch_imgo]
										    [mFwRcnt.rdIdx[irqT]] &&
										    dma_en_recorder[ch_rrzo]
										    [mFwRcnt.rdIdx[irqT]]) {
						mFwRcnt.INC[irqT]
											[mFwRcnt.curIdx[irqT]++] = 1;
											dma_en_recorder[ch_imgo]
											[mFwRcnt.rdIdx[irqT]]
											= dma_en_recorder[ch_rrzo]
											[mFwRcnt.rdIdx[irqT]]
											= MFALSE;
											mFwRcnt.rdIdx[irqT] =
											(++mFwRcnt.rdIdx[irqT]
											>= ISP_RT_BUF_SIZE)
											?
											(mFwRcnt.rdIdx[irqT]
											- ISP_RT_BUF_SIZE)
											:
											(mFwRcnt.rdIdx[irqT]);
										/* LOG_INF("RTBC_DBG2:%d %d\n",*/
										/*mFwRcnt.rdIdx[irqT],mFwRcnt.*/
										/*curIdx[irqT]); */

					} else {
						break;
						}
						}
									/**/            /**/
		} else {
/* rcnt_sync only work when multi-dma ch enabled. but in order to support multi-enque, these mech. also to be */
		/* worked under 1 dma ch enabled */
			if (pstRTBuf->ring_buf[rt_dma].active == MTRUE) {
				if (rt_buf_ctrl.ex_data_ptr != 0) {
					if (((p1_fbc[rt_dma].Bits.FB_NUM - p1_fbc[rt_dma].Bits.FBC_CNT) == 0)
						|| (((p1_fbc[rt_dma].Bits.FB_NUM - 1)
						- p1_fbc[rt_dma].Bits.FBC_CNT) == 0)) {
						mFwRcnt.bLoadBaseAddr[irqT] = MTRUE;
					}
				}
				dma_en_recorder[rt_dma][mFwRcnt.DMA_IDX[rt_dma]] = MTRUE;
				mFwRcnt.DMA_IDX[rt_dma]	= (++mFwRcnt.DMA_IDX[rt_dma] >= ISP_RT_BUF_SIZE) ?
					(mFwRcnt.DMA_IDX[rt_dma] - ISP_RT_BUF_SIZE) :
					(mFwRcnt.DMA_IDX[rt_dma]);

				for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
					if (dma_en_recorder[rt_dma][mFwRcnt.rdIdx[irqT]]) {
						mFwRcnt.INC[irqT][mFwRcnt.curIdx[irqT]++] = 1;
						dma_en_recorder[rt_dma][mFwRcnt.rdIdx[irqT]] = MFALSE;
						mFwRcnt.rdIdx[irqT] =
							(++mFwRcnt.rdIdx[irqT] >= ISP_RT_BUF_SIZE) ?
							(mFwRcnt.rdIdx[irqT] - ISP_RT_BUF_SIZE)	:
							(mFwRcnt.rdIdx[irqT]);
					/**/
					} else {
						break;
					}
				}
			/**/
			} else {
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
				LOG_PR_ERR("[rtbc]error:dma(%d) are not being activated(%d)", rt_dma,
					pstRTBuf->ring_buf[rt_dma].active);
				return -EFAULT;
			}
		}
	/**/
	} else {        /*     camsv case */
		if (pstRTBuf->ring_buf[rt_dma].active == MTRUE) {
			if (rt_dma == _camsv_imgo_)
				enqueImd[_camsv_1] = MFALSE;
			else if (rt_dma == _camsv2_imgo_)
				enqueImd[_camsv_2] = MFALSE;
			if (rt_buf_ctrl.ex_data_ptr != 0) {
				if (((p1_fbc[rt_dma].Bits.FB_NUM - p1_fbc[rt_dma].Bits.FBC_CNT) == 0)
					|| ((p1_fbc[rt_dma].Bits.FB_NUM - 1) ==
					p1_fbc[rt_dma].Bits.FBC_CNT)) {
					mFwRcnt.bLoadBaseAddr[irqT] = MTRUE;
				}
			}
			dma_en_recorder[rt_dma][mFwRcnt.DMA_IDX[rt_dma]] = MTRUE;
			mFwRcnt.DMA_IDX[rt_dma] =
				(++mFwRcnt.DMA_IDX[rt_dma] >= ISP_RT_BUF_SIZE) ?
				(mFwRcnt.DMA_IDX[rt_dma] - ISP_RT_BUF_SIZE) :
				(mFwRcnt.DMA_IDX[rt_dma]);

				for (z = 0; z < ISP_RT_BUF_SIZE; z++) {
					if (dma_en_recorder[rt_dma][mFwRcnt.rdIdx[irqT]]) {
						mFwRcnt.INC[irqT][mFwRcnt.curIdx[irqT]++] = 1;
						dma_en_recorder[rt_dma][mFwRcnt.rdIdx[irqT]]
							= MFALSE;
						mFwRcnt.rdIdx[irqT] =
							(++mFwRcnt.rdIdx[irqT] >= ISP_RT_BUF_SIZE) ?
							(mFwRcnt.rdIdx[irqT] - ISP_RT_BUF_SIZE)	:
							(mFwRcnt.rdIdx[irqT]);
					} else {
						break;
					}
				}
		/**/
		} else {
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
			LOG_PR_ERR("[rtbc]error:dma(%d) are not being activated(%d)",
				rt_dma,	pstRTBuf->ring_buf[rt_dma].active);
			return -EFAULT;
		}
	}
	/**/
					} else {        /* immediate enque mode */
						unsigned int _openedDma = 1;
						bool _bypass = MFALSE;

					if ((MTRUE ==
						pstRTBuf->ring_buf[ch_imgo].active)
							&& (MTRUE ==
							pstRTBuf->ring_buf[ch_rrzo].
							active)) {
							/* record wheather all enabled dma r alredy enqued, */
							/* rcnt_inc     will only be pulled*/
							/*  to high once all enabled dma r enqued. */
							/* inorder to reduce the probability of*/
							/*crossing vsync.*/
							/* this global par. r no use under immediate mode, */
							/* borrow this to shirk memory */
						dma_en_recorder[rt_dma][0] = MTRUE;
							_openedDma = 2;
					if ((dma_en_recorder[ch_imgo][0] == MTRUE)
							&& (dma_en_recorder[ch_rrzo][0]
								== MTRUE))
						dma_en_recorder[ch_imgo][0] =
						dma_en_recorder[ch_rrzo][0] = MFALSE;
					else {
						_bypass = MTRUE;
						}

					}
				if (_bypass == MFALSE) {
					if (((p1_fbc[rt_dma].Bits.FB_NUM - p1_fbc[rt_dma].Bits.FBC_CNT) == 0) ||
						((p1_fbc[rt_dma].Bits.FB_NUM - 1) == p1_fbc[rt_dma].Bits.FBC_CNT) ||
						(rt_dma == _camsv_imgo_) || (rt_dma == _camsv2_imgo_)) {
								/* write to     phy     register */
								/* LOG_INF("[rtbc_%d][ENQUE] write2Phy*/
								/*directly(%d,%d)",rt_dma,p1_fbc[rt_dma].Bits.*/
								/*FB_NUM,p1_fbc[rt_dma].Bits.FBC_CNT); */
						IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
								"[rtbc_%d][ENQUE] write2Phy directly(%d,%d) ",
									rt_dma, p1_fbc[rt_dma].Bits.FB_NUM,
									p1_fbc[rt_dma].Bits.FBC_CNT);
						ISP_WR32(p1_dma_addr_reg[rt_dma],
									pstRTBuf->ring_buf[rt_dma].data[i].
									base_pAddr);
									/* for openedDma=2,*/
									/*it must update 2 dma's based address,*/
									/*or it will occur tearing */
					if (pstRTBuf->ring_buf[ch_imgo].active
									== MTRUE)
						ISP_WR32(p1_dma_addr_reg[ch_imgo],
								pstRTBuf->ring_buf[ch_imgo].data[i]
								.base_pAddr);
					if (pstRTBuf->ring_buf[ch_rrzo].active
									== MTRUE)
						ISP_WR32(p1_dma_addr_reg[ch_rrzo],
								pstRTBuf->ring_buf[ch_rrzo].data[i].
								base_pAddr);
						}
			if ((_camsv_imgo_ == rt_dma) || (_camsv2_imgo_ == rt_dma)) {
				if (rt_dma == _camsv_imgo_)
					enqueImd[_camsv_1] = MTRUE;
				else if (rt_dma == _camsv2_imgo_)
					enqueImd[_camsv_2] = MTRUE;
				p1_fbc[rt_dma].Bits.RCNT_INC = 1;
				ISP_WR32(p1_fbc_reg[rt_dma], p1_fbc[rt_dma].Reg_val);
				p1_fbc[rt_dma].Bits.RCNT_INC = 0;
				ISP_WR32(p1_fbc_reg[rt_dma], p1_fbc[rt_dma].Reg_val);
				IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, " RCNT_INC(dma:0x%x)\n", rt_dma);
				/**/
			} else {
				if (_openedDma == 1) {
					p1_fbc[rt_dma].Bits.RCNT_INC = 1;
					ISP_WR32(p1_fbc_reg[rt_dma], p1_fbc[rt_dma].Reg_val);
					IRQ_LOG_KEEPER(irqT, 0,	_LOG_DBG, " RCNT_INC(dma:0x%x)\n", rt_dma);
					/**/
				} else {
					p1_fbc[ch_imgo].Bits.RCNT_INC =	1;
					ISP_WR32(p1_fbc_reg[ch_imgo], p1_fbc[ch_imgo].Reg_val);
					p1_fbc[ch_rrzo].Bits.RCNT_INC =	1;
					ISP_WR32(p1_fbc_reg[ch_rrzo], p1_fbc[ch_rrzo].Reg_val);
					IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "	 RCNT_INC(dma:0x%x)",
						ch_imgo);
					IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "	 RCNT_INC(dma:0x%x)\n",
						ch_rrzo);
				}
			}
				/**/
			}
		}

#else                           /* for rtbc 1.0 case */
						/* if ( FB_NUM==FBC_CNT || (FB_NUM-1)==FBC_CNT  ) */
				if ((p1_fbc[rt_dma].Bits.FB_NUM ==
					p1_fbc[rt_dma].Bits.FBC_CNT)
					|| ((p1_fbc[rt_dma].Bits.FB_NUM - 1) ==
					p1_fbc[rt_dma].Bits.FBC_CNT)) {
						/* write to     phy     register */
						/* LOG_INF("[rtbc_%d][ENQUE] write2Phy directly(%d,%d)",*/
						/*   rt_dma,p1_fbc[rt_dma].Bits.FB_NUM,*/
						/*   p1_fbc[rt_dma].Bits.FBC_CNT); */
					IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
						"[rtbc_%d][ENQUE] write2Phy directly(%d,%d)\n",
					    rt_dma,
					    p1_fbc[rt_dma].Bits.FB_NUM,
					    p1_fbc[rt_dma].Bits.FBC_CNT);
					ISP_WR32(p1_dma_addr_reg[rt_dma],
						pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr);
				}

					/* patch camsv hw bug */
				if ((_camsv_imgo_ == rt_dma) || (_camsv2_imgo_ == rt_dma)) {
					p1_fbc[rt_dma].Bits.RCNT_INC = 1;
					ISP_WR32(p1_fbc_reg[rt_dma], p1_fbc[rt_dma].Reg_val);
					p1_fbc[rt_dma].Bits.RCNT_INC = 0;
					ISP_WR32(p1_fbc_reg[rt_dma], p1_fbc[rt_dma].Reg_val);

				} else {
					p1_fbc[rt_dma].Bits.RCNT_INC = 1;
					ISP_WR32(p1_fbc_reg[rt_dma], p1_fbc[rt_dma].Reg_val);
				}
#endif
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
				IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
				/*      */
				if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
						/* LOG_DBG("[rtbc][ENQUE]:dma(%d),PA(0x%x),O(0x%x),*/
						/*ZO(0x%x),O_D(0x%x),ZO_D(0x%x),camsv(0x%x/0x%x)*/
						/*fps(%d/%d/%d/%d)us", */
#if (ISP_RAW_D_SUPPORT == 1)
				LOG_DBG
				("[rtbc][ENQUE]:dma(%d),PA(0x%x),O(0x%x),ZO(0x%x),O_D(0x%x),ZO_D(0x%x)\n",
				 rt_dma, rt_buf_info.base_pAddr,
				 ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR), ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR),
				 ISP_RD32(ISP_REG_ADDR_IMGO_D_BASE_ADDR), ISP_RD32(ISP_REG_ADDR_RRZO_D_BASE_ADDR));
				LOG_DBG
				("[rtbc][ENQUE]:camsv(0x%x/0x%x)fps(%d/%d/%d/%d)us,rtctrl_%d\n",
				 ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR),
				 ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR),
				 avg_frame_time[_PASS1], avg_frame_time[_PASS1_D],
				 avg_frame_time[_CAMSV], avg_frame_time[_CAMSV_D],
				 rt_buf_ctrl.ctrl);

#else
		LOG_DBG
		("[rtbc][ENQUE]:dma(%d),PA(0x%x),O(0x%x),ZO(0x%x),camsv(0x%x/0x%x)fps(%d/%d/%d/%d)us,rtctrl_%d\n",
		rt_dma, rt_buf_info.base_pAddr,
		ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR), ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR),
		ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR), ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR),
		avg_frame_time[_PASS1],	avg_frame_time[_PASS1_D],
		avg_frame_time[_CAMSV],	avg_frame_time[_CAMSV_D],
		rt_buf_ctrl.ctrl);
#endif
					}
				}
			} else {
				ISP_RTBC_ENQUE(rt_dma, &rt_buf_info);
			}

		} else {
				LOG_PR_ERR("[rtbc][ENQUE]:copy_from_user fail");
				return -EFAULT;
			}
			break;

		case ISP_RT_BUF_CTRL_DEQUE:
			switch (rt_dma) {
			case _camsv_imgo_:
				irqT_Lock = _CAMSV_IRQ;
				irqT = _CAMSV_IRQ;
				break;
			case _camsv2_imgo_:
				irqT_Lock = _CAMSV_D_IRQ;
				irqT = _CAMSV_D_IRQ;
				break;
			default:
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			}
			/*      */
			reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
			reg_val2 = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON);
			camsv_reg_cal[0] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG_VF_CON);
			camsv_reg_cal[1] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_VF_CON);
			/* VF start already     */
			/* spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags); */
			if ((reg_val & 0x01) || (reg_val2 & 0x01) || (camsv_reg_cal[0] & 0x01)
			    || (camsv_reg_cal[1] & 0x01)) {
				if (_bFlag == MTRUE) {
					unsigned int out = 0;
					unsigned int _magic;

					deque_buf.count = P1_DEQUE_CNT;
					spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
#ifdef _rtbc_buf_que_2_0_
					/* p1_fbc[rt_dma].Bits.WCNT - 1;       //WCNT = [1,2,..]     */
					iBuf = pstRTBuf->ring_buf[rt_dma].read_idx;
					pstRTBuf->ring_buf[rt_dma].read_idx =
						(pstRTBuf->ring_buf[rt_dma].read_idx +
						 1) % pstRTBuf->ring_buf[rt_dma].total_count;
					if (deque_buf.count != P1_DEQUE_CNT) {
						LOG_INF("support only deque	1 buf at 1 time\n");
						deque_buf.count = P1_DEQUE_CNT;
					}
#else
					iBuf = p1_fbc[rt_dma].Bits.RCNT - 1;    /* RCNT = [1,2,3,...] */
#endif
					i = 0;

					if (ISP_RTBC_BUF_LOCKED ==
					    pstRTBuf->ring_buf[rt_dma].data[iBuf + i].bFilled) {
						LOG_INF("the same buffer deque twice\n");
					}

					/* for (i=0; i<deque_buf.count; i++) { */
					/* unsigned int out; */

					deque_buf.data[i].memID =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].memID;
					deque_buf.data[i].size =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].size;
					deque_buf.data[i].base_vAddr =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].base_vAddr;
					deque_buf.data[i].base_pAddr =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].base_pAddr;
					deque_buf.data[i].timeStampS =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].timeStampS;
					deque_buf.data[i].timeStampUs =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].timeStampUs;
					deque_buf.data[i].image.w =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.w;
					deque_buf.data[i].image.h =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.h;
					deque_buf.data[i].image.xsize =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.xsize;
					deque_buf.data[i].image.stride =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.stride;
					deque_buf.data[i].image.bus_size =
						pstRTBuf->ring_buf[rt_dma].data[iBuf +
										i].image.bus_size;
					deque_buf.data[i].image.fmt =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.fmt;
					deque_buf.data[i].image.pxl_id =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.pxl_id;
					deque_buf.data[i].image.wbn =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.wbn;
					deque_buf.data[i].image.ob =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.ob;
					deque_buf.data[i].image.lsc =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.lsc;
					deque_buf.data[i].image.rpg =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.rpg;
					deque_buf.data[i].image.m_num_0 =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.m_num_0;
					deque_buf.data[i].image.frm_cnt =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].image.frm_cnt;
					deque_buf.data[i].bProcessRaw =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].bProcessRaw;
					deque_buf.data[i].rrzInfo.srcX =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].rrzInfo.srcX;
					deque_buf.data[i].rrzInfo.srcY =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].rrzInfo.srcY;
					deque_buf.data[i].rrzInfo.srcW =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].rrzInfo.srcW;
					deque_buf.data[i].rrzInfo.srcH =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].rrzInfo.srcH;
					deque_buf.data[i].rrzInfo.dstW =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].rrzInfo.dstW;
					deque_buf.data[i].rrzInfo.dstH =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].rrzInfo.dstH;
					deque_buf.data[i].dmaoCrop.x =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].dmaoCrop.x;
					deque_buf.data[i].dmaoCrop.y =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].dmaoCrop.y;
					deque_buf.data[i].dmaoCrop.w =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].dmaoCrop.w;
					deque_buf.data[i].dmaoCrop.h =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].dmaoCrop.h;
					deque_buf.data[i].bufIdx =
						pstRTBuf->ring_buf[rt_dma].data[iBuf + i].bufIdx;

#ifdef _MAGIC_NUM_ERR_HANDLING_

					/*LOG_INF("[rtbc][deque][m_num]:d(%d),fc(0x%x),lfc0x%x,m0(0x%x),lm#(0x%x)\n",*/
					/*   rt_dma,      \*/
					/*   deque_buf.data[i].image.frm_cnt, \*/
					/*   m_LastFrmCnt[rt_dma] \*/
					/*   ,deque_buf.data[i].image.m_num_0, \*/
					/*   m_LastMNum[rt_dma]);*/

					_magic = deque_buf.data[i].image.m_num_0;

					if (_DUMMY_MAGIC_ & deque_buf.data[i].image.m_num_0)
						_magic =
							(deque_buf.data[i].image.
							 m_num_0 & (~_DUMMY_MAGIC_));


					if ((deque_buf.data[i].image.frm_cnt == _INVALID_FRM_CNT_)
					    || (m_LastMNum[rt_dma] > _magic)) {

					if ((_DUMMY_MAGIC_ & deque_buf.data[i].image.
						m_num_0) == 0) {
						deque_buf.data[i].image.m_num_0 |=
						_UNCERTAIN_MAGIC_NUM_FLAG_;
						/*      */
						IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
							       "m# uncertain:dma(%d),m0(0x%x),fcnt(0x%x),Lm#(0x%x)",
							       rt_dma,
							       deque_buf.data[i].image.m_num_0,
							       deque_buf.data[i].image.frm_cnt,
							       m_LastMNum[rt_dma]);
					}
#ifdef T_STAMP_2_0
					if (m_T_STAMP.fps > SlowMotion) {
						/*      patch here is because of that uncertain should*/
						/*happen only in missing SOF. And because of FBC, image still*/
						/*can be deque.*/
						/*That's why timestamp still need to be  increased here. */
						m_T_STAMP.T_ns +=
							((unsigned long long)m_T_STAMP.
							 interval_us * 1000);
					if (++m_T_STAMP.fcnt == m_T_STAMP.fps) {
						m_T_STAMP.fcnt = 0;
							m_T_STAMP.T_ns +=
							((unsigned long long)m_T_STAMP.
							compensation_us * 1000);
						}
						}
#endif
					} else {
						m_LastMNum[rt_dma] = _magic;
					}

#endif

					DMA_TRANS(rt_dma, out);
					pstRTBuf->ring_buf[rt_dma].data[iBuf + i].bFilled =
						ISP_RTBC_BUF_LOCKED;
					deque_buf.sof_cnt = sof_count[out];
					deque_buf.img_cnt = pstRTBuf->ring_buf[rt_dma].img_cnt;
					spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]),
							       flags);
					/* LOG_INF("RTBC_DBG7 d_dma_%d:%d %d %d\n",*/
					/*   rt_dma,pstRTBuf->ring_buf[rt_dma].data[0].bFilled,pstRTBuf->ring_buf*/
					/*   [rt_dma].data[1].bFilled,pstRTBuf->ring_buf[rt_dma].data[2].bFilled); */
					if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
						/*LOG_DBG(*/
						IRQ_LOG_KEEPER(irqT,  0, _LOG_DBG,
							       "[rtbc][DEQUE](%d):d(%d)/id(0x%x)/bs(0x%x)/va(0x%llx)/pa(0x%x)/t(%d.%d)/img(%d,%d,%d,%d,%d,%d,%d,%d,%d)/m(0x%x)/fc(%d)/rrz(%d,%d,%d,%d,%d,%d),dmao(%d,%d,%d,%d),lm#(0x%x)",
							       iBuf + i, rt_dma, deque_buf.data[i].memID,
							       deque_buf.data[i].size,
							       deque_buf.data[i].base_vAddr,
							       deque_buf.data[i].base_pAddr,
							       deque_buf.data[i].timeStampS,
							       deque_buf.data[i].timeStampUs,
							       deque_buf.data[i].image.w,
							       deque_buf.data[i].image.h,
							       deque_buf.data[i].image.stride,
							       deque_buf.data[i].image.bus_size,
							       deque_buf.data[i].image.fmt,
							       deque_buf.data[i].image.wbn,
							       deque_buf.data[i].image.ob,
							       deque_buf.data[i].image.lsc,
							       deque_buf.data[i].image.rpg,
							       deque_buf.data[i].image.m_num_0,
							       deque_buf.data[i].image.frm_cnt,
							       deque_buf.data[i].rrzInfo.srcX,
							       deque_buf.data[i].rrzInfo.srcY,
							       deque_buf.data[i].rrzInfo.srcW,
							       deque_buf.data[i].rrzInfo.srcH,
							       deque_buf.data[i].rrzInfo.dstW,
							       deque_buf.data[i].rrzInfo.dstH,
							       deque_buf.data[i].dmaoCrop.x,
							       deque_buf.data[i].dmaoCrop.y,
							       deque_buf.data[i].dmaoCrop.w,
							       deque_buf.data[i].dmaoCrop.h,
							       m_LastMNum[rt_dma]);
					}
					IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
					/*      */
					/* tstamp =     deque_buf.data[i].timeStampS*1000000+*/
					/*deque_buf.data[i].timeStampUs; */
					/* if ( (0 != prv_tstamp) && (prv_tstamp >=     tstamp) ) {     */
				if (prv_tstamp_s[rt_dma] != 0) {
					if ((prv_tstamp_s[rt_dma] >
						deque_buf.data[i].timeStampS)
						||
						((prv_tstamp_s[rt_dma] ==
						deque_buf.data[i].timeStampS)
						&& (prv_tstamp_us[rt_dma] >=
						deque_buf.data[i].timeStampUs))) {
						LOG_INF
							("[rtbc]TS rollback,D(%d),prv\"%d.%06d\",cur\"%d.%06d\"",
							rt_dma, prv_tstamp_s[rt_dma],
							prv_tstamp_us[rt_dma],
							deque_buf.data[i].timeStampS,
							deque_buf.data[i].timeStampUs);
						}
					}
					prv_tstamp_s[rt_dma] = deque_buf.data[i].timeStampS;
					prv_tstamp_us[rt_dma] = deque_buf.data[i].timeStampUs;
					/* }  , mark for for (i=0; i<deque_buf.count; i++) { */
				}
			} else {
				ISP_RTBC_DEQUE(rt_dma, &deque_buf);
			}

			if (deque_buf.count) {
				/*      */
				/* if(copy_to_user((void __user*)rt_buf_ctrl.data_ptr,*/
				/* &deque_buf, sizeof(struct ISP_DEQUE_BUF_INFO_STRUCT)) != 0)     */
				if (copy_to_user
				    ((void __user *)rt_buf_ctrl.pExtend, &deque_buf,
				     sizeof(struct ISP_DEQUE_BUF_INFO_STRUCT)) != 0) {
					LOG_PR_ERR("[rtbc][DEQUE]:copy_to_user	failed");
					Ret = -EFAULT;
				}

			} else {
				/*      */
				/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
				LOG_PR_ERR("[rtbc][DEQUE]:no filled buffer");
				Ret = -EFAULT;
			}

			break;
		case ISP_RT_BUF_CTRL_CUR_STATUS:
			reg_val = ISP_RD32(ISP_REG_ADDR_TG_VF_CON) & 0x1;
			reg_val2 = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON) & 0x1;
			camsv_reg_cal[0] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG_VF_CON) & 0x1;
			camsv_reg_cal[1] = ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_VF_CON) & 0x1;
			ISP_FBC_DUMP(rt_buf_ctrl.buf_id, reg_val, reg_val2, camsv_reg_cal[0],
				     camsv_reg_cal[1]);
			break;
		case ISP_RT_BUF_CTRL_IS_RDY:
			/*      */
			/* spin_lock_irqsave(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
			/*      */
			bWaitBufRdy = 1;
#ifdef _rtbc_buf_que_2_0_
			switch (rt_dma) {
			case _imgo_:
			case _rrzo_:
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			case _imgo_d_:
			case _rrzo_d_:
				irqT = _IRQ_D;
				irqT_Lock = _IRQ;
				break;
			case _camsv_imgo_:
				irqT_Lock = _CAMSV_IRQ;
				irqT = _CAMSV_IRQ;
				break;
			case _camsv2_imgo_:
				irqT_Lock = _CAMSV_D_IRQ;
				irqT = _CAMSV_D_IRQ;
				break;
			default:
				LOG_INF("[rtbc]N.S.(%d)\n", rt_dma);
				irqT_Lock = _IRQ;
				irqT = _IRQ;
				break;
			}

			spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);

			if (ISP_RTBC_BUF_FILLED ==
			    pstRTBuf->ring_buf[rt_dma].data[pstRTBuf->ring_buf[rt_dma].read_idx].
			    bFilled) {
				bWaitBufRdy = 0;
			}

			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL) {
				unsigned int z;

				IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG,
					       "cur dma:%d,read idx = %d,total cnt =	%d,bWaitBufRdy=	%d ,",
					       rt_dma, pstRTBuf->ring_buf[rt_dma].read_idx,
					       pstRTBuf->ring_buf[rt_dma].total_count, bWaitBufRdy);

				for (z = 0; z < pstRTBuf->ring_buf[rt_dma].total_count; z++)
					IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "%d_",
						       pstRTBuf->ring_buf[rt_dma].data[z].bFilled);

				IRQ_LOG_KEEPER(irqT, 0, _LOG_DBG, "\n");
			}
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]), flags);
			IRQ_LOG_PRINTER(irqT, 0, _LOG_DBG);
#else
#if     defined(_rtbc_use_cq0c_)
			bWaitBufRdy = p1_fbc[rt_dma].Bits.FBC_CNT ? 0 : 1;
#else
			bWaitBufRdy = MTRUE;
#endif
#endif

			/*      */
			/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */
			/*      */
			/* if(copy_to_user((void __user*)rt_buf_ctrl.data_ptr,*/
	    /*          &bWaitBufRdy, sizeof(unsigned int)) != 0) */
			if (copy_to_user
			    ((void __user *)rt_buf_ctrl.pExtend, &bWaitBufRdy,
			     sizeof(unsigned int)) != 0) {
				LOG_PR_ERR("[rtbc][IS_RDY]:copy_to_user failed");
				Ret = -EFAULT;
			}
			/*      */
			/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC), flags); */
			/*      */
			break;
		case ISP_RT_BUF_CTRL_GET_SIZE:
			/*      */
			size = pstRTBuf->ring_buf[rt_dma].total_count;
			/*      */
			/* if(IspInfo.DebugMask & ISP_DBG_BUF_CTRL)     { */
			/* LOG_DBG("[rtbc][GET_SIZE]:rt_dma(%d)/size(%d)",rt_dma,size); */
			/* } */
			/* if(copy_to_user((void __user*)rt_buf_ctrl.data_ptr, &size, sizeof(unsigned int)) != 0) */
			if (copy_to_user((void __user *)rt_buf_ctrl.pExtend, &size, sizeof(unsigned int))
			    != 0) {
				LOG_PR_ERR("[rtbc][GET_SIZE]:copy_to_user failed");
				Ret = -EFAULT;
			}
			break;
		case ISP_RT_BUF_CTRL_CLEAR:
			/*      */
			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
				LOG_INF("[rtbc][CLEAR]:rt_dma(%d)", rt_dma);

			/*      */
			switch (rt_dma) {
			case _imgo_:
			case _rrzo_:
				memset((void *)IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST],
				       0, sizeof(unsigned int) * 32);
				memset((void *)IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST],
				       0, sizeof(unsigned int) * 32);
				memset((void *)IspInfo.IrqInfo.Eismeta[ISP_IRQ_TYPE_INT_P1_ST], 0,
				       sizeof(unsigned int) * EISMETA_RINGSIZE);
				gEismetaRIdx = 0;
				gEismetaWIdx = 0;
				gEismetaInSOF = 0;
				memset(&g_DmaErr_p1[0], 0, sizeof(unsigned int) * nDMA_ERR_P1);
				break;
			case _imgo_d_:
			case _rrzo_d_:
				memset((void *)IspInfo.IrqInfo.
				       LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST_D], 0,
				       sizeof(unsigned int) * 32);
				memset((void *)IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST_D],
				       0, sizeof(unsigned int) * 32);
				memset((void *)IspInfo.IrqInfo.Eismeta[ISP_IRQ_TYPE_INT_P1_ST_D], 0,
				       sizeof(unsigned int) * EISMETA_RINGSIZE);
				gEismetaRIdx_D = 0;
				gEismetaWIdx_D = 0;
				gEismetaInSOF_D = 0;
				memset(&g_DmaErr_p1[nDMA_ERR_P1], 0,
				       sizeof(unsigned int) * (nDMA_ERR - nDMA_ERR_P1));
				break;
			case _camsv_imgo_:
				memset((char *)&g_magQue[_camsv_1], 0x0,
					sizeof(unsigned int)*ISP_RT_BUF_SIZE);
				memset((char *)&g_magQIdx[_camsv_1], 0x0,
					sizeof(unsigned int)*2);
				g_firstCnt[_camsv_1] = 0;
				enqueImd[_camsv_1] = MFALSE;
				break;
			case _camsv2_imgo_:
				memset((char *)&g_magQue[_camsv_2], 0x0,
					sizeof(unsigned int)*ISP_RT_BUF_SIZE);
				memset((char *)&g_magQIdx[_camsv_2], 0x0,
					sizeof(unsigned int)*2);
				g_firstCnt[_camsv_2] = 0;
				enqueImd[_camsv_2] = MFALSE;
				break;
			default:
				LOG_PR_ERR("[rtbc][CLEAR]N.S.(%d)\n", rt_dma);
				return -EFAULT;
			}
			/* remove, cause clear will     be involked     only when current module r totally stopped */
			/* spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT_Lock]), flags); */

#if     0
			pstRTBuf->ring_buf[rt_dma].total_count = 0;
			pstRTBuf->ring_buf[rt_dma].start = 0;
			pstRTBuf->ring_buf[rt_dma].empty_count = 0;
			pstRTBuf->ring_buf[rt_dma].active = 0;

			for (i = 0; i < ISP_RT_BUF_SIZE; i++) {
				if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr ==
				    rt_buf_info.base_pAddr) {
					buffer_exist = 1;
					break;
				}
				/*      */
				if (pstRTBuf->ring_buf[rt_dma].data[i].base_pAddr == 0)
					break;

			}
#else
			/* if ((_imgo_ == rt_dma)||(_rrzo_ == rt_dma)||(_imgo_d_ == rt_dma)||(_rrzo_d_ == rt_dma)) */
			/* active */
			pstRTBuf->ring_buf[rt_dma].active = MFALSE;
			memset((char *)&pstRTBuf->ring_buf[rt_dma], 0x00,
			       sizeof(struct ISP_RT_RING_BUF_INFO_STRUCT));

			/* init. frmcnt before vf_en */
			for (i = 0; i < ISP_RT_BUF_SIZE; i++)
				pstRTBuf->ring_buf[rt_dma].data[i].image.frm_cnt =
					_INVALID_FRM_CNT_;


			memset((char *)&prv_tstamp_s[rt_dma], 0x0, sizeof(unsigned int));
			memset((char *)&prv_tstamp_us[rt_dma], 0x0, sizeof(unsigned int));
#ifdef _rtbc_buf_que_2_0_
			memset((void *)dma_en_recorder[rt_dma], 0,
			       sizeof(unsigned char) * ISP_RT_BUF_SIZE);
			mFwRcnt.DMA_IDX[rt_dma] = 0;
#endif

			{
				unsigned int ii = 0;
				unsigned int out[4] = { _IRQ_MAX, _IRQ_MAX, _IRQ_MAX, _IRQ_MAX };

				if ((pstRTBuf->ring_buf[_imgo_].active == MFALSE)
				    && (pstRTBuf->ring_buf[_rrzo_].active == MFALSE)) {
					out[0] = _IRQ;
				}
				if ((pstRTBuf->ring_buf[_imgo_d_].active == MFALSE)
				    && (pstRTBuf->ring_buf[_rrzo_d_].active == MFALSE)) {
					out[1] = _IRQ_D;
				}
				if (pstRTBuf->ring_buf[_camsv_imgo_].active == MFALSE)
					out[2] = _CAMSV_IRQ;
				if (pstRTBuf->ring_buf[_camsv2_imgo_].active == MFALSE)
					out[3] = _CAMSV_D_IRQ;

				for (ii = 0; ii < 4; ii++) {
					if (out[ii] != _IRQ_MAX) {
						sof_count[out[ii]] = 0;
						start_time[out[ii]] = 0;
						avg_frame_time[out[ii]] = 0;
						g1stSof[out[ii]] = MTRUE;
						PrvAddr[out[ii]] = 0;
						g_ISPIntErr[out[ii]] = 0;
#ifdef _rtbc_buf_que_2_0_
						mFwRcnt.bLoadBaseAddr[out[ii]] = 0;
						mFwRcnt.curIdx[out[ii]] = 0;
						memset((void *)mFwRcnt.INC[out[ii]], 0,
						       sizeof(unsigned int) * ISP_RT_BUF_SIZE);
						mFwRcnt.rdIdx[out[ii]] = 0;
#endif
#ifdef T_STAMP_2_0
					/**/if (out[ii] == _IRQ) {
						memset((char *)&m_T_STAMP, 0x0,
							   sizeof(struct T_STAMP));
						bSlowMotion = MFALSE;
						}
#endif
					}
				}
				for (ii = 0; ii < _rt_dma_max_; ii++) {
					if (pstRTBuf->ring_buf[ii].active)
						break;

				}

				if (ii == _rt_dma_max_) {
					pstRTBuf->state = 0;
				}
			}

#ifdef _MAGIC_NUM_ERR_HANDLING_
			m_LastMNum[rt_dma] = 0;
#endif

#endif
			/* spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT_Lock]), flags); */

			break;
#ifdef _rtbc_buf_que_2_0_
		case ISP_RT_BUF_CTRL_DMA_EN: {
			unsigned char array[_rt_dma_max_];
			/* if(copy_from_user(array, (void __user*)rt_buf_ctrl.data_ptr,*/
			/*sizeof(unsigned char)*_rt_dma_max_)     == 0) { */
			if (copy_from_user
			    (array, (void __user *)rt_buf_ctrl.pExtend,
			     sizeof(unsigned char) * _rt_dma_max_) == 0) {
				unsigned int z;

				bRawEn = MFALSE;
				bRawDEn = MFALSE;
				bCamSVEn = MFALSE;
				bCamSVDEn = MFALSE;
				for (z = 0; z < _rt_dma_max_; z++) {
					pstRTBuf->ring_buf[z].active = array[z];
					/**/                    if (array[z] == 0)
						continue;

					/**/    /*if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)*/
					LOG_INF("[rtbc][DMA_EN]:dma_%d:%d", z,
						array[z]);

				}
				if ((pstRTBuf->ring_buf[_imgo_].active == MTRUE) ||
				    (pstRTBuf->ring_buf[_rrzo_].active == MTRUE)) {
					bRawEn = MTRUE;
				}
				if ((pstRTBuf->ring_buf[_imgo_d_].active == MTRUE) ||
				    (pstRTBuf->ring_buf[_rrzo_d_].active == MTRUE)) {
					bRawDEn = MTRUE;
				}
				if (pstRTBuf->ring_buf[_camsv_imgo_].active == MTRUE)
					bCamSVEn = MTRUE;
				if (pstRTBuf->ring_buf[_camsv2_imgo_].active == MTRUE)
					bCamSVDEn = MTRUE;
			} else {
				LOG_PR_ERR("[rtbc][DMA_EN]:copy_from_user failed");
				Ret = -EFAULT;
			}
		}
		break;
#endif
		case ISP_RT_BUF_CTRL_MAX:
			/* Add this     to remove build warning. */
			/* Do nothing. */
			break;

		}
		/*      */
	} else {
		LOG_PR_ERR("[rtbc]copy_from_user failed");
		Ret = -EFAULT;
	}

	return Ret;
}



#if     0
/*	rrzo/imgo/rrzo_d/imgo_d have hw cq,*/
/*	if lost p1 done, need to add start index inorder to     match HW CQ*/
/*	camsv have no hw cq, it will refer to WCNT at SOF.*/
/*	WCNT    have no change when     no p1_done,     so start index no need to change.*/
static signed int ISP_LostP1Done_ErrHandle(unsigned int dma)
{
	switch (dma) {
	case _imgo_:
	case _rrzo_:
	case _imgo_d_:
	case _rrzo_d_:
		pstRTBuf->ring_buf[dma].start++;
		pstRTBuf->ring_buf[dma].start =
			pstRTBuf->ring_buf[dma].start % pstRTBuf->ring_buf[dma].total_count;
		break;
	default:
		break;
	}
}
#endif
/* mark the     behavior of     reading FBC     at local. to prevent hw interruptting duing sw isr flow. */
/* above behavior will make     FBC     write-buffer-patch fail at p1_done */
/* curr_pa also have this prob. too     */
static signed int ISP_SOF_Buf_Get(enum eISPIrq irqT, union CQ_RTBC_FBC *pFbc, unsigned int *pCurr_pa,
			      unsigned long long sec, unsigned long usec, bool bDrop)
{
#if     defined(_rtbc_use_cq0c_)

	union CQ_RTBC_FBC imgo_fbc;
	union CQ_RTBC_FBC rrzo_fbc;
	unsigned int imgo_idx = 0;
    /* (imgo_fbc.Bits.WCNT+imgo_fbc.Bits.FB_NUM-1)%imgo_fbc.Bits.FB_NUM;//[0,1,2,...] */
	unsigned int rrzo_idx = 0;
    /* (img2o_fbc.Bits.WCNT+img2o_fbc.Bits.FB_NUM-1)%img2o_fbc.Bits.FB_NUM;//[0,1,2,...] */
	unsigned int curr_pa = 0;
	unsigned int ch_imgo, ch_rrzo;
	unsigned int i = 0;
	unsigned int _dma_cur_fw_idx = 0;
	unsigned int _dma_cur_hw_idx = 0;
	unsigned int _working_dma = 0;
	unsigned int out = 0;

	if (irqT == _IRQ) {
		imgo_fbc.Reg_val = pFbc[0].Reg_val;
		rrzo_fbc.Reg_val = pFbc[1].Reg_val;
		ch_imgo = _imgo_;
		ch_rrzo = _rrzo_;

		if (pstRTBuf->ring_buf[ch_imgo].active)
			curr_pa = pCurr_pa[0];
		else
			curr_pa = pCurr_pa[1];

		i = _PASS1;
	} else {                /* _IRQ_D     */
		imgo_fbc.Reg_val = pFbc[2].Reg_val;
		rrzo_fbc.Reg_val = pFbc[3].Reg_val;
		ch_imgo = _imgo_d_;
		ch_rrzo = _rrzo_d_;

		if (pstRTBuf->ring_buf[ch_imgo].active)
			curr_pa = pCurr_pa[2];
		else
			curr_pa = pCurr_pa[3];

		i = _PASS1_D;
	}

	if (g1stSof[irqT] == MTRUE) {   /* 1st frame of streaming */
		pstRTBuf->ring_buf[ch_imgo].start = imgo_fbc.Bits.WCNT - 1;
		pstRTBuf->ring_buf[ch_rrzo].start = rrzo_fbc.Bits.WCNT - 1;
		/* move to below because of     1st     sof&done errhandle */
		g1stSof[irqT] = MFALSE;
	}

	/*      */
#if     0                       /* this can't be trusted , because rcnt_in is pull high at sof */
	/* No drop */
	if ((imgo_fbc.Bits.FB_NUM - imgo_fbc.Bits.FBC_CNT) != 0) {
		bDrop = 0;
	} else {
		/* dropped */
		bDrop = 1;
	}
#endif
	/*      */
	/* if(IspInfo.DebugMask & ISP_DBG_INT_2) { */
	/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF,"[rtbc]dropCnt(%d)\n",bDrop); */
	/* } */
	/* No drop */
	if (bDrop == 0) {

		/* verify write buffer */

		/* if(PrvAddr[i] ==     curr_pa) */
		/* { */
		/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF,"PrvAddr:Last(0x%x) == Cur(0x%x)\n",PrvAddr[i],curr_pa); */
		/* ISP_DumpReg(); */
		/* } */
		PrvAddr[i] = curr_pa;
#ifdef _rtbc_buf_que_2_0_
		imgo_idx = pstRTBuf->ring_buf[ch_imgo].start;
		rrzo_idx = pstRTBuf->ring_buf[ch_rrzo].start;
		/* dynamic dma port ctrl */
		if (pstRTBuf->ring_buf[ch_imgo].active) {
			_dma_cur_fw_idx = imgo_idx;
			_dma_cur_hw_idx = imgo_fbc.Bits.WCNT - 1;
			_working_dma = ch_imgo;
		} else if (pstRTBuf->ring_buf[ch_rrzo].active) {
			_dma_cur_fw_idx = rrzo_idx;
			_dma_cur_hw_idx = rrzo_fbc.Bits.WCNT - 1;
			_working_dma = ch_rrzo;
		}

		if (_dma_cur_fw_idx != _dma_cur_hw_idx)
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				       "dma sof after	done %d_%d\n", _dma_cur_fw_idx,
				       _dma_cur_hw_idx);

#else
		/* last update buffer index     */
		rrzo_idx = rrzo_fbc.Bits.WCNT - 1;      /* [0,1,2,...] */
		/* curr_img2o = img2o_fbc.Bits.WCNT     - 1;//[0,1,2,...] */
		imgo_idx = rrzo_idx;
#endif
		/* verify write buffer,once     pass1_done lost, WCNT is untrustful. */
		if (pstRTBuf->ring_buf[_working_dma].total_count > ISP_RT_CQ0C_BUF_SIZE) {
			IRQ_LOG_KEEPER_PR_ERR(irqT, m_CurrentPPB, _LOG_ERR, "buf cnt(%d)\n",
				       pstRTBuf->ring_buf[_working_dma].total_count);
			pstRTBuf->ring_buf[_working_dma].total_count = ISP_RT_CQ0C_BUF_SIZE;
		}
		/*      */
		if (curr_pa != pstRTBuf->ring_buf[_working_dma].data[_dma_cur_fw_idx].base_pAddr) {
			/*      */
			/* LOG_INF("RTBC_DBG6:0x%x_0x%x\n",*/
			/* curr_pa,pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].base_pAddr); */
			for (i = 0; i < pstRTBuf->ring_buf[_working_dma].total_count; i++) {
				/*      */
				if (curr_pa == pstRTBuf->ring_buf[_working_dma].data[i].base_pAddr) {
					/*      */
					if (IspInfo.DebugMask & ISP_DBG_INT_2)
						IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
							       "[rtbc]curr:old/new(%d/%d)\n",
							       _dma_cur_fw_idx, i);

					/* mark */
					/* indx can't be chged if enque by immediate mode, write baseaddress*/
					/*timing issue. */
					/* even if not in immediate mode, this case also should no be happened */
					/* imgo_idx      = i; */
					/* rrzo_idx     = i; */
					/* ignor this log if enque in immediate mode */
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
						       "img header err: PA(%x):0x%x_0x%x, idx:0x%x_0x%x\n",
						       _working_dma, curr_pa,
						       pstRTBuf->ring_buf[_working_dma].
						       data[_dma_cur_fw_idx].base_pAddr,
						       _dma_cur_fw_idx, i);
					break;
				}
			}
		}
		/* LOG_INF("RTBC_DBG3:%d_%d\n",imgo_idx,rrzo_idx); */
		/* LOG_INF("RTBC_DBG7 imgo:%d %d %d\n",*/
		/*   pstRTBuf->ring_buf[_imgo_].data[0].bFilled,*/
		/*   pstRTBuf->ring_buf[_imgo_].data[1].bFilled,*/
		/*   pstRTBuf->ring_buf[_imgo_].data[2].bFilled); */
		/* LOG_INF("RTBC_DBG7 rrzo:%d %d %d\n",*/
		/*   pstRTBuf->ring_buf[_rrzo_].data[0].bFilled,*/
		/*   pstRTBuf->ring_buf[_rrzo_].data[1].bFilled,*/
		/*   pstRTBuf->ring_buf[_rrzo_].data[2].bFilled); */
		/*      */
		pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampS = sec;
		pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampUs = usec;
		pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].timeStampS = sec;
		pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].timeStampUs = usec;
		if (IspInfo.DebugMask & ISP_DBG_INT_3) {
			static unsigned int m_sec = 0, m_usec;
			unsigned int _tmp =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampS * 1000000 +
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampUs;

			if (g1stSof[irqT]) {
				m_sec = 0;
				m_usec = 0;
			} else {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, " timestamp:%d\n",
					       (_tmp - (1000000 * m_sec + m_usec)));
			}
			m_sec = pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampS;
			m_usec = pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].timeStampUs;
		}
		if (irqT == _IRQ) {
			unsigned int _tmp = ISP_RD32(TG_REG_ADDR_GRAB_W);
			unsigned int _p1_sel = ISP_RD32(ISP_INNER_REG_ADDR_CAM_CTRL_SEL_P1);

			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.xsize =
				ISP_RD32(ISP_INNER_REG_ADDR_IMGO_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w =
				((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			_tmp = ISP_RD32(TG_REG_ADDR_GRAB_H);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.h =
				((_tmp >> 16) & 0x1FFF) - (_tmp & 0x1FFF);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.stride =
				ISP_RD32(ISP_INNER_REG_ADDR_IMGO_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.bus_size =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_STRIDE) >> 16) & 0x03;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.fmt =
				(ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1) & 0xF000) >> 12;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id =
				(ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1) & 0x03);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0 =
				ISP_RD32(ISP_REG_ADDR_TG_MAGIC_0);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt =
				(ISP_RD32(ISP_REG_ADDR_TG_INTER_ST) & 0x00FF0000) >> 16;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.x =
				ISP_RD32(ISP_INNER_REG_ADDR_IMGO_CROP) & 0x3fff;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.y =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_CROP) >> 16) & 0x1fff;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.w =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_XSIZE) & 0x3FFF) + 1;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.h =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_YSIZE) & 0x1FFF) + 1;

			if (_p1_sel & ISP_CAM_CTL_SEL_P1_IMGO_SEL)
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].bProcessRaw =
					ISP_PURE_RAW;
			else
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].bProcessRaw =
					ISP_RROCESSED_RAW;

			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.wbn; */
			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.ob; */
			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.lsc; */
			/* pstRTBuf->ring_buf[_imgo_].data[imgo_idx].image.rpg; */
			/*      */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.xsize =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZO_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.w =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.h =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.h;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.stride =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZO_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.bus_size =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_STRIDE) >> 16) & 0x03;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.fmt =
				(ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1) & 0x30) >> 4;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.pxl_id =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.m_num_0 =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.frm_cnt =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].bProcessRaw = ISP_RROCESSED_RAW;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZ_HORI_INT_OFST) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZ_VERT_INT_OFST) & 0x1FFF;
			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_IN_IMG);
			/* pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW =*/
			/*  ((_tmp&0x1FFF)-pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX*2)&0x1FFF; */
			/* pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH =*/
			/* (((_tmp>>16)&0x1FFF)-pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY*2)&0x1FFF; */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW =
				(ISP_RD32(ISP_REG_ADDR_TG_RRZ_CROP_IN) & 0XFFFF);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH =
				(ISP_RD32(ISP_REG_ADDR_TG_RRZ_CROP_IN) >> 16);

			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_OUT_IMG);
			/* pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstW =  _tmp&0x1FFF; */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstW =
				ISP_RD32(ISP_REG_ADDR_RRZ_W);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstH =
				(_tmp >> 16) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.x =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZO_CROP) & 0x3fff;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.y =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_CROP) >> 16) & 0x1fff;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.w =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_XSIZE) & 0x3FFF) + 1;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.h =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_YSIZE) & 0x1FFF) + 1;



#if     0
#ifdef _MAGIC_NUM_ERR_HANDLING_
			LOG_INF("[rtbc][sof0][m_num]:fc(0x%x),m0(0x%x),rrz_src(%d,%d)",
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt,
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH);
#endif

			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc]TStamp(%d.%06d),curr(%d),pa(0x%x/0x%x),cq0c(0x%x)\n",
					       pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].
					       timeStampS,
					       pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].
					       timeStampUs, imgo_idx,
					       pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].
					       base_pAddr,
					       pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].
					       base_pAddr, ISP_RD32(ISP_ADDR + 0xB4));
			}
#endif
		}
#if (ISP_RAW_D_SUPPORT == 1)
	else {
			unsigned int _tmp = ISP_RD32(TG2_REG_ADDR_GRAB_W);
			unsigned int _p1_sel = ISP_RD32(ISP_INNER_REG_ADDR_CAM_CTRL_SEL_P1_D);

			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.xsize =
				ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w =
				((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			_tmp = ISP_RD32(TG2_REG_ADDR_GRAB_H);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.h =
				((_tmp >> 16) & 0x1FFF) - (_tmp & 0x1FFF);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.stride =
				ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.bus_size =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_STRIDE) >> 16) & 0x03;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.fmt =
				(ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1_D) & 0xF000) >> 12;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id =
				(ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1_D) & 0x03);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0 =
				ISP_RD32(ISP_REG_ADDR_TG2_MAGIC_0);
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt =
				(ISP_RD32(ISP_REG_ADDR_TG2_INTER_ST) & 0x00FF0000) >> 16;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.x =
				ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_CROP) & 0x3fff;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.y =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_CROP) >> 16) & 0x1fff;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.w =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_XSIZE) & 0x3FFF) + 1;
			pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].dmaoCrop.h =
				(ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_YSIZE) & 0x1FFF) + 1;

			if (_p1_sel & ISP_CAM_CTL_SEL_P1_D_IMGO_SEL)
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].bProcessRaw =
					ISP_PURE_RAW;
			else
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].bProcessRaw =
					ISP_RROCESSED_RAW;


			/*      */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.xsize =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_XSIZE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.w =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.w;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.h =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.h;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.stride =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_STRIDE) & 0x3FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.bus_size =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_STRIDE) >> 16) & 0x03;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.fmt =
				(ISP_RD32(ISP_INNER_REG_ADDR_FMT_SEL_P1_D) & 0x30) >> 4;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.pxl_id =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.pxl_id;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.m_num_0 =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.m_num_0;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.frm_cnt =
				pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].image.frm_cnt;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].bProcessRaw = ISP_RROCESSED_RAW;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_HORI_INT_OFST) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_VERT_INT_OFST) & 0x1FFF;
			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_IN_IMG);
			/* pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW =*/
			/* ((_tmp&0x1FFF)-(pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcX)*2)&0x1FFF; */
			/* pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH =*/
			/* ((((_tmp>>16)&0x1FFF))-pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcY*2)&0x1FFF; */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW =
				(ISP_RD32(ISP_REG_ADDR_TG_RRZ_CROP_IN_D) & 0XFFFF);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH =
				(ISP_RD32(ISP_REG_ADDR_TG_RRZ_CROP_IN_D) >> 16);

			_tmp = ISP_RD32(ISP_INNER_REG_ADDR_RRZ_D_OUT_IMG);
			/* pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstW      = _tmp&0x1FFF; */
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstW =
				ISP_RD32(ISP_REG_ADDR_RRZ_W_D);
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.dstH =
				(_tmp >> 16) & 0x1FFF;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.x =
				ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_CROP) & 0x3fff;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.y =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_CROP) >> 16) & 0x1fff;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.w =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_XSIZE) & 0x3FFF) + 1;
			pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].dmaoCrop.h =
				(ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_YSIZE) & 0x1FFF) + 1;
			/*      */


#if     0
#ifdef _MAGIC_NUM_ERR_HANDLING_
			LOG_INF("[rtbc][sof0][m_num]:fc(0x%x),m0(0x%x),rrz_src(%d,%d)",
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.frm_cnt,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].image.m_num_0,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcW,
				pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].rrzInfo.srcH);
#endif

			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc]TStamp(%d.%06d),curr(%d),pa(0x%x/0x%x),cq0c(0x%x)\n",
					       pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].
					       timeStampS,
					       pstRTBuf->ring_buf[ch_imgo].data[imgo_idx].
					       timeStampUs, imgo_idx,
					       pstRTBuf->ring_buf[ch_imgo].data[rrzo_idx].
					       base_pAddr,
					       pstRTBuf->ring_buf[ch_rrzo].data[rrzo_idx].
					       base_pAddr, ISP_RD32(ISP_ADDR + 0xB4));
			}
#endif
		}
#endif
		/*      */
	}

	/* frame time profile */
	DMA_TRANS(ch_imgo, out);
	if (start_time[out] == 0) {
		start_time[out] = sec * 1000000 + usec;
	} else {                /* calc once per senond */
		if (avg_frame_time[out]) {
			avg_frame_time[out] += (sec * 1000000 + usec) - avg_frame_time[out];
			avg_frame_time[out] = avg_frame_time[out] >> 1;
		} else {
			avg_frame_time[out] = (sec * 1000000 + usec) - start_time[out];
		}
	}

	sof_count[out]++;
	if (sof_count[out] > 255) {     /* for match vsync cnt */
		sof_count[out] -= 256;
	}
	pstRTBuf->state = ISP_RTBC_STATE_SOF;
#else
#ifdef _rtbc_buf_que_2_0_
#error "isp     kernel define condition is conflicted"
#endif
#endif

	return 0;
}                               /*  */

/* mark the     behavior of     reading FBC     at local. to prevent hw interruptting duing sw isr flow. */
/* above behavior will make     FBC     write-buffer-patch fail at p1_done */
/* curr_pa also have this prob. too     */
static signed int ISP_CAMSV_SOF_Buf_Get(unsigned int dma, union CQ_RTBC_FBC camsv_fbc, unsigned int curr_pa,
				    unsigned long long sec, unsigned long usec, bool bDrop)
{
	unsigned int camsv_imgo_idx = 0;
	enum eISPIrq irqT;
	unsigned int out = 0;

	DMA_TRANS(dma, out);

	if (_camsv_imgo_ == dma)
		irqT = _CAMSV_IRQ;
	else
		irqT = _CAMSV_D_IRQ;


	if (g1stSof[irqT] == MTRUE) {   /* 1st frame of streaming */
		pstRTBuf->ring_buf[dma].start = camsv_fbc.Bits.WCNT - 1;
		g1stSof[irqT] = MFALSE;
	}
#if     0                       /*     this can't be trusted , because rcnt_in is pull high at sof */
	if ((camsv_fbc.Bits.FB_NUM - camsv_fbc.Bits.FBC_CNT) != 0)
		bDrop = 0;
	else
		bDrop = 1;
#endif

	if (IspInfo.DebugMask & ISP_DBG_INT_2)
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d dropCnt(%d)\n", dma,
			       bDrop);


	/* No drop */
	if (bDrop == 0) {
		if (PrvAddr[out] == curr_pa)
			IRQ_LOG_KEEPER_PR_ERR(irqT, m_CurrentPPB, _LOG_ERR,
				       "sv%d overlap prv(0x%x) = Cur(0x%x)\n", dma, PrvAddr[out],
				       curr_pa);
		/* ISP_DumpReg(); */

		PrvAddr[out] = curr_pa;

		/* last update buffer index     */
		camsv_imgo_idx = (camsv_fbc.Bits.WCNT % camsv_fbc.Bits.FB_NUM); /* nest frame */

		/*      */
		camsv_imgo_idx =
			(camsv_imgo_idx > 0) ? (camsv_imgo_idx - 1) : (camsv_fbc.Bits.FB_NUM - 1);
		if (camsv_imgo_idx != pstRTBuf->ring_buf[dma].start) {
			/* theoretically, it shout not be happened( wcnt     is inc. at p1_done)     */
			IRQ_LOG_KEEPER_PR_ERR(irqT, m_CurrentPPB, _LOG_ERR, "sv%d WCNT%d != start%d\n",
				       dma, camsv_fbc.Bits.WCNT, pstRTBuf->ring_buf[dma].start);
		}
		pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampS = sec;
		pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampUs = usec;
		/* camsv support no inner address, these information are truely untrustful, but */
		/* because of no resize in camsv, so these r also ok. */
		if (dma == _camsv_imgo_) {
			unsigned int _tmp = ISP_RD32(CAMSV_TG_REG_ADDR_GRAB_W);

			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.xsize =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_XSIZE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.w =
				((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			_tmp = ISP_RD32(CAMSV_TG_REG_ADDR_GRAB_H);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.h =
				((_tmp >> 16) & 0x1FFF) - (_tmp & 0x1FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.stride =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_STRIDE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.bus_size =
				((ISP_RD32(ISP_REG_ADDR_IMGO_SV_STRIDE) >> 16) & 0x03);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.fmt =
				((ISP_RD32(ISP_REG_ADDR_CAMSV_FMT_SEL) & 0x30000) >> 16);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.frm_cnt =
				((ISP_RD32(ISP_REG_ADDR_CAMSV_TG_INTER_ST) & 0xFF0000) >> 16);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.x =
				ISP_RD32(ISP_REG_ADDR_IMGO_SV_CROP) & 0x3fff;
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.y =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_CROP) >> 16) & 0x1fff;
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.w =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_XSIZE) & 0x3FFF) + 1;
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.h =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_YSIZE) & 0x1FFF) + 1;
			/* Use m_num_0 instead of phy magic num. */
			if (g_firstCnt[_camsv_1] < camsv_fbc.Bits.FB_NUM) {
				if (enqueImd[_camsv_1] == MFALSE) {
				/* For sync buffer with configFrame magic num in HAL1, first 3 buffer: mag# 0. */
					pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 = 0;
				} else {
				/* For sync buffer with DmaCfg magic num in HAL3, first 3 buffer: mag# 1. */
					pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 = 1;
				}
			} else {
				unsigned int lastIdx = 0;

				if (g_magQIdx[_camsv_1][1] >= ISP_RT_BUF_SIZE)
					g_magQIdx[_camsv_1][1] = 0;
				if (g_magQIdx[_camsv_1][1] == 0)
					lastIdx = ISP_RT_BUF_SIZE - 1;
				else
					lastIdx = g_magQIdx[_camsv_1][1] - 1;

				if (enqueImd[_camsv_1] == MFALSE) {
					/* Choose current or last g_magQue because */
					/* configFrame do not comes per-frame. */
					if (g_magQue[_camsv_1][g_magQIdx[_camsv_1][1]] >=
						g_magQue[_camsv_1][lastIdx]) {
						pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 =
							g_magQue[_camsv_1][g_magQIdx[_camsv_1][1]];
						g_magQIdx[_camsv_1][1]++;
					} else {
						pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 =
							g_magQue[_camsv_1][lastIdx];
					}
				} else {
					/* HAL3 immed enque, magic number will be updated per-frame. */
					pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 =
						g_magQue[_camsv_1][g_magQIdx[_camsv_1][1]];
					LOG_DBG("camsv SOF:Ridx(%d), mag(0x%x)", g_magQIdx[_camsv_1][1],
						g_magQue[_camsv_1][g_magQIdx[_camsv_1][1]]);
					g_magQIdx[_camsv_1][1]++;
				}
			}
		} else {
			unsigned int _tmp = ISP_RD32(CAMSV_TG2_REG_ADDR_GRAB_W);

			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.xsize =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_XSIZE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.w =
				((_tmp >> 16) & 0x7FFF) - (_tmp & 0x7FFF);
			_tmp = ISP_RD32(CAMSV_TG2_REG_ADDR_GRAB_H);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.h =
				((_tmp >> 16) & 0x1FFF) - (_tmp & 0x1FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.stride =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_STRIDE) & 0x3FFF);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.bus_size =
				((ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_STRIDE) >> 16) & 0x03);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.fmt =
				((ISP_RD32(ISP_REG_ADDR_CAMSV2_FMT_SEL) & 0x30000) >> 16);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.frm_cnt =
				((ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_INTER_ST) & 0xFF0000) >> 16);
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.x =
				ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_CROP) & 0x3fff;
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.y =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_CROP) >> 16) & 0x1fff;
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.w =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_XSIZE) & 0x3FFF) + 1;
			pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].dmaoCrop.h =
				(ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_YSIZE) & 0x1FFF) + 1;
			/* Use m_num_0 instead of phy magic num. */
			if (g_firstCnt[_camsv_2] < camsv_fbc.Bits.FB_NUM) {
				if (enqueImd[_camsv_2] == MFALSE) {
				/* For sync buffer with configFrame magic num in HAL1, first 3 buffer: mag# 0. */
					pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 = 0;
				} else {
				/* For sync buffer with DmaCfg magic num in HAL3, first 3 buffer: mag# 1. */
					pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 = 1;
				}
			} else {
				unsigned int lastIdx = 0;

				if (g_magQIdx[_camsv_2][1] >= ISP_RT_BUF_SIZE)
					g_magQIdx[_camsv_2][1] = 0;
				if (g_magQIdx[_camsv_2][1]  == 0)
					lastIdx = ISP_RT_BUF_SIZE - 1;
				else
					lastIdx = g_magQIdx[_camsv_2][1] - 1;
				if (enqueImd[_camsv_2] == MFALSE) {
					/* Choose current or last g_magQue because */
					/* configFrame do not comes per-frame. */
					if (g_magQue[_camsv_2][g_magQIdx[_camsv_2][1]] >=
						g_magQue[_camsv_2][lastIdx]) {
						pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 =
							g_magQue[_camsv_2][g_magQIdx[_camsv_2][1]];
						g_magQIdx[_camsv_2][1]++;
					} else {
						pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 =
							g_magQue[_camsv_2][lastIdx];
					}
				} else {
					/* HAL3 immed enque, magic number will be updated per-frame. */
					pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0 =
						g_magQue[_camsv_2][g_magQIdx[_camsv_2][1]];
					LOG_DBG("camsv SOF:Ridx(%d), mag(0x%x)", g_magQIdx[_camsv_2][1],
						g_magQue[_camsv_2][g_magQIdx[_camsv_2][1]]);
					g_magQIdx[_camsv_2][1]++;
				}
			}
		}

		/*      */
		if (IspInfo.DebugMask & ISP_DBG_INT_2) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				"sv%d T(%d.%06d),cur(%d),addr(0x%x),prv(0x%x),fbc(0x%08x),mag(0x%08x),fcnt(0x%08x)\n",
				dma, pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampS,
				pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].timeStampUs,
				camsv_imgo_idx,
				pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].base_pAddr,
				PrvAddr[out], camsv_fbc.Reg_val,
				pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.m_num_0,
				pstRTBuf->ring_buf[dma].data[camsv_imgo_idx].image.frm_cnt);
		}
		/*      */
	}

	if (start_time[out] == 0) {
		start_time[out] = sec * 1000000 + usec;
	} else {                /* calc once per senond */
		if (avg_frame_time[out]) {
			avg_frame_time[out] += (sec * 1000000 + usec) - avg_frame_time[out];
			avg_frame_time[out] = avg_frame_time[out] >> 1;
		} else {
			avg_frame_time[out] = (sec * 1000000 + usec) - start_time[out];
		}
	}
	if ((dma == _camsv_imgo_) && (g_firstCnt[_camsv_1] < 256))
		g_firstCnt[_camsv_1]++;
	else if ((dma == _camsv2_imgo_) && (g_firstCnt[_camsv_2] < 256))
		g_firstCnt[_camsv_2]++;

	sof_count[out]++;
	if (sof_count[out] > 255) /* for match vsync cnt */
		sof_count[out] -= 256;

	pstRTBuf->state = ISP_RTBC_STATE_SOF;
	return 0;
}

/* mark the     behavior of     reading FBC     at local. to prevent hw interruptting duing sw isr flow. */
/* above behavior will make     FBC     write-buffer-patch fail at p1_done */
/* curr_pa also have this prob. too */
static signed int ISP_CAMSV_DONE_Buf_Time(unsigned int dma, union CQ_RTBC_FBC fbc, unsigned long long sec,
				      unsigned long usec)
{
	unsigned int curr;
	enum eISPIrq irqT;
	/* unsigned int loopCount = 0; */
	unsigned int _tmp;
	unsigned int out = 0;

	/*      */
	if (_camsv_imgo_ == dma)
		irqT = _CAMSV_IRQ;
	else
		irqT = _CAMSV_D_IRQ;

	/*      */
	if (pstRTBuf->ring_buf[dma].empty_count == 0) {
		/*      */
		if (IspInfo.DebugMask & ISP_DBG_INT_2)
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d RTB empty,start(%d)\n",
				       dma, pstRTBuf->ring_buf[dma].start);

		/* TODO: err handle     */
		return -1;
	}

	curr = pstRTBuf->ring_buf[dma].start;

	{                   /*     wcnt start at idx1, and +1 at p1_done  by hw */
		_tmp = fbc.Bits.WCNT - 1;
		_tmp = (_tmp > 0) ? (_tmp - 1) : (fbc.Bits.FB_NUM - 1);
	}
	if (curr != _tmp)
		IRQ_LOG_KEEPER_PR_ERR(irqT, m_CurrentPPB, _LOG_ERR, "sv%d:RTBC_%d != FBC cnt_%d\n", dma,
			       curr, _tmp);


	DMA_TRANS(dma, out);
	while (1) {             /* search next start buf, basically loop     1 time only     */
		if (IspInfo.DebugMask & ISP_DBG_INT_2) {
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d,cur(%d),bFilled(%d)\n",
				       dma, curr, pstRTBuf->ring_buf[dma].data[curr].bFilled);
		}
		/* this buf should be empty.If it's non-empty , maybe err in start index(timing shift) */
		if (pstRTBuf->ring_buf[dma].data[curr].bFilled == ISP_RTBC_BUF_EMPTY) {
			pstRTBuf->ring_buf[dma].data[curr].bFilled = ISP_RTBC_BUF_FILLED;
			/* start + 1 */
			pstRTBuf->ring_buf[dma].start =
				(curr + 1) % pstRTBuf->ring_buf[dma].total_count;
			pstRTBuf->ring_buf[dma].empty_count--;
			pstRTBuf->ring_buf[dma].img_cnt = sof_count[out];

			if (g1stSof[irqT] == MTRUE)
				LOG_INF("Done&&Sof receive at the same time	in 1st f\n");

			break;
		}
			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER_PR_ERR(irqT, m_CurrentPPB, _LOG_ERR,
					       "sv%d:curr(%d),bFilled(%d)	!= EMPTY\n", dma, curr,
					       pstRTBuf->ring_buf[dma].data[curr].bFilled);
			}
			/* start + 1 */
			/* curr = (curr+1)%pstRTBuf->ring_buf[dma].total_count; */
			break;
#if     0
		if (++loopCount > pstRTBuf->ring_buf[dma].total_count) {
			IRQ_LOG_KEEPER_PR_ERR(irqT, m_CurrentPPB, _LOG_ERR,
				       "sv%d:find	no empty buf in	total_count(%d)\n", dma,
				       pstRTBuf->ring_buf[dma].total_count);
			break;
		}
			IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
				       "sv%d:buf is not empty	for	current	p1_done\n", dma);
#endif
	}

	/*      */
	if (IspInfo.DebugMask & ISP_DBG_INT_2) {
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "sv%d:start(%d),empty(%d)\n",
			       dma,
			       pstRTBuf->ring_buf[dma].start, pstRTBuf->ring_buf[dma].empty_count);
	}

	/*      */
	/* if(IspInfo.DebugMask & ISP_DBG_INT_2) { */
	/* IRQ_LOG_KEEPER(irqT,m_CurrentPPB,_LOG_INF,"sv%d:curr(%d),sec(%lld),usec(%ld)\n",*/
	/* dma, curr, sec, usec); */
	/* } */


	/*      */
	pstRTBuf->state = ISP_RTBC_STATE_DONE;

	return 0;
}

/* mark the     behavior of     reading FBC     at local. to prevent hw interruptting duing sw isr flow. */
/* above behavior will make     FBC     write-buffer-patch fail at p1_done */
static signed int ISP_DONE_Buf_Time(enum eISPIrq irqT, union CQ_RTBC_FBC *pFbc, unsigned long long sec,
				unsigned long usec)
{
	int i, k, m;
	int i_dma;
	unsigned int curr;
	/* unsigned     int     reg_fbc; */
	/* unsigned int reg_val = 0; */
	unsigned int ch_imgo, ch_rrzo;
	union CQ_RTBC_FBC imgo_fbc;
	union CQ_RTBC_FBC rrzo_fbc;
	union CQ_RTBC_FBC _dma_cur_fbc;
	unsigned int _working_dma = 0;
#ifdef _rtbc_buf_que_2_0_
	/* for isr cb timing shift err hanlde */
	unsigned int shiftT = 0;
	unsigned int out = 0;
#endif
	if (irqT == _IRQ) {
		ch_imgo = _imgo_;
		ch_rrzo = _rrzo_;
		imgo_fbc.Reg_val = pFbc[0].Reg_val;
		rrzo_fbc.Reg_val = pFbc[1].Reg_val;
	} else {
		ch_imgo = _imgo_d_;
		ch_rrzo = _rrzo_d_;
		imgo_fbc.Reg_val = pFbc[2].Reg_val;
		rrzo_fbc.Reg_val = pFbc[3].Reg_val;
	}

#ifdef _rtbc_buf_que_2_0_

	/* dynamic dma port     ctrl */
	if (pstRTBuf->ring_buf[ch_imgo].active) {
		_dma_cur_fbc = imgo_fbc;
		_working_dma = ch_imgo;
	} else if (pstRTBuf->ring_buf[ch_rrzo].active) {
		_dma_cur_fbc = rrzo_fbc;
		_working_dma = ch_rrzo;
	} else {
		LOG_PR_ERR("non-supported dma port(%d/%d)\n", pstRTBuf->ring_buf[ch_imgo].active,
			pstRTBuf->ring_buf[ch_rrzo].active);
		return 0;
	}
	/* isr cb timing shift err handle */
	if (_dma_cur_fbc.Bits.WCNT > 0) {
		if (_dma_cur_fbc.Bits.WCNT > (pstRTBuf->ring_buf[_working_dma].start + 2)) {
			shiftT =
				_dma_cur_fbc.Bits.WCNT - pstRTBuf->ring_buf[_working_dma].start - 2;
			if (shiftT > 0)
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc%d]:alert(%d,%d)\n", irqT,
					       pstRTBuf->ring_buf[_working_dma].start,
					       _dma_cur_fbc.Bits.WCNT);
		} else if (_dma_cur_fbc.Bits.WCNT < (pstRTBuf->ring_buf[_working_dma].start + 2)) {
			shiftT =
				_dma_cur_fbc.Bits.WCNT + _dma_cur_fbc.Bits.FB_NUM -
				(pstRTBuf->ring_buf[_working_dma].start + 2);
			if (shiftT >= _dma_cur_fbc.Bits.FB_NUM) {
				LOG_PR_ERR("err shiftT	= (%d,%d ,%d)\n", _dma_cur_fbc.Bits.WCNT,
					_dma_cur_fbc.Bits.FB_NUM,
					pstRTBuf->ring_buf[_working_dma].start);
				shiftT =
					(_dma_cur_fbc.Bits.
					 FB_NUM ? (_dma_cur_fbc.Bits.FB_NUM -
						   1) : (_dma_cur_fbc.Bits.FB_NUM));
			}
		} else {
		}               /* _dma_cur_fbc.Bits.WCNT     == (pstRTBuf->ring_buf[_working_dma].start + 2) */
	}
#endif


#ifdef _rtbc_buf_que_2_0_
	for (k = 0; k < shiftT + 1; k++)
#endif
	{
		for (i = 0; i <= 1; i++) {
			/*      */
			if (i == 0) {
				i_dma = ch_imgo;
				/* reg_fbc = ch_imgo_fbc; */
			} else {
				i_dma = ch_rrzo;
				/* reg_fbc = ch_rrzo_fbc; */
			}
			/*      */
			if (pstRTBuf->ring_buf[i_dma].empty_count == 0) {
				/*      */
				if (IspInfo.DebugMask & ISP_DBG_INT_2)
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
						       "[rtbc][DONE]:dma(%d)buf num empty,start(%d)\n",
						       i_dma, pstRTBuf->ring_buf[i_dma].start);

				/*      */
				continue;
			}
#if     0
			/* once if buffer put into queue between SOF and ISP_DONE. */
			if (pstRTBuf->ring_buf[i_dma].active == MFALSE) {
				/*      */
				if (IspInfo.DebugMask & ISP_DBG_INT_2)
					LOG_DBG("[rtbc][DONE] ERROR: missing SOF ");

				/*      */
				continue;
			}
#endif
			curr = pstRTBuf->ring_buf[i_dma].start;
			/* unsigned int loopCount = 0; */
			while (1) {
				if (IspInfo.DebugMask & ISP_DBG_INT_2) {
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
						       "i_dma(%d),curr(%d),bFilled(%d)\n", i_dma,
						       curr,
						       pstRTBuf->ring_buf[i_dma].data[curr].
						       bFilled);
				}
				/*      */
				if (pstRTBuf->ring_buf[i_dma].data[curr].bFilled ==
				    ISP_RTBC_BUF_EMPTY) {
					if (IspInfo.DebugMask & ISP_DBG_INT_2)
						IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
							       "[rtbc][DONE]:dma_%d,fill buffer,cur_%d\n",
							       i_dma, curr);
					pstRTBuf->ring_buf[i_dma].data[curr].bFilled =
						ISP_RTBC_BUF_FILLED;
					/* start + 1 */
					pstRTBuf->ring_buf[i_dma].start =
						(curr + 1) % pstRTBuf->ring_buf[i_dma].total_count;
					pstRTBuf->ring_buf[i_dma].empty_count--;
					/*      */
					if (g1stSof[irqT] == MTRUE)
						LOG_INF
						("Done&&Sof receive at the same time	in 1st f(%d)\n",
						 i_dma);

					break;
				}
				if (1) {        /* (IspInfo.DebugMask & ISP_DBG_INT_2) { */
					for (m = 0; m < ISP_RT_BUF_SIZE;) {
						LOG_PR_ERR
						("dma_%d,cur_%d,bFilled_%d != EMPTY(%d %d %d	%d)\n",
							i_dma, curr,
							pstRTBuf->ring_buf[i_dma].
							data[curr].bFilled,
							pstRTBuf->ring_buf[i_dma].data[m].
							bFilled,
							pstRTBuf->ring_buf[i_dma].data[m +
										    1].
							bFilled,
							pstRTBuf->ring_buf[i_dma].data[m +
										    2].
							bFilled,
							pstRTBuf->ring_buf[i_dma].data[m +
											3].
							bFilled);
							m = m + 4;
						}
					}
					/* start + 1 */
					/* pstRTBuf->ring_buf[i_dma].start = */
					/* (curr+1)%pstRTBuf->ring_buf[i_dma].total_count; */
					break;
#if     0
				loopCount++;
				if (loopCount > pstRTBuf->ring_buf[i_dma].total_count) {
					LOG_PR_ERR
					("Can't find	empty dma(%d) buf in total_count(%d)",
					 i_dma, pstRTBuf->ring_buf[i_dma].total_count);
					break;
				}
#endif
			}
#if     0
			/* enable fbc to stall DMA */
			if (pstRTBuf->ring_buf[i_dma].empty_count == 0) {
				if (_imgo_ == i_dma) {
					reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
					reg_val |= 0x4000;
					/* ISP_WR32(ISP_REG_ADDR_IMGO_FBC,reg_val); */
				} else {
					reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
					reg_val |= 0x4000;
					/* ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC,reg_val); */
				}
				/*      */
				if (IspInfo.DebugMask & ISP_DBG_INT_2)
					IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
						       "[rtbc][DONE]:dma(%d),en fbc(0x%x)	stalled	DMA	out",
						       i_dma, ISP_RD32(reg_fbc));

			}
#endif
			/*      */
			if (IspInfo.DebugMask & ISP_DBG_INT_2) {
				IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF,
					       "[rtbc][DONE]:dma(%d),start(%d),empty(%d)\n", i_dma,
					       pstRTBuf->ring_buf[i_dma].start,
					       pstRTBuf->ring_buf[i_dma].empty_count);
			}
#if     0                       /* time stamp move to sof     */
			/*      */
			pstRTBuf->ring_buf[i_dma].data[curr].timeStampS = sec;
			pstRTBuf->ring_buf[i_dma].data[curr].timeStampUs = usec;
			/*      */
			if (IspInfo.DebugMask & ISP_DBG_INT_2)
				LOG_DBG("[rtbc][DONE]:dma(%d),curr(%d),sec(%lld),usec(%ld) ", i_dma,
					curr, sec, usec);

#endif
			/*      */
			DMA_TRANS(i_dma, out);
			pstRTBuf->ring_buf[i_dma].img_cnt = sof_count[out];
		}
	}

	if (pstRTBuf->ring_buf[ch_imgo].active && pstRTBuf->ring_buf[ch_rrzo].active) {
		if (pstRTBuf->ring_buf[ch_imgo].start != pstRTBuf->ring_buf[ch_rrzo].start) {
			LOG_INF("start idx mismatch	%d_%d(%d %d	%d,%d %d %d)",
				pstRTBuf->ring_buf[ch_imgo].start,
				pstRTBuf->ring_buf[ch_rrzo].start,
				pstRTBuf->ring_buf[ch_imgo].data[0].bFilled,
				pstRTBuf->ring_buf[ch_imgo].data[1].bFilled,
				pstRTBuf->ring_buf[ch_imgo].data[2].bFilled,
				pstRTBuf->ring_buf[ch_rrzo].data[0].bFilled,
				pstRTBuf->ring_buf[ch_rrzo].data[1].bFilled,
				pstRTBuf->ring_buf[ch_rrzo].data[2].bFilled);
		}
	}
	/* LOG_INF("RTBC_DBG7 imgo(buf cnt): %d %d %d\n",*/
	/* pstRTBuf->ring_buf[_imgo_].data[0].bFilled,*/
	/* pstRTBuf->ring_buf[_imgo_].data[1].bFilled,*/
	/* pstRTBuf->ring_buf[_imgo_].data[2].bFilled); */
	/* LOG_INF("RTBC_DBG7 rrzo(buf cnt): %d %d %d\n",*/
	/* pstRTBuf->ring_buf[_rrzo_].data[0].bFilled,*/
	/* pstRTBuf->ring_buf[_rrzo_].data[1].bFilled,*/
	/* pstRTBuf->ring_buf[_rrzo_].data[2].bFilled); */
#if     0
	if (IspInfo.DebugMask & ISP_DBG_INT_2)
		IRQ_LOG_KEEPER(irqT, m_CurrentPPB, _LOG_INF, "-:[rtbc]");

#endif
	/*      */
	pstRTBuf->state = ISP_RTBC_STATE_DONE;
	/* spin_unlock_irqrestore(&(IspInfo.SpinLockRTBC),g_Flash_SpinLock); */

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_ED_BufQue_Update_GPtr(int listTag)
{
	signed int ret = 0;
	signed int tmpIdx = 0;
	signed int cnt = 0;
	bool stop = false;
	int i = 0;
	enum ISP_ED_BUF_STATE_ENUM gPtrSts = ISP_ED_BUF_STATE_NONE;

	switch (listTag) {
	case P2_EDBUF_RLIST_TAG:
		/* [1] check global     pointer current sts     */
		gPtrSts = P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts;

		/* ////////////////////////////////////////////////////////////////////// */
		/* Assume we have the buffer list in the following situation */
		/* ++++++                 ++++++                 ++++++ */
		/* +  vss +                     +  prv +                 +      prv     + */
		/* ++++++                 ++++++                 ++++++ */
		/* not deque             erased                   enqued */
		/* done */
		/*      */
		/* if the vss deque     is done, we     should update the CurBufIdx */
		/* to the next "enqued" buffer node instead of just moving to the next buffer node */
		/* ////////////////////////////////////////////////////////////////////// */
		/* [2]calculate traverse count needed */
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			cnt = P2_EDBUF_RList_LastBufIdx - P2_EDBUF_RList_FirstBufIdx;
		} else {
			cnt = _MAX_SUPPORT_P2_FRAME_NUM_ - P2_EDBUF_RList_FirstBufIdx;
			cnt += P2_EDBUF_RList_LastBufIdx;
		}

		/* [3] update */
		tmpIdx = P2_EDBUF_RList_CurBufIdx;
		switch (gPtrSts) {
		case ISP_ED_BUF_STATE_ENQUE:
			P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts =
				ISP_ED_BUF_STATE_RUNNING;
			break;
		case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
		case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
		case ISP_ED_BUF_STATE_DEQUE_FAIL:
			do {    /* to find the newest cur index */
				tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				switch (P2_EDBUF_RingList[tmpIdx].bufSts) {
				case ISP_ED_BUF_STATE_ENQUE:
				case ISP_ED_BUF_STATE_RUNNING:
					P2_EDBUF_RingList[tmpIdx].bufSts = ISP_ED_BUF_STATE_RUNNING;
					P2_EDBUF_RList_CurBufIdx = tmpIdx;
					stop = true;
					break;
				case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
				case ISP_ED_BUF_STATE_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_NONE:
				default:
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
			/* ////////////////////////////////////////////////////////////////////// */
			/* Assume we have the buffer list in the following situation */
			/* ++++++                 ++++++                 ++++++ */
			/* +  vss +                     +  prv +                 +      prv     + */
			/* ++++++                 ++++++                 ++++++ */
			/* not deque             erased                   erased */
			/* done */
			/*      */
			/* all the buffer node are deque done in the current moment, should */
			/*     update current index to the     last node */
			/* if the vss deque     is done, we     should update the CurBufIdx */
			/*    to the last     buffer node     */
			/* ////////////////////////////////////////////////////////////////////// */
			if ((!stop) && (i == (cnt)))
				P2_EDBUF_RList_CurBufIdx = P2_EDBUF_RList_LastBufIdx;

			break;
		case ISP_ED_BUF_STATE_NONE:
		case ISP_ED_BUF_STATE_RUNNING:
		default:
			break;
		}
		break;
	case P2_EDBUF_MLIST_TAG:
	default:
		LOG_INF("Wrong List	tag(%d)\n", listTag);
		break;
	}
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
#if     0                       /* disable it to avoid build warning */
static signed int ISP_ED_BufQue_Set_FailNode(enum ISP_ED_BUF_STATE_ENUM failType, signed int idx)
{
	signed int ret = 0;

	spin_lock(&(SpinLockEDBufQueList));
	/* [1]set fail type */
	P2_EDBUF_RingList[idx].bufSts = failType;

	/* [2]update global pointer */
	ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
	spin_unlock(&(SpinLockEDBufQueList));
	return ret;
}
#endif

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_ED_BufQue_Erase(signed int idx, int listTag)
{
	signed int ret = -1;
	bool stop = false;
	int i = 0;
	signed int cnt = 0;
	int tmpIdx = 0;

	switch (listTag) {
	case P2_EDBUF_MLIST_TAG:
		tmpIdx = P2_EDBUF_MList_FirstBufIdx;
		/* [1] clear buffer     status */
		P2_EDBUF_MgrList[idx].processID = 0x0;
		P2_EDBUF_MgrList[idx].callerID = 0x0;
		P2_EDBUF_MgrList[idx].p2dupCQIdx = -1;
		P2_EDBUF_MgrList[idx].frameNum = 0;
		P2_EDBUF_MgrList[idx].dequedNum = 0;
		P2_EDBUF_MgrList[idx].p2Scenario = -1;
		/* [2] update first     index */
		if (P2_EDBUF_MgrList[tmpIdx].p2dupCQIdx == -1) {
			/* traverse     count needed, cuz user may erase the element */
			/* but not the one at first idx(pip or vss scenario) */
			if (P2_EDBUF_MList_FirstBufIdx <= P2_EDBUF_MList_LastBufIdx) {
				cnt = P2_EDBUF_MList_LastBufIdx - P2_EDBUF_MList_FirstBufIdx;
			} else {
				cnt = _MAX_SUPPORT_P2_PACKAGE_NUM_ - P2_EDBUF_MList_FirstBufIdx;
				cnt += P2_EDBUF_MList_LastBufIdx;
			}
			do {    /* to     find the newest first lindex */
				tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
				switch (P2_EDBUF_MgrList[tmpIdx].p2dupCQIdx) {
				case (-1):
					break;
				default:
					stop = true;
					P2_EDBUF_MList_FirstBufIdx = tmpIdx;
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
			/* current last erased element in list is the one firstBufindex point at */
			/* and all the buffer node are deque done in the current moment, */
			/* should update first index to the last node     */
			if ((!stop) && (i == cnt))
				P2_EDBUF_MList_FirstBufIdx = P2_EDBUF_MList_LastBufIdx;

		}
		break;
	case P2_EDBUF_RLIST_TAG:
		tmpIdx = P2_EDBUF_RList_FirstBufIdx;
		/* [1] clear buffer     status */
		P2_EDBUF_RingList[idx].processID = 0x0;
		P2_EDBUF_RingList[idx].callerID = 0x0;
		P2_EDBUF_RingList[idx].p2dupCQIdx = -1;
		P2_EDBUF_RingList[idx].bufSts = ISP_ED_BUF_STATE_NONE;
		P2_EDBUF_RingList[idx].p2Scenario = -1;
		EDBufQueRemainNodeCnt--;
		/* [2]update first index */
		if (P2_EDBUF_RingList[tmpIdx].bufSts == ISP_ED_BUF_STATE_NONE) {
			/* traverse     count needed, cuz user may erase the element but not the one at first idx */
			if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
				cnt = P2_EDBUF_RList_LastBufIdx - P2_EDBUF_RList_FirstBufIdx;
			} else {
				cnt = _MAX_SUPPORT_P2_FRAME_NUM_ - P2_EDBUF_RList_FirstBufIdx;
				cnt += P2_EDBUF_RList_LastBufIdx;
			}
			/* to find the newest first     lindex */
			do {
				tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				switch (P2_EDBUF_RingList[tmpIdx].bufSts) {
				case ISP_ED_BUF_STATE_ENQUE:
				case ISP_ED_BUF_STATE_RUNNING:
				case ISP_ED_BUF_STATE_WAIT_DEQUE_FAIL:
				case ISP_ED_BUF_STATE_DEQUE_SUCCESS:
				case ISP_ED_BUF_STATE_DEQUE_FAIL:
					stop = true;
					P2_EDBUF_RList_FirstBufIdx = tmpIdx;
					break;
				case ISP_ED_BUF_STATE_NONE:
				default:
					break;
				}
				i++;
			} while ((i < cnt) && (!stop));
			/* current last erased element in list is the one firstBufindex point at */
			/* and all the buffer node are deque done in the current moment, */
			/* should update first index to the last node     */
			if ((!stop) && (i == (cnt)))
				P2_EDBUF_RList_FirstBufIdx = P2_EDBUF_RList_LastBufIdx;

		}
		break;
	default:
		break;
	}
	return ret;
}

/*******************************************************************************
* get first     matched buffer
********************************************************************************/
static signed int ISP_ED_BufQue_Get_FirstMatBuf(struct ISP_ED_BUFQUE_STRUCT param, int ListTag, int type)
{
	signed int idx = -1;
	signed int i = 0;

	switch (ListTag) {
	case P2_EDBUF_MLIST_TAG:
		if (type == 0) { /* for user wait frame, do not care p2 dupCq index, first enqued p2 dupCQ first out */
			if (P2_EDBUF_MList_FirstBufIdx <= P2_EDBUF_MList_LastBufIdx) {
				for (i = P2_EDBUF_MList_FirstBufIdx;
					 i <= P2_EDBUF_MList_LastBufIdx;
					 i++) {
					if (compareMgrNodeLossely(
						 P2_EDBUF_MgrList[i], param)) {
						idx = i;
						break;
					}
				}
			} else {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				     i < _MAX_SUPPORT_P2_PACKAGE_NUM_;
				     i++) {
					if (compareMgrNodeLossely(
						 P2_EDBUF_MgrList[i], param)) {
						idx = i;
						break;
					}
				}
				if (idx != -1) {        /*get in the first     for     loop */
				} else {
					for (i = 0;
						 i <= P2_EDBUF_MList_LastBufIdx;
						 i++) {
					if (compareMgrNodeLossely(
						 P2_EDBUF_MgrList[i], param)) {
						idx = i;
						break;
						}
					}
				}
			}
		} else {        /* for buffer node deque done notify */
			if (P2_EDBUF_MList_FirstBufIdx
				 <= P2_EDBUF_MList_LastBufIdx) {
				for (i = P2_EDBUF_MList_FirstBufIdx;
					 i <= P2_EDBUF_MList_LastBufIdx;
				     i++) {
					if (compareMgrNode(
						 P2_EDBUF_MgrList[i], param)) {
						/* avoid race that dupCQ_1 of
						 *buffer2 enqued while dupCQ_1
						 *of buffer1 have beend deque
						 * done but not been erased yet
						 */
						idx = i;
						break;
					}
				}
			} else {
				for (i = P2_EDBUF_MList_FirstBufIdx;
				     i < _MAX_SUPPORT_P2_PACKAGE_NUM_;
				     i++) {
					if (compareMgrNode(
						 P2_EDBUF_MgrList[i], param)) {
						idx = i;
						break;
					}
				}
				if (idx != -1) {        /*get in the first     for     loop */
				} else {
					for (i = 0; i <= P2_EDBUF_MList_LastBufIdx;
						 i++) {
					if (compareMgrNode(
						 P2_EDBUF_MgrList[i], param)) {
						idx = i;
						break;
					}
				}
			}
		}
	}
		break;
	case P2_EDBUF_RLIST_TAG:
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			for (i = P2_EDBUF_RList_FirstBufIdx; i <= P2_EDBUF_RList_LastBufIdx; i++) {
				if (compareRingBufNode(P2_EDBUF_RingList[i], param)) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_EDBUF_RList_FirstBufIdx; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				if (compareRingBufNode(P2_EDBUF_RingList[i], param)) {
					idx = i;
					break;
				}
			}
			if (idx != -1) {        /*get in the first     for     loop */
			} else {
				for (i = 0; i <= P2_EDBUF_RList_LastBufIdx; i++) {
					if (compareRingBufNode(P2_EDBUF_RingList[i], param)) {
						idx = i;
						break;
					}
				}
			}
		}
		break;
	default:
		break;
	}
	if (idx == -1)
		LOG_PR_ERR("Could not find	match buffer tag(%d) pid/cid/p2dupCQidx(%d/%lu/%d)",
			ListTag, param.processID, param.callerID, param.p2dupCQIdx);

	return idx;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_ED_BufQue_CTRL_FUNC(struct ISP_ED_BUFQUE_STRUCT param)
{
	signed int ret = 0;
	int i = 0;
	int idx = -1, idx2 = -1;
	signed int restTime = 0;

	switch (param.ctrl) {
	case ISP_ED_BUFQUE_CTRL_ENQUE_FRAME:    /* signal that a specific buffer is     enqueued */
		/* [1] check the ring buffer list is full or not */
		spin_lock(&(SpinLockEDBufQueList));
		if (((P2_EDBUF_MList_LastBufIdx + 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_) ==
		    P2_EDBUF_MList_FirstBufIdx && (P2_EDBUF_MList_LastBufIdx != -1)) {
			LOG_PR_ERR("F/L(%d,%d),(%d_%d,%d), RF/C/L(%d,%d,%d),(%d,%d,%d)",
				P2_EDBUF_MList_FirstBufIdx, P2_EDBUF_MList_LastBufIdx, param.frameNum,
				P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].p2dupCQIdx,
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].p2dupCQIdx,
				P2_EDBUF_RList_FirstBufIdx, P2_EDBUF_RList_CurBufIdx,
				P2_EDBUF_RList_LastBufIdx,
				P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].bufSts,
				P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts,
				P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].bufSts);
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_PR_ERR("p2	ring buffer	list is	full, enque	Fail.");
			ret = -EFAULT;
			return ret;
		}
			if (P2_EDBUF_MList_LastBufIdx != -1) {
#if 0
				IRQ_LOG_KEEPER(_CAMSV_D_IRQ, 0, _LOG_DBG,
					       "pD(%d_0x%x) MF/L(%d_%d,%d),(%d,%d), RF/C/L(%d,%d,%d),(%d,%d,%d),dCq(%d)/Bq(%d)\n",
					       param.processID, param.callerID, param.frameNum,
					       P2_EDBUF_MList_FirstBufIdx, P2_EDBUF_MList_LastBufIdx,
					       P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].p2dupCQIdx,
					       P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].p2dupCQIdx,
					       P2_EDBUF_RList_FirstBufIdx, P2_EDBUF_RList_CurBufIdx,
					       P2_EDBUF_RList_LastBufIdx,
					       P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].bufSts,
					       P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts,
					       P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].bufSts,
					       param.p2dupCQIdx, param.p2burstQIdx);
#endif
			}
			/* [2] add new element to the last of the list */
			if (P2_EDBUF_RList_FirstBufIdx == P2_EDBUF_RList_LastBufIdx
			    && P2_EDBUF_RingList[P2_EDBUF_RList_FirstBufIdx].bufSts ==
			    ISP_ED_BUF_STATE_NONE) {
				/* all buffer   node is empty */
				P2_EDBUF_RList_LastBufIdx =
					(P2_EDBUF_RList_LastBufIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_EDBUF_RList_FirstBufIdx = P2_EDBUF_RList_LastBufIdx;
				P2_EDBUF_RList_CurBufIdx = P2_EDBUF_RList_LastBufIdx;
			} else if (P2_EDBUF_RList_CurBufIdx == P2_EDBUF_RList_LastBufIdx
				   && P2_EDBUF_RingList[P2_EDBUF_RList_CurBufIdx].bufSts ==
				   ISP_ED_BUF_STATE_NONE) {
				/*      first node is not empty, but current/last is empty */
				P2_EDBUF_RList_LastBufIdx =
					(P2_EDBUF_RList_LastBufIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
				P2_EDBUF_RList_CurBufIdx = P2_EDBUF_RList_LastBufIdx;
			} else {
				P2_EDBUF_RList_LastBufIdx =
					(P2_EDBUF_RList_LastBufIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
			}
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].processID
				= param.processID;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].callerID
				= param.callerID;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].p2dupCQIdx
				= param.p2dupCQIdx;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].bufSts
				= ISP_ED_BUF_STATE_ENQUE;
			P2_EDBUF_RingList[P2_EDBUF_RList_LastBufIdx].p2Scenario
				= param.p2Scenario;
			EDBufQueRemainNodeCnt++;

			/* [3] add new buffer package in manager list */
			if (param.p2burstQIdx == 0) {
				if (P2_EDBUF_MList_FirstBufIdx == P2_EDBUF_MList_LastBufIdx
				    && P2_EDBUF_MList_FirstBufIdx != -1
				    && P2_EDBUF_MgrList[P2_EDBUF_MList_FirstBufIdx].p2dupCQIdx ==
				    -1) {
					/* all managed buffer node is empty */
					P2_EDBUF_MList_LastBufIdx =
						(P2_EDBUF_MList_LastBufIdx +
						 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
					P2_EDBUF_MList_FirstBufIdx = P2_EDBUF_MList_LastBufIdx;
				} else {
					P2_EDBUF_MList_LastBufIdx =
						(P2_EDBUF_MList_LastBufIdx +
						 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
				}
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].processID
					= param.processID;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].callerID
					= param.callerID;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].p2dupCQIdx
					= param.p2dupCQIdx;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].frameNum
					= param.frameNum;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].dequedNum
					= 0;
				P2_EDBUF_MgrList[P2_EDBUF_MList_LastBufIdx].p2Scenario
					= param.p2Scenario;
			}
		/* [4]update global     index */
		ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
		spin_unlock(&(SpinLockEDBufQueList));
		IRQ_LOG_PRINTER(_CAMSV_D_IRQ, 0, _LOG_DBG);
		/* [5] wake     up thread that wait     for     deque */
		wake_up_interruptible_all(&WaitQueueHead_EDBuf_WaitDeque);
		break;
	case ISP_ED_BUFQUE_CTRL_WAIT_DEQUE:
		/* a    dequeue thread is waiting to do dequeue */
		/* [1]traverse for finding the buffer which had not beed dequeued of the process */
		spin_lock(&(SpinLockEDBufQueList));
		if (P2_EDBUF_RList_FirstBufIdx <= P2_EDBUF_RList_LastBufIdx) {
			for (i = P2_EDBUF_RList_FirstBufIdx; i <= P2_EDBUF_RList_LastBufIdx; i++) {
				if ((P2_EDBUF_RingList[i].processID == param.processID)
				    && ((P2_EDBUF_RingList[i].bufSts == ISP_ED_BUF_STATE_ENQUE)
					|| (P2_EDBUF_RingList[i].bufSts ==
					    ISP_ED_BUF_STATE_RUNNING))) {
					idx = i;
					break;
				}
			}
		} else {
			for (i = P2_EDBUF_RList_FirstBufIdx; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				if ((P2_EDBUF_RingList[i].processID == param.processID)
				    && ((P2_EDBUF_RingList[i].bufSts == ISP_ED_BUF_STATE_ENQUE)
					|| (P2_EDBUF_RingList[i].bufSts ==
					    ISP_ED_BUF_STATE_RUNNING))) {
					idx = i;
					break;
				}
			}
			if (idx != -1) {        /*get in the first     for     loop */
			} else {
				for (i = 0; i <= P2_EDBUF_RList_LastBufIdx; i++) {
					if ((P2_EDBUF_RingList[i].processID == param.processID)
					    &&
					    ((P2_EDBUF_RingList[i].bufSts == ISP_ED_BUF_STATE_ENQUE)
					     || (P2_EDBUF_RingList[i].bufSts ==
						 ISP_ED_BUF_STATE_RUNNING))) {
						idx = i;
						break;
					}
				}
			}
		}
		spin_unlock(&(SpinLockEDBufQueList));
		if (idx == -1) {
			LOG_PR_ERR("Do	not	find match buffer (pid/cid %d/%lu)	to deque!",
				param.processID, param.callerID);
			ret = -EFAULT;
			return ret;
		}
			restTime = wait_event_interruptible_timeout(WaitQueueHead_EDBuf_WaitDeque,
					ISP_GetEDBufQueWaitDequeState(idx), ISP_UsToJiffies(5000000));
			if (restTime == 0) {
				LOG_PR_ERR("Wait Deque	fail, idx(%d) pID(%d),cID(%lu)", idx,
					param.processID, param.callerID);
				ret = -EFAULT;
			} else {
				/* LOG_INF("wakeup and goto deque,rTime(%d), pID(%d)",restTime,param.processID); */
			}
		break;
	case ISP_ED_BUFQUE_CTRL_DEQUE_SUCCESS:
	/* signal that a        buffer is dequeued(success)     */
	case ISP_ED_BUFQUE_CTRL_DEQUE_FAIL:
		/* signal that a        buffer is dequeued(fail) */
		if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
			LOG_DBG("dq cm(%d),pID(%d),cID(%lu)\n", param.ctrl, param.processID,
				param.callerID);

		spin_lock(&(SpinLockEDBufQueList));
		/* [1]update buffer     status for the current buffer */
		/* ////////////////////////////////////////////////////////////////////// */
		/* Assume we have the buffer list in the following situation */
		/* ++++++        ++++++ */
		/* +  vss +        +  prv +     */
		/* ++++++        ++++++ */
		/*      */
		/* if the vss deque     is not done(not blocking deque), dequeThread in */
		/* userspace would change to deque prv     buffer(block deque)     immediately */
		/* to decrease  ioctrl count. */
		/* -> vss buffer would be deque at next turn, so curBuf is still at vss buffer node     */
		/* -> we should use     param to find the current buffer index in Rlikst to */
		/* update the buffer status cuz deque success/fail may not be the first buffer in Rlist */
		/* ////////////////////////////////////////////////////////////////////// */
		idx2 = ISP_ED_BufQue_Get_FirstMatBuf(param, P2_EDBUF_RLIST_TAG, 1);
		if (idx2 == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_PR_ERR("ERRRRRRRRRRR findmatch	index fail");
			ret = -EFAULT;
			return ret;
		}

		if (param.ctrl == ISP_ED_BUFQUE_CTRL_DEQUE_SUCCESS)
			P2_EDBUF_RingList[idx2].bufSts = ISP_ED_BUF_STATE_DEQUE_SUCCESS;
		else
			P2_EDBUF_RingList[idx2].bufSts = ISP_ED_BUF_STATE_DEQUE_FAIL;

		/* [2]update dequeued num in managed buffer     list */
		idx = ISP_ED_BufQue_Get_FirstMatBuf(param, P2_EDBUF_MLIST_TAG, 1);
		if (idx == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_PR_ERR("ERRRRRRRRRRR findmatch	index fail");
			ret = -EFAULT;
			return ret;
		}
		P2_EDBUF_MgrList[idx].dequedNum++;
		/* [3]update global     pointer */
		ISP_ED_BufQue_Update_GPtr(P2_EDBUF_RLIST_TAG);
		/* [4]erase     node in ring buffer     list */
		ISP_ED_BufQue_Erase(idx2, P2_EDBUF_RLIST_TAG);
		spin_unlock(&(SpinLockEDBufQueList));
		/* [5]wake up thread user that wait for a specific buffer and the thread that wait for deque */
		wake_up_interruptible_all(&WaitQueueHead_EDBuf_WaitFrame);
		wake_up_interruptible_all(&WaitQueueHead_EDBuf_WaitDeque);
		break;
	case ISP_ED_BUFQUE_CTRL_WAIT_FRAME:
		/* wait for a specific buffer */
		spin_lock(&(SpinLockEDBufQueList));
		/* [1]find first match buffer */
		idx = ISP_ED_BufQue_Get_FirstMatBuf(param, P2_EDBUF_MLIST_TAG, 0);
		if (idx == -1) {
			spin_unlock(&(SpinLockEDBufQueList));
			LOG_PR_ERR("could not find	match buffer pID/cID (%d/%lu)", param.processID,
				param.callerID);
			ret = -EFAULT;
			return ret;
		}
		/* [2]check the buffer is dequeued or not */
		if (P2_EDBUF_MgrList[idx].dequedNum == P2_EDBUF_MgrList[idx].frameNum) {
			ISP_ED_BufQue_Erase(idx, P2_EDBUF_MLIST_TAG);
			spin_unlock(&(SpinLockEDBufQueList));
			ret = 0;
			LOG_DBG("Frame is alreay dequeued, return user,	pd(%d/%lu),idx(%d)",
				param.processID, param.callerID, idx);
			return ret;
		}
			spin_unlock(&(SpinLockEDBufQueList));

			if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
				LOG_DBG("=pd(%d/%lu_%d)wait(%d	us)=\n", param.processID,
					param.callerID, idx, param.timeoutUs);

			/* [3]if not, goto wait event and wait for a signal     to check */
			restTime = wait_event_interruptible_timeout(WaitQueueHead_EDBuf_WaitFrame,
								    ISP_GetEDBufQueWaitFrameState
								    (idx),
								    ISP_UsToJiffies(param.
										    timeoutUs));
			if (restTime == 0) {
				LOG_PR_ERR
				("Dequeue Buffer	fail, rT(%d),idx(%d) pID(%d),cID(%lu),p2SupportBNum(%d)\n",
				 restTime, idx, param.processID, param.callerID,
				 P2_Support_BurstQNum);
				ret = -EFAULT;
				break;
			}
				spin_lock(&(SpinLockEDBufQueList));
				ISP_ED_BufQue_Erase(idx, P2_EDBUF_MLIST_TAG);
				spin_unlock(&(SpinLockEDBufQueList));
		break;
	case ISP_ED_BUFQUE_CTRL_WAKE_WAITFRAME:
		/* wake all slept users to check buffer is dequeued or not */
		wake_up_interruptible_all(&WaitQueueHead_EDBuf_WaitFrame);
		break;
	case ISP_ED_BUFQUE_CTRL_CLAER_ALL:
		/*      free all recored dequeued buffer */
		spin_lock(&(SpinLockEDBufQueList));
		for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			P2_EDBUF_RingList[i].processID = 0x0;
			P2_EDBUF_RingList[i].callerID = 0x0;
			P2_EDBUF_RingList[i].bufSts = ISP_ED_BUF_STATE_NONE;
			P2_EDBUF_RingList[i].p2Scenario = -1;
		}
		P2_EDBUF_RList_FirstBufIdx = 0;
		P2_EDBUF_RList_CurBufIdx = 0;
		P2_EDBUF_RList_LastBufIdx = -1;
		/*      */
		for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			P2_EDBUF_MgrList[i].processID = 0x0;
			P2_EDBUF_MgrList[i].callerID = 0x0;
			P2_EDBUF_MgrList[i].p2dupCQIdx = -1;
			P2_EDBUF_MgrList[i].frameNum = 0;
			P2_EDBUF_MgrList[i].dequedNum = 0;
			P2_EDBUF_MgrList[i].p2Scenario = -1;
		}
		P2_EDBUF_MList_FirstBufIdx = 0;
		P2_EDBUF_MList_LastBufIdx = -1;
		spin_unlock(&(SpinLockEDBufQueList));
		break;
	default:
		LOG_INF("Error: do not	support	this ctrl cmd(%d)", param.ctrl);
		break;
	}
	return ret;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_REGISTER_IRQ_USERKEY(char *userName)
{
	int key = -1;           /* -1 means there is no any un-locked user key */
	int i = 0;
	int length = 0;
	char m_UserName[USERKEY_STR_LEN];       /* local veriable for saving Username from user space */
	bool bCopyFromUser = MTRUE;

	if (userName == NULL) {
		LOG_INF(" [regUser] userName is NULL\n");
	} else {
		/*get UserName from user space */
		length = strnlen_user(userName, USERKEY_STR_LEN);
		if (length == 0) {
			LOG_PR_ERR(" [regUser] userName address is not valid\n");
			return key;
		}

		if (length > USERKEY_STR_LEN)
			length = USERKEY_STR_LEN;

		if (copy_from_user(m_UserName, (void *)(userName), length * sizeof(char)) != 0)
			bCopyFromUser = MFALSE;

		if (bCopyFromUser == MTRUE) {
			spin_lock((spinlock_t *)(&SpinLock_UserKey));
			/*check String length, add end */
			if (length == USERKEY_STR_LEN) {
				/*string length too long */
				m_UserName[length - 1] = '\0';
				if (IspInfo.DebugMask & ISP_DBG_INT)
					LOG_INF(" [regUser] userName(%s) is too long (>%d)\n",
						m_UserName, USERKEY_STR_LEN);
			}

			if (IspInfo.DebugMask & ISP_DBG_INT)
				LOG_INF(" [regUser] UserName (%s)\n", m_UserName);

			/* 1. check the current users is full or not */
			if (FirstUnusedIrqUserKey >= IRQ_USER_NUM_MAX || FirstUnusedIrqUserKey < 0) {
				key = -1;
			} else {
				/* 2. check the user had registered or not */
				for (i = 1; i < FirstUnusedIrqUserKey; i++) {   /*index 0 is for all the users */
										/*that do not register irq first */
					if (strcmp((const char *)IrqUserKey_UserInfo[i].userName, m_UserName) ==
					    0) {
						key = IrqUserKey_UserInfo[i].userKey;
						break;
					}
				}

				/* 3.return new userkey for user if the user had not registered before */
				if (key > 0) {
				} else {
					memset((void *)IrqUserKey_UserInfo[i].userName, 0, USERKEY_STR_LEN);
					strncpy((char *)IrqUserKey_UserInfo[i].userName, m_UserName,
					USERKEY_STR_LEN - 1);
					IrqUserKey_UserInfo[i].userKey = FirstUnusedIrqUserKey;
					key = FirstUnusedIrqUserKey;
					FirstUnusedIrqUserKey++;
					/* cam3 for flush */
					if (strcmp((const char *)IrqUserKey_UserInfo[i].userName, "VSIrq") == 0)
						IrqFlush_v3[key] =
							(ISP_IRQ_P1_STATUS_VS1_INT_ST |
							 ISP_IRQ_P1_STATUS_D_VS1_INT_ST);
					if (strcmp((const char *)IrqUserKey_UserInfo[i].userName, "AFIrq") == 0)
						IrqFlush_v3[key] =
							(ISP_IRQ_P1_STATUS_AF_DON_ST |
							 ISP_IRQ_P1_STATUS_D_AF_DON_ST);

					if (strcmp((const char *)IrqUserKey_UserInfo[i].userName, "EIS") == 0) {
						IrqFlush_v3[key] =
							(ISP_IRQ_P1_STATUS_VS1_INT_ST |
							 ISP_IRQ_P1_STATUS_D_VS1_INT_ST |
							 ISP_IRQ_P1_STATUS_PASS1_DON_ST |
							 ISP_IRQ_P1_STATUS_D_PASS1_DON_ST);
					}
					if (strcmp((const char *)IrqUserKey_UserInfo[i].userName, "VHDR") == 0) {
						IrqFlush_v3[key] =
							(ISP_IRQ_P1_STATUS_VS1_INT_ST |
							 ISP_IRQ_P1_STATUS_D_VS1_INT_ST |
							 ISP_IRQ_P1_STATUS_PASS1_DON_ST |
							 ISP_IRQ_P1_STATUS_D_PASS1_DON_ST);
					}
				}
			}
			spin_unlock((spinlock_t *)(&SpinLock_UserKey));
		} else {
			LOG_PR_ERR(" [regUser] copy_from_user failed (%d)\n", i);
		}
	}

	LOG_INF("User(%s)key(%d)\n", m_UserName, key);
	return key;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_MARK_IRQ(struct ISP_WAIT_IRQ_STRUCT irqinfo)
{
	enum eISPIrq eIrq = _IRQ;
	int idx;
	unsigned long long sec = 0;
	unsigned long usec = 0;
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */

	switch (irqinfo.UserInfo.Type) {
	case ISP_IRQ_TYPE_INT_CAMSV:
		eIrq = _CAMSV_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV2:
		eIrq = _CAMSV_D_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_P1_ST_D:
	case ISP_IRQ_TYPE_INT_P1_ST2_D:
	default:
		eIrq = _IRQ;
		break;
	}

	if ((irqinfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX)
		|| (irqinfo.UserInfo.UserKey < 0)) {
		LOG_PR_ERR("invalid userKey(%d), max(%d)", irqinfo.UserInfo.UserKey,
			IRQ_USER_NUM_MAX);
		return 0;
	}
	if ((irqinfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
		|| (irqinfo.UserInfo.Type < 0)) {
		LOG_PR_ERR("invalid type(%d), max(%d)", irqinfo.UserInfo.Type,
			ISP_IRQ_TYPE_AMOUNT);
		return 0;
	}

	/* 1. enable marked     flag */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	IspInfo.IrqInfo.MarkedFlag[irqinfo.UserInfo.UserKey][irqinfo.UserInfo.Type] |=
		irqinfo.UserInfo.Status;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

	/* 2. record mark time */
	idx = my_get_pow_idx(irqinfo.UserInfo.Status);

	sec = ktime_get();;     /* ns */
	do_div(sec, 1000);      /*     usec */
	usec = do_div(sec, 1000000);    /* sec and usec */

	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	IspInfo.IrqInfo.MarkedTime_usec[irqinfo.UserInfo.UserKey][irqinfo.UserInfo.Type][idx] =
		(unsigned int)usec;
	IspInfo.IrqInfo.MarkedTime_sec[irqinfo.UserInfo.UserKey][irqinfo.UserInfo.Type][idx] =
		(unsigned int)sec;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

	/* 3. clear     passed by signal count */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	IspInfo.IrqInfo.PassedBySigCnt[irqinfo.UserInfo.UserKey][irqinfo.UserInfo.Type][idx] = 0;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

	LOG_DBG("[MARK]	 key/type/sts/idx (%d/%d/0x%x/%d), t(%d/%d)\n",
		irqinfo.UserInfo.UserKey, irqinfo.UserInfo.Type, irqinfo.UserInfo.Status, idx,
		(int)sec, (int)usec);

	return 0;
}


/*******************************************************************************
*
********************************************************************************/
static signed int ISP_GET_MARKtoQEURY_TIME(struct ISP_WAIT_IRQ_STRUCT *irqinfo)
{
	signed int Ret = 0;
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */
	/*      struct timeval time_getrequest;*/
	struct timeval time_ready2return;

	unsigned long long sec = 0;
	unsigned long usec = 0;
	int idx;

	enum eISPIrq eIrq = _IRQ;

	/* do_gettimeofday(&time_ready2return);*/
	sec = ktime_get();     /* ns */
	do_div(sec, 1000);      /*     usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_ready2return.tv_usec = usec;
	time_ready2return.tv_sec = sec;

	idx = my_get_pow_idx(irqinfo->UserInfo.Status);

	switch (irqinfo->UserInfo.Type) {
	case ISP_IRQ_TYPE_INT_CAMSV:
		eIrq = _CAMSV_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV2:
		eIrq = _CAMSV_D_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_P1_ST_D:
	case ISP_IRQ_TYPE_INT_P1_ST2_D:
	default:
		eIrq = _IRQ;
		break;
	}

	if ((irqinfo->UserInfo.UserKey >= IRQ_USER_NUM_MAX)
		|| (irqinfo->UserInfo.UserKey < 0)) {
		LOG_PR_ERR("invalid userKey(%d), max(%d)", irqinfo->UserInfo.UserKey,
			IRQ_USER_NUM_MAX);
		Ret = -EFAULT;
		return Ret;
	}
	if ((irqinfo->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
		|| (irqinfo->UserInfo.Type < 0)) {
		LOG_PR_ERR("invalid type(%d), max(%d)", irqinfo->UserInfo.Type,
			ISP_IRQ_TYPE_AMOUNT);
		Ret = -EFAULT;
		return Ret;
	}

	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	if (irqinfo->UserInfo.Status & IspInfo.IrqInfo.
	    MarkedFlag[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.Type]) {
		/*      */
		irqinfo->TimeInfo.passedbySigcnt =
			IspInfo.IrqInfo.PassedBySigCnt[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.
										  Type][idx];
		/*      */
		irqinfo->TimeInfo.tMark2WaitSig_usec =
			(time_ready2return.tv_usec -
			 IspInfo.IrqInfo.MarkedTime_usec[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.
										    Type][idx]);
		irqinfo->TimeInfo.tMark2WaitSig_sec =
			(time_ready2return.tv_sec -
			 IspInfo.IrqInfo.MarkedTime_sec[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.
										   Type][idx]);
		if ((int)(irqinfo->TimeInfo.tMark2WaitSig_usec) < 0) {
			irqinfo->TimeInfo.tMark2WaitSig_sec =
				irqinfo->TimeInfo.tMark2WaitSig_sec - 1;

			if ((int)(irqinfo->TimeInfo.tMark2WaitSig_sec) < 0)
				irqinfo->TimeInfo.tMark2WaitSig_sec = 0;

			irqinfo->TimeInfo.tMark2WaitSig_usec =
				1 * 1000000 + irqinfo->TimeInfo.tMark2WaitSig_usec;
		}
		/*      */
		if (irqinfo->TimeInfo.passedbySigcnt > 0) {
			irqinfo->TimeInfo.tLastSig2GetSig_usec =
				(time_ready2return.tv_usec -
				 IspInfo.IrqInfo.LastestSigTime_usec[irqinfo->UserInfo.Type][idx]);
			irqinfo->TimeInfo.tLastSig2GetSig_sec =
				(time_ready2return.tv_sec -
				 IspInfo.IrqInfo.LastestSigTime_sec[irqinfo->UserInfo.Type][idx]);
			if ((int)(irqinfo->TimeInfo.tLastSig2GetSig_usec) < 0) {
				irqinfo->TimeInfo.tLastSig2GetSig_sec =
					irqinfo->TimeInfo.tLastSig2GetSig_sec - 1;

				if ((int)(irqinfo->TimeInfo.tLastSig2GetSig_sec) < 0)
					irqinfo->TimeInfo.tLastSig2GetSig_sec = 0;

				irqinfo->TimeInfo.tLastSig2GetSig_usec =
					1 * 1000000 + irqinfo->TimeInfo.tLastSig2GetSig_usec;
			}

		} else {
			irqinfo->TimeInfo.tLastSig2GetSig_usec = 0;
			irqinfo->TimeInfo.tLastSig2GetSig_sec = 0;
		}
	} else {
		LOG_PR_WARN("plz mark irq first, userKey/Type/Status (%d/%d/0x%x)",
			irqinfo->UserInfo.UserKey, irqinfo->UserInfo.Type,
			irqinfo->UserInfo.Status);
		Ret = -EFAULT;
	}
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	LOG_DBG
	(" [ISP_GET_MARKtoQEURY_TIME] user/type/idx(%d/%d/%d),mark sec/usec (%d/%d),",
	 irqinfo->UserInfo.UserKey, irqinfo->UserInfo.Type, idx,
	 IspInfo.IrqInfo.MarkedTime_sec[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.
								   Type][idx],
	 IspInfo.IrqInfo.MarkedTime_usec[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.
								    Type][idx]);
	LOG_DBG
	("irq sec/usec (%d/%d),query sec/usec(%d/%d),sig(%d)\n",
	 IspInfo.IrqInfo.LastestSigTime_sec[irqinfo->UserInfo.Type][idx],
	 IspInfo.IrqInfo.LastestSigTime_usec[irqinfo->UserInfo.Type][idx],
	 (int)time_ready2return.tv_sec, (int)time_ready2return.tv_usec,
	 IspInfo.IrqInfo.PassedBySigCnt[irqinfo->UserInfo.UserKey][irqinfo->UserInfo.
								   Type][idx]);
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_FLUSH_IRQ(struct ISP_WAIT_IRQ_STRUCT irqinfo)
{
	enum eISPIrq eIrq = _IRQ;
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */

	switch (irqinfo.Type) {
	case ISP_IRQ_TYPE_INT_CAMSV:
		eIrq = _CAMSV_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV2:
		eIrq = _CAMSV_D_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_P1_ST_D:
	case ISP_IRQ_TYPE_INT_P1_ST2_D:
	default:
		eIrq = _IRQ;
		break;
	}

	if ((irqinfo.UserNumber >= ISP_IRQ_USER_MAX) || (irqinfo.UserNumber < 0)) {
		LOG_PR_ERR("invalid userNumber(%d), max(%d)",
			irqinfo.UserNumber, ISP_IRQ_USER_MAX);
		return 0;
	}

	if ((irqinfo.Type >= ISP_IRQ_TYPE_AMOUNT) || (irqinfo.Type < 0)) {
		LOG_PR_ERR("invalid type(%d), max(%d)\n",
			irqinfo.Type, ISP_IRQ_TYPE_AMOUNT);
		return 0;
	}
	/* 1. enable signal     */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	IspInfo.IrqInfo.Status[irqinfo.UserNumber][irqinfo.Type] |= irqinfo.Status;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

	/* 2. force     to wake up the user     that are waiting for that signal */
	wake_up_interruptible_all(&IspInfo.WaitQueueHead);

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_FLUSH_IRQ_V3(struct ISP_WAIT_IRQ_STRUCT irqinfo)
{
	enum eISPIrq eIrq = _IRQ;
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */

	switch (irqinfo.UserInfo.Type) {
	case ISP_IRQ_TYPE_INT_CAMSV:
		eIrq = _CAMSV_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV2:
		eIrq = _CAMSV_D_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_P1_ST_D:
	case ISP_IRQ_TYPE_INT_P1_ST2_D:
	default:
		eIrq = _IRQ;
		break;
	}

	if ((irqinfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX)
		|| (irqinfo.UserInfo.UserKey < 0)) {
		LOG_PR_ERR("invalid userKey(%d), max(%d)\n",
			irqinfo.UserInfo.UserKey, IRQ_USER_NUM_MAX);
		return 0;
	}
	if ((irqinfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
		|| (irqinfo.UserInfo.Type < 0)) {
		LOG_PR_ERR("invalid type(%d), max(%d)\n",
			irqinfo.UserInfo.Type, ISP_IRQ_TYPE_AMOUNT);
		return 0;
	}

	/* 1. enable signal     */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	IspInfo.IrqInfo.Status[irqinfo.UserInfo.UserKey][irqinfo.UserInfo.Type] |=
		irqinfo.UserInfo.Status;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

	/* 2. force     to wake up the user     that are waiting for that signal */
	wake_up_interruptible_all(&IspInfo.WaitQueueHead);

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_WaitIrq(struct ISP_WAIT_IRQ_STRUCT *WaitIrq)
{
	signed int Ret = 0, Timeout = WaitIrq->Timeout;
	/*      unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */
	enum eISPIrq eIrq = _IRQ;
	/*      int cnt = 0;*/
	/*      */
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		if (WaitIrq->
		    Status & (ISP_IRQ_P1_STATUS_SOF1_INT_ST | ISP_IRQ_P1_STATUS_PASS1_DON_ST |
			      ISP_IRQ_P1_STATUS_D_SOF1_INT_ST | ISP_IRQ_P1_STATUS_D_PASS1_DON_ST)) {
			LOG_DBG
			("+WaitIrq Clear(%d), Type(%d), Status(0x%08X), Timeout(%d),user(%d)\n",
			 WaitIrq->Clear, WaitIrq->Type, WaitIrq->Status, WaitIrq->Timeout,
			 WaitIrq->UserNumber);
		}
	}
	/*      */

	switch (WaitIrq->Type) {
	case ISP_IRQ_TYPE_INT_CAMSV:
		eIrq = _CAMSV_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV2:
		eIrq = _CAMSV_D_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_P1_ST_D:
	case ISP_IRQ_TYPE_INT_P1_ST2_D:
	default:
		eIrq = _IRQ;
		break;
	}

	if (WaitIrq->Clear == ISP_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		if (IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] & WaitIrq->Status) {
			/* LOG_DBG("WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X)     has     been cleared",*/
			/*   WaitIrq->Clear,WaitIrq->Type,IspInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status); */
			IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] &=
				(~WaitIrq->Status);
		}
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	} else if (WaitIrq->Clear == ISP_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		/* LOG_DBG("WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X)     has     been cleared",*/
		/*   WaitIrq->Clear,WaitIrq->Type,IspInfo.IrqInfo.Status[WaitIrq->Type]); */
		IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] = 0;
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	} else if (WaitIrq->Clear == ISP_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		/* LOG_DBG("WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X)     has     been cleared",*/
		/*   WaitIrq->Clear,WaitIrq->Type,IspInfo.IrqInfo.Status[WaitIrq->Type]); */
		IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] &= (~WaitIrq->Status);
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
		return Ret;
	}
	/*      */
	Timeout = wait_event_interruptible_timeout(IspInfo.WaitQueueHead,
						   ISP_GetIRQState(eIrq, WaitIrq->Type,
								   WaitIrq->UserNumber,
								   WaitIrq->Status),
						   ISP_MsToJiffies(WaitIrq->Timeout));
	/* check if     user is interrupted     by system signal */
	if ((Timeout != 0)
	    && (!ISP_GetIRQState(eIrq, WaitIrq->Type, WaitIrq->UserNumber, WaitIrq->Status))) {
		LOG_PR_WARN
		("interrupted by	system signal,return value(%d),irq Type/User/Sts(0x%x/%d/0x%x)",
		 Timeout, WaitIrq->Type, WaitIrq->UserNumber, WaitIrq->Status);
		Ret = -ERESTARTSYS;     /* actually     it should be -ERESTARTSYS */
		goto EXIT;
	}

	/* timeout */
	if (Timeout == 0) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		LOG_PR_ERR
		("v1 ERR WaitIrq Timeout Clear(%d),Type(%d),",
		 WaitIrq->Clear, WaitIrq->Type);
		LOG_INF
		("IrqStatus(0x%08X),WaitStatus(0x%08X),Timeout(%d),user(%d)",
		 IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type],
		 WaitIrq->Status, WaitIrq->Timeout, WaitIrq->UserNumber);
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

		if (WaitIrq->bDumpReg || (WaitIrq->UserNumber == ISP_IRQ_USER_3A)
		    || (WaitIrq->UserNumber == ISP_IRQ_USER_MW)) {
			ISP_DumpReg();
		}

		Ret = -EFAULT;
		goto EXIT;
	}


	/*      */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	/*      */
	/*if (IspInfo.DebugMask & ISP_DBG_INT) {*/
		/*   for (i = 0; i <      ISP_IRQ_TYPE_AMOUNT; i++) {*/
		/*   LOG_DBG("Type(%d), IrqStatus(0x%08X)",i,IspInfo.IrqInfo.Status[i]);*/
		/*   }*/
	/*}*/
	/*      */
	/* eis meta     */
	if (WaitIrq->Type < ISP_IRQ_TYPE_INT_STATUSX
	    && WaitIrq->SpecUser == ISP_IRQ_WAITIRQ_SPEUSER_EIS) {
		if (WaitIrq->Type == ISP_IRQ_TYPE_INT_P1_ST) {
			if (gEismetaWIdx == 0) {
				if (gEismetaInSOF == 0)
					gEismetaRIdx = (EISMETA_RINGSIZE - 1);
				else
					gEismetaRIdx = (EISMETA_RINGSIZE - 2);

			} else if (gEismetaWIdx == 1) {
				if (gEismetaInSOF == 0)
					gEismetaRIdx = 0;
				else
					gEismetaRIdx = (EISMETA_RINGSIZE - 1);

			} else {
				gEismetaRIdx = (gEismetaWIdx - gEismetaInSOF - 1);
			}

			if ((gEismetaRIdx < 0) || (gEismetaRIdx >= EISMETA_RINGSIZE))
				/* BUG_ON(1); */
			{
				gEismetaRIdx = 0;
			}
			/* TBD WARNING */

			/* eis meta     */
			WaitIrq->EisMeta.tLastSOF2P1done_sec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->Type][gEismetaRIdx].
				tLastSOF2P1done_sec;
			WaitIrq->EisMeta.tLastSOF2P1done_usec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->Type][gEismetaRIdx].
				tLastSOF2P1done_usec;
		} else if (WaitIrq->Type == ISP_IRQ_TYPE_INT_P1_ST_D) {
			if (gEismetaWIdx_D == 0) {
				if (gEismetaInSOF_D == 0)
					gEismetaRIdx_D = (EISMETA_RINGSIZE - 1);
				else
					gEismetaRIdx_D = (EISMETA_RINGSIZE - 2);

			} else if (gEismetaWIdx_D == 1) {
				if (gEismetaInSOF_D == 0)
					gEismetaRIdx_D = 0;
				else
					gEismetaRIdx_D = (EISMETA_RINGSIZE - 1);

			} else {
				gEismetaRIdx_D = (gEismetaWIdx_D - gEismetaInSOF_D - 1);
			}

			if ((gEismetaRIdx_D < 0) || (gEismetaRIdx_D >= EISMETA_RINGSIZE)) {
				/* BUG_ON(1); */
				gEismetaRIdx_D = 0;
				/* TBD WARNING */
			}
			/* eis meta     */
			WaitIrq->EisMeta.tLastSOF2P1done_sec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->Type][gEismetaRIdx_D].
				tLastSOF2P1done_sec;
			WaitIrq->EisMeta.tLastSOF2P1done_usec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->Type][gEismetaRIdx_D].
				tLastSOF2P1done_usec;
		}
	}
	/* clear the status     if someone get the irq */
	IspInfo.IrqInfo.Status[WaitIrq->UserNumber][WaitIrq->Type] &= (~WaitIrq->Status);
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	/*      */
	/* check CQ     status, when pass2,     pass2b, pass2c done     */
	if (WaitIrq->Type == ISP_IRQ_TYPE_INT_P2_ST) {
		unsigned int CQ_status;

		ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x6000);
		CQ_status = ISP_RD32(ISP_IMGSYS_BASE + 0x4164);
		switch (WaitIrq->Status) {
		case ISP_IRQ_P2_STATUS_PASS2A_DON_ST:
			if ((CQ_status & 0x0000000F) != 0x001) {
				LOG_PR_ERR("CQ1 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(ISP_IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		case ISP_IRQ_P2_STATUS_PASS2B_DON_ST:
			if ((CQ_status & 0x000000F0) != 0x010) {
				LOG_PR_ERR("CQ2 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(ISP_IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		case ISP_IRQ_P2_STATUS_PASS2C_DON_ST:
			if ((CQ_status & 0x00000F00) != 0x100) {
				LOG_PR_ERR("CQ3 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(ISP_IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		default:
			break;
		}
	}

EXIT:
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_WaitIrq_v3(struct ISP_WAIT_IRQ_STRUCT *WaitIrq)
{
	signed int Ret = 0, Timeout = WaitIrq->Timeout;
	/*      unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */
	enum eISPIrq eIrq = _IRQ;
	/*      int cnt = 0;*/
	int idx = my_get_pow_idx(WaitIrq->UserInfo.Status);
	struct timeval time_getrequest;
	struct timeval time_ready2return;
	bool freeze_passbysigcnt = false;
	unsigned long long sec = 0;
	unsigned long usec = 0;


	/* do_gettimeofday(&time_getrequest); */
	sec = ktime_get();     /* ns */
	do_div(sec, 1000);      /*     usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_getrequest.tv_usec = usec;
	time_getrequest.tv_sec = sec;

	/*      */
	if (IspInfo.DebugMask & ISP_DBG_INT) {
		if (WaitIrq->UserInfo.
		    Status & (ISP_IRQ_P1_STATUS_SOF1_INT_ST | ISP_IRQ_P1_STATUS_PASS1_DON_ST |
			      ISP_IRQ_P1_STATUS_D_SOF1_INT_ST | ISP_IRQ_P1_STATUS_D_PASS1_DON_ST)) {
			if (WaitIrq->UserInfo.UserKey > 0) {
				LOG_DBG
				("+WaitIrq Clear(%d), Type(%d), Status(0x%08X), Timeout(%d/%d),user(%d)\n",
				 WaitIrq->Clear, WaitIrq->UserInfo.Type,
				 WaitIrq->UserInfo.Status, Timeout, WaitIrq->Timeout,
				 WaitIrq->UserInfo.UserKey);
			}
		}
	}
	/*      */

	switch (WaitIrq->UserInfo.Type) {
	case ISP_IRQ_TYPE_INT_CAMSV:
		eIrq = _CAMSV_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_CAMSV2:
		eIrq = _CAMSV_D_IRQ;
		break;
	case ISP_IRQ_TYPE_INT_P1_ST_D:
	case ISP_IRQ_TYPE_INT_P1_ST2_D:
	default:
		eIrq = _IRQ;
		break;
	}

	if ((WaitIrq->UserInfo.UserKey >= IRQ_USER_NUM_MAX)
		|| (WaitIrq->UserInfo.UserKey < 0)) {
		LOG_PR_ERR("invalid userKey(%d), max(%d)\n",
			WaitIrq->UserInfo.UserKey, IRQ_USER_NUM_MAX);
		return 0;
	}
	if ((WaitIrq->UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
		|| (WaitIrq->UserInfo.Type < 0)) {
		LOG_PR_ERR("invalid type(%d), max(%d)\n",
			WaitIrq->UserInfo.Type, ISP_IRQ_TYPE_AMOUNT);
		return 0;
	}

	/* 1. wait type update */
	if (WaitIrq->Clear == ISP_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		/* LOG_DBG("WARNING: Clear(%d), Type(%d): IrqStatus(0x%08X) has been cleared",WaitIrq->Clear,*/
		/*WaitIrq->UserInfo.Type,IspInfo.IrqInfo.Status[WaitIrq->UserInfo.Type]); */
		IspInfo.IrqInfo.Status[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type] &=
			(~WaitIrq->UserInfo.Status);
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
		return Ret;
	}
	{
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		if (WaitIrq->UserInfo.Status & IspInfo.IrqInfo.
		    MarkedFlag[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type]) {
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
			/* force to  be non_clear wait if marked before, and check the request wait timing */
			/* if the entry time of wait request after mark is before signal occurring,\ */
			/*we freese the counting for passby signal */

			/*      */
			/* v : kernel receive mark request */
			/* o : kernel receive wait request */
			/* ¡ô: return to user */
			/*      */
			/* case: freeze is true, and passby     signal count = 0 */
			/*      */
			/* |                                                                            |     */
			/* |                                                              (wait)        | */
			/* |       v-------------o++++++ |¡ô */
			/* |                                                                            |     */
			/* Sig                                                                            Sig */
			/*      */
			/* case: freeze is false, and passby signal     count = 1 */
			/*      */
			/* |                                                                             |     */
			/* |                                                                             |     */
			/* |       v---------------------- |-o  ¡ô(return) */
			/* |                                                                             |     */
			/* Sig                                                                             Sig */
			/*      */

			freeze_passbysigcnt =
				!(ISP_GetIRQState
				  (eIrq, WaitIrq->UserInfo.Type, WaitIrq->UserInfo.UserKey,
				   WaitIrq->UserInfo.Status));
		} else {
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

			if (WaitIrq->Clear == ISP_IRQ_CLEAR_WAIT) {
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
				if (IspInfo.IrqInfo.
				    Status[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
								      Type] & WaitIrq->UserInfo.
				    Status) {
					/* LOG_DBG("WARNING: Clear(%d), Type(%d):*/
					/*   IrqStatus(0x%08X)     has     been cleared",*/
					/*   WaitIrq->Clear,WaitIrq->UserInfo.Type,*/
					/*   IspInfo_FrmB.IrqInfo.Status[WaitIrq->UserInfo.Type] & WaitIrq->Status); */
					IspInfo.IrqInfo.Status[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
											  Type] &=
											  (~WaitIrq->UserInfo.Status);
				}
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
			} else if (WaitIrq->Clear == ISP_IRQ_CLEAR_ALL) {
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
				/* LOG_DBG("WARNING: Clear(%d), Type(%d):*/
				/*   IrqStatus(0x%08X)     has     been cleared",*/
				/*   WaitIrq->Clear,WaitIrq->UserInfo.Type,*/
				/*   IspInfo_FrmB.IrqInfo.Status[WaitIrq->UserInfo.Type]); */
				IspInfo.IrqInfo.Status[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
										  Type] = 0;
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
			}
		}
	}
	/* 2. start     to wait signal */
	Timeout = wait_event_interruptible_timeout(IspInfo.WaitQueueHead,
						   ISP_GetIRQState(eIrq, WaitIrq->UserInfo.Type,
								   WaitIrq->UserInfo.UserKey,
								   WaitIrq->UserInfo.Status),
						   ISP_MsToJiffies(WaitIrq->Timeout));
	/* check if     user is interrupted     by system signal */
	if ((Timeout != 0)
	    &&
	    (!ISP_GetIRQState
	     (eIrq, WaitIrq->UserInfo.Type, WaitIrq->UserInfo.UserKey, WaitIrq->UserInfo.Status))) {
		LOG_PR_WARN
		("interrupted by	system signal,return value(%d),irq Type/User/Sts(0x%x/%d/0x%x)",
		 Timeout, WaitIrq->UserInfo.Type, WaitIrq->UserInfo.UserKey,
		 WaitIrq->UserInfo.Status);
		Ret = -ERESTARTSYS;     /* actually     it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
		LOG_PR_ERR
		("v3 ERRRR WaitIrq Timeout(%d) Clear(%d),Type(%d)",
		 WaitIrq->Timeout, WaitIrq->Clear, WaitIrq->UserInfo.Type);
		LOG_PR_ERR
		("IrqStatus(0x%08X),WaitStatus(0x%08X),Timeout(%d),userKey(%d)\n",
		 IspInfo.IrqInfo.Status[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type],
		 WaitIrq->UserInfo.Status, WaitIrq->Timeout, WaitIrq->UserInfo.UserKey);
		spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
		/* TODO:  AF */
		if (WaitIrq->bDumpReg
		    || (strcmp((const char *)IrqUserKey_UserInfo[WaitIrq->UserInfo.UserKey].userName, "HwIRQ3A")
			== 0)) {
			ISP_DumpReg();
		}

		Ret = -EFAULT;
		goto EXIT;
	}

	/* 3. get interrupt     and     update time     related information     that would be return to user */
	/* do_gettimeofday(&time_ready2return); */
	sec = ktime_get();     /* ns */
	do_div(sec, 1000);      /*     usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_ready2return.tv_usec = usec;
	time_ready2return.tv_sec = sec;

	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	/* eis meta     */
	if (WaitIrq->UserInfo.Type < ISP_IRQ_TYPE_INT_STATUSX
	    && WaitIrq->SpecUser == ISP_IRQ_WAITIRQ_SPEUSER_EIS) {
		if (WaitIrq->UserInfo.Type == ISP_IRQ_TYPE_INT_P1_ST) {
			if (gEismetaWIdx == 0) {
				if (gEismetaInSOF == 0)
					gEismetaRIdx = (EISMETA_RINGSIZE - 1);
				else
					gEismetaRIdx = (EISMETA_RINGSIZE - 2);

			} else if (gEismetaWIdx == 1) {
				if (gEismetaInSOF == 0)
					gEismetaRIdx = 0;
				else
					gEismetaRIdx = (EISMETA_RINGSIZE - 1);

			} else {
				gEismetaRIdx = (gEismetaWIdx - gEismetaInSOF - 1);
			}

			if ((gEismetaRIdx < 0) || (gEismetaRIdx >= EISMETA_RINGSIZE)) {
				/* BUG_ON(1); */
				gEismetaRIdx = 0;
				/* TBD WARNING */
			}
			/* eis meta     */
			WaitIrq->EisMeta.tLastSOF2P1done_sec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->UserInfo.Type][gEismetaRIdx].
				tLastSOF2P1done_sec;
			WaitIrq->EisMeta.tLastSOF2P1done_usec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->UserInfo.Type][gEismetaRIdx].
				tLastSOF2P1done_usec;
		} else if (WaitIrq->UserInfo.Type == ISP_IRQ_TYPE_INT_P1_ST_D) {
			if (gEismetaWIdx_D == 0) {
				if (gEismetaInSOF_D == 0)
					gEismetaRIdx_D = (EISMETA_RINGSIZE - 1);
				else
					gEismetaRIdx_D = (EISMETA_RINGSIZE - 2);

			} else if (gEismetaWIdx_D == 1) {
				if (gEismetaInSOF_D == 0)
					gEismetaRIdx_D = 0;
				else
					gEismetaRIdx_D = (EISMETA_RINGSIZE - 1);

			} else {
				gEismetaRIdx_D = (gEismetaWIdx_D - gEismetaInSOF_D - 1);
			}

			if ((gEismetaRIdx_D < 0) || (gEismetaRIdx_D >= EISMETA_RINGSIZE)) {
				/* BUG_ON(1); */
				gEismetaRIdx_D = 0;
				/* TBD WARNING */
			}
			/* eis meta     */
			WaitIrq->EisMeta.tLastSOF2P1done_sec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->UserInfo.Type][gEismetaRIdx_D].
				tLastSOF2P1done_sec;
			WaitIrq->EisMeta.tLastSOF2P1done_usec =
				IspInfo.IrqInfo.Eismeta[WaitIrq->UserInfo.Type][gEismetaRIdx_D].
				tLastSOF2P1done_usec;
		}
		LOG_DBG(" [WAITIRQv3](%d) EisMeta.tLastSOF2P1done_sec(%d)\n",
			WaitIrq->UserInfo.Type, WaitIrq->EisMeta.tLastSOF2P1done_sec);
	}
	/* time period for 3A */
	if (WaitIrq->UserInfo.Status & IspInfo.IrqInfo.
	    MarkedFlag[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type]) {
		WaitIrq->TimeInfo.tMark2WaitSig_usec =
			(time_getrequest.tv_usec -
			 IspInfo.IrqInfo.MarkedTime_usec[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
										    Type][idx]);
		WaitIrq->TimeInfo.tMark2WaitSig_sec =
			(time_getrequest.tv_sec -
			 IspInfo.IrqInfo.MarkedTime_sec[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
										   Type][idx]);
		if ((int)(WaitIrq->TimeInfo.tMark2WaitSig_usec) < 0) {
			WaitIrq->TimeInfo.tMark2WaitSig_sec =
				WaitIrq->TimeInfo.tMark2WaitSig_sec - 1;

			if ((int)(WaitIrq->TimeInfo.tMark2WaitSig_sec) < 0)
				WaitIrq->TimeInfo.tMark2WaitSig_sec = 0;

			WaitIrq->TimeInfo.tMark2WaitSig_usec =
				1 * 1000000 + WaitIrq->TimeInfo.tMark2WaitSig_usec;
		}
		/*      */
		WaitIrq->TimeInfo.tLastSig2GetSig_usec =
			(time_ready2return.tv_usec -
			 IspInfo.IrqInfo.LastestSigTime_usec[WaitIrq->UserInfo.Type][idx]);
		WaitIrq->TimeInfo.tLastSig2GetSig_sec =
			(time_ready2return.tv_sec -
			 IspInfo.IrqInfo.LastestSigTime_sec[WaitIrq->UserInfo.Type][idx]);
		if ((int)(WaitIrq->TimeInfo.tLastSig2GetSig_usec) < 0) {
			WaitIrq->TimeInfo.tLastSig2GetSig_sec =
				WaitIrq->TimeInfo.tLastSig2GetSig_sec - 1;

			if ((int)(WaitIrq->TimeInfo.tLastSig2GetSig_sec) < 0)
				WaitIrq->TimeInfo.tLastSig2GetSig_sec = 0;

			WaitIrq->TimeInfo.tLastSig2GetSig_usec =
				1 * 1000000 + WaitIrq->TimeInfo.tLastSig2GetSig_usec;
		}
		/*      */
		if (freeze_passbysigcnt)
			WaitIrq->TimeInfo.passedbySigcnt =
				IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->UserInfo.UserKey][WaitIrq->
											  UserInfo.
											  Type][idx] -
				1;
		else
			WaitIrq->TimeInfo.passedbySigcnt =
				IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->UserInfo.UserKey][WaitIrq->
											  UserInfo.
											  Type][idx];

	}
	IspInfo.IrqInfo.Status[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type] &=
		(~WaitIrq->UserInfo.Status);
	/* clear the status     if someone get the irq */
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);
	if (WaitIrq->UserInfo.UserKey > 0) {
		LOG_DBG
		("[WAITIRQv3]user(%d) mark sec/usec(%d/%d),last irq sec/usec(%d/%d),enterwait(%d/%d),getIRQ(%d/%d)\n",
			WaitIrq->UserInfo.UserKey,
			IspInfo.IrqInfo.MarkedTime_sec[WaitIrq->UserInfo.UserKey][WaitIrq->
									   UserInfo.Type][idx],
		 IspInfo.IrqInfo.MarkedTime_usec[WaitIrq->UserInfo.UserKey][WaitIrq->
									    UserInfo.Type][idx],
		 IspInfo.IrqInfo.LastestSigTime_sec[WaitIrq->UserInfo.Type][idx],
		 IspInfo.IrqInfo.LastestSigTime_usec[WaitIrq->UserInfo.Type][idx],
		 (int)(time_getrequest.tv_sec), (int)(time_getrequest.tv_usec),
		 (int)(time_ready2return.tv_sec), (int)(time_ready2return.tv_usec));
		LOG_DBG
		("[WAITIRQv3]user(%d) sigNum(%d/%d),mark sec/usec(%d/%d),irq sec/usec (%d/%d),user(0x%x)\n",
		 WaitIrq->UserInfo.UserKey,
		 IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->UserInfo.UserKey][WaitIrq->
									   UserInfo.
									   Type][idx],
		 WaitIrq->TimeInfo.passedbySigcnt, WaitIrq->TimeInfo.tMark2WaitSig_sec,
		 WaitIrq->TimeInfo.tMark2WaitSig_usec,
		 WaitIrq->TimeInfo.tLastSig2GetSig_sec,
		 WaitIrq->TimeInfo.tLastSig2GetSig_usec, WaitIrq->UserInfo.UserKey);
	}
	/*      */
	/* check CQ     status, when pass2,     pass2b, pass2c done     */
	if (WaitIrq->UserInfo.Type == ISP_IRQ_TYPE_INT_P2_ST) {
		unsigned int CQ_status;

		ISP_WR32(ISP_IMGSYS_BASE + 0x4160, 0x6000);
		CQ_status = ISP_RD32(ISP_IMGSYS_BASE + 0x4164);

		switch (WaitIrq->UserInfo.Status) {
		case ISP_IRQ_P2_STATUS_PASS2A_DON_ST:
			if ((CQ_status & 0x0000000F) != 0x001) {
				LOG_PR_ERR("CQ1 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(ISP_IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		case ISP_IRQ_P2_STATUS_PASS2B_DON_ST:
			if ((CQ_status & 0x000000F0) != 0x010) {
				LOG_PR_ERR("CQ2 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(ISP_IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		case ISP_IRQ_P2_STATUS_PASS2C_DON_ST:
			if ((CQ_status & 0x00000F00) != 0x100) {
				LOG_PR_ERR("CQ3 not idle dbg(0x%08x 0x%08x)",
					ISP_RD32(ISP_IMGSYS_BASE + 0x4160), CQ_status);
			}
			break;
		default:
			break;
		}
	}

EXIT:
	/* 4. clear     mark flag /     reset marked time /   reset time related infor and passedby signal count */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[eIrq]), flags);
	if (WaitIrq->UserInfo.Status & IspInfo.IrqInfo.
	    MarkedFlag[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type]) {
		IspInfo.IrqInfo.MarkedFlag[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.Type] &=
			(~WaitIrq->UserInfo.Status);
		IspInfo.IrqInfo.MarkedTime_usec[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
									   Type][idx] = 0;
		IspInfo.IrqInfo.MarkedTime_sec[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
									  Type][idx] = 0;
		IspInfo.IrqInfo.PassedBySigCnt[WaitIrq->UserInfo.UserKey][WaitIrq->UserInfo.
									  Type][idx] = 0;
	}
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[eIrq]), flags);

	return Ret;
}


/* #define _debug_dma_err_ */
#if     defined(_debug_dma_err_)
#define bit(x) (0x1<<(x))

unsigned int DMA_ERR[3 * 12] = {
	bit(1), 0xF50043A8, 0x00000011, /*     IMGI */
	bit(2), 0xF50043AC, 0x00000021, /*     IMGCI */
	bit(4), 0xF50043B0, 0x00000031, /*     LSCI */
	bit(5), 0xF50043B4, 0x00000051, /*     FLKI */
	bit(6), 0xF50043B8, 0x00000061, /*     LCEI */
	bit(7), 0xF50043BC, 0x00000071, /*     VIPI */
	bit(8), 0xF50043C0, 0x00000081, /*     VIP2I */
	bit(9), 0xF50043C4, 0x00000194, /*     IMGO */
	bit(10), 0xF50043C8, 0x000001a4,        /*     IMG2O */
	bit(11), 0xF50043CC, 0x000001b4,        /*     LCSO */
	bit(12), 0xF50043D0, 0x000001c4,        /*     ESFKO */
	bit(13), 0xF50043D4, 0x000001d4,        /*     AAO     */
};

static signed int DMAErrHandler(void)
{
	unsigned int err_ctrl = ISP_RD32(0xF50043A4);

	LOG_DBG("err_ctrl(0x%08x)", err_ctrl);

	unsigned int i = 0;

	unsigned int *pErr = DMA_ERR;

	for (i = 0; i < 12; i++) {
		unsigned int addr = 0;
#if     1
		if (err_ctrl & (*pErr)) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			LOG_DBG("(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
				addr, ISP_RD32(addr), ISP_RD32(0xF5004160), ISP_RD32(0xF5004164));
		}
#else
		addr = pErr[1];
		unsigned int status = ISP_RD32(addr);

		if (status & 0x0000FFFF) {
			ISP_WR32(0xF5004160, pErr[2]);
			addr = pErr[1];

			LOG_DBG("(0x%08x, 0x%08x), dbg(0x%08x, 0x%08x)",
				addr, status, ISP_RD32(0xF5004160), ISP_RD32(0xF5004164));
		}
#endif
		pErr = pErr + 3;
	}

}
#endif

/* ///////////////////////////////////////////////////////////////////////////// */
/* for CAMSV */
static __tcmfunc irqreturn_t ISP_Irq_CAMSV(signed int Irq, void *DeviceId)
{
	/* unsigned int result=0x0; */
	unsigned int i = 0;
	/* signed int  idx=0; */
	unsigned int IrqStatus_CAMSV;
	union CQ_RTBC_FBC fbc;

	unsigned int curr_pa;
	struct timeval time_frmb;
	unsigned int j = 0, idx = 0, k = 0;

	unsigned long long sec = 0;
	unsigned long usec = 0;

	/* do_gettimeofday(&time_frmb);*/
	sec = ktime_get();     /* ns */
	do_div(sec, 1000);      /*     usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_frmb.tv_usec = usec;
	time_frmb.tv_sec = sec;

	fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC);
	curr_pa = ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR);
	spin_lock(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]));
	IrqStatus_CAMSV =
		(ISP_RD32(ISP_REG_ADDR_CAMSV_INT) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_CAMSV]));

	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		/* 1. update interrupt status to all users */
		IspInfo.IrqInfo.Status[i][ISP_IRQ_TYPE_INT_CAMSV] |=
			(IrqStatus_CAMSV & IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV]);

		/* 2. update signal occurring time and passed by signal count */
		if (IspInfo.IrqInfo.MarkedFlag[i][ISP_IRQ_TYPE_INT_CAMSV] & IspInfo.IrqInfo.
		    Mask[ISP_IRQ_TYPE_INT_CAMSV]) {
			for (k = 0; k < 32; k++) {
				if ((IrqStatus_CAMSV & IspInfo.IrqInfo.
				     Mask[ISP_IRQ_TYPE_INT_CAMSV]) & (1 << k)) {
					idx = my_get_pow_idx(1 << k);
					IspInfo.IrqInfo.
					LastestSigTime_usec[ISP_IRQ_TYPE_INT_CAMSV][idx] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.
					LastestSigTime_sec[ISP_IRQ_TYPE_INT_CAMSV][idx] =
						(unsigned int)time_frmb.tv_sec;
					IspInfo.IrqInfo.
					PassedBySigCnt[i][ISP_IRQ_TYPE_INT_CAMSV][k]++;
				}
			}
		} else {        /* no any interrupt is not marked and  in read mask     in this irq  type */
		}
	}

	if (IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV] & IrqStatus_CAMSV)
		IRQ_LOG_KEEPER_PR_ERR(_CAMSV_IRQ, m_CurrentPPB, _LOG_ERR,
			       CAMSV_TAG "Err IRQ, Type(%d),	Status(0x%x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV,
			       IrqStatus_CAMSV);

	if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
		IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
			       CAMSV_TAG "Type(%d), IrqStatus(0x%x |	0x%x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV,
			       IspInfo.IrqInfo.Status[ISP_IRQ_USER_ISPDRV][ISP_IRQ_TYPE_INT_CAMSV],
			       IrqStatus_CAMSV);


	if (IrqStatus_CAMSV & ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST) {
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV_TAG "DONE_%d_%d(0x%x,0x%x,0x%x,0x%x)\n",
				       (sof_count[_CAMSV]) ? (sof_count[_CAMSV] -
							      1) : (sof_count[_CAMSV]),
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG_INTER_ST) & 0x00FF0000) >>
					16), (unsigned int)(fbc.Reg_val),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_YSIZE));
		}
#if     0                       /* time stamp move to sof */
		unsigned long long sec = 0;
		unsigned long usec = 0;

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			sec = ktime_get();     /* ns */
			do_div(sec, 1000);      /*     usec */
			usec = do_div(sec, 1000000);    /* sec and usec */
		}
		ISP_CAMSV_DONE_Buf_Time(_camsv_imgo_, sec, usec);
#else
		ISP_CAMSV_DONE_Buf_Time(_camsv_imgo_, fbc, 0, 0);
		lost_camsv_done_cnt = 0;
#endif
	}
	if (IrqStatus_CAMSV & ISP_IRQ_CAMSV_STATUS_TG_SOF1_ST) {
		unsigned int rt_dma = 0;
		unsigned long long sec;
		unsigned long usec;
		unsigned int z, buf_idx;

		if (pstRTBuf->ring_buf[_camsv_imgo_].active)
			rt_dma = _camsv_imgo_;
		else
			LOG_DBG("no main dma port opened at CAMSV SOF\n");
		buf_idx = (pstRTBuf->ring_buf[_camsv_imgo_].start + 1) %
			pstRTBuf->ring_buf[_camsv_imgo_].total_count;
		/* chk this frame have EOF or not */
		if ((fbc.Bits.FB_NUM - fbc.Bits.FBC_CNT) == 0) {
			gSof_camsvdone[0] = 1;
			lost_camsv_done_cnt++;
			if (lost_camsv_done_cnt == 2) {
				/*check any buffer is ready? */
				for (k = 0; k < pstRTBuf->ring_buf[rt_dma].total_count; k++) {

					if (pstRTBuf->ring_buf[rt_dma].data[k].bFilled
						== ISP_RTBC_BUF_FILLED) {

					for (j = 0; j < ISP_IRQ_USER_MAX; j++) {
						IspInfo.IrqInfo.Status[j][ISP_IRQ_TYPE_INT_CAMSV] |=
							ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST;
					}

					lost_camsv_done_cnt = 0;
					IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
						"camsv buf rdy but lost pass1 done, wakeup by sof!!");
					break;

					}
				}
			}
			IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV_TAG "Lost done_%d\n", sof_count[_CAMSV]);

		} else {
			gSof_camsvdone[0] = 0;
			if (fbc.Bits.FB_NUM == (fbc.Bits.FBC_CNT + 1))
				gSof_camsvdone[0] = 2;
		}
#ifdef _rtbc_buf_que_2_0_

		if ((enqueImd[_camsv_1] == MFALSE) ||
		((enqueImd[_camsv_1] == MTRUE) && (g_firstCnt[_camsv_1] < fbc.Bits.FB_NUM))) {
			/**/
		if (pstRTBuf->ring_buf[_camsv_imgo_].active) {
			if (pstRTBuf->ring_buf[_camsv_imgo_].empty_count > 0) {
				IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
					CAMSV_TAG "wr2Phy 0x%x,",
					pstRTBuf->ring_buf[_camsv_imgo_].data[buf_idx].base_pAddr);
				ISP_WR32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR,
					pstRTBuf->ring_buf[_camsv_imgo_].data[buf_idx].base_pAddr);
			}
			if ((fbc.Bits.FB_NUM - fbc.Bits.FBC_CNT) == 0) {
				IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
					CAMSV_TAG "drop wr2Phy_0x%x: ",
					pstRTBuf->ring_buf[_camsv_imgo_].
					data[pstRTBuf->ring_buf[_camsv_imgo_].start].base_pAddr);
				ISP_WR32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR,
					pstRTBuf->ring_buf[_camsv_imgo_].
					data[pstRTBuf->ring_buf[_camsv_imgo_].start].base_pAddr);
			}
		}
		/* equal case is for clear curidx */
		for (z = 0; z <= mFwRcnt.curIdx[_CAMSV_IRQ]; z++) {
			if (mFwRcnt.INC[_CAMSV_IRQ][z] == 1) {
				mFwRcnt.INC[_CAMSV_IRQ][z] = 0;
				/* patch hw bug */
				fbc.Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_CAMSV_IMGO_FBC, fbc.Reg_val);
				fbc.Bits.RCNT_INC = 0;
				ISP_WR32(ISP_REG_ADDR_CAMSV_IMGO_FBC, fbc.Reg_val);
				IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
					       CAMSV_TAG "RCNT_INC\n");
			} else {
				mFwRcnt.curIdx[_CAMSV_IRQ] = 0;
				break;
			}
		}
			/**/
		}

#endif
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			union CQ_RTBC_FBC _fbc_chk;

			/*     in order to     log     newest fbc condition */
			_fbc_chk.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC);
			IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV_TAG "SOF_%d_%d(0x%x,0x%x,0x%x,0x%x)\n",
				       sof_count[_CAMSV],
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG_INTER_ST) & 0x00FF0000) >>
					16), _fbc_chk.Reg_val,
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_YSIZE));

			if (pstRTBuf->ring_buf[_camsv_imgo_].active) {
				if ((_fbc_chk.Bits.WCNT - fbc.Bits.WCNT) != 0)
					IRQ_LOG_KEEPER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF,
					"sv1:SW ISR right on next hw p1_done(0x%x_0x%x)\n",
					_fbc_chk.Reg_val, fbc.Reg_val);
			}
		}
		/*		unsigned long long sec;*/
		/*		unsigned long usec;*/
		/*		ktime_t time;*/
		sec = ktime_get();	/* ns */
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		curr_pa = ISP_RD32(ISP_REG_ADDR_IMGO_SV_BASE_ADDR);
		if (gSof_camsvdone[0] == 1)
			ISP_CAMSV_SOF_Buf_Get(_camsv_imgo_, fbc, curr_pa, sec, usec, MTRUE);
		else
			ISP_CAMSV_SOF_Buf_Get(_camsv_imgo_, fbc, curr_pa, sec, usec, MFALSE);
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]));
#ifdef ISR_LOG_ON
	/*IRQ_LOG_PRINTER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF);*/
	/*IRQ_LOG_PRINTER_PR_ERR(_CAMSV_IRQ, m_CurrentPPB, _LOG_ERR);*/
#endif
	wake_up_interruptible_all(&IspInfo.WaitQueueHead);

	if ((IrqStatus_CAMSV & (ISP_IRQ_CAMSV_STATUS_PASS1_DON_ST)) ||
		(IrqStatus_CAMSV & (ISP_IRQ_CAMSV_STATUS_TG_SOF1_ST))) {
		tasklet_schedule(&isp_tasklet);
	}

	return IRQ_HANDLED;
}

static __tcmfunc irqreturn_t ISP_Irq_CAMSV2(signed int Irq, void *DeviceId)
{
	/* unsigned int result=0x0; */
	unsigned int i = 0;
	/* signed int  idx=0; */
	unsigned int IrqStatus_CAMSV2;
	union CQ_RTBC_FBC fbc;

	unsigned int curr_pa;
	struct timeval time_frmb;
	unsigned int j = 0, idx = 0, k = 0;

	unsigned long long sec = 0;
	unsigned long usec = 0;

	/* do_gettimeofday(&time_frmb);*/
	sec = ktime_get();     /* ns */
	do_div(sec, 1000);      /*     usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_frmb.tv_usec = usec;
	time_frmb.tv_sec = sec;

	fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC);
	curr_pa = ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR);
	spin_lock(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]));
	IrqStatus_CAMSV2 =
		(ISP_RD32(ISP_REG_ADDR_CAMSV2_INT) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV2] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_CAMSV2]));

	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		/* 1. update interrupt status to all users */
		IspInfo.IrqInfo.Status[i][ISP_IRQ_TYPE_INT_CAMSV2] |=
			(IrqStatus_CAMSV2 & IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV2]);

		/* 2. update signal occurring time and passed by signal count */
		if (IspInfo.IrqInfo.MarkedFlag[i][ISP_IRQ_TYPE_INT_CAMSV2] & IspInfo.IrqInfo.
		    Mask[ISP_IRQ_TYPE_INT_CAMSV2]) {
			for (k = 0; k < 32; k++) {
				if ((IrqStatus_CAMSV2 & IspInfo.IrqInfo.
				     Mask[ISP_IRQ_TYPE_INT_CAMSV2]) & (1 << k)) {
					idx = my_get_pow_idx(1 << k);
					IspInfo.IrqInfo.
					LastestSigTime_usec[ISP_IRQ_TYPE_INT_CAMSV2][idx] =
						(unsigned int)time_frmb.tv_usec;
					IspInfo.IrqInfo.
					LastestSigTime_sec[ISP_IRQ_TYPE_INT_CAMSV2][idx] =
						(unsigned int)time_frmb.tv_sec;
					IspInfo.IrqInfo.
					PassedBySigCnt[i][ISP_IRQ_TYPE_INT_CAMSV2][k]++;
				}
			}
		} else {        /* no any interrupt     is not marked and  in read mask in this irq     type */
		}
	}

	if (IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV2] & IrqStatus_CAMSV2)
		IRQ_LOG_KEEPER_PR_ERR(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_ERR,
			       CAMSV2_TAG "Error IRQ, Type(%d), Status(0x%08x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV2,
			       IrqStatus_CAMSV2);

	if (IspInfo.DebugMask & ISP_DBG_BUF_CTRL)
		IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
			       CAMSV2_TAG "Type(%d), IrqStatus(0x%x | 0x%08x)\n",
			       ISP_IRQ_TYPE_INT_CAMSV2,
			       IspInfo.IrqInfo.Status[ISP_IRQ_USER_ISPDRV][ISP_IRQ_TYPE_INT_CAMSV2],
			       (unsigned int)(IrqStatus_CAMSV2));

	if (IrqStatus_CAMSV2 & ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST) {
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			/* IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF, CAMSV2_TAG "fbc(0x%x)",
			 *	       (unsigned int)(fbc.Reg_val));
			 */

			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV2_TAG
				       "DONE_%d_%d(0x%x,0x%x,0x%x,0x%x,camsv support no	inner addr)\n",
				       (sof_count[_CAMSV_D]) ? (sof_count[_CAMSV_D] -
								1) : (sof_count[_CAMSV_D]),
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_INTER_ST) &
					 0x00FF0000) >> 16), (unsigned int)(fbc.Reg_val),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_YSIZE));
		}
#if     0                       /* time stamp move to sof     */
		unsigned long long sec;
		unsigned long usec;

		sec = ktime_get();     /* ns */
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */

		ISP_CAMSV_DONE_Buf_Time(_camsv2_imgo_, sec, usec);
#else
		ISP_CAMSV_DONE_Buf_Time(_camsv2_imgo_, fbc, 0, 0);
		lost_camsv2_done_cnt = 0;
#endif
	}
	if (IrqStatus_CAMSV2 & ISP_IRQ_CAMSV2_STATUS_TG_SOF1_ST) {
		unsigned int rt_dma = 0;

		unsigned long long sec;
		unsigned long usec;
		unsigned int z, buf_idx;

		if (pstRTBuf->ring_buf[_camsv2_imgo_].active)
			rt_dma = _camsv2_imgo_;
		else
			LOG_DBG("no main dma port opened at CAMSV2 SOF\n");

		buf_idx = (pstRTBuf->ring_buf[_camsv2_imgo_].start + 1) %
			pstRTBuf->ring_buf[_camsv2_imgo_].total_count;
		/* chk this     frame have EOF or not */
		if ((fbc.Bits.FB_NUM - fbc.Bits.FBC_CNT) == 0) {
			gSof_camsvdone[1] = 1;
			lost_camsv2_done_cnt++;
			if (lost_camsv2_done_cnt == 2) {
				/* check any buffer is ready? */
				for (k = 0; k < pstRTBuf->ring_buf[rt_dma].total_count; k++) {

					if (pstRTBuf->ring_buf[rt_dma].data[k].bFilled
						== ISP_RTBC_BUF_FILLED) {

					for (j = 0; j < ISP_IRQ_USER_MAX; j++) {

					IspInfo.IrqInfo.Status[j][ISP_IRQ_TYPE_INT_CAMSV2] |=
						ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST;
					}
					lost_camsv2_done_cnt = 0;
					IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
						"camsv2 buf rdy but lost pass1 done, wakeup by sof!!");
					break;

					}
				}
			}
			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV2_TAG "Lost done %d", sof_count[_CAMSV_D]);

		} else {
			gSof_camsvdone[1] = 0;
			if (fbc.Bits.FB_NUM == (fbc.Bits.FBC_CNT - 1))
				gSof_camsvdone[1] = 2;
		}
#ifdef _rtbc_buf_que_2_0_

		if ((enqueImd[_camsv_2] == MFALSE) ||
		((enqueImd[_camsv_2] == MTRUE) && (g_firstCnt[_camsv_2] < fbc.Bits.FB_NUM))) {
			/**/
		if (pstRTBuf->ring_buf[_camsv2_imgo_].active) {
			if (pstRTBuf->ring_buf[_camsv2_imgo_].empty_count > 0) {
				IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
					 CAMSV2_TAG "wr2Phy, 0x%x",
					 pstRTBuf->ring_buf[_camsv2_imgo_].data[buf_idx].base_pAddr);
				ISP_WR32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR,
					pstRTBuf->ring_buf[_camsv2_imgo_].data[buf_idx].base_pAddr);
			}
			if ((fbc.Bits.FB_NUM - fbc.Bits.FBC_CNT) == 0) {
				IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
					CAMSV2_TAG "drop wr2Phy_0x%x: ",
					pstRTBuf->ring_buf[_camsv2_imgo_].
					data[pstRTBuf->ring_buf[_camsv2_imgo_].start].base_pAddr);
				ISP_WR32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR,
					pstRTBuf->ring_buf[_camsv2_imgo_].
					data[pstRTBuf->ring_buf[_camsv2_imgo_].start].base_pAddr);
			}
		}
		/* equal case is for clear curidx */
		for (z = 0; z <= mFwRcnt.curIdx[_CAMSV_D_IRQ]; z++) {
			if (mFwRcnt.INC[_CAMSV_D_IRQ][z] == 1) {
				mFwRcnt.INC[_CAMSV_D_IRQ][z] = 0;
				/* path hw bug */
				fbc.Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_CAMSV2_IMGO_FBC, fbc.Reg_val);
				fbc.Bits.RCNT_INC = 0;
				ISP_WR32(ISP_REG_ADDR_CAMSV2_IMGO_FBC, fbc.Reg_val);
				IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
					       CAMSV2_TAG "RCNT_INC\n");
			} else {
				mFwRcnt.curIdx[_CAMSV_D_IRQ] = 0;
				break;
			}
		}
			/**/
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			union CQ_RTBC_FBC _fbc_chk;

			/* in order to log newest fbc condition */
			_fbc_chk.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC);

			IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
				       CAMSV2_TAG
				       "SOF_%d_%d(0x%x,0x%x,0x%x,0x%x,camsv	support	no inner addr)\n",
				       sof_count[_CAMSV_D],
				       ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_INTER_ST) & 0x00FF0000)
					>> 16), _fbc_chk.Reg_val,
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_XSIZE),
				       ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_YSIZE));
			if (pstRTBuf->ring_buf[_camsv2_imgo_].active) {
				if ((_fbc_chk.Bits.WCNT - fbc.Bits.WCNT) != 0)
					IRQ_LOG_KEEPER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF,
						"sv2:SW ISR right on next hw p1_done(0x%x_0x%x)\n",
						_fbc_chk.Reg_val, fbc.Reg_val);
			}
		}
		/*              unsigned long long sec;*/
		/*              unsigned long usec;*/
		/*              ktime_t time;*/

		sec = ktime_get();	/* ns */
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		curr_pa = ISP_RD32(ISP_REG_ADDR_IMGO_SV_D_BASE_ADDR);
		if (gSof_camsvdone[1] == 1)
			ISP_CAMSV_SOF_Buf_Get(_camsv2_imgo_, fbc, curr_pa, sec, usec, MTRUE);
		else
			ISP_CAMSV_SOF_Buf_Get(_camsv2_imgo_, fbc, curr_pa, sec, usec, MFALSE);
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]));
	/* dump log     during spin     lock */
#ifdef ISR_LOG_ON
	/*IRQ_LOG_PRINTER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF);*/
	/*IRQ_LOG_PRINTER_PR_ERR(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_ERR);*/
#endif
	wake_up_interruptible_all(&IspInfo.WaitQueueHead);

	if ((IrqStatus_CAMSV2 & (ISP_IRQ_CAMSV2_STATUS_PASS1_DON_ST)) ||
		(IrqStatus_CAMSV2 & (ISP_IRQ_CAMSV2_STATUS_TG_SOF1_ST))) {
		tasklet_schedule(&isp_tasklet);
	}

	return IRQ_HANDLED;
}

/* ///////////////////////////////////////////////////////////////////////////// */

/*******************************************************************************
*
********************************************************************************/
static __tcmfunc irqreturn_t ISP_Irq_CAM(signed int Irq, void *DeviceId)
{

	/* LOG_DBG("- E.");*/
	unsigned int i;

	unsigned int IrqStatus[ISP_IRQ_TYPE_AMOUNT] = {0};
	/* unsigned int IrqStatus_fbc_int; */
	union CQ_RTBC_FBC p1_fbc[4];

	unsigned int curr_pa[4];    /* debug only at sof */
	unsigned int cur_v_cnt = 0;
	unsigned int d_cur_v_cnt = 0;
	unsigned int j = 0, idx = 0, k = 0;
	struct timeval time_frmb;
	unsigned long long sec = 0;
	unsigned long usec = 0;

	for (i = 0; i < 4; i++)
		curr_pa[i] = 0;
#if     0
	if ((ISP_RD32(ISP_REG_ADDR_TG_VF_CON) & 0x1) == 0x0) {
		LOG_INF("before	vf:0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
			ISP_RD32(ISP_REG_ADDR_INT_P1_ST), ISP_RD32(ISP_REG_ADDR_INT_P1_ST2),
			ISP_RD32(ISP_REG_ADDR_INT_P1_ST_D), ISP_RD32(ISP_REG_ADDR_INT_P1_ST2_D),
			ISP_RD32(ISP_REG_ADDR_INT_P2_ST), ISP_RD32(ISP_REG_ADDR_INT_STATUSX),
			ISP_RD32(ISP_REG_ADDR_INT_STATUS2X), ISP_RD32(ISP_REG_ADDR_INT_STATUS3X));
	}
#endif
	/*      */
	/* do_gettimeofday(&time_frmb);*/
	sec = ktime_get();     /* ns */
	do_div(sec, 1000);      /*     usec */
	usec = do_div(sec, 1000000);    /* sec and usec */
	time_frmb.tv_usec = usec;
	time_frmb.tv_sec = sec;

	/* Read irq     status */
	/* spin_lock(&(IspInfo.SpinLockIrq[_IRQ])); */
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] =
		(ISP_RD32(ISP_REG_ADDR_INT_P1_ST) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_P1_ST]));
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST2] = (ISP_RD32(ISP_REG_ADDR_INT_P1_ST2));
	/* & (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2]|IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST2])); */
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] =
		(ISP_RD32(ISP_REG_ADDR_INT_P1_ST_D) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST_D] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_P1_ST_D]));
	IrqStatus[ISP_IRQ_TYPE_INT_P1_ST2_D] =
		(ISP_RD32(ISP_REG_ADDR_INT_P1_ST2_D) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2_D] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_P1_ST2_D]));
#ifdef __debugused__
	IrqStatus[ISP_IRQ_TYPE_INT_P2_ST] =
		(ISP_RD32(ISP_REG_ADDR_INT_P2_ST) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P2_ST] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_P2_ST]));
#endif
	/* below may need to read elsewhere     */
	IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] =
		(ISP_RD32(ISP_REG_ADDR_INT_STATUSX) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUSX] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_STATUSX]));
	IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X] =
		(ISP_RD32(ISP_REG_ADDR_INT_STATUS2X) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS2X] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_STATUS2X]));
	IrqStatus[ISP_IRQ_TYPE_INT_STATUS3X] =
		(ISP_RD32(ISP_REG_ADDR_INT_STATUS3X) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS3X] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_STATUS3X]));
	/* spin_unlock(&(IspInfo.SpinLockIrq[_IRQ])); */
	/* IrqStatus_fbc_int = ISP_RD32(ISP_ADDR + 0xFC); */

	p1_fbc[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
	p1_fbc[1].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);
	p1_fbc[2].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
	p1_fbc[3].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);
	curr_pa[0] = ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR);
	curr_pa[1] = ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR);
#if (ISP_RAW_D_SUPPORT == 1)
	curr_pa[2] = ISP_RD32(ISP_REG_ADDR_IMGO_D_BASE_ADDR);
	curr_pa[3] = ISP_RD32(ISP_REG_ADDR_RRZO_D_BASE_ADDR);
#endif
#ifdef __debugused__
	LOG_INF("irq status:0x%x,0x%x,0x%x,0x%x,0x%x\n",
		IrqStatus[ISP_IRQ_TYPE_INT_P1_ST],
		IrqStatus[ISP_IRQ_TYPE_INT_P1_ST2],
		IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D],
		IrqStatus[ISP_IRQ_TYPE_INT_P1_ST2_D], IrqStatus[ISP_IRQ_TYPE_INT_P2_ST]);
#endif

#if     1                       /* err status mechanism */
#define STATUSX_WARNING (ISP_IRQ_STATUSX_ESFKO_ERR_ST | \
			 ISP_IRQ_STATUSX_RRZO_ERR_ST | \
			 ISP_IRQ_STATUSX_LCSO_ERR_ST | \
			 ISP_IRQ_STATUSX_AAO_ERR_ST | \
			 ISP_IRQ_STATUSX_IMGO_ERR_ST | \
			 ISP_IRQ_STATUSX_RRZO_ERR_ST)
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX] =
		IspInfo.IrqInfo.
		ErrMask[ISP_IRQ_TYPE_INT_STATUSX] &
		(~(ISP_IRQ_STATUSX_IMGO_DROP_FRAME_ST | ISP_IRQ_STATUSX_RRZO_DROP_FRAME_ST));
	/* p1   && p1_d share the same interrupt status */
	if (IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX]) {
		if ((IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & ISP_IRQ_STATUSX_DMA_ERR_ST)
		    || (IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X] & ISP_IRQ_STATUSX_DMA_ERR_ST)) {
			g_bDmaERR_p1_d = MTRUE;
			g_bDmaERR_p1 = MTRUE;
			g_bDmaERR_deepDump = MFALSE;
			ISP_DumpDmaDeepDbg();
		}

		/* if(IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] &     ISP_IRQ_STATUSX_AAO_ERR_ST)     { */
		/* ISP_DumpReg(); */
		/* } */
		/* mark, can ignor fifo may     overrun if dma_err isn't pulled. */
		/* if(IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] &     STATUSX_WARNING) { */
		/* LOG_INF("warning: fifo may overrun"); */
		/* } */
		if (IrqStatus[ISP_IRQ_TYPE_INT_STATUSX] & (~STATUSX_WARNING)) {
			LOG_PR_ERR("ISP INT ERR_P1	0x%x\n", IrqStatus[ISP_IRQ_TYPE_INT_STATUSX]);
			g_ISPIntErr[_IRQ] |= IrqStatus[ISP_IRQ_TYPE_INT_STATUSX];
		}
		if (IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X] & (~STATUSX_WARNING)) {
			LOG_PR_ERR("ISP INT ERR_P1_D 0x%x\n", IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X]);
			g_ISPIntErr[_IRQ_D] |= IrqStatus[ISP_IRQ_TYPE_INT_STATUS2X];
		}
	}
	/* LOG_INF("isp irq     status:0x%x_0x%x",*/
	/*   IrqStatus[ISP_IRQ_TYPE_INT_P1_ST],IrqStatus[ISP_IRQ_TYPE_INT_STATUSX]); */
	/* LOG_INF("imgo fill:%d %d     %d\n",*/
	/*   pstRTBuf->ring_buf[_imgo_].data[0].bFilled,pstRTBuf->ring_buf[_imgo_].data[1].bFilled,*/
	/*   pstRTBuf->ring_buf[_imgo_].data[2].bFilled); */
	/* LOG_INF("rrzo fill:%d %d     %d\n",*/
	/*   pstRTBuf->ring_buf[_rrzo_].data[0].bFilled,pstRTBuf->ring_buf[_rrzo_].data[1].bFilled,*/
	/*   pstRTBuf->ring_buf[_rrzo_].data[2].bFilled); */
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_PASS1_DON_ST)
	    || (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_SOF1_INT_ST)) {
		cur_v_cnt = ((ISP_RD32(ISP_REG_ADDR_TG_INTER_ST) & 0x00FF0000) >> 16);
	}
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_D_PASS1_DON_ST)
	    || (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_D_SOF1_INT_ST)) {
		d_cur_v_cnt = ((ISP_RD32(ISP_REG_ADDR_TG2_INTER_ST) & 0x00FF0000) >> 16);
	}
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_PASS1_DON_ST)
	    && (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_SOF1_INT_ST)) {
		if (cur_v_cnt != sof_count[_PASS1])
			LOG_INF("isp sof_don block,	%d_%d\n", cur_v_cnt, sof_count[_PASS1]);
	}
#endif

#if 0
	/ sensor interface would use another isr id
	/* sensor interface     related irq     */
	IrqStatus[ISP_IRQ_TYPE_INT_SENINF1] =
		(ISP_RD32(ISP_REG_ADDR_SENINF1_INT) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF1] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_SENINF1]));
	IrqStatus[ISP_IRQ_TYPE_INT_SENINF2] =
		(ISP_RD32(ISP_REG_ADDR_SENINF2_INT) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF2] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_SENINF2]));
	IrqStatus[ISP_IRQ_TYPE_INT_SENINF3] =
		(ISP_RD32(ISP_REG_ADDR_SENINF3_INT) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF3] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_SENINF3]));
	IrqStatus[ISP_IRQ_TYPE_INT_SENINF4] =
		(ISP_RD32(ISP_REG_ADDR_SENINF4_INT) &
		 (IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF4] | IspInfo.IrqInfo.
		  ErrMask[ISP_IRQ_TYPE_INT_SENINF4]));
#endif


	/* service pass1_done first     once if SOF/PASS1_DONE are coming together.     */
	/* get time     stamp */
	/* push hw filled buffer to     sw list */
	/* LOG_INF("RTBC_DBG %x_%x\n",IrqStatus[ISP_IRQ_TYPE_INT_P1_ST],IrqStatus[ISP_IRQ_TYPE_INT_STATUSX]); */
	spin_lock(&(IspInfo.SpinLockIrq[_IRQ]));
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_PASS1_DON_ST) {
#ifdef _rtbc_buf_que_2_0_
		unsigned long long sec;
		unsigned long usec;

		sec = ktime_get();     /* ns */
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with the time stamp in image header) */
		IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST][10] =
			(unsigned int)(sec);
		gEismetaInSOF = 0;
		ISP_DONE_Buf_Time(_IRQ, p1_fbc, 0, 0);
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, "P1_DON_%d(0x%x,0x%x)\n",
				       (sof_count[_PASS1]) ? (sof_count[_PASS1] -
							      1) : (sof_count[_PASS1]),
				       (unsigned int)(p1_fbc[0].Reg_val),
				       (unsigned int)(p1_fbc[1].Reg_val));
		}
		lost_pass1_done_cnt = 0;
#else
#if     defined(_rtbc_use_cq0c_)
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, "P1_DON_%d(0x%x,0x%x)\n",
				       (sof_count[_PASS1]) ? (sof_count[_PASS1] -
							      1) : (sof_count[_PASS1]),
				       (unsigned int)(p1_fbc[0].Reg_val),
				       (unsigned int)(p1_fbc[1].Reg_val));
		}
#else
		/* LOG_DBG("[k_js_test]Pass1_done(0x%x)",IrqStatus[ISP_IRQ_TYPE_INT]); */
		unsigned long long sec;
		unsigned long usec;

		sec = ktime_get();     /* ns */
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for     eis     user(need match with the time stamp in image header) */
		IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST][10] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST][10] =
			(unsigned int)(usec);
		gEismetaInSOF = 0;
		ISP_DONE_Buf_Time(p1_fbc, sec, usec);
		/*Check Timesamp reverse */
		/* what's this? */
		/*      */
#endif
#endif
	}

	/* switch pass1 WDMA buffer     */
	/* fill time stamp for cq0c     */
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_SOF1_INT_ST) {
		unsigned int _dmaport = 0;
		unsigned int rt_dma = 0;
		unsigned long long sec;
		unsigned long usec;
		unsigned int z;

		if (pstRTBuf->ring_buf[_imgo_].active) {
			_dmaport = 0;
			rt_dma = _imgo_;
		} else if (pstRTBuf->ring_buf[_rrzo_].active) {
			_dmaport = 1;
			rt_dma = _rrzo_;
		} else {
			LOG_INF("no main dma port opened at	SOF\n");
		}
		/* chk this     frame have EOF or not, dynimic dma port chk     */
		if ((p1_fbc[_dmaport].Bits.FB_NUM - p1_fbc[_dmaport].Bits.FBC_CNT) == 0) {
			sof_pass1done[0] = 1;
#ifdef _rtbc_buf_que_2_0_
			/* ISP_LostP1Done_ErrHandle(_imgo_); */
			/* ISP_LostP1Done_ErrHandle(_rrzo_); */
			/* IRQ_LOG_KEEPER(_IRQ,m_CurrentPPB,_LOG_INF,"lost p1Done ErrHandle\n"); */
#endif
			lost_pass1_done_cnt++;
			if (lost_pass1_done_cnt == 2) {
				/*check any buffer is ready? */
				for (k = 0; k < pstRTBuf->ring_buf[rt_dma].total_count; k++) {
					if (pstRTBuf->ring_buf[rt_dma].data[k].bFilled == ISP_RTBC_BUF_FILLED) {

					for (j = 0; j < ISP_IRQ_USER_MAX; j++) {
						IspInfo.IrqInfo.Status[j][ISP_IRQ_TYPE_INT_P1_ST] |=
						ISP_IRQ_P1_STATUS_PASS1_DON_ST;
					}

					lost_pass1_done_cnt = 0;
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
					       "raw buf rdy but lost pass1 done, wakeup by sof!!");
					break;
					}
				}
			}
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, "Lost p1 done_%d (0x%x): ",
				       sof_count[_PASS1], cur_v_cnt);
		} else {
			sof_pass1done[0] = 0;
			if (p1_fbc[_dmaport].Bits.FB_NUM == (p1_fbc[_dmaport].Bits.FBC_CNT + 1))
				sof_pass1done[0] = 2;

		}
#ifdef _rtbc_buf_que_2_0_

		if (mFwRcnt.bLoadBaseAddr[_IRQ] == 1) {
			if (pstRTBuf->ring_buf[_imgo_].active) {
				IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, " p1_%d:wr2Phy_0x%x: ",
					       _imgo_,
					       pstRTBuf->ring_buf[_imgo_].data[pstRTBuf->
									       ring_buf[_imgo_].
									       start].base_pAddr);
				ISP_WR32(ISP_REG_ADDR_IMGO_BASE_ADDR,
					 pstRTBuf->ring_buf[_imgo_].data[pstRTBuf->ring_buf[_imgo_].
									 start].base_pAddr);
			}
			if (pstRTBuf->ring_buf[_rrzo_].active) {
				IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF, " p1_%d:wr2Phy_0x%x: ",
					       _rrzo_,
					       pstRTBuf->ring_buf[_rrzo_].data[pstRTBuf->
									       ring_buf[_rrzo_].
									       start].base_pAddr);
				ISP_WR32(ISP_REG_ADDR_RRZO_BASE_ADDR,
					 pstRTBuf->ring_buf[_rrzo_].data[pstRTBuf->ring_buf[_rrzo_].
									 start].base_pAddr);
			}
			mFwRcnt.bLoadBaseAddr[_IRQ] = 0;
		}
		/* equal case is for clear curidx */
		for (z = 0; z <= mFwRcnt.curIdx[_IRQ]; z++) {
			/* LOG_INF("curidx:%d\n",mFwRcnt.curIdx[_IRQ]); */
			if (mFwRcnt.INC[_IRQ][z] == 1) {
				mFwRcnt.INC[_IRQ][z] = 0;
				p1_fbc[0].Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_IMGO_FBC, p1_fbc[0].Reg_val);
				p1_fbc[1].Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_RRZO_FBC, p1_fbc[1].Reg_val);
				if (IspInfo.DebugMask & ISP_DBG_INT)
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
						       " p1:RCNT_INC:	");
			} else {
				/* LOG_INF("RTBC_DBG:%d %d %d %d %d     %d %d %d %d     %d",*/
				/*   mFwRcnt.INC[_IRQ][0],mFwRcnt.INC[_IRQ][1],mFwRcnt.INC[_IRQ][2],*/
				/*   mFwRcnt.INC[_IRQ][3],mFwRcnt.INC[_IRQ][4],\ */
				/* mFwRcnt.INC[_IRQ][5],mFwRcnt.INC[_IRQ][6],mFwRcnt.INC[_IRQ][7],*/
				/*   mFwRcnt.INC[_IRQ][8],mFwRcnt.INC[_IRQ][9]); */
				mFwRcnt.curIdx[_IRQ] = 0;
				break;
			}
		}
#endif
		if (IspInfo.DebugMask & ISP_DBG_INT) {
			union CQ_RTBC_FBC _fbc_chk[2];        /* can chk fbc status compare to p1_fbc.*/
						/*	(the difference is the     timing of reading) */
			 /* in order to log newest fbc condition */
			_fbc_chk[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_FBC);
			_fbc_chk[1].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_FBC);
			IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
				       "P1_SOF_%d_%d(0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x)\n",
				       sof_count[_PASS1], cur_v_cnt,
				       (unsigned int)(_fbc_chk[0].Reg_val),
				       (unsigned int)(_fbc_chk[1].Reg_val),
				       ISP_RD32(ISP_REG_ADDR_IMGO_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_RRZO_BASE_ADDR),
				       ISP_RD32(ISP_INNER_REG_ADDR_IMGO_YSIZE),
				       ISP_RD32(ISP_INNER_REG_ADDR_RRZO_YSIZE),
				       ISP_RD32(ISP_REG_ADDR_TG_MAGIC_0));
			/* 1 port is enough     */
			if (pstRTBuf->ring_buf[_imgo_].active) {
				if ((_fbc_chk[0].Bits.WCNT - p1_fbc[0].Bits.WCNT) != 0)
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
						       "imgo:SW ISR right	on next	hw p1_done(0x%x_0x%x)\n",
						       _fbc_chk[0].Reg_val, p1_fbc[0].Reg_val);
			} else if (pstRTBuf->ring_buf[_rrzo_].active) {
				if ((_fbc_chk[1].Bits.WCNT - p1_fbc[1].Bits.WCNT) != 0)
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
						       "rrzo:SW ISR right	on next	hw p1_done(0x%x_0x%x)\n",
						       _fbc_chk[1].Reg_val, p1_fbc[1].Reg_val);
			}
		}
		/*              unsigned long long sec;*/
		/*              unsigned long usec;*/
		/*              ktime_t time;*/

		sec = ktime_get();	/* ns */
#ifdef T_STAMP_2_0
		if (g1stSof[_IRQ] == MTRUE)
			m_T_STAMP.T_ns = sec;

		if (m_T_STAMP.fps > SlowMotion) {
			m_T_STAMP.fcnt++;
			if (g1stSof[_IRQ] == MFALSE) {
				m_T_STAMP.T_ns +=
					((unsigned long long)m_T_STAMP.interval_us * 1000);
				if (m_T_STAMP.fcnt == m_T_STAMP.fps) {
					m_T_STAMP.fcnt = 0;
					m_T_STAMP.T_ns +=
						((unsigned long long)m_T_STAMP.compensation_us * 1000);
				}
			}
			sec = m_T_STAMP.T_ns;
		}
#endif
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update SOF time stamp for eis user(need match with the time stamp in image header) */
		IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST][12] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST][12] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST][0] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST][0] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.Eismeta[ISP_IRQ_TYPE_INT_P1_ST][gEismetaWIdx].tLastSOF2P1done_sec =
			(unsigned int)(sec);
		IspInfo.IrqInfo.Eismeta[ISP_IRQ_TYPE_INT_P1_ST][gEismetaWIdx].tLastSOF2P1done_usec =
			(unsigned int)(usec);
		gEismetaInSOF = 1;
		gEismetaWIdx = ((gEismetaWIdx + 1) % EISMETA_RINGSIZE);
		if (sof_pass1done[0] == 1)
			ISP_SOF_Buf_Get(_IRQ, p1_fbc, (unsigned int *)curr_pa, sec, usec, MTRUE);
		else
			ISP_SOF_Buf_Get(_IRQ, p1_fbc, (unsigned int *)curr_pa, sec, usec, MFALSE);
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_IRQ]));
#if     0                       /* in order to keep the isr stability. */
	if (bSlowMotion == MFALSE) {
		if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_PASS1_DON_ST)
			LOG_INF("1D_%d\n",
				(sof_count[_PASS1]) ? (sof_count[_PASS1] -
						       1) : (sof_count[_PASS1]));

		if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & ISP_IRQ_P1_STATUS_SOF1_INT_ST)
			LOG_INF("1S_%d\n",
				(sof_count[_PASS1]) ? (sof_count[_PASS1] -
						       1) : (sof_count[_PASS1]));

	}
#endif
	spin_lock(&(IspInfo.SpinLockIrq[_IRQ]));
#if (ISP_RAW_D_SUPPORT == 1)
	/* TG_D Done */
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & ISP_IRQ_P1_STATUS_D_PASS1_DON_ST) {
		unsigned long long sec;
		unsigned long usec;

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF,
				       "P1_D_DON_%d_%d(0x%x,0x%x)\n",
				       (sof_count[_PASS1_D]) ? (sof_count[_PASS1_D] -
								1) : (sof_count[_PASS1_D]),
				       d_cur_v_cnt, (unsigned int)(p1_fbc[2].Reg_val),
				       (unsigned int)(p1_fbc[3].Reg_val));
		}
#ifdef _rtbc_buf_que_2_0_

		sec = ktime_get();     /* ns */
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update pass1 done time stamp for eis user(need match with the time stamp in image header) */
		IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST_D][10] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST_D][10] =
			(unsigned int)(sec);
		gEismetaInSOF_D = 0;
		ISP_DONE_Buf_Time(_IRQ_D, p1_fbc, 0, 0);
#endif
		lost_pass1_d_done_cnt = 0;
	}

	/* TG_D SOF     */
	if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & ISP_IRQ_P1_STATUS_D_SOF1_INT_ST) {
		unsigned int _dmaport = 0;
		unsigned int rt_dma = 0;
		unsigned long long sec;
		unsigned long usec;
		ktime_t time;
		unsigned int z;

		if (pstRTBuf->ring_buf[_imgo_d_].active) {
			_dmaport = 2;
			rt_dma = _imgo_d_;
		} else if (pstRTBuf->ring_buf[_rrzo_d_].active) {
			_dmaport = 3;
			rt_dma = _rrzo_d_;
		} else {
			LOG_INF("no main dma port opened at	SOF_D\n");
		}

		/* chk this     frame have EOF or not,dynamic dma port chk */
		if ((p1_fbc[_dmaport].Bits.FB_NUM - p1_fbc[_dmaport].Bits.FBC_CNT) == 0) {
			sof_pass1done[1] = 1;
#ifdef _rtbc_buf_que_2_0_
			/* ISP_LostP1Done_ErrHandle(_imgo_d_); */
			/* ISP_LostP1Done_ErrHandle(_rrzo_d_); */
			/* IRQ_LOG_KEEPER(_IRQ_D,m_CurrentPPB,_LOG_INF,"lost p1d_Done ErrHandle\n"); */
#endif
			lost_pass1_d_done_cnt++;
			if (lost_pass1_d_done_cnt == 2) {
				/* check any buffer is ready? */
				for (k = 0; k < pstRTBuf->ring_buf[rt_dma].total_count; k++) {
				if (pstRTBuf->ring_buf[rt_dma].data[k].bFilled == ISP_RTBC_BUF_FILLED) {
					for (j = 0; j < ISP_IRQ_USER_MAX; j++)
						IspInfo.IrqInfo.Status[j][ISP_IRQ_TYPE_INT_P1_ST_D] |=
						ISP_IRQ_P1_STATUS_D_PASS1_DON_ST;

						lost_pass1_d_done_cnt = 0;
						IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
							       "raw-D buf rdy but lost pass1 done, wakeup by sof!!");
						break;
					}
				}
			}
			IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF, "Lost p1_d done_%d (0x%x): ",
				       sof_count[_PASS1_D], d_cur_v_cnt);
		} else {
			sof_pass1done[1] = 0;
			if (p1_fbc[_dmaport].Bits.FB_NUM == (p1_fbc[_dmaport].Bits.FBC_CNT - 1))
				sof_pass1done[1] = 2;
		}
#ifdef _rtbc_buf_que_2_0_
		if (mFwRcnt.bLoadBaseAddr[_IRQ_D] == 1) {
			if (pstRTBuf->ring_buf[_imgo_d_].active) {
				IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF, "p1_d_%d:wr2Phy:	",
					       _imgo_d_);
				ISP_WR32(ISP_REG_ADDR_IMGO_D_BASE_ADDR,
					 pstRTBuf->ring_buf[_imgo_d_].data[pstRTBuf->
									   ring_buf[_imgo_d_].
									   start].base_pAddr);
			}
			if (pstRTBuf->ring_buf[_rrzo_d_].active) {
				IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF, "p1_d_%d:wr2Phy:	",
					       _rrzo_d_);
				ISP_WR32(ISP_REG_ADDR_RRZO_D_BASE_ADDR,
					 pstRTBuf->ring_buf[_rrzo_d_].data[pstRTBuf->
									   ring_buf[_rrzo_d_].
									   start].base_pAddr);
			}
			mFwRcnt.bLoadBaseAddr[_IRQ_D] = 0;
		}

		/* equal case is for clear curidx */
		for (z = 0; z <= mFwRcnt.curIdx[_IRQ_D]; z++) {
			if (mFwRcnt.INC[_IRQ_D][z] == 1) {
				mFwRcnt.INC[_IRQ_D][z] = 0;
				p1_fbc[2].Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_IMGO_D_FBC, p1_fbc[2].Reg_val);
				p1_fbc[3].Bits.RCNT_INC = 1;
				ISP_WR32(ISP_REG_ADDR_RRZO_D_FBC, p1_fbc[3].Reg_val);
				if (IspInfo.DebugMask & ISP_DBG_INT)
					IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF,
						       "p1_d:RCNT_INC: ");
			} else {
				mFwRcnt.curIdx[_IRQ_D] = 0;
				break;
			}
		}
#endif

		if (IspInfo.DebugMask & ISP_DBG_INT) {
			union CQ_RTBC_FBC _fbc_chk[2];        /* can chk fbc status compare to p1_fbc. */
			/* (the difference is the timing of reading) */

			_fbc_chk[0].Reg_val = ISP_RD32(ISP_REG_ADDR_IMGO_D_FBC);
			_fbc_chk[1].Reg_val = ISP_RD32(ISP_REG_ADDR_RRZO_D_FBC);

			IRQ_LOG_KEEPER(_IRQ_D, m_CurrentPPB, _LOG_INF,
				       "P1_D_SOF_%d_%d(0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x)\n",
				       sof_count[_PASS1_D], d_cur_v_cnt,
				       (unsigned int)(_fbc_chk[0].Reg_val),
				       (unsigned int)(_fbc_chk[1].Reg_val),
				       ISP_RD32(ISP_REG_ADDR_IMGO_D_BASE_ADDR),
				       ISP_RD32(ISP_REG_ADDR_RRZO_D_BASE_ADDR),
				       ISP_RD32(ISP_INNER_REG_ADDR_IMGO_D_YSIZE),
				       ISP_RD32(ISP_INNER_REG_ADDR_RRZO_D_YSIZE),
				       ISP_RD32(ISP_REG_ADDR_TG2_MAGIC_0));

			/* 1 port is enough     */
			if (pstRTBuf->ring_buf[_imgo_d_].active) {
				if ((_fbc_chk[0].Bits.WCNT - p1_fbc[2].Bits.WCNT) != 0)
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
						       "imgo_d:SW ISR right on next hw p1_done(0x%x_0x%x)\n",
						       _fbc_chk[0].Reg_val, p1_fbc[0].Reg_val);
			} else if (pstRTBuf->ring_buf[_rrzo_d_].active) {
				if ((_fbc_chk[1].Bits.WCNT - p1_fbc[3].Bits.WCNT) != 0)
					IRQ_LOG_KEEPER(_IRQ, m_CurrentPPB, _LOG_INF,
						       "rrzo_d:SW ISR right on next hw p1_done(0x%x_0x%x)\n",
						       _fbc_chk[1].Reg_val, p1_fbc[1].Reg_val);
			}
		}
		/*              unsigned long long sec;*/
		/*              unsigned long usec;*/
		/*              ktime_t time;*/
		/*      */
		time = ktime_get();     /* ns */
		sec = time.tv64;
		do_div(sec, 1000);      /*     usec */
		usec = do_div(sec, 1000000);    /* sec and usec */
		/* update SOF time stamp for eis user(need match with the time stamp in image header) */
		IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST_D][12] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST_D][12] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.LastestSigTime_usec[ISP_IRQ_TYPE_INT_P1_ST_D][0] =
			(unsigned int)(sec);
		IspInfo.IrqInfo.LastestSigTime_sec[ISP_IRQ_TYPE_INT_P1_ST_D][0] =
			(unsigned int)(usec);
		IspInfo.IrqInfo.Eismeta[ISP_IRQ_TYPE_INT_P1_ST_D][gEismetaWIdx_D].
		tLastSOF2P1done_sec = (unsigned int)(sec);
		IspInfo.IrqInfo.Eismeta[ISP_IRQ_TYPE_INT_P1_ST_D][gEismetaWIdx_D].
		tLastSOF2P1done_usec = (unsigned int)(usec);
		gEismetaInSOF_D = 1;
		gEismetaWIdx_D = ((gEismetaWIdx_D + 1) % EISMETA_RINGSIZE);
		/*      */
		if (sof_pass1done[1] == 1)
			ISP_SOF_Buf_Get(_IRQ_D, p1_fbc, (unsigned int *)curr_pa, sec, usec, MTRUE);
		else
			ISP_SOF_Buf_Get(_IRQ_D, p1_fbc, (unsigned int *)curr_pa, sec, usec, MFALSE);
	}
#endif
	/*      */
	/* make sure isr sequence r     all     done after this status switch */

	for (j = 0; j < ISP_IRQ_TYPE_AMOUNT; j++) {
		for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
			/* 1. update interrupt status to all users */
			IspInfo.IrqInfo.Status[i][j] |= (IrqStatus[j] & IspInfo.IrqInfo.Mask[j]);

			/* 2. update signal occurring time and passed by signal count */
			if (IspInfo.IrqInfo.MarkedFlag[i][j] & IspInfo.IrqInfo.Mask[j]) {
				for (k = 0; k < 32; k++) {
					if ((IrqStatus[j] & IspInfo.IrqInfo.Mask[j]) & (1 << k)) {
						idx = my_get_pow_idx(1 << k);
						IspInfo.IrqInfo.LastestSigTime_usec[j][idx] =
							(unsigned int)time_frmb.tv_usec;
						IspInfo.IrqInfo.LastestSigTime_sec[j][idx] =
							(unsigned int)time_frmb.tv_sec;
						IspInfo.IrqInfo.PassedBySigCnt[i][j][k]++;
					}
				}
			} else {  /* no any interrupt is not marked and  in read mask in this irq type */
			}

		}
	}
	spin_unlock(&(IspInfo.SpinLockIrq[_IRQ]));
#if     0                       /* in order to keep the isr stability. */
	if (bSlowMotion == MFALSE) {
		if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & ISP_IRQ_P1_STATUS_D_PASS1_DON_ST)
			LOG_INF("2D_%d\n",
				(sof_count[_PASS1]) ? (sof_count[_PASS1] -
						       1) : (sof_count[_PASS1]));

		if (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & ISP_IRQ_P1_STATUS_D_SOF1_INT_ST)
			LOG_INF("2S_%d\n",
				(sof_count[_PASS1]) ? (sof_count[_PASS1] -
						       1) : (sof_count[_PASS1]));

	}
#endif
	/* dump log     during spin     lock */
#ifdef ISR_LOG_ON
	/* IRQ_LOG_PRINTER(_IRQ,m_CurrentPPB,_LOG_INF); */
	/* IRQ_LOG_PRINTER_PR_ERR(_IRQ,m_CurrentPPB,_LOG_ERR); */

	/* IRQ_LOG_PRINTER(_IRQ_D,m_CurrentPPB,_LOG_INF); */
	/* IRQ_LOG_PRINTER_PR_ERR(_IRQ_D,m_CurrentPPB,_LOG_ERR); */
#endif
	/*      */
	wake_up_interruptible_all(&IspInfo.WaitQueueHead);

	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & (ISP_IRQ_P1_STATUS_PASS1_DON_ST)) ||
	    (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & (ISP_IRQ_P1_STATUS_SOF1_INT_ST)) ||
	    (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & (ISP_IRQ_P1_STATUS_D_PASS1_DON_ST)) ||
	    (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & (ISP_IRQ_P1_STATUS_D_SOF1_INT_ST))) {
		tasklet_schedule(&isp_tasklet);
	}
	/* Work queue. It is interruptible,     so there can be "Sleep" in work queue function. */
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & (ISP_IRQ_P1_STATUS_VS1_INT_ST)) &
	    (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & (ISP_IRQ_P1_STATUS_D_VS1_INT_ST))) {
		IspInfo.TimeLog.Vd = ISP_JiffiesToMs(jiffies);
		schedule_work(&IspInfo.ScheduleWorkVD);
		tasklet_schedule(&IspTaskletVD);
	}
	/* Tasklet.     It is uninterrupted, so there can NOT be "Sleep" in     tasklet function. */
	if ((IrqStatus[ISP_IRQ_TYPE_INT_P1_ST] & (ISP_IRQ_P1_STATUS_EXPDON1_ST)) &
	    (IrqStatus[ISP_IRQ_TYPE_INT_P1_ST_D] & (ISP_IRQ_P1_STATUS_D_EXPDON1_ST))) {
		IspInfo.TimeLog.Expdone = ISP_JiffiesToMs(jiffies);
		schedule_work(&IspInfo.ScheduleWorkEXPDONE);
		tasklet_schedule(&IspTaskletEXPDONE);
	}

	/* LOG_DBG("- X."); */
	/*      */
	return IRQ_HANDLED;
}


static void ISP_TaskletFunc(unsigned long data)
{
	if (bSlowMotion == MFALSE) {
		if (bRawEn == MTRUE) {
			/*LOG_INF("tks_%d",*/
			/*	(sof_count[_PASS1]) ? (sof_count[_PASS1] -*/
			/*			       1) : (sof_count[_PASS1]));*/
			IRQ_LOG_PRINTER(_IRQ, m_CurrentPPB, _LOG_INF);
			/*LOG_INF("tke_%d",-*/
			/*	(sof_count[_PASS1]) ? (sof_count[_PASS1] -*/
			/*			       1) : (sof_count[_PASS1]));*/
		}
		if (bRawDEn == MTRUE) {
			/*LOG_INF("dtks_%d",*/
			/*	(sof_count[_PASS1_D]) ? (sof_count[_PASS1_D] -*/
			/*				 1) : (sof_count[_PASS1_D]));*/
			IRQ_LOG_PRINTER(_IRQ_D, m_CurrentPPB, _LOG_INF);
			/*LOG_INF("dtke_%d",*/
			/*	(sof_count[_PASS1_D]) ? (sof_count[_PASS1_D] -*/
			/*				 1) : (sof_count[_PASS1_D]));*/
		}
		if (bCamSVEn == MTRUE)
			IRQ_LOG_PRINTER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF);
		if (bCamSVDEn == MTRUE)
			IRQ_LOG_PRINTER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF);
	} else {
		IRQ_LOG_PRINTER(_IRQ, m_CurrentPPB, _LOG_INF);
		IRQ_LOG_PRINTER(_IRQ_D, m_CurrentPPB, _LOG_INF);
		IRQ_LOG_PRINTER(_CAMSV_IRQ, m_CurrentPPB, _LOG_INF);
		IRQ_LOG_PRINTER(_CAMSV_D_IRQ, m_CurrentPPB, _LOG_INF);
	}
}

/*******************************************************************************
*
********************************************************************************/
static long ISP_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;
	/*      */
	bool HoldEnable = MFALSE;
	unsigned int DebugFlag[2] = { 0 };
	struct ISP_REG_IO_STRUCT RegIo;
	enum ISP_HOLD_TIME_ENUM HoldTime;
	struct ISP_WAIT_IRQ_STRUCT IrqInfo;
	struct ISP_READ_IRQ_STRUCT ReadIrq;
	struct ISP_CLEAR_IRQ_STRUCT ClearIrq;
	struct ISP_USER_INFO_STRUCT *pUserInfo;
	struct ISP_ED_BUFQUE_STRUCT edQueBuf;
	unsigned int regScenInfo_value = 0xa5a5a5a5;
	signed int burstQNum;
	unsigned int wakelock_ctrl;
	unsigned long flags; /* old: unsigned int flags;*//* FIX to avoid build warning */
	unsigned int magInfo[2] = {0};
	int userKey = -1;
	struct ISP_REGISTER_USERKEY_STRUCT RegUserKey;
	/*      */
	if (pFile->private_data == NULL) {
		LOG_PR_WARN("private_data is NULL,(process,	pid, tgid)=(%s,	%d,	%d)", current->comm,
			current->pid, current->tgid);
		return -EFAULT;
	}
	/*      */
	pUserInfo = (struct ISP_USER_INFO_STRUCT *)(pFile->private_data);
	/*      */
	switch (Cmd) {
	case ISP_WAKELOCK_CTRL: {
		if (copy_from_user(&wakelock_ctrl, (void *)Param, sizeof(unsigned int)) != 0) {
			LOG_PR_ERR("get ISP_WAKELOCK_CTRL from	user fail");
			Ret = -EFAULT;
		} else {
			if (wakelock_ctrl == 1) {       /* Enable     wakelock */
				if (g_bWaitLock == 0) {
#ifdef CONFIG_PM_SLEEP
					__pm_stay_awake(&isp_wake_lock);
#endif
					g_bWaitLock = 1;
					LOG_DBG("wakelock enable!!\n");
				}
			} else {        /* Disable wakelock */
				if (g_bWaitLock == 1) {
#ifdef CONFIG_PM_SLEEP
					__pm_relax(&isp_wake_lock);
#endif
					g_bWaitLock = 0;
					LOG_DBG("wakelock disable!!\n");
				}
			}
		}

	}
	break;
	case ISP_GET_DROP_FRAME:
		if (copy_from_user(&DebugFlag[0], (void *)Param, sizeof(unsigned int)) != 0) {
			LOG_PR_ERR("get irq from user fail");
			Ret = -EFAULT;
		} else {
			switch (DebugFlag[0]) {
			case _IRQ:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[_IRQ]), flags);
				DebugFlag[1] = sof_pass1done[0];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_IRQ]), flags);
				break;
			case _IRQ_D:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[_IRQ]), flags);
				DebugFlag[1] = sof_pass1done[1];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_IRQ]), flags);
				break;
			case _CAMSV_IRQ:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]), flags);
				DebugFlag[1] = gSof_camsvdone[0];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_CAMSV_IRQ]), flags);
				break;
			case _CAMSV_D_IRQ:
				spin_lock_irqsave(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]), flags);
				DebugFlag[1] = gSof_camsvdone[1];
				spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[_CAMSV_D_IRQ]), flags);
				break;
			default:
				LOG_PR_ERR("err TG(0x%x)\n", DebugFlag[0]);
				Ret = -EFAULT;
				goto EXIT;
			}
			if (copy_to_user((void *)Param, &DebugFlag[1], sizeof(unsigned int)) != 0) {
				LOG_PR_ERR("copy to user fail");
				Ret = -EFAULT;
			}
		}
		break;
	case ISP_GET_INT_ERR:
		if (copy_to_user((void *)Param, (void *)g_ISPIntErr, sizeof(unsigned int) * _IRQ_MAX) != 0)
			LOG_PR_ERR("get int err fail\n");

		break;
	case ISP_GET_DMA_ERR:
		if (copy_to_user((void *)Param, &g_DmaErr_p1[0], sizeof(unsigned int) * nDMA_ERR) != 0)
			LOG_PR_ERR("get dma_err fail\n");

		break;
	case ISP_GET_CUR_SOF:
		if (copy_from_user(&DebugFlag[0], (void *)Param, sizeof(unsigned int)) != 0) {
			LOG_PR_ERR("get cur sof from user fail");
			Ret = -EFAULT;
		} else {
			/* TG sof update at expo_done, causes potential timing issue, using sw sof */
			switch (DebugFlag[0]) {
			case _IRQ:
				/* DebugFlag[1] = ((ISP_RD32(ISP_REG_ADDR_TG_INTER_ST)&0x00FF0000)>>16); */
				DebugFlag[1] = sof_count[_PASS1];
				break;
			case _IRQ_D:
				/* DebugFlag[1] = ((ISP_RD32(ISP_REG_ADDR_TG2_INTER_ST)&0x00FF0000)>>16); */
				DebugFlag[1] = sof_count[_PASS1_D];
				break;
			case _CAMSV_IRQ:
				/* DebugFlag[1] = ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG_INTER_ST)&0x00FF0000)>>16); */
				DebugFlag[1] = sof_count[_CAMSV];
				break;
			case _CAMSV_D_IRQ:
				/* DebugFlag[1] = ((ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_INTER_ST)&0x00FF0000)>>16); */
				DebugFlag[1] = sof_count[_CAMSV_D];
				break;
			default:
				LOG_PR_ERR("err TG(0x%x)\n", DebugFlag[0]);
				Ret = -EFAULT;
				goto EXIT;
			}
		}
		if (copy_to_user((void *)Param, &DebugFlag[1], sizeof(unsigned int)) != 0) {
			LOG_PR_ERR("copy to user fail");
			Ret = -EFAULT;
		}
		break;
	case ISP_RESET_CAM_P1:
		spin_lock(&(IspInfo.SpinLockIsp));
		ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P1);
		spin_unlock(&(IspInfo.SpinLockIsp));
		break;
	case ISP_RESET_CAM_P2:
		spin_lock(&(IspInfo.SpinLockIsp));
		ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);
		spin_unlock(&(IspInfo.SpinLockIsp));
		break;
	case ISP_RESET_CAMSV:
		spin_lock(&(IspInfo.SpinLockIsp));
		ISP_Reset(ISP_REG_SW_CTL_RST_CAMSV);
		spin_unlock(&(IspInfo.SpinLockIsp));
		break;
	case ISP_RESET_CAMSV2:
		spin_lock(&(IspInfo.SpinLockIsp));
		ISP_Reset(ISP_REG_SW_CTL_RST_CAMSV2);
		spin_unlock(&(IspInfo.SpinLockIsp));
		break;
	case ISP_RESET_BUF:
		spin_lock_bh(&(IspInfo.SpinLockHold));
		ISP_ResetBuf();
		spin_unlock_bh(&(IspInfo.SpinLockHold));
		break;
	case ISP_READ_REGISTER:
		if (copy_from_user(&RegIo, (void *)Param, sizeof(struct ISP_REG_IO_STRUCT)) == 0) {
			Ret = ISP_ReadReg(&RegIo);
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_WRITE_REGISTER:
		if (copy_from_user(&RegIo, (void *)Param, sizeof(struct ISP_REG_IO_STRUCT)) == 0) {
			Ret = ISP_WriteReg(&RegIo);
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_HOLD_REG_TIME:
		if (copy_from_user(&HoldTime, (void *)Param, sizeof(enum ISP_HOLD_TIME_ENUM)) == 0) {
			spin_lock(&(IspInfo.SpinLockIsp));
			Ret = ISP_SetHoldTime(HoldTime);
			spin_unlock(&(IspInfo.SpinLockIsp));
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_HOLD_REG:
		if (copy_from_user(&HoldEnable, (void *)Param, sizeof(bool)) == 0) {
			Ret = ISP_EnableHoldReg(HoldEnable);
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_WAIT_IRQ:
		if (copy_from_user(&IrqInfo, (void *)Param, sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			/*      */
			if ((IrqInfo.Type >= ISP_IRQ_TYPE_AMOUNT) || (IrqInfo.Type < 0)) {
				Ret = -EFAULT;
				LOG_PR_ERR("invalid type(%d)", IrqInfo.Type);
				goto EXIT;
			}
			if ((IrqInfo.UserNumber >= ISP_IRQ_USER_MAX) || (IrqInfo.UserNumber < 0)) {
				LOG_PR_ERR("errUserEnum(%d)", IrqInfo.UserNumber);
				Ret = -EFAULT;
				goto EXIT;
			}

			/* check v1/v3 */
			if (IrqInfo.UserNumber > 0) {   /* v1 flow */
				Ret = ISP_WaitIrq(&IrqInfo);
			}
#if     1
			else {  /* isp driver related operation in v1 is redirected */
				/* to v3 flow cuz userNumer and default UserKey are 0 */
				/* v3 */
				if ((IrqInfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
				    || (IrqInfo.UserInfo.Type < 0)) {
					Ret = -EFAULT;
					LOG_PR_ERR("invalid type(%d)", IrqInfo.UserInfo.Type);
					goto EXIT;
				}
				if ((IrqInfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX)
				    || ((IrqInfo.UserInfo.UserKey <= 0)
					&& (IrqInfo.UserNumber == 0))) {
					/* LOG_INF("invalid userKey(%d), max(%d)", */
					/* WaitIrq_FrmB.UserInfo.UserKey,IRQ_USER_NUM_MAX); */
					userKey = 0;
					IrqInfo.UserInfo.UserKey = 0;
				}
				if ((IrqInfo.UserInfo.UserKey >= 0)
				    && (IrqInfo.UserInfo.UserKey < IRQ_USER_NUM_MAX)) {
					/* avoid other users in v3 do not set UserNumber and */
					/* UserNumber is set as 0 in isp driver */
					userKey = IrqInfo.UserInfo.UserKey;
				} else {
					Ret = -EFAULT;
					LOG_PR_ERR("invalid userkey error(%d)", IrqInfo.UserInfo.UserKey);
					goto EXIT;
				}
				IrqInfo.UserInfo.UserKey = userKey;
					Ret = ISP_WaitIrq_v3(&IrqInfo);
			}
#endif
			if (copy_to_user((void *)Param, &IrqInfo, sizeof(struct ISP_WAIT_IRQ_STRUCT)) != 0) {
				LOG_PR_ERR("copy_to_user failed\n");
				Ret = -EFAULT;
			}
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_READ_IRQ:
		if (copy_from_user(&ReadIrq, (void *)Param, sizeof(struct ISP_READ_IRQ_STRUCT)) == 0) {
			enum eISPIrq irqT = _IRQ;

			LOG_DBG("ISP_READ_IRQ Type(%d)", ReadIrq.Type);
			if ((ReadIrq.Type >= ISP_IRQ_TYPE_AMOUNT) || (ReadIrq.Type < 0)) {
				Ret = -EFAULT;
				LOG_PR_ERR("invalid type(%d)", ReadIrq.Type);
				goto EXIT;
			}
			/*      */
			if ((ReadIrq.UserNumber >= ISP_IRQ_USER_MAX) || (ReadIrq.UserNumber < 0)) {
				LOG_PR_ERR("errUserEnum(%d)", ReadIrq.UserNumber);
				Ret = -EFAULT;
				goto EXIT;
			}
			/*      */
			switch (ReadIrq.Type) {
			case ISP_IRQ_TYPE_INT_CAMSV:
				irqT = _CAMSV_IRQ;
				break;
			case ISP_IRQ_TYPE_INT_CAMSV2:
				irqT = _CAMSV_D_IRQ;
				break;
			default:
				irqT = _IRQ;
				break;
			}
			spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT]), flags);
			ReadIrq.Status = IspInfo.IrqInfo.Status[ReadIrq.UserNumber][ReadIrq.Type];
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT]), flags);
			/*      */
			if (copy_to_user((void *)Param, &ReadIrq, sizeof(struct ISP_READ_IRQ_STRUCT)) != 0) {
				LOG_PR_ERR("copy_to_user failed");
				Ret = -EFAULT;
			}
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_CLEAR_IRQ:
		if (copy_from_user(&ClearIrq, (void *)Param, sizeof(struct ISP_CLEAR_IRQ_STRUCT)) == 0) {
			enum eISPIrq irqT;

			LOG_DBG("ISP_CLEAR_IRQ Type(%d)", ClearIrq.Type);

			if ((ClearIrq.Type >= ISP_IRQ_TYPE_AMOUNT) || (ClearIrq.Type < 0)) {
				Ret = -EFAULT;
				LOG_PR_ERR("invalid type(%d)", ClearIrq.Type);
				goto EXIT;
			}
			/*      */
			if ((ClearIrq.UserNumber >= ISP_IRQ_USER_MAX) || (ClearIrq.UserNumber < 0)) {
				LOG_PR_ERR("errUserEnum(%d)", ClearIrq.UserNumber);
				Ret = -EFAULT;
				goto EXIT;
			}
			/*      */
			switch (ClearIrq.Type) {
			case ISP_IRQ_TYPE_INT_CAMSV:
				irqT = _CAMSV_IRQ;
				break;
			case ISP_IRQ_TYPE_INT_CAMSV2:
				irqT = _CAMSV_D_IRQ;
				break;
			default:
				irqT = _IRQ;
				break;
			}
			LOG_DBG("ISP_CLEAR_IRQ:Type(%d),Status(0x%08X),IrqStatus(0x%08X)",
				ClearIrq.Type, ClearIrq.Status,
				IspInfo.IrqInfo.Status[ClearIrq.UserNumber][ClearIrq.Type]);
			spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqT]), flags);
			IspInfo.IrqInfo.Status[ClearIrq.UserNumber][ClearIrq.Type] &=
				(~ClearIrq.Status);
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqT]), flags);
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	/*      */
	case ISP_REGISTER_IRQ_USER_KEY:
		if (copy_from_user(&RegUserKey, (void *)Param, sizeof(struct ISP_REGISTER_USERKEY_STRUCT))
		    == 0) {
			userKey = ISP_REGISTER_IRQ_USERKEY(RegUserKey.userName);
			RegUserKey.userKey = userKey;
			if (copy_to_user
			    ((void *)Param, &RegUserKey, sizeof(struct ISP_REGISTER_USERKEY_STRUCT)) != 0) {
				LOG_PR_ERR("copy_to_user failed\n");
			}

			if (RegUserKey.userKey < 0) {
				LOG_PR_ERR("query irq user	key	fail\n");
				Ret = -1;
			}
		}
		break;
	/*      */
	case ISP_MARK_IRQ_REQUEST:
		if (copy_from_user(&IrqInfo, (void *)Param, sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			if ((IrqInfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX)
			    || (IrqInfo.UserInfo.UserKey < 0)) {
				LOG_PR_ERR("invalid userKey(%d), max(%d)", IrqInfo.UserInfo.UserKey,
					IRQ_USER_NUM_MAX);
				Ret = -EFAULT;
				break;
			}
			if ((IrqInfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
			    || (IrqInfo.UserInfo.Type < 0)) {
				LOG_PR_ERR("invalid type(%d), max(%d)", IrqInfo.UserInfo.Type,
					ISP_IRQ_TYPE_AMOUNT);
				Ret = -EFAULT;
				break;
			}
			Ret = ISP_MARK_IRQ(IrqInfo);
		} else {
			LOG_PR_ERR("ISP_MARK_IRQ, copy_from_user failed");
			Ret = -EFAULT;
		}
		break;
	/*      */
	case ISP_GET_MARK2QUERY_TIME:
		if (copy_from_user(&IrqInfo, (void *)Param, sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			if ((IrqInfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX)
			    || (IrqInfo.UserInfo.UserKey < 0)) {
				LOG_PR_ERR("invalid userKey(%d), max(%d)", IrqInfo.UserInfo.UserKey,
					IRQ_USER_NUM_MAX);
				Ret = -EFAULT;
				break;
			}
			if ((IrqInfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
			    || (IrqInfo.UserInfo.Type < 0)) {
				LOG_PR_ERR("invalid type(%d), max(%d)", IrqInfo.UserInfo.Type,
					ISP_IRQ_TYPE_AMOUNT);
				Ret = -EFAULT;
				break;
			}
			Ret = ISP_GET_MARKtoQEURY_TIME(&IrqInfo);
			/*      */
			if (copy_to_user((void *)Param, &IrqInfo, sizeof(struct ISP_WAIT_IRQ_STRUCT)) != 0) {
				LOG_PR_ERR("copy_to_user failed");
				Ret = -EFAULT;
			}
		} else {
			LOG_PR_ERR("ISP_GET_MARK2QUERY_TIME, copy_from_user failed");
			Ret = -EFAULT;
		}
		break;
	/*      */
	case ISP_FLUSH_IRQ_REQUEST:
		if (copy_from_user(&IrqInfo, (void *)Param, sizeof(struct ISP_WAIT_IRQ_STRUCT)) == 0) {
			if (IrqInfo.UserNumber > 0) {   /*     v1 flow / v1 ISP_IRQ_USER_MAX */
				if ((IrqInfo.UserNumber >= ISP_IRQ_USER_MAX)
				    || (IrqInfo.UserNumber < 0)) {
					LOG_PR_ERR("invalid userNumber(%d), max(%d)",
						IrqInfo.UserNumber, ISP_IRQ_USER_MAX);
					Ret = -EFAULT;
					break;
				}
				if ((IrqInfo.Type >= ISP_IRQ_TYPE_AMOUNT) || (IrqInfo.Type < 0)) {
					LOG_PR_ERR("invalid type(%d), max(%d)\n", IrqInfo.Type,
						ISP_IRQ_TYPE_AMOUNT);
					Ret = -EFAULT;
					break;
				}
				/* check UserNumber     */
				if ((IrqInfo.UserNumber != ISP_IRQ_USER_3A) &&
				    (IrqInfo.UserNumber != ISP_IRQ_USER_EIS) &&
				    (IrqInfo.UserNumber != ISP_IRQ_USER_VHDR) &&
				    (IrqInfo.UserNumber != ISP_IRQ_USER_ISPDRV)) {
					LOG_PR_ERR("invalid userNumber(%d)\n", IrqInfo.UserNumber);
					Ret = -EFAULT;
					break;
				}
				if (0 == (IrqInfo.Type | IrqFlush[IrqInfo.UserNumber])) {
					LOG_PR_ERR("UserNumber(%d), invalid type(%d)\n",
						IrqInfo.UserNumber, IrqInfo.Type);
					Ret = -EFAULT;
					break;
				}
				LOG_INF("[FlushIrq]UserNumber(%d), type(%d), status(0x08%X)\n",
					IrqInfo.UserNumber, IrqInfo.Type, IrqInfo.Status);
				Ret = ISP_FLUSH_IRQ(IrqInfo);
			} else {	/* v3 flow /v3 IRQ_USER_NUM_MAX */
				if ((IrqInfo.UserInfo.UserKey >= IRQ_USER_NUM_MAX)
				    || (IrqInfo.UserInfo.UserKey < 0)) {
					LOG_PR_ERR("invalid userKey(%d), max(%d)\n",
						IrqInfo.UserInfo.UserKey, IRQ_USER_NUM_MAX);
					Ret = -EFAULT;
					break;
				}
				if ((IrqInfo.UserInfo.Type >= ISP_IRQ_TYPE_AMOUNT)
				    || (IrqInfo.UserInfo.Type < 0)) {
					LOG_PR_ERR("invalid type(%d), max(%d)\n",
						IrqInfo.UserInfo.Type, ISP_IRQ_TYPE_AMOUNT);
					Ret = -EFAULT;
					break;
				}
				if (0 ==
				    (IrqInfo.UserInfo.
				     Type | IrqFlush_v3[IrqInfo.UserInfo.UserKey])) {
					LOG_PR_ERR("User_%s(%d), invalid type(%d)",
						IrqUserKey_UserInfo[IrqInfo.UserInfo.UserKey].
						userName, IrqInfo.UserInfo.UserKey,
						IrqInfo.UserInfo.Type);
					Ret = -EFAULT;
					break;
				}
				LOG_INF("User_%s(%d), type(%d)",
					IrqUserKey_UserInfo[IrqInfo.UserInfo.UserKey].userName,
					IrqInfo.UserInfo.UserKey, IrqInfo.UserInfo.Type);
				Ret = ISP_FLUSH_IRQ_V3(IrqInfo);
			}
		}
		break;
	/*      */
	case ISP_ED_QUEBUF_CTRL:
		if (copy_from_user(&edQueBuf, (void *)Param, sizeof(struct ISP_ED_BUFQUE_STRUCT)) == 0) {
			edQueBuf.processID = pUserInfo->Pid;
			Ret = ISP_ED_BufQue_CTRL_FUNC(edQueBuf);
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	/*      */
	case ISP_UPDATE_REGSCEN:
		if (copy_from_user(&regScenInfo_value, (void *)Param, sizeof(unsigned int)) == 0) {
			spin_lock((spinlock_t *)(&SpinLockRegScen));
			g_regScen = regScenInfo_value;
			spin_unlock((spinlock_t *)(&SpinLockRegScen));
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_QUERY_REGSCEN:
		spin_lock((spinlock_t *)(&SpinLockRegScen));
		regScenInfo_value = g_regScen;
		spin_unlock((spinlock_t *)(&SpinLockRegScen));
		/*      */
		if (copy_to_user((void *)Param, &regScenInfo_value, sizeof(unsigned int)) != 0) {
			LOG_PR_ERR("copy_to_user failed");
			Ret = -EFAULT;
		}
		break;
	/*      */
	case ISP_UPDATE_BURSTQNUM:
		if (copy_from_user(&burstQNum, (void *)Param, sizeof(signed int)) == 0) {
			if ((burstQNum > (PAGE_SIZE/sizeof(signed int))) || (burstQNum < 0)) {
				LOG_PR_ERR("invalid burstQNum\n");
				Ret = -EFAULT;
			}
			spin_lock((spinlock_t *)(&SpinLockRegScen));
			P2_Support_BurstQNum = burstQNum;
			spin_unlock((spinlock_t *)(&SpinLockRegScen));
			LOG_DBG("new BurstQNum(%d)", P2_Support_BurstQNum);
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
	case ISP_QUERY_BURSTQNUM:
		spin_lock((spinlock_t *)(&SpinLockRegScen));
		burstQNum = P2_Support_BurstQNum;
		spin_unlock((spinlock_t *)(&SpinLockRegScen));
		/*      */
		if (copy_to_user((void *)Param, &burstQNum, sizeof(unsigned int)) != 0) {
			LOG_PR_ERR("copy_to_user failed");
			Ret = -EFAULT;
		}
		break;
	/*      */
	case ISP_DUMP_REG:
		Ret = ISP_DumpReg();
		break;
	case ISP_DEBUG_FLAG:
		if (copy_from_user(DebugFlag, (void *)Param, sizeof(unsigned int) * 2) == 0) {
			unsigned int lock_key = _IRQ_MAX;

			if (DebugFlag[1] >= _IRQ_MAX) {
				LOG_PR_ERR("unsupported module:0x%x\n", DebugFlag[1]);
				Ret = -EFAULT;
				break;
			}
			if (DebugFlag[1] == _IRQ_D)
				lock_key = _IRQ;
			else
				lock_key = DebugFlag[1];
			spin_lock_irqsave(&(IspInfo.SpinLockIrq[lock_key]), flags);
			IspInfo.DebugMask = DebugFlag[0];
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[lock_key]), flags);
			/* LOG_DBG("FBC kernel debug level = %x\n",IspInfo.DebugMask); */
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
#ifdef ISP_KERNEL_MOTIFY_SIGNAL_TEST
	case ISP_SET_USER_PID:
		/*if (copy_from_user(&pid, (void *)Param, sizeof(unsigned int)) == 0) {
		 *	spin_lock(&(IspInfo.SpinLockIsp));
		 *	getTaskInfo((pid_t) pid);
		 *
		 *	sendSignal();
		 *
		 *	LOG_DBG("[ISP_KERNEL_MOTIFY_SIGNAL_TEST]:0x08%x	", pid);
		 *	spin_unlock(&(IspInfo.SpinLockIsp));
		*} else {
		*	LOG_PR_ERR("copy_from_user	failed");
		*	Ret = -EFAULT;
		*}
		*/
		LOG_PR_ERR("Unsupport Cmd: ISP_SET_USER_PID");
		Ret = -EFAULT;
		break;
#endif
	case ISP_BUFFER_CTRL:
		Ret = ISP_Buf_CTRL_FUNC(Param);
		break;
	case ISP_REF_CNT_CTRL:
		Ret = ISP_REF_CNT_CTRL_FUNC(Param);
		break;
	case ISP_DUMP_ISR_LOG:
		if (copy_from_user(DebugFlag, (void *)Param, sizeof(unsigned int)) == 0) {
			unsigned int currentPPB = m_CurrentPPB;
			unsigned int lock_key = _IRQ_MAX;

			if (DebugFlag[0] >= _IRQ_MAX) {
				LOG_PR_ERR("unsupported module:0x%x\n", DebugFlag[0]);
				Ret = -EFAULT;
				break;
			}
			if (DebugFlag[0] == _IRQ_D)
				lock_key = _IRQ;
			else
				lock_key = DebugFlag[0];
			spin_lock_irqsave(&(IspInfo.SpinLockIrq[lock_key]), flags);
			m_CurrentPPB = (m_CurrentPPB + 1) % LOG_PPNUM;
			spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[lock_key]), flags);

			IRQ_LOG_PRINTER(DebugFlag[0], currentPPB, _LOG_INF);
			IRQ_LOG_PRINTER_PR_ERR(DebugFlag[0], currentPPB, _LOG_ERR);
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
#ifdef T_STAMP_2_0
	case ISP_SET_FPS:
		if (copy_from_user(DebugFlag, (void *)Param, sizeof(unsigned int)) == 0) {
			if (m_T_STAMP.fps == 0) {
				m_T_STAMP.fps = DebugFlag[0];
				m_T_STAMP.interval_us = 1000000 / m_T_STAMP.fps;
				m_T_STAMP.compensation_us =
					1000000 - (m_T_STAMP.interval_us * m_T_STAMP.fps);
				if (m_T_STAMP.fps > 90) {
					bSlowMotion = MTRUE;
					LOG_INF("slow motion enable:%d", m_T_STAMP.fps);
				}
			}
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;
#endif
	case ISP_UPDATE_MAGNUM:
		if (copy_from_user(magInfo, (void *)Param, sizeof(magInfo)) == 0) {
			union CQ_RTBC_FBC fbc;
			unsigned int camsv_reg_cal = 0;

			if ((magInfo[1] != _camsv_tg_) && (magInfo[1] != _camsv2_tg_)) {
				LOG_PR_ERR("invalid channel(0x%x)\n", magInfo[1]);
				Ret = -EFAULT;
				break;
			}
			spin_lock((spinlock_t *)(&SpinLockMagNum));
			if (magInfo[1] == _camsv_tg_) {
				fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV_IMGO_FBC);
				camsv_reg_cal = ISP_RD32(ISP_REG_ADDR_CAMSV_TG_VF_CON);
				if ((g_firstCnt[_camsv_1] >= fbc.Bits.FB_NUM) && (camsv_reg_cal & 0x1)) {
					if (g_magQIdx[_camsv_1][0] >= ISP_RT_BUF_SIZE)
						g_magQIdx[_camsv_1][0] = 0;
					g_magQue[_camsv_1][g_magQIdx[_camsv_1][0]] = magInfo[0];
					/*LOG_INF("ISP_UPDATE_MAGNUM: idx1=%d mag=0x%x",
					 *   g_magQIdx[_camsv_1][0], magInfo[0]);
					*/
					g_magQIdx[_camsv_1][0]++;
				} else if (!(camsv_reg_cal & 0x1)) {
					/* Need to know is HAL3 before ISP start,
					 * only hal3 will enter this case.
					*/
					enqueImd[_camsv_1] = MTRUE;
				}
			} else if (magInfo[1] == _camsv2_tg_) {
				fbc.Reg_val = ISP_RD32(ISP_REG_ADDR_CAMSV2_IMGO_FBC);
				camsv_reg_cal = ISP_RD32(ISP_REG_ADDR_CAMSV_TG2_VF_CON);
				if ((g_firstCnt[_camsv_2] >= fbc.Bits.FB_NUM) && (camsv_reg_cal & 0x1)) {
					if (g_magQIdx[_camsv_2][0] >= ISP_RT_BUF_SIZE)
						g_magQIdx[_camsv_2][0] = 0;
					g_magQue[_camsv_2][g_magQIdx[_camsv_2][0]] = magInfo[0];
					/*LOG_INF("ISP_UPDATE_MAGNUM: idx2=%d mag=0x%x",
					 *g_magQIdx[_camsv_2][0], magInfo[0]);
					 */
					g_magQIdx[_camsv_2][0]++;
				} else if (!(camsv_reg_cal & 0x1)) {
					/* Need to know is HAL3 before ISP start,
					 *only hal3 will enter this case.
					 */
					enqueImd[_camsv_2] = MTRUE;
				}
			}
			spin_unlock((spinlock_t *)(&SpinLockMagNum));
		} else {
			LOG_PR_ERR("copy_from_user	failed");
			Ret = -EFAULT;
		}
		break;

	default:
		LOG_PR_ERR("Unknown Cmd(%d)", Cmd);
		Ret = -EPERM;
		break;
	}
	/*      */
EXIT:
	if (Ret != 0)
		LOG_PR_ERR("Fail, Cmd(%d),	Pid(%d), (process, pid,	tgid)=(%s, %d, %d)", Cmd,
			pUserInfo->Pid, current->comm, current->pid, current->tgid);

	/*      */
	return Ret;
}

#ifdef CONFIG_COMPAT

/*******************************************************************************
*
********************************************************************************/
static int compat_get_isp_read_register_data(struct compat_ISP_REG_IO_STRUCT __user *data32,
					     struct ISP_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err;

	err = get_user(uptr, &data32->pData);
	err |= put_user(compat_ptr(uptr), &data->pData);
	err |= get_user(count, &data32->Count);
	err |= put_user(count, &data->Count);
	return err;
}

static int compat_put_isp_read_register_data(struct compat_ISP_REG_IO_STRUCT __user *data32,
					     struct ISP_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	/*      compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr),     &data->pData); */
	/* err |= put_user(uptr, &data32->pData); */
	err |= get_user(count, &data->Count);
	err |= put_user(count, &data32->Count);
	return err;
}

static int compat_get_isp_waitirq_data(struct compat_ISP_WAIT_IRQ_STRUCT __user *data32,
				       struct ISP_WAIT_IRQ_STRUCT __user *data)
{
	compat_uint_t tmp, clear, type;
	compat_uptr_t uptr;
	struct ISP_IRQ_USER_STRUCT isp_irq_user_tmp;
	struct ISP_IRQ_TIME_STRUCT isp_irq_time_tmp;
	struct ISP_EIS_META_STRUCT isp_eis_meta_tmp;
	int err;

	err = get_user(clear, &data32->Clear);
	err |= put_user(clear, &data->Clear);
	err |= get_user(type, &data32->Type);
	err |= put_user(type, &data->Type);
	err |= get_user(tmp, &data32->Status);
	err |= put_user(tmp, &data->Status);
	err |= get_user(tmp, &data32->UserNumber);
	err |= put_user(tmp, &data->UserNumber);
	err |= get_user(tmp, &data32->Timeout);
	err |= put_user(tmp, &data->Timeout);
	err |= get_user(uptr, &data32->UserName);
	err |= put_user(compat_ptr(uptr), &data->UserName);
	err |= get_user(tmp, &data32->irq_TStamp);
	err |= put_user(tmp, &data->irq_TStamp);
	err |= get_user(tmp, &data32->bDumpReg);
	err |= put_user(tmp, &data->bDumpReg);
	/* structure copy */
	err |= copy_from_user(&isp_irq_user_tmp, &data32->UserInfo, sizeof(struct ISP_IRQ_USER_STRUCT));
	err |=
		copy_to_user((void *)&data->UserInfo, &isp_irq_user_tmp, sizeof(struct ISP_IRQ_USER_STRUCT));
	err |= copy_from_user(&isp_irq_time_tmp, &data32->TimeInfo, sizeof(struct ISP_IRQ_TIME_STRUCT));
	err |=
		copy_to_user((void *)&data->TimeInfo, &isp_irq_time_tmp, sizeof(struct ISP_IRQ_TIME_STRUCT));
	err |= copy_from_user(&isp_eis_meta_tmp, &data32->EisMeta, sizeof(struct ISP_EIS_META_STRUCT));
	err |= copy_to_user((void *)&data->EisMeta, &isp_eis_meta_tmp, sizeof(struct ISP_EIS_META_STRUCT));
	err |= get_user(tmp, &data32->SpecUser);
	err |= put_user(tmp, &data->SpecUser);
	return err;
}

static int compat_put_isp_waitirq_data(struct compat_ISP_WAIT_IRQ_STRUCT __user *data32,
				       struct ISP_WAIT_IRQ_STRUCT __user *data)
{
	compat_uint_t tmp, clear, type;
	/*      compat_uptr_t uptr;*/
	struct ISP_IRQ_USER_STRUCT isp_irq_user_tmp;
	struct ISP_IRQ_TIME_STRUCT isp_irq_time_tmp;
	struct ISP_EIS_META_STRUCT isp_eis_meta_tmp;
	int err;

	err = get_user(clear, &data->Clear);
	err |= put_user(clear, &data32->Clear);
	err |= get_user(type, &data->Type);
	err |= put_user(type, &data32->Type);
	err |= get_user(tmp, &data->Status);
	err |= put_user(tmp, &data32->Status);
	err |= get_user(tmp, &data->UserNumber);
	err |= put_user(tmp, &data32->UserNumber);
	err |= get_user(tmp, &data->Timeout);
	err |= put_user(tmp, &data32->Timeout);
	/* Assume data pointer is unchanged. */
	/* err |= get_user(uptr, &data->UserName); */
	/* err |= put_user(uptr, &data32->UserName); */
	err |= get_user(tmp, &data->irq_TStamp);
	err |= put_user(tmp, &data32->irq_TStamp);
	err |= get_user(tmp, &data->bDumpReg);
	err |= put_user(tmp, &data32->bDumpReg);

	/* structure copy */
	err |= copy_from_user(&isp_irq_user_tmp, &data->UserInfo, sizeof(struct ISP_IRQ_USER_STRUCT));
	err |=
		copy_to_user((void *)&data32->UserInfo, &isp_irq_user_tmp, sizeof(struct ISP_IRQ_USER_STRUCT));
	err |= copy_from_user(&isp_irq_time_tmp, &data->TimeInfo, sizeof(struct ISP_IRQ_TIME_STRUCT));
	err |=
		copy_to_user((void *)&data32->TimeInfo, &isp_irq_time_tmp, sizeof(struct ISP_IRQ_TIME_STRUCT));
	err |= copy_from_user(&isp_eis_meta_tmp, &data->EisMeta, sizeof(struct ISP_EIS_META_STRUCT));
	err |=
		copy_to_user((void *)&data32->EisMeta, &isp_eis_meta_tmp, sizeof(struct ISP_EIS_META_STRUCT));
	err |= get_user(tmp, &data->SpecUser);
	err |= put_user(tmp, &data32->SpecUser);
	return err;
}

static int compat_get_isp_readirq_data(struct compat_ISP_READ_IRQ_STRUCT __user *data32,
				       struct ISP_READ_IRQ_STRUCT __user *data)
{
	compat_uint_t type, usernum, status;
	int err;

	err = get_user(type, &data32->Type);
	err |= put_user(type, &data->Type);
	err |= get_user(usernum, &data32->UserNumber);
	err |= put_user(usernum, &data->UserNumber);
	err |= get_user(status, &data32->Status);
	err |= put_user(status, &data->Status);
	return err;
}


static int compat_put_isp_readirq_data(struct compat_ISP_READ_IRQ_STRUCT __user *data32,
				       struct ISP_READ_IRQ_STRUCT __user *data)
{
	compat_uint_t tmp, type, usernum;
	int err;

	err = get_user(type, &data->Type);
	err |= put_user(type, &data32->Type);
	err |= get_user(usernum, &data->UserNumber);
	err |= put_user(usernum, &data32->UserNumber);
	err |= get_user(tmp, &data->Status);
	err |= put_user(tmp, &data32->Status);
	return err;
}

static int compat_get_isp_buf_ctrl_struct_data(struct compat_ISP_BUFFER_CTRL_STRUCT __user *data32,
					       struct ISP_BUFFER_CTRL_STRUCT __user *data)
{
	compat_uint_t ctrl, id;
	compat_uptr_t uptr;
	int err;

	err = get_user(ctrl, &data32->ctrl);
	err |= put_user(ctrl, &data->ctrl);
	err |= get_user(id, &data32->buf_id);
	err |= put_user(id, &data->buf_id);
	err |= get_user(uptr, &data32->data_ptr);
	err |= put_user(compat_ptr(uptr), &data->data_ptr);
	err |= get_user(uptr, &data32->ex_data_ptr);
	err |= put_user(compat_ptr(uptr), &data->ex_data_ptr);
	err |= get_user(uptr, &data32->pExtend);
	err |= put_user(compat_ptr(uptr), &data->pExtend);

	return err;
}

static int compat_put_isp_buf_ctrl_struct_data(struct compat_ISP_BUFFER_CTRL_STRUCT __user *data32,
					       struct ISP_BUFFER_CTRL_STRUCT __user *data)
{
	compat_uint_t ctrl, id;
	/*      compat_uptr_t uptr;*/
	int err;

	err = get_user(ctrl, &data->ctrl);
	err |= put_user(ctrl, &data32->ctrl);
	err |= get_user(id, &data->buf_id);
	err |= put_user(id, &data32->buf_id);
	/* Assume data pointer is unchanged. */
	/* err |= get_user(compat_ptr(uptr), &data->data_ptr); */
	/* err |= put_user(uptr, &data32->data_ptr); */
	/* err |= get_user(compat_ptr(uptr), &data->ex_data_ptr); */
	/* err |= put_user(uptr, &data32->ex_data_ptr); */
	/* err |= get_user(compat_ptr(uptr), &data->pExtend); */
	/* err |= put_user(uptr, &data32->pExtend); */

	return err;
}

static int compat_get_isp_ref_cnt_ctrl_struct_data(struct compat_ISP_REF_CNT_CTRL_STRUCT __user *data32,
						   struct ISP_REF_CNT_CTRL_STRUCT __user *data)
{
	compat_uint_t ctrl, id;
	compat_uptr_t uptr;
	int err;

	err = get_user(ctrl, &data32->ctrl);
	err |= put_user(ctrl, &data->ctrl);
	err |= get_user(id, &data32->id);
	err |= put_user(id, &data->id);
	err |= get_user(uptr, &data32->data_ptr);
	err |= put_user(compat_ptr(uptr), &data->data_ptr);

	return err;
}

static int compat_put_isp_ref_cnt_ctrl_struct_data(struct compat_ISP_REF_CNT_CTRL_STRUCT __user *data32,
						   struct ISP_REF_CNT_CTRL_STRUCT __user *data)
{
	compat_uint_t ctrl, id;
	/*      compat_uptr_t uptr;*/
	int err;

	err = get_user(ctrl, &data->ctrl);
	err |= put_user(ctrl, &data32->ctrl);
	err |= get_user(id, &data->id);
	err |= put_user(id, &data32->id);
	/* Assume data pointer is unchanged. */
	/* err |= get_user(compat_ptr(uptr), &data->data_ptr); */
	/* err |= put_user(uptr, &data32->data_ptr); */

	return err;
}


static int compat_get_isp_register_userkey_struct_data(struct compat_ISP_REGISTER_USERKEY_STRUCT __user *
						       data32,
						       struct ISP_REGISTER_USERKEY_STRUCT __user *data)
{
	compat_uint_t tmp;
	compat_uptr_t uptr;
	int err;

	err = get_user(tmp, &data32->userKey);
	err |= put_user(tmp, &data->userKey);
	err |= get_user(uptr, &data32->userName);
	err |= put_user(compat_ptr(uptr), &data->userName);

	return err;
}

static int compat_put_isp_register_userkey_struct_data(struct compat_ISP_REGISTER_USERKEY_STRUCT __user *
						       data32,
						       struct ISP_REGISTER_USERKEY_STRUCT __user *data)
{
	compat_uint_t tmp;
	/*      compat_uptr_t uptr;*/
	int err;

	err = get_user(tmp, &data->userKey);
	err |= put_user(tmp, &data32->userKey);
	/* Assume data pointer is unchanged. */
	/* err |= get_user(uptr, &data->userName); */
	/* err |= put_user(uptr, &data32->userName); */

	return err;
}

static long ISP_ioctl_compat(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ISP_READ_REGISTER: {
		struct compat_ISP_REG_IO_STRUCT __user *data32;
		struct ISP_REG_IO_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_read_register_data(data32, data);
		if (err) {
			LOG_INF("compat_get_isp_read_register_data error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_READ_REGISTER,
						   (unsigned long)data);
		err = compat_put_isp_read_register_data(data32, data);
		if (err) {
			LOG_INF("compat_put_isp_read_register_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_WRITE_REGISTER: {
		struct compat_ISP_REG_IO_STRUCT __user *data32;
		struct ISP_REG_IO_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_read_register_data(data32, data);
		if (err) {
			LOG_INF("COMPAT_ISP_WRITE_REGISTER error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_WRITE_REGISTER,
						   (unsigned long)data);
		return ret;
	}
	case COMPAT_ISP_WAIT_IRQ: {
		struct compat_ISP_WAIT_IRQ_STRUCT __user *data32;
		struct ISP_WAIT_IRQ_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_get_isp_waitirq_data error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_WAIT_IRQ, (unsigned long)data);
		err = compat_put_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_put_isp_waitirq_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_MARK_IRQ_REQUEST: {
		struct compat_ISP_WAIT_IRQ_STRUCT __user *data32;
		struct ISP_WAIT_IRQ_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_get_ISP_MARK_IRQ_REQUEST error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_MARK_IRQ_REQUEST,
						   (unsigned long)data);
		err = compat_put_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_put_ISP_MARK_IRQ_REQUEST error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_GET_MARK2QUERY_TIME: {
		struct compat_ISP_WAIT_IRQ_STRUCT __user *data32;
		struct ISP_WAIT_IRQ_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_get_ISP_GET_MARK2QUERY_TIME	error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_MARK2QUERY_TIME,
						   (unsigned long)data);
		err = compat_put_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_put_ISP_GET_MARK2QUERY_TIME	error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_FLUSH_IRQ_REQUEST: {
		struct compat_ISP_WAIT_IRQ_STRUCT __user *data32;
		struct ISP_WAIT_IRQ_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_get_ISP_FLUSH_IRQ_REQUEST error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_FLUSH_IRQ_REQUEST,
						   (unsigned long)data);
		err = compat_put_isp_waitirq_data(data32, data);
		if (err) {
			LOG_INF("compat_put_ISP_FLUSH_IRQ_REQUEST error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_READ_IRQ: {
		struct compat_ISP_READ_IRQ_STRUCT __user *data32;
		struct ISP_READ_IRQ_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_readirq_data(data32, data);
		if (err) {
			LOG_INF("compat_get_isp_readirq_data error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, ISP_READ_IRQ, (unsigned long)data);
		err = compat_put_isp_readirq_data(data32, data);
		if (err) {
			LOG_INF("compat_put_isp_readirq_data error!!!\n");
			return err;
		}

		return ret;
	}
	case COMPAT_ISP_BUFFER_CTRL: {
		struct compat_ISP_BUFFER_CTRL_STRUCT __user *data32;
		struct ISP_BUFFER_CTRL_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_buf_ctrl_struct_data(data32, data);
		if (err) {
			LOG_INF("compat_get_isp_buf_ctrl_struct_data error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_BUFFER_CTRL, (unsigned long)data);
		err = compat_put_isp_buf_ctrl_struct_data(data32, data);

		if (err) {
			LOG_INF("compat_put_isp_buf_ctrl_struct_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_REF_CNT_CTRL: {
		struct compat_ISP_REF_CNT_CTRL_STRUCT __user *data32;
		struct ISP_REF_CNT_CTRL_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_ref_cnt_ctrl_struct_data(data32, data);
		if (err) {
			LOG_INF("compat_get_isp_ref_cnt_ctrl_struct_data error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_REF_CNT_CTRL, (unsigned long)data);
		err = compat_put_isp_ref_cnt_ctrl_struct_data(data32, data);
		if (err) {
			LOG_INF("compat_put_isp_ref_cnt_ctrl_struct_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_REGISTER_IRQ_USER_KEY: {
		struct compat_ISP_REGISTER_USERKEY_STRUCT __user *data32;
		struct ISP_REGISTER_USERKEY_STRUCT __user *data;
		int err;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_isp_register_userkey_struct_data(data32, data);
		if (err) {
			LOG_INF("compat_get_isp_register_userkey_struct_data error!!!\n");
			return err;
		}
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_REGISTER_IRQ_USER_KEY,
						   (unsigned long)data);
		err = compat_put_isp_register_userkey_struct_data(data32, data);
		if (err) {
			LOG_INF("compat_put_isp_register_userkey_struct_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_ISP_DEBUG_FLAG: {
		/* compat_ptr(arg) will convert the     arg     */
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_DEBUG_FLAG,
						   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_INT_ERR: {
		/* compat_ptr(arg) will convert the     arg     */
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_INT_ERR,
						   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_GET_DMA_ERR: {
		/* compat_ptr(arg) will convert the     arg     */
		ret =
			filp->f_op->unlocked_ioctl(filp, ISP_GET_DMA_ERR,
						   (unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_ISP_UPDATE_MAGNUM: {
	/* compat_ptr(arg) will convert the     arg     */
	ret =
		filp->f_op->unlocked_ioctl(filp, ISP_UPDATE_MAGNUM,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	case ISP_GET_CUR_SOF:
	case ISP_GET_DROP_FRAME:
	case ISP_RESET_CAM_P1:
	case ISP_RESET_CAM_P2:
	case ISP_RESET_CAMSV:
	case ISP_RESET_CAMSV2:
	case ISP_RESET_BUF:
	case ISP_HOLD_REG_TIME:
	/* enum must check.     */
	case ISP_HOLD_REG:
	/* mbool value must     check. */
	case ISP_CLEAR_IRQ:
	/* structure (no pointer) */
	case ISP_REGISTER_IRQ:
	case ISP_UNREGISTER_IRQ:
	case ISP_ED_QUEBUF_CTRL:
	/* structure (no pointer) */
	case ISP_UPDATE_REGSCEN:
	case ISP_QUERY_REGSCEN:
	case ISP_UPDATE_BURSTQNUM:
	case ISP_QUERY_BURSTQNUM:
	case ISP_DUMP_REG:
	case ISP_SET_USER_PID:
	/* structure    use     unsigned long , but     the     code is unsigned int */
	case ISP_SET_FPS:
	case ISP_DUMP_ISR_LOG:
	case ISP_WAKELOCK_CTRL:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return ISP_ioctl(filep, cmd, arg); */
	}
}

#endif

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i;
	int q = 0, p = 0;
	struct ISP_USER_INFO_STRUCT *pUserInfo;

	LOG_INF("- E. UserCount: %d.", IspInfo.UserCount);
	/*      */
	spin_lock(&(IspInfo.SpinLockIspRef));
	/*      */
	/* LOG_DBG("UserCount(%d)",IspInfo.UserCount); */
	/*      */
	pFile->private_data = NULL;
	pFile->private_data = kmalloc(sizeof(struct ISP_USER_INFO_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_DBG("ERROR:	kmalloc	failed,	(process, pid, tgid)=(%s, %d, %d)", current->comm,
			current->pid, current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct ISP_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*      */
	if (IspInfo.UserCount > 0) {
		IspInfo.UserCount++;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d),	users exist",
			IspInfo.UserCount, current->comm, current->pid, current->tgid);
		goto EXIT;
	} else {
		IspInfo.UserCount++;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d),	first user",
			IspInfo.UserCount, current->comm, current->pid, current->tgid);
	}

	/* kernel log */
#if (LOG_CONSTRAINT_ADJ == 1)
	g_log_def_constraint = get_detect_count();
	set_detect_count(g_log_def_constraint + 100);
#endif

	/* do wait queue head init when re-enter in camera */
	EDBufQueRemainNodeCnt = 0;
	spin_lock((spinlock_t *)(&SpinLockRegScen));
	P2_Support_BurstQNum = 2;
	spin_unlock((spinlock_t *)(&SpinLockRegScen));

	/*      */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		memset((void *)IrqUserKey_UserInfo[i].userName, '\0', USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;

	/* flushIRQ     v3 */
	IrqFlush_v3[i] =
		(ISP_IRQ_P1_STATUS_VS1_INT_ST | ISP_IRQ_P1_STATUS_D_VS1_INT_ST |
		 ISP_IRQ_P1_STATUS_AF_DON_ST | ISP_IRQ_P1_STATUS_D_AF_DON_ST |
		 ISP_IRQ_P1_STATUS_PASS1_DON_ST | ISP_IRQ_P1_STATUS_D_PASS1_DON_ST);
	}

	/* flushIRQ     v1 */
	for (i = 0; i < ISP_IRQ_USER_MAX; i++)
		IrqFlush[i] = 0x0;

	IrqFlush[ISP_IRQ_USER_3A] =
		(ISP_IRQ_P1_STATUS_VS1_INT_ST | ISP_IRQ_P1_STATUS_D_VS1_INT_ST |
		 ISP_IRQ_P1_STATUS_AF_DON_ST | ISP_IRQ_P1_STATUS_D_AF_DON_ST);
	IrqFlush[ISP_IRQ_USER_EIS] =
		(ISP_IRQ_P1_STATUS_VS1_INT_ST | ISP_IRQ_P1_STATUS_D_VS1_INT_ST |
		 ISP_IRQ_P1_STATUS_PASS1_DON_ST | ISP_IRQ_P1_STATUS_D_PASS1_DON_ST);
	IrqFlush[ISP_IRQ_USER_VHDR] =
		(ISP_IRQ_P1_STATUS_VS1_INT_ST | ISP_IRQ_P1_STATUS_D_VS1_INT_ST |
		 ISP_IRQ_P1_STATUS_PASS1_DON_ST | ISP_IRQ_P1_STATUS_D_PASS1_DON_ST);
	/*      */
	spin_lock(&(SpinLockEDBufQueList));
	for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
		P2_EDBUF_RingList[i].processID = 0x0;
		P2_EDBUF_RingList[i].callerID = 0x0;
		P2_EDBUF_RingList[i].p2dupCQIdx = -1;
		P2_EDBUF_RingList[i].bufSts = ISP_ED_BUF_STATE_NONE;
		P2_EDBUF_RingList[i].p2Scenario = -1;
	}
	P2_EDBUF_RList_FirstBufIdx = 0;
	P2_EDBUF_RList_CurBufIdx = 0;
	P2_EDBUF_RList_LastBufIdx = -1;
	/*      */
	for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
		P2_EDBUF_MgrList[i].processID = 0x0;
		P2_EDBUF_MgrList[i].callerID = 0x0;
		P2_EDBUF_MgrList[i].p2dupCQIdx = -1;
		P2_EDBUF_MgrList[i].dequedNum = 0;
		P2_EDBUF_MgrList[i].p2Scenario = -1;
	}
	P2_EDBUF_MList_FirstBufIdx = 0;
	P2_EDBUF_MList_LastBufIdx = -1;
	spin_unlock(&(SpinLockEDBufQueList));
	/*      */
	spin_lock((spinlock_t *)(&SpinLockRegScen));
	g_regScen = 0xa5a5a5a5;
	spin_unlock((spinlock_t *)(&SpinLockRegScen));
	/*      */
	spin_lock(&(IspInfo.SpinLockIspRef));
	IspInfo.BufInfo.Read.pData = kmalloc(ISP_BUF_SIZE, GFP_ATOMIC);
	IspInfo.BufInfo.Read.Size = ISP_BUF_SIZE;
	IspInfo.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY;
	if (IspInfo.BufInfo.Read.pData == NULL) {
		LOG_DBG("ERROR:	BufRead	kmalloc	failed");
		Ret = -ENOMEM;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		goto EXIT;
	}
	spin_unlock(&(IspInfo.SpinLockIspRef));
	/*      */
	if (!ISP_BufWrite_Alloc()) {
		LOG_DBG("ERROR:	BufWrite kmalloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/*      */
	atomic_set(&(IspInfo.HoldInfo.HoldEnable), 0);
	atomic_set(&(IspInfo.HoldInfo.WriteEnable), 0);

	for (i = 0; i < ISP_REF_CNT_ID_MAX; i++)
		atomic_set(&g_imem_ref_cnt[i], 0);
	/*      */
	for (q = 0; q < IRQ_USER_NUM_MAX; q++) {
		for (i = 0; i < ISP_IRQ_TYPE_AMOUNT; i++) {
			IspInfo.IrqInfo.Status[q][i] = 0;
			IspInfo.IrqInfo.MarkedFlag[q][i] = 0;
			for (p = 0; p < 32; p++) {
				IspInfo.IrqInfo.MarkedTime_sec[q][i][p] = 0;
				IspInfo.IrqInfo.MarkedTime_usec[q][i][p] = 0;
				IspInfo.IrqInfo.PassedBySigCnt[q][i][p] = 0;
				IspInfo.IrqInfo.LastestSigTime_sec[i][p] = 0;
				IspInfo.IrqInfo.LastestSigTime_usec[i][p] = 0;
			}
			if (i < ISP_IRQ_TYPE_INT_STATUSX) {
				for (p = 0; p < EISMETA_RINGSIZE; p++) {
					IspInfo.IrqInfo.Eismeta[i][p].tLastSOF2P1done_sec = 0;
					IspInfo.IrqInfo.Eismeta[i][p].tLastSOF2P1done_usec = 0;
				}
			}
		}
	}
	gEismetaRIdx = 0;
	gEismetaWIdx = 0;
	gEismetaInSOF = 0;
	gEismetaRIdx_D = 0;
	gEismetaWIdx_D = 0;
	gEismetaInSOF_D = 0;
	/* */
	for (i = 0; i < ISP_CALLBACK_AMOUNT; i++)
		IspInfo.Callback[i].Func = NULL;

	/*      */
#ifdef KERNEL_LOG
	IspInfo.DebugMask = (ISP_DBG_INT | ISP_DBG_BUF_CTRL);
#endif
	/*      */
EXIT:
	if (Ret < 0) {
		spin_lock(&(IspInfo.SpinLockIspRef));
		if (IspInfo.BufInfo.Read.pData != NULL) {
			kfree(IspInfo.BufInfo.Read.pData);
			IspInfo.BufInfo.Read.pData = NULL;
		}
		spin_unlock(&(IspInfo.SpinLockIspRef));
		ISP_BufWrite_Free();
	} else {
		/* Enable clock.
		 *  1. clkmgr: G_u4EnableClockCount=0, call clk_enable/disable
		 *  2. CCF: call clk_enable/disable every time
		 */
		ISP_EnableClock(MTRUE);
		LOG_DBG("isp open G_u4EnableClockCount:	%d", G_u4EnableClockCount);
	}

	/* LOG_DBG("Before spm_disable_sodi()."); */
	/* Disable sodi (Multi-Core     Deep Idle).     */


#if     0                       /* _mt6593fpga_dvt_use_ */
	spm_disable_sodi();
#endif

	LOG_INF("- X. Ret: %d. UserCount: %d.", Ret, IspInfo.UserCount);
	return Ret;

}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_release(struct inode *pInode, struct file *pFile)
{
	struct ISP_USER_INFO_STRUCT *pUserInfo;
	unsigned int Reg;
	unsigned int i = 0;

	LOG_INF("- E. UserCount: %d.", IspInfo.UserCount);
	/*      */

	/*      */
	/* LOG_DBG("UserCount(%d)",IspInfo.UserCount); */
	/*      */
	if (pFile->private_data != NULL) {
		pUserInfo = (struct ISP_USER_INFO_STRUCT *) pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*      */
	spin_lock(&(IspInfo.SpinLockIspRef));
	IspInfo.UserCount--;
	if (IspInfo.UserCount > 0) {
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d),	users exist",
			IspInfo.UserCount, current->comm, current->pid, current->tgid);
		goto EXIT;
	} else {
		spin_unlock(&(IspInfo.SpinLockIspRef));
	}
	/*      */
	LOG_DBG("Curr UserCount(%d), (process, pid, tgid)=(%s, %d, %d),	last user",
		IspInfo.UserCount, current->comm, current->pid, current->tgid);

	/* reason of close vf is to     make sure camera can serve regular after previous abnormal exit */
	Reg = ISP_RD32(ISP_REG_ADDR_TG_VF_CON);
	Reg &= 0xfffffffE;      /* close Vfinder */
	ISP_WR32(ISP_REG_ADDR_TG_VF_CON, Reg);

	Reg = ISP_RD32(ISP_REG_ADDR_TG2_VF_CON);
	Reg &= 0xfffffffE;      /* close Vfinder */
	ISP_WR32(ISP_REG_ADDR_TG2_VF_CON, Reg);

	/* why i add this wake_unlock here, because     the     Ap is not expected to be dead. */
	/* The driver must releae the wakelock, otherwise the system will not enter     */
	/* the power-saving mode */
	if (g_bWaitLock == 1) {
#ifdef CONFIG_PM_SLEEP
		__pm_relax(&isp_wake_lock);
#endif
		g_bWaitLock = 0;
	}
	/* reset */
	/*      */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		memset((void *)IrqUserKey_UserInfo[i].userName, '\0', USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;
	}
	spin_lock(&(IspInfo.SpinLockIspRef));
	if (IspInfo.BufInfo.Read.pData != NULL) {
		kfree(IspInfo.BufInfo.Read.pData);
		IspInfo.BufInfo.Read.pData = NULL;
		IspInfo.BufInfo.Read.Size = 0;
		IspInfo.BufInfo.Read.Status = ISP_BUF_STATUS_EMPTY;
	}
	spin_unlock(&(IspInfo.SpinLockIspRef));
	/* Reset MCLK   */
	mMclk1User = 0;
	mMclk2User = 0;
	mMclk3User = 0;
	ISP_WR32(ISP_ADDR + 0x4200, 0x00000001);
	ISP_WR32(ISP_ADDR + 0x4600, 0x00000001);
	ISP_WR32(ISP_ADDR + 0x4a00, 0x00000001);
	LOG_DBG("ISP_MCLK1_EN Release");
	ISP_BufWrite_Free();

#if (LOG_CONSTRAINT_ADJ == 1)
	set_detect_count(g_log_def_constraint);
#endif
	/*      */
EXIT:

	/*      */
	/* LOG_DBG("Before spm_enable_sodi()."); */
	/* Enable sodi (Multi-Core Deep Idle). */

#if     0                       /* _mt6593fpga_dvt_use_ */
	spm_enable_sodi();
#endif

	/* Disable clock.
	 *  1. clkmgr: G_u4EnableClockCount=0, call clk_enable/disable
	 *  2. CCF: call clk_enable/disable every time
	 */
	ISP_EnableClock(MFALSE);
	LOG_DBG("isp release G_u4EnableClockCount: %d", G_u4EnableClockCount);
	/*  */
	LOG_INF("- X. UserCount: %d.", IspInfo.UserCount);
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
/* helper function,     mmap's the kmalloc'd area which is physically contiguous */
static signed int mmap_kmem(struct file *filp, struct vm_area_struct *vma)
{
	int ret;
	unsigned long length = 0;

	length = (vma->vm_end - vma->vm_start);

	/* check length - do not allow larger mappings than     the     number of */
	/* pages allocated */
	if (length > RT_BUF_TBL_NPAGES * PAGE_SIZE)
		return -EIO;

	/* map the whole physically     contiguous area in one piece */
	LOG_DBG("Vma->vm_pgoff(0x%lx),Vma->vm_start(0x%lx),Vma->vm_end(0x%lx),length(0x%lx)",
		vma->vm_pgoff, vma->vm_start, vma->vm_end, length);
	if (length > ISP_RTBUF_REG_RANGE) {
		LOG_PR_ERR("mmap range	error! : length(0x%lx),ISP_RTBUF_REG_RANGE(0x%x)!", length,
			ISP_RTBUF_REG_RANGE);
		return -EAGAIN;
	}
	ret =
		remap_pfn_range(vma, vma->vm_start, virt_to_phys((void *)pTbl_RTBuf) >> PAGE_SHIFT,
				length, vma->vm_page_prot);
	if (ret < 0)
		return ret;

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_mmap(struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned long pfn = 0x0;

	LOG_DBG("- E.");
	length = (pVma->vm_end - pVma->vm_start);
	/* at offset RT_BUF_TBL_NPAGES we map the kmalloc'd     area */
	if (pVma->vm_pgoff == RT_BUF_TBL_NPAGES)
		return mmap_kmem(pFile, pVma);
	/*      */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	LOG_DBG
	("pVma->vm_pgoff(0x%lx),phy(0x%lx),pVmapVma->vm_start(0x%lx),pVma->vm_end(0x%lx),length(0x%lx)",
		 pVma->vm_pgoff, pVma->vm_pgoff << PAGE_SHIFT, pVma->vm_start, pVma->vm_end,
		 length);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;
	switch (pfn) {
		case IMGSYS_BASE_ADDR:  /* imgsys */
			if (length > ISP_REG_RANGE) {
				LOG_PR_ERR("mmap range	error :	length(0x%lx),ISP_REG_RANGE(0x%x)!",
				length, ISP_REG_RANGE);
				return -EAGAIN;
			}
			break;
		case SENINF_BASE_ADDR:
			if (length > SENINF_REG_RANGE) {
				LOG_PR_ERR
				("mmap range	error :	length(0x%lx),SENINF_REG_RANGE(0x%x)!",
				length, SENINF_REG_RANGE);
				return -EAGAIN;
			}
			break;
		case PLL_BASE_ADDR:
			if (length > PLL_RANGE) {
				LOG_PR_ERR("mmap range	error :	length(0x%lx),PLL_RANGE(0x%x)!",
				length, PLL_RANGE);
				return -EAGAIN;
			}
			break;
		case MIPIRX_CONFIG_ADDR:
			if (length > MIPIRX_CONFIG_RANGE) {
				LOG_PR_ERR
				("mmap range	error :	length(0x%lx),MIPIRX_CONFIG_RANGE(0x%x)!",
				length, MIPIRX_CONFIG_RANGE);
				return -EAGAIN;
			}
			break;
		case MIPIRX_ANALOG_ADDR:
			if (length > MIPIRX_ANALOG_RANGE) {
				LOG_PR_ERR
				("mmap range	error :	length(0x%lx),MIPIRX_ANALOG_RANGE(0x%x)!",
				 length, MIPIRX_ANALOG_RANGE);
				return -EAGAIN;
			}
			break;
		case GPIO_BASE_ADDR:
			if (length > GPIO_RANGE) {
				LOG_PR_ERR("mmap range	error :	length(0x%lx),GPIO_RANGE(0x%x)!",
				length, GPIO_RANGE);
				return -EAGAIN;
			}
			break;
		default:
			LOG_PR_ERR("Illegal starting HW addr for mmap!");
			return -EAGAIN;
		}

	if (remap_pfn_range
	    (pVma, pVma->vm_start, pVma->vm_pgoff, pVma->vm_end - pVma->vm_start,
	     pVma->vm_page_prot)) {
		return -EAGAIN;
	}

	/*      */
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
#ifdef CONFIG_OF
struct cam_isp_device {
	void __iomem *regs[ISP_CAM_BASEADDR_NUM];
	struct device *dev;
	int irq[ISP_CAM_IRQ_IDX_NUM];
};

static struct cam_isp_device *cam_isp_devs;
static int nr_camisp_devs;
#endif
static dev_t IspDevNo;
static struct cdev *pIspCharDrv;
static struct class *pIspClass;

static const struct file_operations IspFileOper = {
	.owner = THIS_MODULE,
	.open = ISP_open,
	.release = ISP_release,
	/* .flush       = mt_isp_flush, */
	.mmap = ISP_mmap,
	.unlocked_ioctl = ISP_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ISP_ioctl_compat,
#endif
};

/*******************************************************************************
*
********************************************************************************/
static inline void ISP_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*      */
	/* Release char driver */
	if (pIspCharDrv != NULL) {
		cdev_del(pIspCharDrv);
		pIspCharDrv = NULL;
	}
	/*      */
	unregister_chrdev_region(IspDevNo, 1);
}

/*******************************************************************************
*
********************************************************************************/
static inline signed int ISP_RegCharDev(void)
{
	signed int Ret = 0;
	/*      */
	LOG_DBG("- E.");
	/*      */
	Ret = alloc_chrdev_region(&IspDevNo, 0, 1, ISP_DEV_NAME);
	if (Ret < 0) {
		LOG_PR_ERR("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate     driver */
	pIspCharDrv = cdev_alloc();
	if (pIspCharDrv == NULL) {
		LOG_PR_ERR("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pIspCharDrv, &IspFileOper);
	/*      */
	pIspCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pIspCharDrv, IspDevNo, 1);
	if (Ret < 0) {
		LOG_PR_ERR("Attatch file operation	failed,	%d", Ret);
		goto EXIT;
	}
	/*      */
EXIT:
	if (Ret < 0)
		ISP_UnregCharDev();

	/*      */

	LOG_DBG("- X.");
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/* struct resource *pRes = NULL; */
	signed int i = 0;
	unsigned char n;
	int new_count;
#ifdef CONFIG_OF
	struct cam_isp_device *cam_isp_dev;
#endif
	/* signed int j=0; */
	/*      */
	LOG_INF("kk:+ ISP_probe\n");

	LOG_DBG("- E.");
	LOG_INF("ISP driver proble.");
	/* Check platform_device parameters     */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_err(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	new_count = nr_camisp_devs + 1;
	cam_isp_devs =
		krealloc(cam_isp_devs, sizeof(struct cam_isp_device) * new_count, GFP_KERNEL);
	if (!cam_isp_devs) {
		dev_err(&pDev->dev, "Unable to allocate	cam_isp_devs\n");
		return -ENOMEM;
	}

	cam_isp_dev = &(cam_isp_devs[nr_camisp_devs]);
	cam_isp_dev->dev = &pDev->dev;

	/* iomap registers and irq */
	for (i = 0; i < ISP_CAM_BASEADDR_NUM; i++) {
		cam_isp_dev->regs[i] = of_iomap(pDev->dev.of_node, i);
		if (!cam_isp_dev->regs[i]) {
			dev_err(&pDev->dev, "Unable to ioremap registers, of_iomap fail, i=%d\n",
				i);
			return -ENOMEM;
		}

		gISPSYS_Reg[i] = (unsigned long)(cam_isp_dev->regs[i]);
		LOG_INF("DT, i=%d, map_addr=0x%lx\n", i, (unsigned long)cam_isp_dev->regs[i]);
	}

	/* get IRQ ID   and     request IRQ     */
	for (i = 0; i < ISP_CAM_IRQ_IDX_NUM; i++) {
		cam_isp_dev->irq[i] = irq_of_parse_and_map(pDev->dev.of_node, i);
		gISPSYS_Irq[i] = cam_isp_dev->irq[i];
		if (i == ISP_CAM0_IRQ_IDX) {
			Ret = request_irq(cam_isp_dev->irq[i], (irq_handler_t) ISP_Irq_CAM,
				IRQF_TRIGGER_NONE, "ISP", NULL);
			/* IRQF_TRIGGER_NONE dose not take effect here, real trigger mode set in dts file */
		} else if (i == ISP_CAMSV0_IRQ_IDX) {
			Ret = request_irq(cam_isp_dev->irq[i], (irq_handler_t) ISP_Irq_CAMSV,
				IRQF_TRIGGER_NONE, "ISP", NULL);
			/* IRQF_TRIGGER_NONE dose not take effect here, real trigger mode set in dts file */
		} else if (i == ISP_CAMSV1_IRQ_IDX) {
			Ret = request_irq(cam_isp_dev->irq[i], (irq_handler_t) ISP_Irq_CAMSV2,
				IRQF_TRIGGER_NONE, "ISP", NULL);
			/* IRQF_TRIGGER_NONE dose not take effect here, real trigger mode set in dts file */
		}

		if (Ret) {
			dev_err(&pDev->dev,
				"Unable to request IRQ, request_irq	fail,	i=%d, irq=%d\n", i,
				cam_isp_dev->irq[i]);
			return Ret;
		}
		LOG_INF("DT, i=%d, map_irq=%d\n", i, cam_isp_dev->irq[i]);
	}
	nr_camisp_devs = new_count;

	/* Register char driver */
	Ret = ISP_RegCharDev();
	if (Ret) {
		dev_err(&pDev->dev, "register char failed");
		return Ret;
	}
	/* Mapping CAM_REGISTERS */
#if     0
	for (i = 0; i < 1; i++) {       /* NEED_TUNING_BY_CHIP. 1: Only one IORESOURCE_MEM type resource */
		/* in kernel\mt_devs.c\mt_resource_isp[]. */
		LOG_DBG("Mapping CAM_REGISTERS.	i: %d.", i);
		pRes = platform_get_resource(pDev, IORESOURCE_MEM, i);
		if (pRes == NULL) {
			dev_err(&pDev->dev, "platform_get_resource failed");
			Ret = -ENOMEM;
			goto EXIT;
		}
		pRes = request_mem_region(pRes->start, pRes->end - pRes->start + 1, pDev->name);
		if (pRes == NULL) {
			dev_err(&pDev->dev, "request_mem_region	failed");
			Ret = -ENOMEM;
			goto EXIT;
		}
	}
#endif

#if defined(CONFIG_MTK_CLKMGR) || defined(EP_NO_CLKMGR)
#else
	/*CCF: Grab clock pointer (struct clk*) */
	isp_clk.CG_SCP_SYS_MM0 = devm_clk_get(&pDev->dev, "CG_SCP_SYS_MM0");
	isp_clk.CG_SCP_SYS_ISP = devm_clk_get(&pDev->dev, "CG_SCP_SYS_ISP");
	isp_clk.CG_IMG_CAM_SMI = devm_clk_get(&pDev->dev, "CG_IMG_CAM_SMI");
	isp_clk.CG_IMG_CAM_CAM = devm_clk_get(&pDev->dev, "CG_IMG_CAM_CAM");
	isp_clk.CG_IMG_SEN_TG = devm_clk_get(&pDev->dev, "CG_IMG_SEN_TG");
	isp_clk.CG_IMG_SEN_CAM = devm_clk_get(&pDev->dev, "CG_IMG_SEN_CAM");
	isp_clk.CG_IMG_CAM_SV = devm_clk_get(&pDev->dev, "CG_IMG_CAM_SV");

	if (IS_ERR(isp_clk.CG_SCP_SYS_MM0)) {
		LOG_PR_ERR("cannot get CG_SCP_SYS_MM0 clock\n");
		return PTR_ERR(isp_clk.CG_SCP_SYS_MM0);
	}
	if (IS_ERR(isp_clk.CG_SCP_SYS_ISP)) {
		LOG_PR_ERR("cannot get CG_SCP_SYS_ISP clock\n");
		return PTR_ERR(isp_clk.CG_SCP_SYS_ISP);
	}
	if (IS_ERR(isp_clk.CG_IMG_CAM_SMI)) {
		LOG_PR_ERR("cannot get CG_IMG_CAM_SMI clock\n");
		return PTR_ERR(isp_clk.CG_IMG_CAM_SMI);
	}
	if (IS_ERR(isp_clk.CG_IMG_CAM_CAM)) {
		LOG_PR_ERR("cannot get CG_IMG_CAM_CAM clock\n");
		return PTR_ERR(isp_clk.CG_IMG_CAM_CAM);
	}
	if (IS_ERR(isp_clk.CG_IMG_SEN_TG)) {
		LOG_PR_ERR("cannot get CG_IMG_SEN_TG clock\n");
		return PTR_ERR(isp_clk.CG_IMG_SEN_TG);
	}
	if (IS_ERR(isp_clk.CG_IMG_SEN_CAM)) {
		LOG_PR_ERR("cannot get CG_IMG_SEN_CAM clock\n");
		return PTR_ERR(isp_clk.CG_IMG_SEN_CAM);
	}
	if (IS_ERR(isp_clk.CG_IMG_CAM_SV)) {
		LOG_PR_ERR("cannot get CG_IMG_CAM_SV clock\n");
		return PTR_ERR(isp_clk.CG_IMG_CAM_SV);
	}
#endif

	/* Create class register */
	pIspClass = class_create(THIS_MODULE, "ispdrv");
	if (IS_ERR(pIspClass)) {
		Ret = PTR_ERR(pIspClass);
		LOG_PR_ERR("Unable	to create class, err = %d", Ret);
		return Ret;
	}
	/* FIXME: error handling */
	device_create(pIspClass, NULL, IspDevNo, NULL, ISP_DEV_NAME);

#endif
	/*      */
	init_waitqueue_head(&IspInfo.WaitQueueHead);
	tasklet_init(&isp_tasklet, ISP_TaskletFunc, 0);

#ifdef CONFIG_PM_SLEEP
	wakeup_source_init(&isp_wake_lock, "isp_lock_wakelock");
#endif

	/*      */
	INIT_WORK(&IspInfo.ScheduleWorkVD, ISP_ScheduleWork_VD);
	INIT_WORK(&IspInfo.ScheduleWorkEXPDONE, ISP_ScheduleWork_EXPDONE);
	/*      */
	spin_lock_init(&(IspInfo.SpinLockIspRef));
	spin_lock_init(&(IspInfo.SpinLockIsp));

	for (n = 0; n < _IRQ_MAX; n++)
		spin_lock_init(&(IspInfo.SpinLockIrq[n]));

	spin_lock_init(&(IspInfo.SpinLockHold));
	spin_lock_init(&(IspInfo.SpinLockRTBC));
	spin_lock_init(&(IspInfo.SpinLockClock));
	/*      */
	IspInfo.UserCount = 0;
	IspInfo.HoldInfo.Time = ISP_HOLD_TIME_EXPDONE;
	/*      */
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST] = ISP_REG_MASK_INT_P1_ST;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2] = ISP_REG_MASK_INT_P1_ST2;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST_D] = ISP_REG_MASK_INT_P1_ST_D;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P1_ST2_D] = ISP_REG_MASK_INT_P1_ST2_D;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_P2_ST] = ISP_REG_MASK_INT_P2_ST;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUSX] = ISP_REG_MASK_INT_STATUSX;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS2X] = ISP_REG_MASK_INT_STATUS2X;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_STATUS3X] = ISP_REG_MASK_INT_STATUS3X;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF1] = ISP_REG_MASK_INT_SENINF1;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF2] = ISP_REG_MASK_INT_SENINF2;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF3] = ISP_REG_MASK_INT_SENINF3;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_SENINF4] = ISP_REG_MASK_INT_SENINF4;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV] = ISP_REG_MASK_CAMSV_ST;
	IspInfo.IrqInfo.Mask[ISP_IRQ_TYPE_INT_CAMSV2] = ISP_REG_MASK_CAMSV2_ST;
	/*      */
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST] = ISP_REG_MASK_INT_P1_ST_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST2] = ISP_REG_MASK_INT_P1_ST2_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST_D] = ISP_REG_MASK_INT_P1_ST_D_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P1_ST2_D] = ISP_REG_MASK_INT_P1_ST2_D_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_P2_ST] = ISP_REG_MASK_INT_P2_ST_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUSX] = ISP_REG_MASK_INT_STATUSX_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUS2X] = ISP_REG_MASK_INT_STATUS2X_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_STATUS3X] = ISP_REG_MASK_INT_STATUS3X_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF1] = ISP_REG_MASK_INT_SENINF1_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF2] = ISP_REG_MASK_INT_SENINF2_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF3] = ISP_REG_MASK_INT_SENINF3_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_SENINF4] = ISP_REG_MASK_INT_SENINF4_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV] = ISP_REG_MASK_CAMSV_ST_ERR;
	IspInfo.IrqInfo.ErrMask[ISP_IRQ_TYPE_INT_CAMSV2] = ISP_REG_MASK_CAMSV2_ST_ERR;

	/* enqueue/dequeue control in ihalpipe wrapper */
	init_waitqueue_head(&WaitQueueHead_EDBuf_WaitDeque);
	init_waitqueue_head(&WaitQueueHead_EDBuf_WaitFrame);
	spin_lock_init(&(SpinLockEDBufQueList));
	spin_lock_init((spinlock_t *)(&(SpinLockRegScen)));
	spin_lock_init((spinlock_t *)(&(SpinLock_UserKey)));
	spin_lock_init((spinlock_t *)(&(SpinLockMagNum)));
	/* Request CAM_ISP IRQ */
#ifndef CONFIG_OF
	/* FIXME */

	/* if (request_irq(CAMERA_ISP_IRQ0_ID, (irq_handler_t)ISP_Irq, IRQF_TRIGGER_HIGH, "isp", NULL)) */

	if (request_irq
	    (CAM0_IRQ_BIT_ID, (irq_handler_t) ISP_Irq_CAM, IRQF_TRIGGER_LOW, "ISP", NULL)) {
		LOG_PR_ERR("MT6593_CAM_IRQ_LINE IRQ LINE NOT AVAILABLE!!");
		goto EXIT;
	}
	/* mt_irq_unmask(CAMERA_ISP_IRQ0_ID); */
	/* request CAM_SV IRQ */
	if (request_irq
	    (CAM_SV0_IRQ_BIT_ID, (irq_handler_t) ISP_Irq_CAMSV, IRQF_TRIGGER_LOW, "ISP", NULL)) {
		LOG_PR_ERR("MT6593_CAMSV1_IRQ_LINE	IRQ LINE NOT AVAILABLE!!");
		goto EXIT;
	}
	/* request CAM_SV2 IRQ */
	if (request_irq
	    (CAM_SV1_IRQ_BIT_ID, (irq_handler_t) ISP_Irq_CAMSV2, IRQF_TRIGGER_LOW, "ISP", NULL)) {
		LOG_PR_ERR("MT6593_CAMSV2_IRQ_LINE	IRQ LINE NOT AVAILABLE!!");
		goto EXIT;
	}
#endif
#if 0
EXIT:
#endif
	/*if (Ret < 0)//note: Ret won't < 0 at here*/
	/*	ISP_UnregCharDev();*/

	/*      */
	LOG_DBG("- X.");
	/*      */
	LOG_INF("kk:- ISP_probe, ret=%d\n", Ret);
	/*      */
	return Ret;
}

/*******************************************************************************
* Called when the device is     being detached from     the     driver
********************************************************************************/
static signed int ISP_remove(struct platform_device *pDev)
{
	/* struct resource *pRes; */
	/* signed int i; */
	signed int IrqNum;
	/*      */
	LOG_DBG("- E.");
	/* unregister char driver. */
	ISP_UnregCharDev();
	/* unmaping ISP CAM_REGISTER registers */
#if     0
	for (i = 0; i < 2; i++) {
		pRes = platform_get_resource(pDev, IORESOURCE_MEM, 0);
		release_mem_region(pRes->start, (pRes->end - pRes->start + 1));
	}
#endif
	/* Release IRQ */
	disable_irq(IspInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	tasklet_kill(&isp_tasklet);
#if     0
	/* free all     registered irq(child nodes)     */
	ISP_UnRegister_AllregIrq();
	/* free father nodes of irq     user list */
	struct my_list_head *head;
	struct my_list_head *father;

	head = ((struct my_list_head *)(&SupIrqUserListHead.list));
	while (1) {
		father = head;
		if (father->nextirq != father) {
			father = father->nextirq;
			REG_IRQ_NODE *accessNode;

			typeof(((REG_IRQ_NODE *) 0)->list) * __mptr = (father);
			accessNode =
				((REG_IRQ_NODE *)((char *)__mptr - offsetof(REG_IRQ_NODE, list)));
			LOG_INF("free father,reg_T(%d)\n", accessNode->reg_T);
			if (father->nextirq != father) {
				head->nextirq = father->nextirq;
				father->nextirq = father;
			} else {        /* last father node */
				head->nextirq = head;
				LOG_INF("break\n");
				break;
			}
			kfree(accessNode);
		}
	}
#endif
	/*      */
	device_destroy(pIspClass, IspDevNo);
	/*      */
	class_destroy(pIspClass);
	pIspClass = NULL;
	/*      */
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static signed int bPass1_On_In_Resume_TG1;
static signed int bPass1_On_In_Resume_TG2;
static signed int ISP_suspend(struct platform_device *pDev, pm_message_t Mesg)
{
	unsigned int regTG1Val, regTG2Val;

	if (IspInfo.UserCount == 0) {
		LOG_DBG("ISP UserCount=0");
		return 0;
	}

	/* TG_VF_CON[0] (0x15004414[0]): VFDATA_EN.     TG1     Take Picture Request. */
	regTG1Val = ISP_RD32(ISP_ADDR + 0x414);
	/* TG2_VF_CON[0] (0x150044B4[0]): VFDATA_EN. TG2 Take Picture Request. */
	regTG2Val = ISP_RD32(ISP_ADDR + 0x4B4);

	LOG_DBG
	("bPass1_On_In_Resume_TG1(%d). bPass1_On_In_Resume_TG2(%d). regTG1Val(0x%08x). regTG2Val(0x%08x)\n",
	 bPass1_On_In_Resume_TG1, bPass1_On_In_Resume_TG2, regTG1Val, regTG2Val);

	bPass1_On_In_Resume_TG1 = 0;
	if (regTG1Val & 0x01) { /* For TG1 Main sensor. */
		bPass1_On_In_Resume_TG1 = 1;
		ISP_WR32(ISP_ADDR + 0x414, (regTG1Val & (~0x01)));
	}

	bPass1_On_In_Resume_TG2 = 0;
	if (regTG2Val & 0x01) { /* For TG2 Sub sensor. */
		bPass1_On_In_Resume_TG2 = 1;
		ISP_WR32(ISP_ADDR + 0x4B4, (regTG2Val & (~0x01)));
	}

	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static signed int ISP_resume(struct platform_device *pDev)
{
	unsigned int regTG1Val, regTG2Val;

	if (IspInfo.UserCount == 0) {
		LOG_DBG("ISP UserCount=0");
		return 0;
	}

	/* TG_VF_CON[0] (0x15004414[0]): VFDATA_EN.     TG1     Take Picture Request. */
	regTG1Val = ISP_RD32(ISP_ADDR + 0x414);
	/* TG2_VF_CON[0] (0x150044B4[0]): VFDATA_EN. TG2 Take Picture Request. */
	regTG2Val = ISP_RD32(ISP_ADDR + 0x4B4);

	LOG_DBG
	("bPass1_On_In_Resume_TG1(%d). bPass1_On_In_Resume_TG2(%d). regTG1Val(0x%x) regTG2Val(0x%x)\n",
	 bPass1_On_In_Resume_TG1, bPass1_On_In_Resume_TG2, regTG1Val, regTG2Val);


	if (bPass1_On_In_Resume_TG1) {
		bPass1_On_In_Resume_TG1 = 0;
		ISP_WR32(ISP_ADDR + 0x414, (regTG1Val | 0x01)); /* For TG1 Main sensor. */
	}

	if (bPass1_On_In_Resume_TG2) {
		bPass1_On_In_Resume_TG2 = 0;
		ISP_WR32(ISP_ADDR + 0x4B4, (regTG2Val | 0x01)); /* For TG2 Sub sensor. */
	}

	return 0;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
int ISP_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	pr_debug("calling %s()\n", __func__);

	return ISP_suspend(pdev, PMSG_SUSPEND);
}

int ISP_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	pr_debug("calling %s()\n", __func__);

	return ISP_resume(pdev);
}

/* move to camera_isp_D1.h */
/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
int ISP_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
	/* Originally CAM0_IRQ_BIT_ID was define in mt_irq.h and x_define_irq.h */
	/* #define X_DEFINE_IRQ(__name, __num, __pol, __sens)  __name = __num,  */
	/* X_DEFINE_IRQ(CAM0_IRQ_BIT_ID, 215, L, level)                         */
	/* 32+183=215                                                           */
#define CAM0_IRQ_BIT_ID 215
	mt_irq_set_sens(CAM0_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);
	mt_irq_set_polarity(CAM0_IRQ_BIT_ID, MT_POLARITY_LOW);
#endif
	return 0;

}

/*---------------------------------------------------------------------------*/
#else                           /*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define ISP_pm_suspend NULL
#define ISP_pm_resume  NULL
#define ISP_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif                          /*CONFIG_PM */
/*---------------------------------------------------------------------------*/

#ifdef CONFIG_OF
static const struct of_device_id isp_of_ids[] = {
	{.compatible = "mediatek,mt6739-ispsys",},
	{}
};
#endif

const struct dev_pm_ops ISP_pm_ops = {
	.suspend = ISP_pm_suspend,
	.resume = ISP_pm_resume,
	.freeze = ISP_pm_suspend,
	.thaw = ISP_pm_resume,
	.poweroff = ISP_pm_suspend,
	.restore = ISP_pm_resume,
	.restore_noirq = ISP_pm_restore_noirq,
};


/*******************************************************************************
*
********************************************************************************/
static struct platform_driver IspDriver = {
	.probe = ISP_probe,
	.remove = ISP_remove,
	.suspend = ISP_suspend,
	.resume = ISP_resume,
	.driver = {
		.name = ISP_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = isp_of_ids,
#endif
#ifdef CONFIG_PM
		.pm = &ISP_pm_ops,
#endif
	}
};
/*******************************************************************************
*
********************************************************************************/
/* ssize_t (*read) (struct file *, char __user *, size_t, loff_t *) */
static ssize_t ISP_DumpRegToProc(
	struct file *pFile,
	char *pStart,
	size_t off,
	loff_t *Count)
{
	LOG_INF("ISP_DumpRegToProc: Not implement");
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
/* ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *) */
static ssize_t ISP_RegDebug(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
{
	LOG_INF("ISP_RegDebug: Not implement");
	return 0;
}

/* ssize_t (*read) (struct file *, char __user *, size_t, loff_t *) */
static ssize_t CAMIO_DumpRegToProc(
	struct file *pFile,
	char *pStart,
	size_t off,
	loff_t *Count)
{
	LOG_INF("CAMIO_DumpRegToProc: Not implement");
	return 0;
}


/*******************************************************************************
*
********************************************************************************/
/* ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *) */
static ssize_t CAMIO_RegDebug(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
{
	LOG_INF("CAMIO_RegDebug: Not implement");
	return 0;
}

/*******************************************************************************
*
********************************************************************************/
static const struct file_operations fcameraisp_proc_fops = {
	.read = ISP_DumpRegToProc,
	.write = ISP_RegDebug,
};

static const struct file_operations fcameraio_proc_fops = {
	.read = CAMIO_DumpRegToProc,
	.write = CAMIO_RegDebug,
};

/*******************************************************************************
*
********************************************************************************/
bool ISP_RegCallback(struct ISP_CALLBACK_STRUCT *pCallback)
{
	/*      */
	if (pCallback == NULL) {
		LOG_PR_ERR("pCallback is null");
		return MFALSE;
	}
	/*      */
	if (pCallback->Func == NULL) {
		LOG_PR_ERR("Func is null");
		return MFALSE;
	}
	/*      */
	LOG_DBG("Type(%d)", pCallback->Type);
	IspInfo.Callback[pCallback->Type].Func = pCallback->Func;
	/*      */
	return MTRUE;
}
EXPORT_SYMBOL(ISP_RegCallback);

/*******************************************************************************
*
********************************************************************************/
bool ISP_UnregCallback(enum ISP_CALLBACK_ENUM Type)
{
	if (Type >= ISP_CALLBACK_AMOUNT) {
		LOG_PR_ERR("Type(%d) must smaller than %d", Type, ISP_CALLBACK_AMOUNT);
		return MFALSE;
	}
	/*      */
	LOG_DBG("Type(%d)", Type);
	IspInfo.Callback[Type].Func = NULL;
	/*      */
	return MTRUE;
}
EXPORT_SYMBOL(ISP_UnregCallback);


void ISP_MCLK1_EN(unsigned char En)
{
	unsigned int temp = 0;
	if (IspInfo.UserCount <= 0)
		return;
	if (En == 1) {
		mMclk1User++;
	} else {
		mMclk1User--;
		if (mMclk1User <= 0)
			mMclk1User = 0;
	}

	temp = ISP_RD32(ISP_ADDR + 0x4200);
	if (En) {
		if (mMclk1User > 0) {
			temp |= 0x20000000;
			ISP_WR32(ISP_ADDR + 0x4200, temp);
		}
	} else {
		if (mMclk1User == 0) {
			temp &= 0xDFFFFFFF;
			ISP_WR32(ISP_ADDR + 0x4200, temp);
		}
	}
	temp = ISP_RD32(ISP_ADDR + 0x4200);
	LOG_INF("ISP_MCLK1_EN(%d), mMclk1User(%d)", temp, mMclk1User);

}
EXPORT_SYMBOL(ISP_MCLK1_EN);




void ISP_MCLK2_EN(unsigned char En)
{
	unsigned int temp = 0;
	if (IspInfo.UserCount <= 0)
		return;
	if (En == 1) {
		mMclk2User++;
	} else {
		mMclk2User--;
		if (mMclk2User <= 0)
			mMclk2User = 0;
	}

	temp = ISP_RD32(ISP_ADDR + 0x4600);
	if (En) {
		if (mMclk2User > 0) {
			temp |= 0x20000000;
			ISP_WR32(ISP_ADDR + 0x4600, temp);
		}
	} else {
		if (mMclk2User == 0) {
			temp &= 0xDFFFFFFF;
			ISP_WR32(ISP_ADDR + 0x4600, temp);
		}
	}
	LOG_INF("ISP_MCLK2_EN(%d), mMclk2User(%d)", temp, mMclk2User);
}
EXPORT_SYMBOL(ISP_MCLK2_EN);



void ISP_MCLK3_EN(unsigned char En)
{
	unsigned int temp = 0;
	if (IspInfo.UserCount <= 0)
		return;
	if (En == 1) {
		mMclk3User++;
	} else {
		mMclk3User--;
		if (mMclk3User <= 0)
			mMclk3User = 0;
	}

	temp = ISP_RD32(ISP_ADDR + 0x4A00);
	if (En) {
		if (mMclk3User > 0) {
			temp |= 0x20000000;
			ISP_WR32(ISP_ADDR + 0x4A00, temp);
		}
	} else {
		if (mMclk3User == 0) {
			temp &= 0xDFFFFFFF;
			ISP_WR32(ISP_ADDR + 0x4A00, temp);
		}
	}
	LOG_INF("ISP_MCLK3_EN(%d), mMclk3User(%d)", temp, mMclk3User);
}
EXPORT_SYMBOL(ISP_MCLK3_EN);

int32_t ISP_MDPClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("ISP_MDPClockOnCallback"); */
	LOG_DBG("+MDPEn:%d", G_u4EnableClockCount);
	ISP_EnableClock(MTRUE);

	return 0;
}

int32_t ISP_MDPDumpCallback(uint64_t engineFlag, int level)
{
	LOG_DBG("ISP_MDPDumpCallback");

	ISP_DumpReg();

	return 0;
}

int32_t ISP_MDPResetCallback(uint64_t engineFlag)
{
	/* LOG_DBG("ISP_MDPResetCallback"); */

	ISP_Reset(ISP_REG_SW_CTL_RST_CAM_P2);

	return 0;
}

int32_t ISP_MDPClockOffCallback(uint64_t engineFlag)
{
	/* LOG_DBG("ISP_MDPClockOffCallback"); */
	ISP_EnableClock(MFALSE);
	LOG_DBG("-MDPEn:%d", G_u4EnableClockCount);
	return 0;
}


static uint32_t *addressToDump[] = {

#if     0                       /*     QQ */
#if     CONFIG_OF
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4018),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x401C),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4024),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4030),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x403C),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4040),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4080),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4084),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4088),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x40A0),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x40A4),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x40A8),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48A0),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48A4),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48A8),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48AC),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48B0),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48B4),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48B8),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48BC),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48C0),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48C4),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48C8),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48CC),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48D0),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48D4),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48D8),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48DC),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48E0),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48E4),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x48E8),
	/*      */
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4948),
	/*      */
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B00),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B04),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B08),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B0C),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B10),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B14),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B18),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B1C),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B20),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B24),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B28),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B2C),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B30),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B34),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x4B38),
	/*      */
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7204),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7208),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x720C),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7230),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7240),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7288),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x72a4),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7300),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7320),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7440),
	(uint32_t *)(ISP_IMGSYS_BASE_PHY + 0x7460)
#else
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4018),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x401C),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4024),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4030),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x403C),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4040),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4080),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4084),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4088),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x40A0),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x40A4),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x40A8),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48A0),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48A4),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48A8),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48AC),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48B0),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48B4),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48B8),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48BC),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48C0),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48C4),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48C8),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48CC),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48D0),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48D4),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48D8),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48DC),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48E0),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48E4),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x48E8),
	/*      */
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4948),
	/*      */
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B00),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B04),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B08),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B0C),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B10),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B14),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B18),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B1C),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B20),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B24),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B28),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B2C),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B30),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B34),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x4B38),
	/*      */
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7204),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7208),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x720C),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7230),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7240),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7288),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x72a4),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7300),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7320),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7440),
	(uint32_t *) IO_VIRT_TO_PHYS(ISP_IMGSYS_BASE + 0x7460)
#endif
#endif
};

int32_t ISP_BeginGCECallback(uint32_t taskID, uint32_t *regCount, uint32_t **regAddress)
{
	LOG_DBG("+,taskID(%d)", taskID);

	*regCount = ARRAY_SIZE(addressToDump);
	*regAddress = (uint32_t *) addressToDump;

	LOG_DBG("-,*regCount(%d)", *regCount);

	return 0;
}

int32_t ISP_EndGCECallback(uint32_t taskID, uint32_t regCount, uint32_t *regValues)
{
#define PER_LINE_LOG_SIZE       10
	int32_t i, j, pos;
	/* uint32_t     add[PER_LINE_LOG_SIZE]; */
	uint32_t *add[PER_LINE_LOG_SIZE];
	uint32_t val[PER_LINE_LOG_SIZE];

	LOG_DBG("End taskID(%d),regCount(%d)", taskID, regCount);

	for (i = 0; i < regCount; i += PER_LINE_LOG_SIZE) {
		for (j = 0; j < PER_LINE_LOG_SIZE; j++) {
			pos = i + j;
			if (pos < regCount) {
				/* addr[j] = (uint32_t)addressToDump[pos]&0xffff; */
				add[j] = addressToDump[pos];
				val[j] = regValues[pos];
			}
		}
		/* LOG_DBG("[0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x] */
		/* [0x%04x,0x%08x] [0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x][0x%04x,0x%08x]\n", */
		/* add[0],val[0],add[1],val[1],add[2],val[2],add[3],val[3],add[4],val[4],add[5],val[5],add[6],val[6],*/
		/* add[7],val[7],add[8],val[8],add[9],val[9]); */
		LOG_DBG
		("[0x%p,0x%08x][0x%p,0x%08x][0x%p,0x%08x][0x%p,0x%08x][0x%p,0x%08x]",
		 add[0], val[0], add[1], val[1], add[2], val[2], add[3], val[3], add[4], val[4]);
		LOG_DBG
		("[0x%p,0x%08x][0x%p,0x%08x][0x%p,0x%08x][0x%p,0x%08x][0x%p,0x%08x]\n",
		 add[5], val[5], add[6], val[6], add[7], val[7], add[8], val[8], add[9], val[9]);
	}
	return 0;
}

enum m4u_callback_ret_t ISP_M4U_TranslationFault_callback(
			int port, unsigned int mva, void *data)
{
	LOG_DBG("[ISP_M4U]fault	call port=%d, mva=0x%x", port, mva);

	switch (port) {
	case M4U_PORT_CAM_IMGO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3300),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3300));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3304),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3304));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3308),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3308));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x330c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x330c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3310),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3310));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3314),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3314));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3318),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3318));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x331c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x331c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3320),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3320));
		break;
	case M4U_PORT_CAM_RRZO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3320),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3320));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3324),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3324));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3328),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3328));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x332c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x332c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3330),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3330));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3334),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3334));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3338),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3338));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x333c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x333c));
		break;
	case M4U_PORT_CAM_AAO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3364),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3364));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3368),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3368));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3388),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3388));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x338c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x338c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3390),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3390));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3394),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3394));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3398),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3398));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x339c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x339c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33a0));
		break;
#if     0
	case M4U_PORT_CAM_LCSO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3340),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3340));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3344),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3344));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3348),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3348));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x334c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x334c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3350),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3350));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3354),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3354));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3358),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3358));
		break;
#endif
	case M4U_PORT_CAM_ESFKO:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x335c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x335c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3360),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3360));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x336c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x336c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3370),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3370));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3374),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3374));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3378),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3378));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x337c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x337c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3380),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3380));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3384),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3384));
		break;
	case M4U_PORT_CAM_SV0:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00cc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00cc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00d0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00d0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00d4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00d4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00d8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00d8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34d4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34d4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34d8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34d8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34dc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34dc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34e0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34e0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34e4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34e4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34e8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34e8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34ec),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34ec));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34f0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34f0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34f4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34f4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34f8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34f8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34fc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34fc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3500),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3500));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3504),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3504));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3508),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3508));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x350c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x350c));
		break;
	case M4U_PORT_CAM_LSCI_0:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x326c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x326c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3270),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3270));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3274),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3274));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3278),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3278));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x327c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x327c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3280),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3280));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3284),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3284));
		break;
	case M4U_PORT_CAM_LSCI_1:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34c4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34c4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34c8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34c8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34cc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34cc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x34d0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x34d0));
		break;
	case M4U_PORT_CAM_BPCI_0:
	case M4U_PORT_CAM_BPCI_1:
		break;
	case M4U_PORT_CAM_IMGI:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3230),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3230));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3230),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3230));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3234),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3234));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3238),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3238));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x323c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x323c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3240),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3240));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3248),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3248));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x324c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x324c));
		break;
#if 0
	case M4U_PORT_LCEI:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32bc));
		break;
	case M4U_PORT_UFDI:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3288),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3288));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x328c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x328c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3290),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3290));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3294),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3294));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3298),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3298));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x329c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x329c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32a0));
		break;
#endif
	case M4U_PORT_CAM_IMG2O:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3440),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3440));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3444),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3444));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3448),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3448));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x344c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x344c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3450),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3450));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3454),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3454));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3458),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3458));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x345c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x345c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3480),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3480));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3484),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3484));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3488),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3488));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x348c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x348c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3490),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3490));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3494),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3494));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3498),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3498));
		break;
#if 0
	case M4U_PORT_IMG3O:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3460),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3460));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3464),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3464));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3468),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3468));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x346c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x346c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3470),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3470));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3474),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3474));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3478),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3478));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x347c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x347c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3400),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3400));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3404),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3404));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3408),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3408));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x340c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x340c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3410),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3410));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3414),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3414));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3418),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3418));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x341c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x341c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3420),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3420));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3424),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3424));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3428),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3428));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x342c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x342c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3430),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3430));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3434),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3434));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3438),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3438));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x343c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x343c));
		break;
	case M4U_PORT_VIPI:
	case M4U_PORT_VIP2I:
	case M4U_PORT_VIP3I:
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3204),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3204));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x3230),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x3230));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32c0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32c4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32c4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32c8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32c8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32cc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32cc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32d0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32d0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32d4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32d4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32d8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32d8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32dc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32dc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32e0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32e0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32e4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32e4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32e8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32e8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32ec),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32ec));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32f0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32f0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32f4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32f4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32f8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32f8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x32fc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x32fc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33a4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33a8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33ac),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33ac));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33b0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33b0));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33b4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33b4));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33b8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33b8));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33bc),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33bc));
		LOG_DBG("[TF_%d]0x%08X %08X", port, (unsigned int)(ISP_TPIPE_ADDR + 0x33c0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x33c0));
		break;
#endif
	default:
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0000),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0000));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0004),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0004));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0008),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0008));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0010),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0010));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0014),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0014));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0018),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0018));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x001c),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x001c));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x0020),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x0020));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x00a0),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a0));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x00a4),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a4));
		LOG_DBG("0x%08X	%08X", (unsigned int)(ISP_TPIPE_ADDR + 0x00a8),
			(unsigned int)ISP_RD32(ISP_ADDR + 0x00a8));
		break;
	}

	return M4U_CALLBACK_HANDLED;
}

/*******************************************************************************
*
********************************************************************************/

static signed int __init ISP_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
#if     0
	struct proc_dir_entry *pEntry;
#endif
	int i;
	/*      */
	LOG_DBG("- E.");
	/*      */
	Ret = platform_driver_register(&IspDriver);
	if (Ret < 0) {
		LOG_PR_ERR("platform_driver_register fail");
		return Ret;
	}
	/*      */
	/* FIX-ME: linux-3.10 procfs API changed */
#if     1
	proc_create("driver/isp_reg", 0, NULL, &fcameraisp_proc_fops);
	proc_create("driver/camio_reg", 0, NULL, &fcameraio_proc_fops);
#else
	pEntry = create_proc_entry("driver/isp_reg", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = ISP_DumpRegToProc;
		pEntry->write_proc = ISP_RegDebug;
	} else {
		LOG_INF("add /proc/driver/isp_reg entry	fail");
	}

	pEntry = create_proc_entry("driver/camio_reg", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = CAMIO_DumpRegToProc;
		pEntry->write_proc = CAMIO_RegDebug;
	} else {
		LOG_INF("add /proc/driver/camio_reg entry fail");
	}
#endif
	/*      */
	/* allocate     a memory area with kmalloc.     Will be rounded up to a page boundary */
	/* RT_BUF_TBL_NPAGES*4096(1page) = 64k Bytes */

	if (sizeof(struct ISP_RT_BUF_STRUCT) > ((RT_BUF_TBL_NPAGES) * PAGE_SIZE)) {
		i = 0;

		while (i < sizeof(struct ISP_RT_BUF_STRUCT))
			i += PAGE_SIZE;
		pBuf_kmalloc = kmalloc(i + 2 * PAGE_SIZE, GFP_KERNEL);
		if (pBuf_kmalloc == NULL) {
			LOG_PR_ERR("mem not enough\n");
			return -ENOMEM;
		}
		memset(pBuf_kmalloc, 0x00, i);
	} else {
		pBuf_kmalloc = kmalloc((RT_BUF_TBL_NPAGES + 2) * PAGE_SIZE, GFP_KERNEL);
		if (pBuf_kmalloc == NULL) {
			LOG_PR_ERR("mem not enough\n");
			return -ENOMEM;
		}
		memset(pBuf_kmalloc, 0x00, RT_BUF_TBL_NPAGES * PAGE_SIZE);
	}
	/* round it     up to the page bondary */
	pTbl_RTBuf = (int *)((((unsigned long)pBuf_kmalloc) + PAGE_SIZE - 1) & PAGE_MASK);
	pstRTBuf = (struct ISP_RT_BUF_STRUCT *) pTbl_RTBuf;
	pstRTBuf->state = ISP_RTBC_STATE_INIT;
	/* isr log */
	if (PAGE_SIZE <
	    ((_IRQ_MAX * NORMAL_STR_LEN * ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i <
		       ((_IRQ_MAX * NORMAL_STR_LEN * ((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
			LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if (pLog_kmalloc == NULL) {
		LOG_PR_ERR("mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < _IRQ_MAX; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			/* tmp = (void *) ((unsigned int)tmp + (NORMAL_STR_LEN*DBG_PAGE)); */
			tmp = (void *)((char *)tmp + (NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			/* tmp = (void *) ((unsigned int)tmp + (NORMAL_STR_LEN*INF_PAGE)); */
			tmp = (void *)((char *)tmp + (NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			/* tmp = (void *) ((unsigned int)tmp + (NORMAL_STR_LEN*ERR_PAGE)); */
			tmp = (void *)((char *)tmp + (NORMAL_STR_LEN * ERR_PAGE));
		}
		/* tmp = (void *) ((unsigned int)tmp + NORMAL_STR_LEN);//log    buffer ,in case of overflow     */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);   /* log buffer     ,in     case of overflow */
	}
	/* mark the     pages reserved , FOR MMAP */
	for (i = 0; i < RT_BUF_TBL_NPAGES * PAGE_SIZE; i += PAGE_SIZE)
		SetPageReserved(virt_to_page(((unsigned long)pTbl_RTBuf) + i));
	/*      */
	/* Register ISP callback */
	LOG_DBG("register isp callback for MDP");
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP, ISP_MDPClockOnCallback, ISP_MDPDumpCallback,
			   ISP_MDPResetCallback, ISP_MDPClockOffCallback);
	/* Register GCE callback for dumping ISP register */
	LOG_DBG("register isp callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB(ISP_BeginGCECallback, ISP_EndGCECallback);

	/* Register M4U callback dump */
	LOG_DBG("register M4U callback dump");
	m4u_register_fault_callback(M4U_PORT_CAM_IMGI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_IMGO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_RRZO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_AAO, ISP_M4U_TranslationFault_callback, NULL);
	/* m4u_register_fault_callback(M4U_PORT_CAM_LCSO, ISP_M4U_TranslationFault_callback, NULL); */
	m4u_register_fault_callback(M4U_PORT_CAM_ESFKO, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_SV0, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_LSCI_0, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_LSCI_1, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_BPCI_0, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_BPCI_1, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CAM_IMG2O, ISP_M4U_TranslationFault_callback, NULL);
#if 0
	m4u_register_fault_callback(M4U_PORT_UFDI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_IMG3O, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_VIPI, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_VIP2I, ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_VIP3I, ISP_M4U_TranslationFault_callback, NULL);
#endif


#ifdef _MAGIC_NUM_ERR_HANDLING_
	LOG_DBG("init m_LastMNum");
	for (i = 0; i < _rt_dma_max_; i++)
		m_LastMNum[i] = 0;
#endif


	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/*******************************************************************************
*
********************************************************************************/
static void __exit ISP_Exit(void)
{
	int i;

	LOG_DBG("- E.");
	/*      */
	platform_driver_unregister(&IspDriver);
	/*      */
	/* Unregister ISP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP, NULL, NULL, NULL, NULL);
	/* Un-Register GCE callback */
	LOG_DBG("Un-register isp callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB(NULL, NULL);

	/*      */
	/* Un-Register M4U callback dump */
	LOG_DBG("Un-Register M4U callback dump");
	m4u_unregister_fault_callback(M4U_PORT_CAM_IMGI);


	/* unreserve the pages */
	for (i = 0; i < RT_BUF_TBL_NPAGES * PAGE_SIZE; i += PAGE_SIZE)
		SetPageReserved(virt_to_page(((unsigned long)pTbl_RTBuf) + i));

	/* free the     memory areas */
	kfree(pBuf_kmalloc);
	kfree(pLog_kmalloc);

	/*      */
}

/*******************************************************************************
*
********************************************************************************/
module_init(ISP_Init);
module_exit(ISP_Exit);
MODULE_DESCRIPTION("Camera ISP driver");
MODULE_AUTHOR("ME3");
MODULE_LICENSE("GPL");
