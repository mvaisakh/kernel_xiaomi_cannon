/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/gl_proc.c#2
*/

/*! \file   "gl_proc.c"
*    \brief  This file defines the interface which can interact with users in /proc fs.
*
*    Detail description.
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"
#include "gl_os.h"
#include "gl_kal.h"
#include "debug.h"
#include "wlan_lib.h"
#include "debug.h"
#include "wlan_oid.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define PROC_MCR_ACCESS                         "mcr"
#define PROC_ROOT_NAME							"wlan"

#if CFG_SUPPORT_DEBUG_FS
#define PROC_ROAM_PARAM							"roam_param"
#define PROC_COUNTRY							"country"
#endif
#define PROC_DRV_STATUS                         "status"
#define PROC_RX_STATISTICS                      "rx_statistics"
#define PROC_TX_STATISTICS                      "tx_statistics"
#define PROC_DBG_LEVEL_NAME                     "dbg_level"
#define PROC_DRIVER_CMD                         "driver"
#define PROC_CFG                                "cfg"
#define PROC_EFUSE_DUMP                         "efuse_dump"



#define PROC_MCR_ACCESS_MAX_USER_INPUT_LEN      20
#define PROC_RX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_TX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_DBG_LEVEL_MAX_USER_INPUT_LEN       20
#define PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN      30
#define PROC_UID_SHELL							2000
#define PROC_GID_WIFI							1010

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/


/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static P_GLUE_INFO_T g_prGlueInfo_proc;
static UINT_32 u4McrOffset;
static struct proc_dir_entry *gprProcRoot;
static UINT_8 aucDbModuleName[][PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN] = {
	"INIT", "HAL", "INTR", "REQ", "TX", "RX", "RFTEST", "EMU", "SW1", "SW2",
	"SW3", "SW4", "HEM", "AIS", "RLM", "MEM", "CNM", "RSN", "BSS", "SCN",
	"SAA", "AAA", "P2P", "QM", "SEC", "BOW", "WAPI", "ROAMING", "TDLS", "PF",
	"OID", "NIC"
};
/* This buffer could be overwrite by any proc commands */
static UINT_8 g_aucProcBuf[3000];

/* This u32 is only for DriverCmdRead/Write, should not be used by other function */
static UINT_32 g_u4NextDriverReadLen;
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static ssize_t procDbgLevelRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
		UINT_8 *temp = &g_aucProcBuf[0];
		UINT_32 u4CopySize = 0;
		UINT_16 i;
		UINT_16 u2ModuleNum = 0;
		UINT_32 u4BufMax = sizeof(g_aucProcBuf);
		INT_32 i4Pos = 0;

		/* if *f_ops>0, we should return 0 to make cat command exit */
		if (*f_pos > 0)
			return 0;

		i4Pos = scnprintf(temp, (sizeof(g_aucProcBuf) - i4Pos),
				"\nERROR|WARN|STATE|EVENT|TRACE|INFO|LOUD|TEMP\n"
				"bit0 |bit1|bit2 |bit3 |bit4 |bit5|bit6|bit7\n\n"
				"Debug Module\tIndex\tLevel\tDebug Module\tIndex\tLevel\n\n");

		u2ModuleNum = (sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0xfe;
		for (i = 0; i < u2ModuleNum; i += 2)
			i4Pos += scnprintf((temp + i4Pos), (u4BufMax - i4Pos),
				"DBG_%s_IDX\t(0x%02x):\t0x%02x\tDBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[i][0], i, aucDebugModule[i],
					&aucDbModuleName[i+1][0], i+1, aucDebugModule[i+1]);

		if ((sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0x1)
			i4Pos += scnprintf((temp + i4Pos), (u4BufMax - i4Pos),
				"DBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[u2ModuleNum][0], u2ModuleNum, aucDebugModule[u2ModuleNum]);

		u4CopySize = i4Pos;
		if (u4CopySize > count)
			u4CopySize = count;
		if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
			DBGLOG(INIT, ERROR, "copy to user failed\n");
			return -EFAULT;
		}

		*f_pos += u4CopySize;
		return (ssize_t)u4CopySize;
}

#if WLAN_INCLUDE_PROC
#if	CFG_SUPPORT_EASY_DEBUG

static void *procEfuseDump_start(struct seq_file *s, loff_t *pos)
{
	static unsigned long counter;

	if (*pos == 0)
		counter = *pos; /* read file init */

	if (counter >= EFUSE_ADDR_MAX)
		return NULL;
	return &counter;
}
static void *procEfuseDump_next(struct seq_file *s, void *v, loff_t *pos)
{
	unsigned long *tmp_v = (unsigned long *)v;

	(*tmp_v) += EFUSE_BLOCK_SIZE;

	if (*tmp_v >= EFUSE_ADDR_MAX)
		return NULL;
	return tmp_v;
}
static void procEfuseDump_stop(struct seq_file *s, void *v)
{
	/* nothing to do, we use a static value in start() */
}
static int procEfuseDump_show(struct seq_file *s, void *v)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4BufLen = 0;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 idx_addr, idx_value;
	PARAM_CUSTOM_ACCESS_EFUSE_T rAccessEfuseInfo = {};

	prGlueInfo = g_prGlueInfo_proc;

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	idx_addr = *(loff_t *)v;
	rAccessEfuseInfo.u4Address = (idx_addr / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;

	rStatus = kalIoctl(prGlueInfo,
		wlanoidQueryProcessAccessEfuseRead,
		&rAccessEfuseInfo,
		sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), TRUE, TRUE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		seq_printf(s, "efuse read fail (0x%03X)\n", rAccessEfuseInfo.u4Address);
		return 0;
	}

	for (idx_value = 0; idx_value < EFUSE_BLOCK_SIZE; idx_value++)
		seq_printf(s, "0x%03X=0x%02X\n"
					, rAccessEfuseInfo.u4Address+idx_value
					, prGlueInfo->prAdapter->aucEepromVaule[idx_value]);
	return 0;
#else
	seq_puts(s, "efuse ops is invalid\n");
	return -EPERM; /* return negative value to stop read process */
#endif
}
static int procEfuseDumpOpen(struct inode *inode, struct file *file)
{
	static const struct seq_operations procEfuseDump_ops = {
		.start = procEfuseDump_start,
		.next  = procEfuseDump_next,
		.stop  = procEfuseDump_stop,
		.show  = procEfuseDump_show
	};

	return seq_open(file, &procEfuseDump_ops);
}


static ssize_t procCfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &g_aucProcBuf[0];
	UINT_32 u4CopySize = 0;
	UINT_16 i;
	INT_32 i4Pos = 0;
	UINT_32 u4BufMax = sizeof(g_aucProcBuf);

#define BUFFER_RESERVE_BYTE 50

	P_GLUE_INFO_T prGlueInfo;

	P_WLAN_CFG_ENTRY_T prWlanCfgEntry;
	P_ADAPTER_T prAdapter;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(gPrDev));

	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "procCfgRead prGlueInfo is  NULL?\n");
		return -EFAULT;
	}

	prAdapter = prGlueInfo->prAdapter;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	i4Pos = scnprintf(temp, (sizeof(g_aucProcBuf) - i4Pos), "\nDUMP CONFIGURATION :\n"
		"<KEY|VALUE> OR <D:KEY|VALUE>\n"
		"'D': driver part current setting\n"
		"===================================\n");

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, 0);

		if ((!prWlanCfgEntry) || (prWlanCfgEntry->aucKey[0] == '\0'))
			break;

		i4Pos += scnprintf((temp + i4Pos), (u4BufMax - i4Pos),
					"%s|%s\n", prWlanCfgEntry->aucKey, prWlanCfgEntry->aucValue);

		if (i4Pos > (sizeof(g_aucProcBuf)-BUFFER_RESERVE_BYTE))
			break;
	}

	for (i = 0; i < WLAN_CFG_REC_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, 1);

		if ((!prWlanCfgEntry) || (prWlanCfgEntry->aucKey[0] == '\0'))
			break;

		i4Pos += scnprintf((temp + i4Pos), (u4BufMax - i4Pos),
					"D:%s|%s\n", prWlanCfgEntry->aucKey, prWlanCfgEntry->aucValue);

		if (i4Pos > (sizeof(g_aucProcBuf)-BUFFER_RESERVE_BYTE))
			break;
	}

	u4CopySize = i4Pos;
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;


}


static ssize_t procCfgWrite(struct file *file, const char __user *buffer,
		size_t count, loff_t *data)
{

	/*	UINT_32 u4DriverCmd, u4DriverValue;
	*UINT_8 *temp = &g_aucProcBuf[0];
	*/
	UINT_32 u4CopySize = sizeof(g_aucProcBuf);
	P_GLUE_INFO_T prGlueInfo;
	PUINT_8	pucTmp;
	INT_32 i4Pos = 0;
	/*	PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P; */


	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	if (u4CopySize >= (count+1))
		u4CopySize = count;
	else
		u4CopySize -= 1;

	pucTmp = g_aucProcBuf;
	i4Pos = scnprintf(pucTmp, sizeof(g_aucProcBuf), "%s ", "set_cfg");
	pucTmp += i4Pos;
	u4CopySize -= i4Pos;

	if (copy_from_user(pucTmp, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';


	prGlueInfo = g_prGlueInfo_proc;
	/* if g_u4NextDriverReadLen >0,
	 * the content for next DriverCmdRead will be in : g_aucProcBuf with length : g_u4NextDriverReadLen
	 */
	g_u4NextDriverReadLen = priv_driver_set_cfg(prGlueInfo->prDevHandler, g_aucProcBuf, sizeof(g_aucProcBuf));

	return count;

}

static ssize_t procDriverCmdRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	/* DriverCmd read should only be executed right after a DriverCmd write
	 * because content buffer 'g_aucProcBuf' is a global buffer for all proc command,
	 *  otherwise , the content could be overwrite by other proc command
	 */
	UINT_32 u4CopySize = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	if (g_u4NextDriverReadLen > 0) /* Detect content to show */
		u4CopySize = g_u4NextDriverReadLen;
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}
	g_u4NextDriverReadLen = 0;

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}



static ssize_t procDriverCmdWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{

/*	UINT_32 u4DriverCmd, u4DriverValue;
*	UINT_8 *temp = &g_aucProcBuf[0];
*/
	UINT_32 u4CopySize = sizeof(g_aucProcBuf);
	P_GLUE_INFO_T prGlueInfo;
/*	PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P; */


	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	if (u4CopySize >= (count+1))
		u4CopySize = count;
	else
		u4CopySize -= 1;

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';


	prGlueInfo = g_prGlueInfo_proc;
	/* if g_u4NextDriverReadLen >0,
	 * the content for next DriverCmdRead will be in : g_aucProcBuf with length : g_u4NextDriverReadLen
	 */
	g_u4NextDriverReadLen = priv_driver_cmds(prGlueInfo->prDevHandler, g_aucProcBuf, sizeof(g_aucProcBuf));

	return count;
}

#endif
#endif

static ssize_t procDbgLevelWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{
	UINT_32 u4NewDbgModule, u4NewDbgLevel;
	UINT_8 *temp = &g_aucProcBuf[0];
	UINT_32 u4CopySize = sizeof(g_aucProcBuf);

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	if (u4CopySize >= count+1)
		u4CopySize = count;
	else
		u4CopySize -= 1;

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	while (temp) {
		if (sscanf(temp, "0x%x:0x%x", &u4NewDbgModule, &u4NewDbgLevel) != 2)  {
			DBGLOG(INIT, ERROR, "debug module and debug level should be one byte in length\n");
			break;
		}
		if (u4NewDbgModule == 0xFF) {
			UINT_8 i = 0;

			for (; i < DBG_MODULE_NUM; i++)
				aucDebugModule[i] = u4NewDbgLevel & DBG_CLASS_MASK;

			break;
		}
		if (u4NewDbgModule >= DBG_MODULE_NUM) {
			DBGLOG(INIT, ERROR, "debug module index should less than %d\n", DBG_MODULE_NUM);
			break;
		}
		aucDebugModule[u4NewDbgModule] =  u4NewDbgLevel & DBG_CLASS_MASK;
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++; /* skip ',' */
	}
	return count;
}


static const struct file_operations dbglevel_ops = {
	.owner = THIS_MODULE,
	.read = procDbgLevelRead,
	.write = procDbgLevelWrite,
};

#if WLAN_INCLUDE_PROC
#if	CFG_SUPPORT_EASY_DEBUG

static const struct file_operations efusedump_ops = {
	.owner	 = THIS_MODULE,
	.open	 = procEfuseDumpOpen,
	.read	 = seq_read,
	.llseek  = seq_lseek,
	.release = seq_release,
};

static const struct file_operations drivercmd_ops = {
	.owner = THIS_MODULE,
	.read = procDriverCmdRead,
	.write = procDriverCmdWrite,
};

static const struct file_operations cfg_ops = {
	.owner = THIS_MODULE,
	.read = procCfgRead,
	.write = procCfgWrite,
};
#endif
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading MCR register to User Space, the offset of
*        the MCR is specified in u4McrOffset.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static ssize_t procMCRRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	P_GLUE_INFO_T prGlueInfo;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	UINT_32 u4BufLen;
	UINT_8 *temp = &g_aucProcBuf[0];
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	INT_32 i4Count = 0;

	/* Kevin: Apply PROC read method 1. */
	if (*f_pos > 0)
		return 0;	/* To indicate end of file. */

	prGlueInfo = g_prGlueInfo_proc;

	rMcrInfo.u4McrOffset = u4McrOffset;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryMcrRead, (PVOID)&rMcrInfo, sizeof(rMcrInfo), TRUE, TRUE, TRUE, &u4BufLen);
	kalMemZero(g_aucProcBuf, sizeof(g_aucProcBuf));

	i4Count = scnprintf(temp, sizeof(g_aucProcBuf),
				 "MCR (0x%08xh): 0x%08x\n", rMcrInfo.u4McrOffset, rMcrInfo.u4McrData);

	if (copy_to_user(buf, g_aucProcBuf, i4Count)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += i4Count;

	return i4Count;

}				/* end of procMCRRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for writing MCR register to HW or update u4McrOffset
*        for reading MCR later.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static ssize_t procMCRWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{
	P_GLUE_INFO_T prGlueInfo;
	char acBuf[PROC_MCR_ACCESS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	int i4CopySize;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	UINT_32 u4BufLen;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	int num = 0;

	ASSERT(data);

	i4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, buffer, i4CopySize))
		return 0;
	acBuf[i4CopySize] = '\0';

	num = sscanf(acBuf, "0x%x 0x%x", &rMcrInfo.u4McrOffset, &rMcrInfo.u4McrData);
	switch (num) {
	case 2:
		/* NOTE: Sometimes we want to test if bus will still be ok, after accessing
		 * the MCR which is not align to DW boundary.
		 */
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		{
			prGlueInfo = (P_GLUE_INFO_T) netdev_priv((struct net_device *)data);

			u4McrOffset = rMcrInfo.u4McrOffset;

			/* printk("Write 0x%lx to MCR 0x%04lx\n", */
			/* rMcrInfo.u4McrOffset, rMcrInfo.u4McrData); */

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetMcrWrite,
					   (PVOID)&rMcrInfo, sizeof(rMcrInfo), FALSE, FALSE, TRUE, &u4BufLen);

		}
		break;
	case 1:
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		{
			u4McrOffset = rMcrInfo.u4McrOffset;
		}
		break;

	default:
		break;
	}

	return count;

}				/* end of procMCRWrite() */

static const struct file_operations mcr_ops = {
	.owner = THIS_MODULE,
	.read = procMCRRead,
	.write = procMCRWrite,
};

#if CFG_SUPPORT_DEBUG_FS
static ssize_t procRoamRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	/* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidGetRoamParams, g_aucProcBuf, sizeof(g_aucProcBuf),
							TRUE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to read roam params\n");
		return -EINVAL;
	}

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t procRoamWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen = 0;
	UINT_32 u4CopySize = sizeof(g_aucProcBuf);

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	if (u4CopySize >= count+1)
		u4CopySize = count;
	else
		u4CopySize -= 1;

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	if (kalStrnCmp(g_aucProcBuf, "force_roam", 10) == 0)
		rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetForceRoam, NULL, 0,
						FALSE, FALSE, TRUE, &u4BufLen);
	else
		rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetRoamParams, g_aucProcBuf,
					kalStrLen(g_aucProcBuf), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to set roam params: %s\n", g_aucProcBuf);
		return -EINVAL;
	}
	return count;
}

static const struct file_operations roam_ops = {
	.owner = THIS_MODULE,
	.read = procRoamRead,
	.write = procRoamWrite,
};

static ssize_t procCountryRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	UINT_16 u2CountryCode = 0;
	UINT_32 u4BufLen;
	WLAN_STATUS rStatus;

	/* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidGetCountryCode,
			&u2CountryCode, 2, TRUE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to get country code\n");
		return -EINVAL;
	}
	if (u2CountryCode)
		kalSprintf(g_aucProcBuf, "Current Country Code: %c%c\n",
				(u2CountryCode>>8) & 0xff, u2CountryCode & 0xff);
	else
		kalStrCpy(g_aucProcBuf, "Current Country Code: NULL\n");

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t procCountryWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{
	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus;
	UINT_32 u4CopySize = sizeof(g_aucProcBuf);

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	if (u4CopySize >= count+1)
		u4CopySize = count;
	else
		u4CopySize -= 1;

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetCountryCode,
			&g_aucProcBuf[0], 2, FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed set country code: %s\n", g_aucProcBuf);
		return -EINVAL;
	}
	return count;
}

static const struct file_operations country_ops = {
	.owner = THIS_MODULE,
	.read = procCountryRead,
	.write = procCountryWrite,
};
#endif

INT_32 procInitFs(VOID)
{

	g_u4NextDriverReadLen = 0;

	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		DBGLOG(INIT, ERROR, "init proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}

	/*
	 * Directory: Root (/proc/net/wlan0)
	 */

	gprProcRoot = proc_mkdir(PROC_ROOT_NAME, init_net.proc_net);
	if (!gprProcRoot) {
		DBGLOG(INIT, ERROR, "gprProcRoot == NULL\n");
		return -ENOENT;
	}
	proc_set_user(gprProcRoot, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));


	return 0;
}				/* end of procInitProcfs() */

INT_32 procUninitProcFs(VOID)
{
	remove_proc_subtree(PROC_ROOT_NAME, init_net.proc_net);
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clean up a PROC fs created by procInitProcfs().
*
* \param[in] prDev      Pointer to the struct net_device.
* \param[in] pucDevName Pointer to the name of net_device.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
INT_32 procRemoveProcfs(VOID)
{
	remove_proc_entry(PROC_MCR_ACCESS, gprProcRoot);
	remove_proc_entry(PROC_DRIVER_CMD, gprProcRoot);
	remove_proc_entry(PROC_DBG_LEVEL_NAME, gprProcRoot);
	remove_proc_entry(PROC_CFG, gprProcRoot);
	remove_proc_entry(PROC_EFUSE_DUMP, gprProcRoot);

#if CFG_SUPPORT_DEBUG_FS
	remove_proc_entry(PROC_ROAM_PARAM, gprProcRoot);
	remove_proc_entry(PROC_COUNTRY, gprProcRoot);

#endif
	return 0;
} /* end of procRemoveProcfs() */

INT_32 procCreateFsEntry(P_GLUE_INFO_T prGlueInfo)
{
	struct proc_dir_entry *prEntry;

	DBGLOG(INIT, INFO, "[%s]\n", __func__);
	g_prGlueInfo_proc = prGlueInfo;

	prEntry = proc_create(PROC_MCR_ACCESS, 0664, gprProcRoot, &mcr_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
#if CFG_SUPPORT_DEBUG_FS
	prEntry = proc_create(PROC_ROAM_PARAM, 0664, gprProcRoot, &roam_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
	prEntry = proc_create(PROC_COUNTRY, 0664, gprProcRoot, &country_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
#endif
#if	CFG_SUPPORT_EASY_DEBUG

	prEntry = proc_create(PROC_DRIVER_CMD, 0664, gprProcRoot, &drivercmd_ops);
		if (prEntry == NULL) {
			DBGLOG(INIT, ERROR, "Unable to create /proc entry for driver command\n\r");
			return -1;
		}

	prEntry = proc_create(PROC_CFG, 0664, gprProcRoot, &cfg_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry for driver command\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_EFUSE_DUMP, 0664, gprProcRoot, &efusedump_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry efuse\n\r");
		return -1;
	}
#endif

	prEntry = proc_create(PROC_DBG_LEVEL_NAME, 0664, gprProcRoot, &dbglevel_ops);
		if (prEntry == NULL) {
			DBGLOG(INIT, ERROR, "Unable to create /proc entry dbgLevel\n\r");
			return -1;
		}

	return 0;
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver Status to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procDrvStatusRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("GLUE LAYER STATUS:"));
	SPRINTF(p, ("\n=================="));

	SPRINTF(p, ("\n* Number of Pending Frames: %ld\n", prGlueInfo->u4TxPendingFrameNum));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryDrvStatusForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procDrvStatusRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver RX Statistic Counters to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procRxStatisticsRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("RX STATISTICS (Write 1 to clear):"));
	SPRINTF(p, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryRxStatisticsForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procRxStatisticsRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reset Driver RX Statistic Counters.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procRxStatisticsWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	UINT_32 u4ClearCounter;
	INT_32 rv;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	rv = kstrtoint(acBuf, 0, &u4ClearCounter);
	if (rv == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetRxStatisticsForLinuxProc(prGlueInfo->prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

}				/* end of procRxStatisticsWrite() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver TX Statistic Counters to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procTxStatisticsRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("TX STATISTICS (Write 1 to clear):"));
	SPRINTF(p, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryTxStatisticsForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procTxStatisticsRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reset Driver TX Statistic Counters.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procTxStatisticsWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	UINT_32 u4ClearCounter;
	INT_32 rv;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	rv = kstrtoint(acBuf, 0, &u4ClearCounter);
	if (rv == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetTxStatisticsForLinuxProc(prGlueInfo->prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

}				/* end of procTxStatisticsWrite() */
#endif


