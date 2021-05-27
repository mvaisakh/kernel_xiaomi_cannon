/*************************************************************************/ /*!
@File
@Title          Rogue firmware utility routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Rogue firmware utility routines
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if defined(LINUX)
#include <linux/stddef.h>
#else
#include <stddef.h>
#endif

#include "img_defs.h"

#include "rgxdefs_km.h"
#include "rgx_fwif_km.h"
#include "pdump_km.h"
#include "osfunc.h"
#include "oskm_apphint.h"
#include "cache_km.h"
#include "allocmem.h"
#include "physheap.h"
#include "devicemem.h"
#include "devicemem_pdump.h"
#include "devicemem_server.h"

#include "pvr_debug.h"
#include "pvr_notifier.h"
#include "rgxfwutils.h"
#include "rgx_options.h"
#include "rgx_fwif_alignchecks.h"
#include "rgx_fwif_resetframework.h"
#include "rgx_pdump_panics.h"
#include "fwtrace_string.h"
#include "rgxheapconfig.h"
#include "pvrsrv.h"
#include "rgxdebug.h"
#include "rgxhwperf.h"
#include "rgxccb.h"
#include "rgxcompute.h"
#include "rgxtransfer.h"
#include "rgxpower.h"
#include "rgxtdmtransfer.h"
#if defined(SUPPORT_DISPLAY_CLASS)
#include "dc_server.h"
#endif
#include "rgxmem.h"
#include "rgxta3d.h"
#include "rgxkicksync.h"
#include "rgxutils.h"
#include "rgxtimecorr.h"
#include "sync_internal.h"
#include "sync.h"
#include "sync_checkpoint.h"
#include "sync_checkpoint_external.h"
#include "tlstream.h"
#include "devicemem_server_utils.h"
#include "htbuffer.h"
#include "rgx_bvnc_defs_km.h"
#include "info_page.h"

#include "physmem_lma.h"
#include "physmem_osmem.h"

#ifdef __linux__
#include <linux/kernel.h>	/* sprintf */
#include "rogue_trace_events.h"
#else
#include <stdio.h>
#include <string.h>
#endif
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
#include "process_stats.h"
#endif

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
#include "rgxworkest.h"
#endif

#if defined(SUPPORT_PDVFS)
#include "rgxpdvfs.h"
#endif

#if defined(PVRSRV_SYNC_CHECKPOINT_CCB)
#if defined(SUPPORT_BUFFER_SYNC)
#include "pvr_buffer_sync.h"
#endif
#endif

#include "vz_vmm_pvz.h"
#include "rgx_heaps.h"

#if defined(MTK_GPU_BM_SUPPORT)
#include <gpu_bm.h>
#endif

/*!
 ******************************************************************************
 * HWPERF
 *****************************************************************************/
/* Size of the Firmware L1 HWPERF buffer in bytes (2MB). Accessed by the
 * Firmware and host driver. */
#define RGXFW_HWPERF_L1_SIZE_MIN        (16U)
#define RGXFW_HWPERF_L1_SIZE_DEFAULT    PVRSRV_APPHINT_HWPERFFWBUFSIZEINKB
#define RGXFW_HWPERF_L1_SIZE_MAX        (12288U)

/* Kernel CCB length */
/* Reducing the size of the KCCB in an attempt to avoid flooding and overflowing the FW kick queue
 * in the case of multiple OSes */
#define RGXFWIF_KCCB_NUMCMDS_LOG2_GPUVIRT_WITHOUT_FEATURE  (6)
#define RGXFWIF_KCCB_NUMCMDS_LOG2_DEFAULT                  (7)


/* Firmware CCB length */
#if defined(NO_HARDWARE) && defined(PDUMP)
#define RGXFWIF_FWCCB_NUMCMDS_LOG2   (10)
#elif defined(SUPPORT_PDVFS)
#define RGXFWIF_FWCCB_NUMCMDS_LOG2   (8)
#else
#define RGXFWIF_FWCCB_NUMCMDS_LOG2   (5)
#endif

#if defined(RGX_FW_IRQ_OS_COUNTERS)
const IMG_UINT32 gaui32FwOsIrqCntRegAddr[RGXFW_MAX_NUM_OS] = {IRQ_COUNTER_STORAGE_REGS};
#endif

#if defined(PVRSRV_SYNC_CHECKPOINT_CCB)
/* Checkpoint CCB length */
#define RGXFWIF_CHECKPOINTCCB_NUMCMDS_LOG2 (10)
#endif

/*
 * Maximum length of time a DM can run for before the DM will be marked
 * as out-of-time. CDM has an increased value due to longer running kernels.
 *
 * These deadlines are increased on FPGA, EMU and VP due to the slower
 * execution time of these platforms.
 */
#if defined(FPGA) || defined(EMULATOR) || defined(VIRTUAL_PLATFORM)
#define RGXFWIF_MAX_WORKLOAD_DEADLINE_MS     (60000)
#define RGXFWIF_MAX_CDM_WORKLOAD_DEADLINE_MS (90000)
#else
#define RGXFWIF_MAX_WORKLOAD_DEADLINE_MS     (30000)
#define RGXFWIF_MAX_CDM_WORKLOAD_DEADLINE_MS (90000)
#endif

/* Workload Estimation Firmware CCB length */
#define RGXFWIF_WORKEST_FWCCB_NUMCMDS_LOG2   (7)

/* Size of memory buffer for firmware gcov data
 * The actual data size is several hundred kilobytes. The buffer is an order of magnitude larger. */
#define RGXFWIF_FIRMWARE_GCOV_BUFFER_SIZE (4*1024*1024)

#if defined(CONFIG_L4) && !defined(RGX_FEATURE_GPU_VIRTUALISATION) && defined(RGX_NUM_OS_SUPPORTED) && (RGX_NUM_OS_SUPPORTED > 1)
#define MTS_SCHEDULE_DM_VAL (RGXFWIF_DM_GP + PVRSRV_VZ_MODE_IS(GUEST) ? (1) : (0))
#else
#define MTS_SCHEDULE_DM_VAL (RGXFWIF_DM_GP)
#endif

typedef struct
{
	RGXFWIF_KCCB_CMD        sKCCBcmd;
	DLLIST_NODE             sListNode;
	PDUMP_FLAGS_T           uiPdumpFlags;
	PVRSRV_RGXDEV_INFO      *psDevInfo;
} RGX_DEFERRED_KCCB_CMD;

#if defined(PDUMP)
/* ensure PIDs are 32-bit because a 32-bit PDump load is generated for the
 * PID filter example entries
 */
static_assert(sizeof(IMG_PID) == sizeof(IMG_UINT32),
		"FW PID filtering assumes the IMG_PID type is 32-bits wide as it "
		"generates WRW commands for loading the PID values");
#endif

static void RGXFreeFwOsData(PVRSRV_RGXDEV_INFO *psDevInfo);
static void RGXFreeFwSysData(PVRSRV_RGXDEV_INFO *psDevInfo);

static PVRSRV_ERROR _AllocateSLC3Fence(PVRSRV_RGXDEV_INFO* psDevInfo, RGXFWIF_SYSINIT* psFwSysInit)
{
	PVRSRV_ERROR eError;
	DEVMEM_MEMDESC** ppsSLC3FenceMemDesc = &psDevInfo->psSLC3FenceMemDesc;
	IMG_UINT32 ui32CacheLineSize = GET_ROGUE_CACHE_LINE_SIZE(
			RGX_GET_FEATURE_VALUE(psDevInfo, SLC_CACHE_LINE_SIZE_BITS));

	PVR_DPF_ENTERED;

	eError = DevmemAllocate(psDevInfo->psFirmwareMainHeap,
			1,
			ui32CacheLineSize,
			PVRSRV_MEMALLOCFLAG_GPU_READABLE |
			PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE |
			PVRSRV_MEMALLOCFLAG_UNCACHED |
			PVRSRV_MEMALLOCFLAG_FW_ALLOC_MAIN,
			"FwSLC3FenceWA",
			ppsSLC3FenceMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF_RETURN_RC(eError);
	}

	/* We need to map it so the heap for this allocation is set */
	eError = DevmemMapToDevice(*ppsSLC3FenceMemDesc,
			psDevInfo->psFirmwareMainHeap,
			&psFwSysInit->sSLC3FenceDevVAddr);
	if (eError != PVRSRV_OK)
	{
		DevmemFree(*ppsSLC3FenceMemDesc);
		*ppsSLC3FenceMemDesc = NULL;
	}

	PVR_DPF_RETURN_RC1(eError, *ppsSLC3FenceMemDesc);
}

static void _FreeSLC3Fence(PVRSRV_RGXDEV_INFO* psDevInfo)
{
	DEVMEM_MEMDESC* psSLC3FenceMemDesc = psDevInfo->psSLC3FenceMemDesc;

	if (psSLC3FenceMemDesc)
	{
		DevmemReleaseDevVirtAddr(psSLC3FenceMemDesc);
		DevmemFree(psSLC3FenceMemDesc);
	}
}

static void __MTSScheduleWrite(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_UINT32 ui32Value)
{
	/* ensure memory is flushed before kicking MTS */
	OSWriteMemoryBarrier();

	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_MTS_SCHEDULE, ui32Value);

	/* ensure the MTS kick goes through before continuing */
	OSMemoryBarrier();
}

/*************************************************************************/ /*!
@Function       RGXSetupFwAllocation

@Description    Sets a pointer in a firmware data structure.

@Input          psDevInfo       Device Info struct
@Input          uiAllocFlags    Flags determining type of memory allocation
@Input          ui32Size        Size of memory allocation
@Input          pszName         Allocation label
@Input          ppsMemDesc      pointer to the allocation's memory descriptor
@Input          psFwPtr         Address of the firmware pointer to set
@Input          ppvCpuPtr       Address of the cpu pointer to set
@Input          ui32DevVAFlags  Any combination of  RFW_FWADDR_*_FLAG

@Return         PVRSRV_ERROR
*/ /**************************************************************************/
PVRSRV_ERROR RGXSetupFwAllocation(PVRSRV_RGXDEV_INFO*  psDevInfo,
								  DEVMEM_FLAGS_T       uiAllocFlags,
								  IMG_UINT32           ui32Size,
								  const IMG_CHAR       *pszName,
								  DEVMEM_MEMDESC       **ppsMemDesc,
								  RGXFWIF_DEV_VIRTADDR *psFwPtr,
								  void                 **ppvCpuPtr,
								  IMG_UINT32           ui32DevVAFlags)
{
	PVRSRV_ERROR eError;
#if defined(SUPPORT_AUTOVZ)
	IMG_BOOL bClearByMemset = ((uiAllocFlags & PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC) != 0);

	/* Under AutoVz the ZERO_ON_ALLOC flag is avoided as it causes the memory to
	 * be allocated from a different PMR than an allocation without the flag.
	 * When the content of an allocation needs to be recovered from physical memory
	 * on a later driver reboot, the memory then cannot be zeroed but the allocation
	 * addresses must still match.
	 * If the memory requires clearing, perform a memset after the allocation. */
	uiAllocFlags &= ~PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC;
#endif

	PDUMPCOMMENT("Allocate %s", pszName);
	eError = DevmemFwAllocate(psDevInfo,
							  ui32Size,
							  uiAllocFlags,
							  pszName,
							  ppsMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				 "%s: Failed to allocate %u bytes for %s (%u)",
				 __func__,
				 ui32Size,
				 pszName,
				 eError));
		goto fail_alloc;
	}

	if (psFwPtr)
	{
		eError = RGXSetFirmwareAddress(psFwPtr, *ppsMemDesc, 0, ui32DevVAFlags);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					 "%s: Failed to acquire firmware virtual address for %s (%u)",
					 __func__,
					 pszName,
					 eError));
			goto fail_fwaddr;
		}
	}

#if defined(SUPPORT_AUTOVZ)
	if ((bClearByMemset) || (ppvCpuPtr))
#else
	if (ppvCpuPtr)
#endif
	{
		void *pvTempCpuPtr;

		eError = DevmemAcquireCpuVirtAddr(*ppsMemDesc, &pvTempCpuPtr);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: Failed to acquire CPU virtual address for %s (%u)",
					__func__,
					 pszName,
					eError));
			goto fail_cpuva;
		}

#if defined(SUPPORT_AUTOVZ)
		if (bClearByMemset)
		{
			OSDeviceMemSet(pvTempCpuPtr, 0, ui32Size);
		}
#endif
		if (ppvCpuPtr)
		{
			*ppvCpuPtr = pvTempCpuPtr;
		}
		else
		{
			DevmemReleaseCpuVirtAddr(*ppsMemDesc);
			pvTempCpuPtr = NULL;
		}
	}

	PVR_DPF((PVR_DBG_MESSAGE, "%s: %s set up at Fw VA 0x%x and CPU VA 0x%p",
			 __func__, pszName,
			 (psFwPtr)   ? (psFwPtr->ui32Addr) : (0),
			 (ppvCpuPtr) ? (*ppvCpuPtr)        : (NULL)));

	return eError;

fail_cpuva:
	if (psFwPtr)
	{
		RGXUnsetFirmwareAddress(*ppsMemDesc);
	}
fail_fwaddr:
	DevmemFree(*ppsMemDesc);
fail_alloc:
	return eError;
}

/*************************************************************************/ /*!
@Function       GetHwPerfBufferSize

@Description    Computes the effective size of the HW Perf Buffer
@Input          ui32HWPerfFWBufSizeKB       Device Info struct
@Return         HwPerfBufferSize
*/ /**************************************************************************/
static IMG_UINT32 GetHwPerfBufferSize(IMG_UINT32 ui32HWPerfFWBufSizeKB)
{
	IMG_UINT32 HwPerfBufferSize;

	/* HWPerf: Determine the size of the FW buffer */
	if (ui32HWPerfFWBufSizeKB == 0 ||
			ui32HWPerfFWBufSizeKB == RGXFW_HWPERF_L1_SIZE_DEFAULT)
	{
		/* Under pvrsrvctl 0 size implies AppHint not set or is set to zero,
		 * use default size from driver constant. Set it to the default
		 * size, no logging.
		 */
		HwPerfBufferSize = RGXFW_HWPERF_L1_SIZE_DEFAULT<<10;
	}
	else if (ui32HWPerfFWBufSizeKB > (RGXFW_HWPERF_L1_SIZE_MAX))
	{
		/* Size specified as a AppHint but it is too big */
		PVR_DPF((PVR_DBG_WARNING,
				"%s: HWPerfFWBufSizeInKB value (%u) too big, using maximum (%u)",
				__func__,
				ui32HWPerfFWBufSizeKB, RGXFW_HWPERF_L1_SIZE_MAX));
		HwPerfBufferSize = RGXFW_HWPERF_L1_SIZE_MAX<<10;
	}
	else if (ui32HWPerfFWBufSizeKB > (RGXFW_HWPERF_L1_SIZE_MIN))
	{
		/* Size specified as in AppHint HWPerfFWBufSizeInKB */
		PVR_DPF((PVR_DBG_WARNING,
				"%s: Using HWPerf FW buffer size of %u KB",
				__func__,
				ui32HWPerfFWBufSizeKB));
		HwPerfBufferSize = ui32HWPerfFWBufSizeKB<<10;
	}
	else
	{
		/* Size specified as a AppHint but it is too small */
		PVR_DPF((PVR_DBG_WARNING,
				"%s: HWPerfFWBufSizeInKB value (%u) too small, using minimum (%u)",
				__func__,
				ui32HWPerfFWBufSizeKB, RGXFW_HWPERF_L1_SIZE_MIN));
		HwPerfBufferSize = RGXFW_HWPERF_L1_SIZE_MIN<<10;
	}

	return HwPerfBufferSize;
}

#if defined(PDUMP)
/*!
*******************************************************************************
 @Function		RGXFWSetupSignatureChecks
 @Description
 @Input			psDevInfo

 @Return		PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXFWSetupSignatureChecks(PVRSRV_RGXDEV_INFO* psDevInfo,
		DEVMEM_MEMDESC**    ppsSigChecksMemDesc,
		IMG_UINT32          ui32SigChecksBufSize,
		RGXFWIF_SIGBUF_CTL* psSigBufCtl,
		const IMG_CHAR*     pszBufferName)
{
	PVRSRV_ERROR	eError;

	/* Allocate memory for the checks */
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS,
								  ui32SigChecksBufSize,
								  "FwSignatureChecks",
								  ppsSigChecksMemDesc,
								  &psSigBufCtl->sBuffer,
								  NULL,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	DevmemPDumpLoadMem(	*ppsSigChecksMemDesc,
			0,
			ui32SigChecksBufSize,
			PDUMP_FLAGS_CONTINUOUS);

	psSigBufCtl->ui32LeftSizeInRegs = ui32SigChecksBufSize / sizeof(IMG_UINT32);
fail:
	return eError;
}
#endif


#if defined(SUPPORT_FIRMWARE_GCOV)
/*!
*******************************************************************************
 @Function		RGXFWSetupFirmwareGcovBuffer
 @Description
 @Input			psDevInfo

 @Return		PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXFWSetupFirmwareGcovBuffer(PVRSRV_RGXDEV_INFO*			psDevInfo,
		DEVMEM_MEMDESC**			ppsBufferMemDesc,
		IMG_UINT32					ui32FirmwareGcovBufferSize,
		RGXFWIF_FIRMWARE_GCOV_CTL*	psFirmwareGcovCtl,
		const IMG_CHAR*				pszBufferName)
{
	PVRSRV_ERROR	eError;

	/* Allocate memory for gcov */
	eError = RGXSetupFwAllocation(psDevInfo,
								  (RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS |
								   PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED)),
								  ui32FirmwareGcovBufferSize,
								  pszBufferName,
								  ppsBufferMemDesc,
								  &psFirmwareGcovCtl->sBuffer,
								  NULL,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	psFirmwareGcovCtl->ui32Size = ui32FirmwareGcovBufferSize;

	return PVRSRV_OK;
}
#endif

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
/*!
 ******************************************************************************
 @Function		RGXFWSetupCounterBuffer
 @Description
 @Input			psDevInfo

 @Return		PVRSRV_ERROR
 *****************************************************************************/
static PVRSRV_ERROR RGXFWSetupCounterBuffer(PVRSRV_RGXDEV_INFO* psDevInfo,
		DEVMEM_MEMDESC**			ppsBufferMemDesc,
		IMG_UINT32					ui32CounterDataBufferSize,
		RGXFWIF_COUNTER_DUMP_CTL*	psCounterDumpCtl,
		const IMG_CHAR*				pszBufferName)
{
	PVRSRV_ERROR	eError;

	eError = RGXSetupFwAllocation(psDevInfo,
								  (RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS |
								   PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED)),
								  ui32CounterDataBufferSize,
								  "FwCounterBuffer",
								  ppsBufferMemDesc,
								  &psCounterDumpCtl->sBuffer,
								  NULL,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_RETURN_IF_ERROR(eError, "RGXSetupFwAllocation");

	psCounterDumpCtl->ui32SizeInDwords = ui32CounterDataBufferSize >> 2;

	return PVRSRV_OK;
}
#endif

/*!
 ******************************************************************************
 @Function      RGXFWSetupAlignChecks
 @Description   This functions allocates and fills memory needed for the
                aligns checks of the UM and KM structures shared with the
                firmware. The format of the data in the memory is as follows:
                    <number of elements in the KM array>
                    <array of KM structures' sizes and members' offsets>
                    <number of elements in the UM array>
                    <array of UM structures' sizes and members' offsets>
                The UM array is passed from the user side. Now the firmware is
                is responsible for filling this part of the memory. If that
                happens the check of the UM structures will be performed
                by the host driver on client's connect.
                If the macro is not defined the client driver fills the memory
                and the firmware checks for the alignment of all structures.
 @Input			psDeviceNode

 @Return		PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXFWSetupAlignChecks(PVRSRV_DEVICE_NODE *psDeviceNode,
								RGXFWIF_DEV_VIRTADDR	*psAlignChecksDevFW,
								IMG_UINT32				*pui32RGXFWAlignChecks,
								IMG_UINT32				ui32RGXFWAlignChecksArrLength)
{
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	IMG_UINT32			aui32RGXFWAlignChecksKM[] = { RGXFW_ALIGN_CHECKS_INIT_KM };
	IMG_UINT32			ui32RGXFWAlingChecksTotal;
	IMG_UINT32*			paui32AlignChecks;
	PVRSRV_ERROR		eError;

	/* In this case we don't know the number of elements in UM array.
	 * We have to assume something so we assume RGXFW_ALIGN_CHECKS_UM_MAX.
	 */
	PVR_ASSERT(ui32RGXFWAlignChecksArrLength == 0);
	ui32RGXFWAlingChecksTotal = sizeof(aui32RGXFWAlignChecksKM)
	                            + RGXFW_ALIGN_CHECKS_UM_MAX * sizeof(IMG_UINT32)
	                            + 2 * sizeof(IMG_UINT32);

	/* Allocate memory for the checks */
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  ui32RGXFWAlingChecksTotal,
								  "FwAlignmentChecks",
								  &psDevInfo->psRGXFWAlignChecksMemDesc,
								  psAlignChecksDevFW,
								  (void**) &paui32AlignChecks,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	if (!psDeviceNode->bAutoVzFwIsUp)
	{
		/* Copy the values */
		*paui32AlignChecks++ = ARRAY_SIZE(aui32RGXFWAlignChecksKM);
		OSDeviceMemCopy(paui32AlignChecks, &aui32RGXFWAlignChecksKM[0], sizeof(aui32RGXFWAlignChecksKM));
		paui32AlignChecks += ARRAY_SIZE(aui32RGXFWAlignChecksKM);

		*paui32AlignChecks = 0;
	}

	DevmemPDumpLoadMem(	psDevInfo->psRGXFWAlignChecksMemDesc,
						0,
						ui32RGXFWAlingChecksTotal,
						PDUMP_FLAGS_CONTINUOUS);

	return PVRSRV_OK;

fail:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

static void RGXFWFreeAlignChecks(PVRSRV_RGXDEV_INFO* psDevInfo)
{
	if (psDevInfo->psRGXFWAlignChecksMemDesc != NULL)
	{
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWAlignChecksMemDesc);
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWAlignChecksMemDesc);
		psDevInfo->psRGXFWAlignChecksMemDesc = NULL;
	}
}

PVRSRV_ERROR RGXSetFirmwareAddress(RGXFWIF_DEV_VIRTADDR	*ppDest,
						   DEVMEM_MEMDESC		*psSrc,
						   IMG_UINT32			uiExtraOffset,
						   IMG_UINT32			ui32Flags)
{
	PVRSRV_ERROR		eError;
	IMG_DEV_VIRTADDR	psDevVirtAddr;
	PVRSRV_DEVICE_NODE	*psDeviceNode;
	PVRSRV_RGXDEV_INFO	*psDevInfo;

	psDeviceNode = (PVRSRV_DEVICE_NODE *) DevmemGetConnection(psSrc);
	psDevInfo = (PVRSRV_RGXDEV_INFO *)psDeviceNode->pvDevice;

	if (RGX_IS_FEATURE_VALUE_SUPPORTED(psDevInfo, META))
	{
		IMG_UINT32          ui32Offset;
		IMG_BOOL            bCachedInMETA;
		DEVMEM_FLAGS_T      uiDevFlags;
		IMG_UINT32          uiGPUCacheMode;

		eError = DevmemAcquireDevVirtAddr(psSrc, &psDevVirtAddr);
		PVR_LOG_GOTO_IF_ERROR(eError, "DevmemAcquireDevVirtAddr", failDevVAAcquire);

		/* Convert to an address in META memmap */
		ui32Offset = psDevVirtAddr.uiAddr + uiExtraOffset - RGX_FIRMWARE_RAW_HEAP_BASE;

		/* Check in the devmem flags whether this memory is cached/uncached */
		DevmemGetFlags(psSrc, &uiDevFlags);

		/* Honour the META cache flags */
		bCachedInMETA = (PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) & uiDevFlags) != 0;

		/* Honour the SLC cache flags */
		eError = DevmemDeviceCacheMode(psDeviceNode, uiDevFlags, &uiGPUCacheMode);
		PVR_LOG_GOTO_IF_ERROR(eError, "DevmemDeviceCacheMode", failDevCacheMode);

		ui32Offset += RGXFW_SEGMMU_DATA_BASE_ADDRESS;

		if (bCachedInMETA)
		{
			ui32Offset |= RGXFW_SEGMMU_DATA_META_CACHED;
		}
		else
		{
			ui32Offset |= RGXFW_SEGMMU_DATA_META_UNCACHED;
		}

		if (PVRSRV_CHECK_GPU_CACHED(uiGPUCacheMode))
		{
			ui32Offset |= RGXFW_SEGMMU_DATA_VIVT_SLC_CACHED;
		}
		else
		{
			ui32Offset |= RGXFW_SEGMMU_DATA_VIVT_SLC_UNCACHED;
		}
		ppDest->ui32Addr = ui32Offset;
	}
	else if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, MIPS))
	{
		eError = DevmemAcquireDevVirtAddr(psSrc, &psDevVirtAddr);
		PVR_GOTO_IF_ERROR(eError, failDevVAAcquire);

		ppDest->ui32Addr = (IMG_UINT32)((psDevVirtAddr.uiAddr + uiExtraOffset) & 0xFFFFFFFF);
	}
	else
	{
		IMG_UINT32      ui32Offset;
		IMG_BOOL        bCachedInRISCV;
		DEVMEM_FLAGS_T  uiDevFlags;

		eError = DevmemAcquireDevVirtAddr(psSrc, &psDevVirtAddr);
		PVR_LOG_GOTO_IF_ERROR(eError, "DevmemAcquireDevVirtAddr", failDevVAAcquire);

		/* Convert to an address in RISCV memmap */
		ui32Offset = psDevVirtAddr.uiAddr + uiExtraOffset - RGX_FIRMWARE_RAW_HEAP_BASE;

		/* Check in the devmem flags whether this memory is cached/uncached */
		DevmemGetFlags(psSrc, &uiDevFlags);

		/* Honour the RISCV cache flags */
		bCachedInRISCV = (PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) & uiDevFlags) != 0;

		if (bCachedInRISCV)
		{
			ui32Offset |= RGXRISCVFW_SHARED_CACHED_DATA_BASE;
		}
		else
		{
			ui32Offset |= RGXRISCVFW_SHARED_UNCACHED_DATA_BASE;
		}

		ppDest->ui32Addr = ui32Offset;
	}

	if ((ppDest->ui32Addr & 0x3U) != 0)
	{
		IMG_CHAR *pszAnnotation;
		/* It is expected that the annotation returned by DevmemGetAnnotation() is always valid */
		DevmemGetAnnotation(psSrc, &pszAnnotation);

		PVR_DPF((PVR_DBG_ERROR, "%s: %s @ 0x%x is not aligned to 32 bit",
				 __func__, pszAnnotation, ppDest->ui32Addr));

		return PVRSRV_ERROR_INVALID_ALIGNMENT;
	}

	if (ui32Flags & RFW_FWADDR_NOREF_FLAG)
	{
		DevmemReleaseDevVirtAddr(psSrc);
	}

	return PVRSRV_OK;

failDevCacheMode:
	DevmemReleaseDevVirtAddr(psSrc);
failDevVAAcquire:
	return eError;
}

void RGXSetMetaDMAAddress(RGXFWIF_DMA_ADDR		*psDest,
		DEVMEM_MEMDESC		*psSrcMemDesc,
		RGXFWIF_DEV_VIRTADDR	*psSrcFWDevVAddr,
		IMG_UINT32			uiOffset)
{
	PVRSRV_ERROR		eError;
	IMG_DEV_VIRTADDR	sDevVirtAddr;

	eError = DevmemAcquireDevVirtAddr(psSrcMemDesc, &sDevVirtAddr);
	PVR_ASSERT(eError == PVRSRV_OK);

	psDest->psDevVirtAddr.uiAddr = sDevVirtAddr.uiAddr;
	psDest->psDevVirtAddr.uiAddr += uiOffset;
	psDest->pbyFWAddr.ui32Addr = psSrcFWDevVAddr->ui32Addr;

	DevmemReleaseDevVirtAddr(psSrcMemDesc);
}


void RGXUnsetFirmwareAddress(DEVMEM_MEMDESC *psSrc)
{
	DevmemReleaseDevVirtAddr(psSrc);
}

struct _RGX_SERVER_COMMON_CONTEXT_ {
	PVRSRV_RGXDEV_INFO *psDevInfo;
	DEVMEM_MEMDESC *psFWCommonContextMemDesc;
	PRGXFWIF_FWCOMMONCONTEXT sFWCommonContextFWAddr;
	DEVMEM_MEMDESC *psFWMemContextMemDesc;
	DEVMEM_MEMDESC *psFWFrameworkMemDesc;
	DEVMEM_MEMDESC *psContextStateMemDesc;
	RGX_CLIENT_CCB *psClientCCB;
	DEVMEM_MEMDESC *psClientCCBMemDesc;
	DEVMEM_MEMDESC *psClientCCBCtrlMemDesc;
	IMG_BOOL bCommonContextMemProvided;
	IMG_UINT32 ui32ContextID;
	DLLIST_NODE sListNode;
	RGXFWIF_CONTEXT_RESET_REASON eLastResetReason;
	IMG_UINT32 ui32LastResetJobRef;
};

PVRSRV_ERROR FWCommonContextAllocate(CONNECTION_DATA *psConnection,
		PVRSRV_DEVICE_NODE *psDeviceNode,
		RGX_CCB_REQUESTOR_TYPE eRGXCCBRequestor,
		RGXFWIF_DM eDM,
		DEVMEM_MEMDESC *psAllocatedMemDesc,
		IMG_UINT32 ui32AllocatedOffset,
		DEVMEM_MEMDESC *psFWMemContextMemDesc,
		DEVMEM_MEMDESC *psContextStateMemDesc,
		IMG_UINT32 ui32CCBAllocSizeLog2,
		IMG_UINT32 ui32CCBMaxAllocSizeLog2,
		IMG_UINT32 ui32ContextFlags,
		IMG_UINT32 ui32Priority,
		IMG_UINT32 ui32MaxDeadlineMS,
		IMG_UINT64 ui64RobustnessAddress,
		RGX_COMMON_CONTEXT_INFO *psInfo,
		RGX_SERVER_COMMON_CONTEXT **ppsServerCommonContext)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGX_SERVER_COMMON_CONTEXT *psServerCommonContext;
	RGXFWIF_FWCOMMONCONTEXT *psFWCommonContext;
	IMG_UINT32 ui32FWCommonContextOffset;
	IMG_UINT8 *pui8Ptr;
	PVRSRV_ERROR eError;

	/*
	 * Allocate all the resources that are required
	 */
	psServerCommonContext = OSAllocMem(sizeof(*psServerCommonContext));
	if (psServerCommonContext == NULL)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto fail_alloc;
	}

	psServerCommonContext->psDevInfo = psDevInfo;

	if (psAllocatedMemDesc)
	{
		PDUMPCOMMENT("Using existing MemDesc for Rogue firmware %s context (offset = %d)",
				aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT],
				ui32AllocatedOffset);
		ui32FWCommonContextOffset = ui32AllocatedOffset;
		psServerCommonContext->psFWCommonContextMemDesc = psAllocatedMemDesc;
		psServerCommonContext->bCommonContextMemProvided = IMG_TRUE;
	}
	else
	{
		/* Allocate device memory for the firmware context */
		PDUMPCOMMENT("Allocate Rogue firmware %s context", aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT]);
		eError = DevmemFwAllocate(psDevInfo,
				sizeof(*psFWCommonContext),
				RGX_FWCOMCTX_ALLOCFLAGS,
				"FwContext",
				&psServerCommonContext->psFWCommonContextMemDesc);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Failed to allocate firmware %s context (%s)",
			         __func__,
			         aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT],
			         PVRSRVGetErrorString(eError)));
			goto fail_contextalloc;
		}
		ui32FWCommonContextOffset = 0;
		psServerCommonContext->bCommonContextMemProvided = IMG_FALSE;
	}

	/* Record this context so we can refer to it if the FW needs to tell us it was reset. */
	psServerCommonContext->eLastResetReason    = RGXFWIF_CONTEXT_RESET_REASON_NONE;
	psServerCommonContext->ui32LastResetJobRef = 0;
	psServerCommonContext->ui32ContextID       = psDevInfo->ui32CommonCtxtCurrentID++;

	/*
	 * Temporarily map the firmware context to the kernel and initialise it
	 */
	eError = DevmemAcquireCpuVirtAddr(psServerCommonContext->psFWCommonContextMemDesc,
	                                  (void **)&pui8Ptr);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to map firmware %s context to CPU (%s)",
		         __func__,
		         aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT],
		         PVRSRVGetErrorString(eError)));
		goto fail_cpuvirtacquire;
	}

	/* Allocate the client CCB */
	eError = RGXCreateCCB(psDevInfo,
			ui32CCBAllocSizeLog2,
			ui32CCBMaxAllocSizeLog2,
			ui32ContextFlags,
			psConnection,
			eRGXCCBRequestor,
			psServerCommonContext,
			&psServerCommonContext->psClientCCB,
			&psServerCommonContext->psClientCCBMemDesc,
			&psServerCommonContext->psClientCCBCtrlMemDesc);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: failed to create CCB for %s context (%s)",
		         __func__,
		         aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT],
		         PVRSRVGetErrorString(eError)));
		goto fail_allocateccb;
	}

	psFWCommonContext = (RGXFWIF_FWCOMMONCONTEXT *) (pui8Ptr + ui32FWCommonContextOffset);
	psFWCommonContext->eDM = eDM;

	/* Set the firmware CCB device addresses in the firmware common context */
	eError = RGXSetFirmwareAddress(&psFWCommonContext->psCCB,
			psServerCommonContext->psClientCCBMemDesc,
			0, RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:1", fail_cccbfwaddr);

	eError = RGXSetFirmwareAddress(&psFWCommonContext->psCCBCtl,
			psServerCommonContext->psClientCCBCtrlMemDesc,
			0, RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:2", fail_cccbctrlfwaddr);

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, META_DMA))
	{
		RGXSetMetaDMAAddress(&psFWCommonContext->sCCBMetaDMAAddr,
				psServerCommonContext->psClientCCBMemDesc,
				&psFWCommonContext->psCCB,
				0);
	}

	/* Set the memory context device address */
	psServerCommonContext->psFWMemContextMemDesc = psFWMemContextMemDesc;
	eError = RGXSetFirmwareAddress(&psFWCommonContext->psFWMemContext,
			psFWMemContextMemDesc,
			0, RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:3", fail_fwmemctxfwaddr);

	/* Set the framework register updates address */
	psServerCommonContext->psFWFrameworkMemDesc = psInfo->psFWFrameworkMemDesc;
	if (psInfo->psFWFrameworkMemDesc != NULL)
	{
		eError = RGXSetFirmwareAddress(&psFWCommonContext->psRFCmd,
				psInfo->psFWFrameworkMemDesc,
				0, RFW_FWADDR_FLAG_NONE);
		PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:4", fail_fwframeworkfwaddr);
	}
	else
	{
		/* This should never be touched in this contexts without a framework
		 * memdesc, but ensure it is zero so we see crashes if it is.
		 */
		psFWCommonContext->psRFCmd.ui32Addr = 0;
	}

	psFWCommonContext->ui32Priority = ui32Priority;
	psFWCommonContext->ui32PrioritySeqNum = 0;
	psFWCommonContext->ui32MaxDeadlineMS = MIN(ui32MaxDeadlineMS,
											   (eDM == RGXFWIF_DM_CDM ?
												RGXFWIF_MAX_CDM_WORKLOAD_DEADLINE_MS :
												RGXFWIF_MAX_WORKLOAD_DEADLINE_MS));
	psFWCommonContext->ui64RobustnessAddress = ui64RobustnessAddress;

	/* Store a references to Server Common Context and PID for notifications back from the FW. */
	psFWCommonContext->ui32ServerCommonContextID = psServerCommonContext->ui32ContextID;
	psFWCommonContext->ui32PID                   = OSGetCurrentClientProcessIDKM();

	/* Set the firmware GPU context state buffer */
	psServerCommonContext->psContextStateMemDesc = psContextStateMemDesc;
	if (psContextStateMemDesc)
	{
		eError = RGXSetFirmwareAddress(&psFWCommonContext->psContextState,
				psContextStateMemDesc,
				0,
				RFW_FWADDR_FLAG_NONE);
		PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:5", fail_ctxstatefwaddr);
	}

	/*
	 * Dump the created context
	 */
	PDUMPCOMMENT("Dump %s context", aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT]);
	DevmemPDumpLoadMem(psServerCommonContext->psFWCommonContextMemDesc,
			ui32FWCommonContextOffset,
			sizeof(*psFWCommonContext),
			PDUMP_FLAGS_CONTINUOUS);

	/* We've finished the setup so release the CPU mapping */
	DevmemReleaseCpuVirtAddr(psServerCommonContext->psFWCommonContextMemDesc);

	/* Map this allocation into the FW */
	eError = RGXSetFirmwareAddress(&psServerCommonContext->sFWCommonContextFWAddr,
			psServerCommonContext->psFWCommonContextMemDesc,
			ui32FWCommonContextOffset,
			RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:6", fail_fwcommonctxfwaddr);

#if defined(LINUX)
	{
		IMG_UINT32 ui32FWAddr;
		switch (eDM) {
		case RGXFWIF_DM_GEOM:
			ui32FWAddr = (IMG_UINT32) ((uintptr_t) IMG_CONTAINER_OF((void *) ((uintptr_t)
					psServerCommonContext->sFWCommonContextFWAddr.ui32Addr), RGXFWIF_FWRENDERCONTEXT, sTAContext));
			break;
		case RGXFWIF_DM_3D:
			ui32FWAddr = (IMG_UINT32) ((uintptr_t) IMG_CONTAINER_OF((void *) ((uintptr_t)
					psServerCommonContext->sFWCommonContextFWAddr.ui32Addr), RGXFWIF_FWRENDERCONTEXT, s3DContext));
			break;
		default:
			ui32FWAddr = psServerCommonContext->sFWCommonContextFWAddr.ui32Addr;
			break;
		}

		trace_rogue_create_fw_context(OSGetCurrentClientProcessNameKM(),
				aszCCBRequestors[eRGXCCBRequestor][REQ_PDUMP_COMMENT],
				ui32FWAddr);
	}
#endif
	/*Add the node to the list when finalised */
	OSWRLockAcquireWrite(psDevInfo->hCommonCtxtListLock);
	dllist_add_to_tail(&(psDevInfo->sCommonCtxtListHead), &(psServerCommonContext->sListNode));
	OSWRLockReleaseWrite(psDevInfo->hCommonCtxtListLock);

	*ppsServerCommonContext = psServerCommonContext;
	return PVRSRV_OK;

fail_fwcommonctxfwaddr:
	if (psContextStateMemDesc)
	{
		RGXUnsetFirmwareAddress(psContextStateMemDesc);
	}
fail_ctxstatefwaddr:
	if (psInfo->psFWFrameworkMemDesc != NULL)
	{
		RGXUnsetFirmwareAddress(psInfo->psFWFrameworkMemDesc);
	}
fail_fwframeworkfwaddr:
	RGXUnsetFirmwareAddress(psFWMemContextMemDesc);
fail_fwmemctxfwaddr:
	RGXUnsetFirmwareAddress(psServerCommonContext->psClientCCBCtrlMemDesc);
fail_cccbctrlfwaddr:
	RGXUnsetFirmwareAddress(psServerCommonContext->psClientCCBMemDesc);
fail_cccbfwaddr:
	RGXDestroyCCB(psDevInfo, psServerCommonContext->psClientCCB);
fail_allocateccb:
	DevmemReleaseCpuVirtAddr(psServerCommonContext->psFWCommonContextMemDesc);
fail_cpuvirtacquire:
	if (!psServerCommonContext->bCommonContextMemProvided)
	{
		DevmemFwUnmapAndFree(psDevInfo, psServerCommonContext->psFWCommonContextMemDesc);
		psServerCommonContext->psFWCommonContextMemDesc = NULL;
	}
fail_contextalloc:
	OSFreeMem(psServerCommonContext);
fail_alloc:
	return eError;
}

void FWCommonContextFree(RGX_SERVER_COMMON_CONTEXT *psServerCommonContext)
{

	OSWRLockAcquireWrite(psServerCommonContext->psDevInfo->hCommonCtxtListLock);
	/* Remove the context from the list of all contexts. */
	dllist_remove_node(&psServerCommonContext->sListNode);
	OSWRLockReleaseWrite(psServerCommonContext->psDevInfo->hCommonCtxtListLock);

	/*
		Unmap the context itself and then all its resources
	*/

	/* Unmap the FW common context */
	RGXUnsetFirmwareAddress(psServerCommonContext->psFWCommonContextMemDesc);
	/* Umap context state buffer (if there was one) */
	if (psServerCommonContext->psContextStateMemDesc)
	{
		RGXUnsetFirmwareAddress(psServerCommonContext->psContextStateMemDesc);
	}
	/* Unmap the framework buffer */
	if (psServerCommonContext->psFWFrameworkMemDesc)
	{
		RGXUnsetFirmwareAddress(psServerCommonContext->psFWFrameworkMemDesc);
	}
	/* Unmap client CCB and CCB control */
	RGXUnsetFirmwareAddress(psServerCommonContext->psClientCCBCtrlMemDesc);
	RGXUnsetFirmwareAddress(psServerCommonContext->psClientCCBMemDesc);
	/* Unmap the memory context */
	RGXUnsetFirmwareAddress(psServerCommonContext->psFWMemContextMemDesc);

	/* Destroy the client CCB */
	RGXDestroyCCB(psServerCommonContext->psDevInfo, psServerCommonContext->psClientCCB);


	/* Free the FW common context (if there was one) */
	if (!psServerCommonContext->bCommonContextMemProvided)
	{
		DevmemFwUnmapAndFree(psServerCommonContext->psDevInfo,
				psServerCommonContext->psFWCommonContextMemDesc);
		psServerCommonContext->psFWCommonContextMemDesc = NULL;
	}
	/* Free the hosts representation of the common context */
	OSFreeMem(psServerCommonContext);
}

PRGXFWIF_FWCOMMONCONTEXT FWCommonContextGetFWAddress(RGX_SERVER_COMMON_CONTEXT *psServerCommonContext)
{
	return psServerCommonContext->sFWCommonContextFWAddr;
}

RGX_CLIENT_CCB *FWCommonContextGetClientCCB(RGX_SERVER_COMMON_CONTEXT *psServerCommonContext)
{
	return psServerCommonContext->psClientCCB;
}

RGXFWIF_CONTEXT_RESET_REASON FWCommonContextGetLastResetReason(RGX_SERVER_COMMON_CONTEXT *psServerCommonContext,
		IMG_UINT32 *pui32LastResetJobRef)
{
	RGXFWIF_CONTEXT_RESET_REASON eLastResetReason;

	PVR_ASSERT(psServerCommonContext != NULL);
	PVR_ASSERT(pui32LastResetJobRef != NULL);

	/* Take the most recent reason & job ref and reset for next time... */
	eLastResetReason      = psServerCommonContext->eLastResetReason;
	*pui32LastResetJobRef = psServerCommonContext->ui32LastResetJobRef;
	psServerCommonContext->eLastResetReason = RGXFWIF_CONTEXT_RESET_REASON_NONE;
	psServerCommonContext->ui32LastResetJobRef = 0;

	if (eLastResetReason == RGXFWIF_CONTEXT_RESET_REASON_HARD_CONTEXT_SWITCH)
	{
		PVR_DPF((PVR_DBG_WARNING,
		         "A Hard Context Switch was triggered on the GPU to ensure Quality of Service."));
	}

	return eLastResetReason;
}

PVRSRV_RGXDEV_INFO* FWCommonContextGetRGXDevInfo(RGX_SERVER_COMMON_CONTEXT *psServerCommonContext)
{
	return psServerCommonContext->psDevInfo;
}

PVRSRV_ERROR FWCommonContextSetFlags(RGX_SERVER_COMMON_CONTEXT *psServerCommonContext,
                                     IMG_UINT32 ui32ContextFlags)
{
	return RGXSetCCBFlags(psServerCommonContext->psClientCCB,
	                      ui32ContextFlags);
}

/*!
*******************************************************************************
 @Function		RGXFreeCCB
 @Description	Free the kernel or firmware CCB
 @Input			psDevInfo
 @Input			ppsCCBCtl
 @Input			ppsCCBCtlMemDesc
 @Input			ppsCCBMemDesc
 @Input			psCCBCtlFWAddr
******************************************************************************/
static void RGXFreeCCB(PVRSRV_RGXDEV_INFO	*psDevInfo,
					   RGXFWIF_CCB_CTL		**ppsCCBCtl,
					   DEVMEM_MEMDESC		**ppsCCBCtlMemDesc,
					   IMG_UINT8			**ppui8CCB,
					   DEVMEM_MEMDESC		**ppsCCBMemDesc)
{
	if (*ppsCCBMemDesc != NULL)
	{
		if (*ppui8CCB != NULL)
		{
			DevmemReleaseCpuVirtAddr(*ppsCCBMemDesc);
			*ppui8CCB = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, *ppsCCBMemDesc);
		*ppsCCBMemDesc = NULL;
	}
	if (*ppsCCBCtlMemDesc != NULL)
	{
		if (*ppsCCBCtl != NULL)
		{
			DevmemReleaseCpuVirtAddr(*ppsCCBCtlMemDesc);
			*ppsCCBCtl = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, *ppsCCBCtlMemDesc);
		*ppsCCBCtlMemDesc = NULL;
	}
}

/*!
*******************************************************************************
 @Function		RGXFreeCCBReturnSlots
 @Description	Free the kernel CCB's return slot array and associated mappings
 @Input			psDevInfo              Device Info struct
 @Input			ppui32CCBRtnSlots      CPU mapping of slot array
 @Input			ppsCCBRtnSlotsMemDesc  Slot array's device memdesc
******************************************************************************/
static void RGXFreeCCBReturnSlots(PVRSRV_RGXDEV_INFO *psDevInfo,
                                  IMG_UINT32         **ppui32CCBRtnSlots,
								  DEVMEM_MEMDESC     **ppsCCBRtnSlotsMemDesc)
{
	/* Free the return slot array if allocated */
	if (*ppsCCBRtnSlotsMemDesc != NULL)
	{
		/* Before freeing, ensure the CPU mapping as well is released */
		if (*ppui32CCBRtnSlots != NULL)
		{
			DevmemReleaseCpuVirtAddr(*ppsCCBRtnSlotsMemDesc);
			*ppui32CCBRtnSlots = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, *ppsCCBRtnSlotsMemDesc);
		*ppsCCBRtnSlotsMemDesc = NULL;
	}
}

/*!
*******************************************************************************
 @Function		RGXSetupCCB
 @Description	Allocate and initialise a circular command buffer
 @Input			psDevInfo
 @Input			ppsCCBCtl
 @Input			ppsCCBCtlMemDesc
 @Input			ppui8CCB
 @Input			ppsCCBMemDesc
 @Input			psCCBCtlFWAddr
 @Input			ui32NumCmdsLog2
 @Input			ui32CmdSize
 @Input			uiCCBMemAllocFlags
 @Input			pszName

 @Return		PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXSetupCCB(PVRSRV_RGXDEV_INFO	*psDevInfo,
								RGXFWIF_CCB_CTL		**ppsCCBCtl,
								DEVMEM_MEMDESC		**ppsCCBCtlMemDesc,
								IMG_UINT8			**ppui8CCB,
								DEVMEM_MEMDESC		**ppsCCBMemDesc,
								PRGXFWIF_CCB_CTL	*psCCBCtlFWAddr,
								PRGXFWIF_CCB		*psCCBFWAddr,
								IMG_UINT32			ui32NumCmdsLog2,
								IMG_UINT32			ui32CmdSize,
								DEVMEM_FLAGS_T		uiCCBMemAllocFlags,
								const IMG_CHAR		*pszName)
{
	PVRSRV_ERROR		eError;
	RGXFWIF_CCB_CTL		*psCCBCtl;
	IMG_UINT32		ui32CCBSize = (1U << ui32NumCmdsLog2);
	IMG_CHAR		szCCBCtlName[DEVMEM_ANNOTATION_MAX_LEN];
	IMG_INT32		iStrLen;

	/* Append "Control" to the name for the control struct. */
	iStrLen = OSSNPrintf(szCCBCtlName, sizeof(szCCBCtlName), "%sControl", pszName);
	PVR_ASSERT(iStrLen < sizeof(szCCBCtlName));

	if (unlikely(iStrLen < 0))
	{
		szCCBCtlName[0] = '\0';
	}

	/* Allocate memory for the CCB control.*/
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_ALLOCFLAGS,
								  sizeof(RGXFWIF_CCB_CTL),
								  szCCBCtlName,
								  ppsCCBCtlMemDesc,
								  psCCBCtlFWAddr,
								  (void**) ppsCCBCtl,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	/*
	 * Allocate memory for the CCB.
	 * (this will reference further command data in non-shared CCBs)
	 */
	eError = RGXSetupFwAllocation(psDevInfo,
								  uiCCBMemAllocFlags,
								  ui32CCBSize * ui32CmdSize,
								  pszName,
								  ppsCCBMemDesc,
								  psCCBFWAddr,
								  (void**) ppui8CCB,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	/*
	 * Initialise the CCB control.
	 */
	psCCBCtl = *ppsCCBCtl;
	psCCBCtl->ui32WriteOffset = 0;
	psCCBCtl->ui32ReadOffset = 0;
	psCCBCtl->ui32WrapMask = ui32CCBSize - 1;
	psCCBCtl->ui32CmdSize = ui32CmdSize;

	/* Pdump the CCB control */
	PDUMPCOMMENT("Initialise %s", szCCBCtlName);
	DevmemPDumpLoadMem(*ppsCCBCtlMemDesc,
					   0,
					   sizeof(RGXFWIF_CCB_CTL),
					   0);

	return PVRSRV_OK;

fail:
	RGXFreeCCB(psDevInfo,
			   ppsCCBCtl,
			   ppsCCBCtlMemDesc,
			   ppui8CCB,
			   ppsCCBMemDesc);

	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

static void RGXSetupFaultReadRegisterRollback(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	PMR *psPMR;

	if (psDevInfo->psRGXFaultAddressMemDesc)
	{
		if (DevmemServerGetImportHandle(psDevInfo->psRGXFaultAddressMemDesc, (void **)&psPMR) == PVRSRV_OK)
		{
			PMRUnlockSysPhysAddresses(psPMR);
		}
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFaultAddressMemDesc);
		psDevInfo->psRGXFaultAddressMemDesc = NULL;
	}
}

static PVRSRV_ERROR RGXSetupFaultReadRegister(PVRSRV_DEVICE_NODE	*psDeviceNode, RGXFWIF_SYSINIT *psFwSysInit)
{
	PVRSRV_ERROR		eError = PVRSRV_OK;
	IMG_UINT32			*pui32MemoryVirtAddr;
	IMG_UINT32			i;
	size_t				ui32PageSize = OSGetPageSize();
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDeviceNode->pvDevice;
	PMR					*psPMR;

	/* Allocate page of memory to use for page faults on non-blocking memory transactions.
	 * Doesn't need to be cleared as it is initialised with the 0xDEADBEE0 pattern below. */
	psDevInfo->psRGXFaultAddressMemDesc = NULL;
	eError = DevmemFwAllocateExportable(psDeviceNode,
			ui32PageSize,
			ui32PageSize,
			RGX_FWSHAREDMEM_ALLOCFLAGS & (~PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC),
			"FwExFaultAddress",
			&psDevInfo->psRGXFaultAddressMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to allocate mem for fault address (%u)",
		         __func__, eError));
		goto failFaultAddressDescAlloc;
	}

	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psRGXFaultAddressMemDesc,
									  (void **)&pui32MemoryVirtAddr);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to acquire mem for fault address (%u)",
		         __func__, eError));
		goto failFaultAddressDescAqCpuVirt;
	}

	if (!psDeviceNode->bAutoVzFwIsUp)
	{
		/* fill the page with a known pattern when booting the firmware */
		for (i = 0; i < ui32PageSize/sizeof(IMG_UINT32); i++)
		{
			*(pui32MemoryVirtAddr + i) = 0xDEADBEE0;
		}
	}

	eError = DevmemServerGetImportHandle(psDevInfo->psRGXFaultAddressMemDesc, (void **)&psPMR);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Error getting PMR for fault address (%u)",
		         __func__, eError));

		goto failFaultAddressDescGetPMR;
	}
	else
	{
		IMG_BOOL bValid;
		IMG_UINT32 ui32Log2PageSize = OSGetPageShift();

		eError = PMRLockSysPhysAddresses(psPMR);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Error locking physical address for fault address MemDesc (%u)",
			         __func__, eError));

			goto failFaultAddressDescLockPhys;
		}

		eError = PMR_DevPhysAddr(psPMR,ui32Log2PageSize, 1, 0, &(psFwSysInit->sFaultPhysAddr), &bValid);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Error getting physical address for fault address MemDesc (%u)",
			         __func__, eError));

			goto failFaultAddressDescGetPhys;
		}

		if (!bValid)
		{
			psFwSysInit->sFaultPhysAddr.uiAddr = 0;
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Failed getting physical address for fault address MemDesc - invalid page (0x%" IMG_UINT64_FMTSPECX ")",
			         __func__, psFwSysInit->sFaultPhysAddr.uiAddr));

			goto failFaultAddressDescGetPhys;
		}
	}

	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFaultAddressMemDesc);

	return PVRSRV_OK;

failFaultAddressDescGetPhys:
	PMRUnlockSysPhysAddresses(psPMR);

failFaultAddressDescLockPhys:

failFaultAddressDescGetPMR:
	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFaultAddressMemDesc);

failFaultAddressDescAqCpuVirt:
	DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFaultAddressMemDesc);
	psDevInfo->psRGXFaultAddressMemDesc = NULL;

failFaultAddressDescAlloc:

	return eError;
}

#if defined(PDUMP)
/* Replace the DevPhy address with the one Pdump allocates at pdump_player run time */
static PVRSRV_ERROR RGXPDumpFaultReadRegister(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	PVRSRV_ERROR eError;
	PMR *psFWInitPMR, *psFaultAddrPMR;
	IMG_UINT32 ui32Dstoffset;

	psFWInitPMR = (PMR *)(psDevInfo->psRGXFWIfSysInitMemDesc->psImport->hPMR);
	ui32Dstoffset = psDevInfo->psRGXFWIfSysInitMemDesc->uiOffset + offsetof(RGXFWIF_SYSINIT, sFaultPhysAddr.uiAddr);

	psFaultAddrPMR = (PMR *)(psDevInfo->psRGXFaultAddressMemDesc->psImport->hPMR);

	eError = PDumpMemLabelToMem64(psFaultAddrPMR,
			psFWInitPMR,
			0,
			ui32Dstoffset,
			PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Dump of Fault Page Phys address failed(%u)", __func__, eError));
	}
	return eError;
}
#endif

#if defined(SUPPORT_TBI_INTERFACE)
/*************************************************************************/ /*!
@Function       RGXTBIBufferIsInitRequired

@Description    Returns true if the firmware tbi buffer is not allocated and
		might be required by the firmware soon. TBI buffer allocated
		on-demand to reduce RAM footprint on systems not needing
		tbi.

@Input          psDevInfo	 RGX device info

@Return		IMG_BOOL	Whether on-demand allocation(s) is/are needed
				or not
*/ /**************************************************************************/
INLINE IMG_BOOL RGXTBIBufferIsInitRequired(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_TRACEBUF*  psTraceBufCtl = psDevInfo->psRGXFWIfTraceBufCtl;

	/* The firmware expects a tbi buffer only when:
	 *	- Logtype is "tbi"
	 */
	if ((psDevInfo->psRGXFWIfTBIBufferMemDesc == NULL)
			&& (psTraceBufCtl->ui32LogType & ~RGXFWIF_LOG_TYPE_TRACE)
			&& (psTraceBufCtl->ui32LogType & RGXFWIF_LOG_TYPE_GROUP_MASK))
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

/*************************************************************************/ /*!
@Function       RGXTBIBufferDeinit

@Description    Deinitialises all the allocations and references that are made
		for the FW tbi buffer

@Input          ppsDevInfo	 RGX device info
@Return		void
*/ /**************************************************************************/
static void RGXTBIBufferDeinit(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfTBIBufferMemDesc);
	psDevInfo->psRGXFWIfTBIBufferMemDesc = NULL;
	psDevInfo->ui32RGXFWIfHWPerfBufSize = 0;
}

/*************************************************************************/ /*!
@Function       RGXTBIBufferInitOnDemandResources

@Description    Allocates the firmware TBI buffer required for reading SFs
		strings and initialize it with SFs.

@Input          psDevInfo	 RGX device info

@Return		PVRSRV_OK	If all went good, PVRSRV_ERROR otherwise.
*/ /**************************************************************************/
PVRSRV_ERROR RGXTBIBufferInitOnDemandResources(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	PVRSRV_ERROR       eError = PVRSRV_OK;
	IMG_UINT32         i, ui32Len;
	const IMG_UINT32   ui32FWTBIBufsize = g_ui32SFsCount * sizeof(RGXFW_STID_FMT);
	RGXFW_STID_FMT     *psFW_SFs = NULL;

	/* Firmware address should not be already set */
	if (psDevInfo->sRGXFWIfTBIBuffer.ui32Addr)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: FW address for FWTBI is already set. Resetting it with newly allocated one",
		         __func__));
	}

	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_GPU_RO_ALLOCFLAGS,
								  ui32FWTBIBufsize,
								  "FwTBIBuffer",
								  &psDevInfo->psRGXFWIfTBIBufferMemDesc,
								  &psDevInfo->sRGXFWIfTBIBuffer,
								  (void**)&psFW_SFs,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	/* Copy SFs entries to FW buffer */
	for (i = 0; i < g_ui32SFsCount; i++)
	{
		OSDeviceMemCopy(&psFW_SFs[i].ui32Id, &SFs[i].ui32Id, sizeof(SFs[i].ui32Id));
		ui32Len = OSStringLength(SFs[i].psName);
		OSDeviceMemCopy(psFW_SFs[i].sName, SFs[i].psName, MIN(ui32Len, IMG_SF_STRING_MAX_SIZE - 1));
	}

	/* Set size of TBI buffer */
	psDevInfo->ui32FWIfTBIBufferSize = ui32FWTBIBufsize;

	/* release CPU mapping */
	DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfTBIBufferMemDesc);

	return PVRSRV_OK;
fail:
	RGXTBIBufferDeinit(psDevInfo);
	return eError;
}
#endif

/*************************************************************************/ /*!
@Function       RGXTraceBufferIsInitRequired

@Description    Returns true if the firmware trace buffer is not allocated and
		might be required by the firmware soon. Trace buffer allocated
		on-demand to reduce RAM footprint on systems not needing
		firmware trace.

@Input          psDevInfo	 RGX device info

@Return		IMG_BOOL	Whether on-demand allocation(s) is/are needed
				or not
*/ /**************************************************************************/
INLINE IMG_BOOL RGXTraceBufferIsInitRequired(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_TRACEBUF*  psTraceBufCtl = psDevInfo->psRGXFWIfTraceBufCtl;

	/* The firmware expects a trace buffer only when:
	 *	- Logtype is "trace" AND
	 *	- at least one LogGroup is configured
	 *	- the Driver Mode is not Guest
	 */
	if ((psDevInfo->psRGXFWIfTraceBufferMemDesc[0] == NULL)
		&& (psTraceBufCtl->ui32LogType & RGXFWIF_LOG_TYPE_TRACE)
		&& (psTraceBufCtl->ui32LogType & RGXFWIF_LOG_TYPE_GROUP_MASK)
		&& !PVRSRV_VZ_MODE_IS(GUEST))
	{
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

/*************************************************************************/ /*!
@Function       RGXTraceBufferDeinit

@Description    Deinitialises all the allocations and references that are made
		for the FW trace buffer(s)

@Input          ppsDevInfo	 RGX device info
@Return		void
*/ /**************************************************************************/
static void RGXTraceBufferDeinit(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_TRACEBUF*  psTraceBufCtl = psDevInfo->psRGXFWIfTraceBufCtl;
	IMG_UINT32 i;

	for (i = 0; i < RGXFW_THREAD_NUM; i++)
	{
		if (psDevInfo->psRGXFWIfTraceBufferMemDesc[i])
		{
			if (psTraceBufCtl->sTraceBuf[i].pui32TraceBuffer != NULL)
			{
				DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfTraceBufferMemDesc[i]);
				psTraceBufCtl->sTraceBuf[i].pui32TraceBuffer = NULL;
			}

			DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfTraceBufferMemDesc[i]);
			psDevInfo->psRGXFWIfTraceBufferMemDesc[i] = NULL;
		}
	}
}

/*************************************************************************/ /*!
@Function       RGXTraceBufferInitOnDemandResources

@Description    Allocates the firmware trace buffer required for dumping trace
		info from the firmware.

@Input          psDevInfo	 RGX device info

@Return		PVRSRV_OK	If all went good, PVRSRV_ERROR otherwise.
*/ /**************************************************************************/
PVRSRV_ERROR RGXTraceBufferInitOnDemandResources(PVRSRV_RGXDEV_INFO* psDevInfo,
												 DEVMEM_FLAGS_T uiAllocFlags)
{
	RGXFWIF_TRACEBUF*  psTraceBufCtl = psDevInfo->psRGXFWIfTraceBufCtl;
	PVRSRV_ERROR       eError = PVRSRV_OK;
	IMG_UINT32         ui32FwThreadNum;
	IMG_UINT32         ui32DefaultTraceBufSize;
	IMG_DEVMEM_SIZE_T  uiTraceBufSizeInBytes;
	void               *pvAppHintState = NULL;
	IMG_CHAR           pszBufferName[] = "FwTraceBuffer_Thread0";

	/* Check AppHint value for module-param FWTraceBufSizeInDWords */
	OSCreateKMAppHintState(&pvAppHintState);
	ui32DefaultTraceBufSize = RGXFW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS;
	OSGetKMAppHintUINT32(pvAppHintState,
	                     FWTraceBufSizeInDWords,
						 &ui32DefaultTraceBufSize,
						 &psTraceBufCtl->ui32TraceBufSizeInDWords);
	OSFreeKMAppHintState(pvAppHintState);
	pvAppHintState = NULL;

	uiTraceBufSizeInBytes = psTraceBufCtl->ui32TraceBufSizeInDWords * sizeof(IMG_UINT32);

	for (ui32FwThreadNum = 0; ui32FwThreadNum < RGXFW_THREAD_NUM; ui32FwThreadNum++)
	{
#if !defined(SUPPORT_AUTOVZ)
		/* Ensure allocation API is only called when not already allocated */
		PVR_ASSERT(psDevInfo->psRGXFWIfTraceBufferMemDesc[ui32FwThreadNum] == NULL);
		/* Firmware address should not be already set */
		PVR_ASSERT(psTraceBufCtl->sTraceBuf[ui32FwThreadNum].pui32RGXFWIfTraceBuffer.ui32Addr == 0x0);
#endif

		/* update the firmware thread number in the Trace Buffer's name */
		pszBufferName[sizeof(pszBufferName) - 2] += ui32FwThreadNum;

		eError = RGXSetupFwAllocation(psDevInfo,
									  uiAllocFlags,
									  uiTraceBufSizeInBytes,
									  pszBufferName,
									  &psDevInfo->psRGXFWIfTraceBufferMemDesc[ui32FwThreadNum],
									  &psTraceBufCtl->sTraceBuf[ui32FwThreadNum].pui32RGXFWIfTraceBuffer,
									  (void**)&psTraceBufCtl->sTraceBuf[ui32FwThreadNum].pui32TraceBuffer,
									  RFW_FWADDR_NOREF_FLAG);
		PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);
	}

	return PVRSRV_OK;

fail:
	RGXTraceBufferDeinit(psDevInfo);
	return eError;
}

#if defined(PDUMP)
/*************************************************************************/ /*!
@Function       RGXPDumpLoadFWInitData

@Description    Allocates the firmware trace buffer required for dumping trace
                info from the firmware.

@Input          psDevInfo RGX device info
 */ /*************************************************************************/
static void RGXPDumpLoadFWInitData(PVRSRV_RGXDEV_INFO *psDevInfo,
								   IMG_UINT32         ui32HWPerfCountersDataSize,
								   IMG_BOOL           bEnableSignatureChecks)
{
	IMG_UINT32 ui32ConfigFlags    = psDevInfo->psRGXFWIfFwSysData->ui32ConfigFlags;
	IMG_UINT32 ui32FwOsCfgFlags   = psDevInfo->psRGXFWIfFwOsData->ui32FwOsConfigFlags;

	PDUMPCOMMENT("Dump RGXFW Init data");
	if (!bEnableSignatureChecks)
	{
		PDUMPCOMMENT("(to enable rgxfw signatures place the following line after the RTCONF line)");
		DevmemPDumpLoadMem(psDevInfo->psRGXFWIfSysInitMemDesc,
						   offsetof(RGXFWIF_SYSINIT, asSigBufCtl),
						   sizeof(RGXFWIF_SIGBUF_CTL)*(RGXFWIF_DM_MAX),
						   PDUMP_FLAGS_CONTINUOUS);
	}

	PDUMPCOMMENT("Dump initial state of FW runtime configuration");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfRuntimeCfgMemDesc,
					   0,
					   sizeof(RGXFWIF_RUNTIME_CFG),
					   PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("Dump rgxfw hwperfctl structure");
	DevmemPDumpLoadZeroMem(psDevInfo->psRGXFWIfHWPerfCountersMemDesc,
						   0,
						   ui32HWPerfCountersDataSize,
						   PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("Dump rgxfw trace control structure");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfTraceBufCtlMemDesc,
					   0,
					   sizeof(RGXFWIF_TRACEBUF),
					   PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("Dump firmware system data structure");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfFwSysDataMemDesc,
					   0,
					   sizeof(RGXFWIF_SYSDATA),
					   PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("Dump firmware OS data structure");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfFwOsDataMemDesc,
					   0,
					   sizeof(RGXFWIF_OSDATA),
					   PDUMP_FLAGS_CONTINUOUS);

#if defined(SUPPORT_TBI_INTERFACE)
	PDUMPCOMMENT("Dump rgx TBI buffer");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfTBIBufferMemDesc,
					   0,
					   psDevInfo->ui32FWIfTBIBufferSize,
					   PDUMP_FLAGS_CONTINUOUS);
#endif /* defined(SUPPORT_TBI_INTERFACE) */

#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	PDUMPCOMMENT("Dump rgxfw register configuration buffer");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfRegCfgMemDesc,
					   0,
					   sizeof(RGXFWIF_REG_CFG),
					   PDUMP_FLAGS_CONTINUOUS);
#endif /* defined(SUPPORT_USER_REGISTER_CONFIGURATION) */
	PDUMPCOMMENT("Dump rgxfw system init structure");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfSysInitMemDesc,
					   0,
					   sizeof(RGXFWIF_SYSINIT),
					   PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("Dump rgxfw os init structure");
	DevmemPDumpLoadMem(psDevInfo->psRGXFWIfOsInitMemDesc,
					   0,
					   sizeof(RGXFWIF_OSINIT),
					   PDUMP_FLAGS_CONTINUOUS);

	/* RGXFW Init structure needs to be loaded before we overwrite FaultPhysAddr, else this address patching won't have any effect */
	PDUMPCOMMENT("Overwrite FaultPhysAddr of FwSysInit in pdump with actual physical address");
	RGXPDumpFaultReadRegister(psDevInfo);

	PDUMPCOMMENT("RTCONF: run-time configuration");


	/* Dump the config options so they can be edited.
	 *
	 */
	PDUMPCOMMENT("(Set the FW system config options here)");
	PDUMPCOMMENT("( Ctx Switch Rand mode:                      0x%08x)", RGXFWIF_INICFG_CTXSWITCH_MODE_RAND);
	PDUMPCOMMENT("( Ctx Switch Soft Reset Enable:              0x%08x)", RGXFWIF_INICFG_CTXSWITCH_SRESET_EN);
	PDUMPCOMMENT("( Enable HWPerf:                             0x%08x)", RGXFWIF_INICFG_HWPERF_EN);
	PDUMPCOMMENT("( Enable generic DM Killing Rand mode:       0x%08x)", RGXFWIF_INICFG_DM_KILL_MODE_RAND_EN);
	PDUMPCOMMENT("( Rascal+Dust Power Island:                  0x%08x)", RGXFWIF_INICFG_POW_RASCALDUST);
	PDUMPCOMMENT("( Enable HWR:                                0x%08x)", RGXFWIF_INICFG_HWR_EN);
	PDUMPCOMMENT("( FBCDC Version 3.1 Enable:                  0x%08x)", RGXFWIF_INICFG_FBCDC_V3_1_EN);
	PDUMPCOMMENT("( Check MList:                               0x%08x)", RGXFWIF_INICFG_CHECK_MLIST_EN);
	PDUMPCOMMENT("( Disable Auto Clock Gating:                 0x%08x)", RGXFWIF_INICFG_DISABLE_CLKGATING_EN);
	PDUMPCOMMENT("( Enable HWPerf Polling Perf Counter:        0x%08x)", RGXFWIF_INICFG_POLL_COUNTERS_EN);

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, VDM_OBJECT_LEVEL_LLS))
	{
	PDUMPCOMMENT("( Ctx Switch Object mode Index:              0x%08x)", RGXFWIF_INICFG_VDM_CTX_STORE_MODE_INDEX);
	PDUMPCOMMENT("( Ctx Switch Object mode Instance:           0x%08x)", RGXFWIF_INICFG_VDM_CTX_STORE_MODE_INSTANCE);
	PDUMPCOMMENT("( Ctx Switch Object mode List:               0x%08x)", RGXFWIF_INICFG_VDM_CTX_STORE_MODE_LIST);
	}

	PDUMPCOMMENT("( Enable register configuration:             0x%08x)", RGXFWIF_INICFG_REGCONFIG_EN);
	PDUMPCOMMENT("( Assert on TA Out-of-Memory:                0x%08x)", RGXFWIF_INICFG_ASSERT_ON_OUTOFMEMORY);
	PDUMPCOMMENT("( Disable HWPerf custom counter filter:      0x%08x)", RGXFWIF_INICFG_HWP_DISABLE_FILTER);
	PDUMPCOMMENT("( Enable HWPerf custom performance timer:    0x%08x)", RGXFWIF_INICFG_CUSTOM_PERF_TIMER_EN);
	PDUMPCOMMENT("( Enable Ctx Switch profile mode: 0x%08x (none=b'000, fast=b'001, medium=b'010, slow=b'011, nodelay=b'100))", RGXFWIF_INICFG_CTXSWITCH_PROFILE_MASK);
	PDUMPCOMMENT("( Disable DM overlap (except TA during SPM): 0x%08x)", RGXFWIF_INICFG_DISABLE_DM_OVERLAP);
	PDUMPCOMMENT("( Assert on HWR trigger (page fault, lockup, overrun or poll failure): 0x%08x)", RGXFWIF_INICFG_ASSERT_ON_HWR_TRIGGER);
	PDUMPCOMMENT("( Enable coherent memory accesses:           0x%08x)", RGXFWIF_INICFG_FABRIC_COHERENCY_ENABLED);
	PDUMPCOMMENT("( Enable IRQ validation:                     0x%08x)", RGXFWIF_INICFG_VALIDATE_IRQ);
	PDUMPCOMMENT("( SPU power state mask change Enable:        0x%08x)", RGXFWIF_INICFG_SPU_POWER_STATE_MASK_CHANGE_EN);
#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	PDUMPCOMMENT("( Enable Workload Estimation:                0x%08x)", RGXFWIF_INICFG_WORKEST);
#if defined(SUPPORT_PDVFS)
	PDUMPCOMMENT("( Enable Proactive DVFS:                     0x%08x)", RGXFWIF_INICFG_PDVFS);
#endif /* defined(SUPPORT_PDVFS) */
#endif /* defined(SUPPORT_WORKLOAD_ESTIMATION) */
	PDUMPCOMMENT("( CDM Arbitration Mode (task demand=b'01, round robin=b'10): 0x%08x)", RGXFWIF_INICFG_CDM_ARBITRATION_MASK);
	PDUMPCOMMENT("( ISP Scheduling Mode (v1=b'01, v2=b'10):    0x%08x)", RGXFWIF_INICFG_ISPSCHEDMODE_MASK);
	PDUMPCOMMENT("( Validate SOC & USC timers:                 0x%08x)", RGXFWIF_INICFG_VALIDATE_SOCUSC_TIMER);

	DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfFwSysDataMemDesc,
							offsetof(RGXFWIF_SYSDATA, ui32ConfigFlags),
							ui32ConfigFlags,
							PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("( Extended FW system config options not used.)");

	PDUMPCOMMENT("(Set the FW OS config options here)");
	PDUMPCOMMENT("( Ctx Switch TA Enable:                      0x%08x)", RGXFWIF_INICFG_OS_CTXSWITCH_TA_EN);
	PDUMPCOMMENT("( Ctx Switch 3D Enable:                      0x%08x)", RGXFWIF_INICFG_OS_CTXSWITCH_3D_EN);
	PDUMPCOMMENT("( Ctx Switch CDM Enable:                     0x%08x)", RGXFWIF_INICFG_OS_CTXSWITCH_CDM_EN);
	PDUMPCOMMENT("( Lower Priority Ctx Switch  2D Enable:      0x%08x)", RGXFWIF_INICFG_OS_LOW_PRIO_CS_TDM);
	PDUMPCOMMENT("( Lower Priority Ctx Switch  TA Enable:      0x%08x)", RGXFWIF_INICFG_OS_LOW_PRIO_CS_TA);
	PDUMPCOMMENT("( Lower Priority Ctx Switch  3D Enable:      0x%08x)", RGXFWIF_INICFG_OS_LOW_PRIO_CS_3D);
	PDUMPCOMMENT("( Lower Priority Ctx Switch CDM Enable:      0x%08x)", RGXFWIF_INICFG_OS_LOW_PRIO_CS_CDM);

	DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfFwOsDataMemDesc,
							  offsetof(RGXFWIF_OSDATA, ui32FwOsConfigFlags),
							  ui32FwOsCfgFlags,
							  PDUMP_FLAGS_CONTINUOUS);


#if defined(SUPPORT_SECURITY_VALIDATION)
	PDUMPCOMMENT("(Select one or more security tests here)");
	PDUMPCOMMENT("( Read/write FW private data from non-FW contexts: 0x%08x)", RGXFWIF_SECURE_ACCESS_TEST_READ_WRITE_FW_DATA);
	PDUMPCOMMENT("( Read/write FW code from non-FW contexts:         0x%08x)", RGXFWIF_SECURE_ACCESS_TEST_READ_WRITE_FW_CODE);
	PDUMPCOMMENT("( Execute FW code from non-secure memory:          0x%08x)", RGXFWIF_SECURE_ACCESS_TEST_RUN_FROM_NONSECURE);
	PDUMPCOMMENT("( Execute FW code from secure (non-FW) memory:     0x%08x)", RGXFWIF_SECURE_ACCESS_TEST_RUN_FROM_SECURE);

	DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfSysInitMemDesc,
	                          offsetof(RGXFWIF_SYSINIT, ui32SecurityTestFlags),
	                          psDevInfo->psRGXFWIfSysInit->ui32SecurityTestFlags,
	                          PDUMP_FLAGS_CONTINUOUS);
#endif

	PDUMPCOMMENT("( PID filter type: %X=INCLUDE_ALL_EXCEPT, %X=EXCLUDE_ALL_EXCEPT)",
			RGXFW_PID_FILTER_INCLUDE_ALL_EXCEPT,
			RGXFW_PID_FILTER_EXCLUDE_ALL_EXCEPT);

	DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfSysInitMemDesc,
			offsetof(RGXFWIF_SYSINIT, sPIDFilter.eMode),
			psDevInfo->psRGXFWIfSysInit->sPIDFilter.eMode,
			PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("( PID filter PID/OSID list (Up to %u entries. Terminate with a zero PID))",
			RGXFWIF_PID_FILTER_MAX_NUM_PIDS);
	{
		IMG_UINT32 i;

		/* generate a few WRWs in the pdump stream as an example */
		for (i = 0; i < MIN(RGXFWIF_PID_FILTER_MAX_NUM_PIDS, 8); i++)
		{
			/*
			 * Some compilers cannot cope with the uses of offsetof() below - the specific problem being the use of
			 * a non-const variable in the expression, which it needs to be const. Typical compiler output is
			 * "expression must have a constant value".
			 */
			const IMG_DEVMEM_OFFSET_T uiPIDOff
			= (IMG_DEVMEM_OFFSET_T)(uintptr_t)&(((RGXFWIF_SYSINIT *)0)->sPIDFilter.asItems[i].uiPID);

			const IMG_DEVMEM_OFFSET_T uiOSIDOff
			= (IMG_DEVMEM_OFFSET_T)(uintptr_t)&(((RGXFWIF_SYSINIT *)0)->sPIDFilter.asItems[i].ui32OSID);

			PDUMPCOMMENT("(PID and OSID pair %u)", i);

			PDUMPCOMMENT("(PID)");
			DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfSysInitMemDesc,
									  uiPIDOff,
									  0,
									  PDUMP_FLAGS_CONTINUOUS);

			PDUMPCOMMENT("(OSID)");
			DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfSysInitMemDesc,
									  uiOSIDOff,
									  0,
									  PDUMP_FLAGS_CONTINUOUS);
		}
	}

	/*
	 * Dump the log config so it can be edited.
	 */
	PDUMPCOMMENT("(Set the log config here)");
	PDUMPCOMMENT("( Log Type: set bit 0 for TRACE, reset for TBI)");
	PDUMPCOMMENT("( MAIN Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_MAIN);
	PDUMPCOMMENT("( MTS Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_MTS);
	PDUMPCOMMENT("( CLEANUP Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_CLEANUP);
	PDUMPCOMMENT("( CSW Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_CSW);
	PDUMPCOMMENT("( BIF Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_BIF);
	PDUMPCOMMENT("( PM Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_PM);
	PDUMPCOMMENT("( RTD Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_RTD);
	PDUMPCOMMENT("( SPM Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_SPM);
	PDUMPCOMMENT("( POW Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_POW);
	PDUMPCOMMENT("( HWR Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_HWR);
	PDUMPCOMMENT("( HWP Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_HWP);

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, META_DMA))
	{
		PDUMPCOMMENT("( DMA Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_DMA);
	}

	PDUMPCOMMENT("( MISC Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_MISC);
	PDUMPCOMMENT("( DEBUG Group Enable: 0x%08x)", RGXFWIF_LOG_TYPE_GROUP_DEBUG);
	DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfTraceBufCtlMemDesc,
							  offsetof(RGXFWIF_TRACEBUF, ui32LogType),
							  psDevInfo->psRGXFWIfTraceBufCtl->ui32LogType,
							  PDUMP_FLAGS_CONTINUOUS);

	PDUMPCOMMENT("Set the HWPerf Filter config here");
	DevmemPDumpLoadMemValue64(psDevInfo->psRGXFWIfSysInitMemDesc,
							  offsetof(RGXFWIF_SYSINIT, ui64HWPerfFilter),
							  psDevInfo->psRGXFWIfSysInit->ui64HWPerfFilter,
							  PDUMP_FLAGS_CONTINUOUS);

#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	PDUMPCOMMENT("(Number of registers configurations for types(byte index): pow on(%d), dust change(%d), ta(%d), 3d(%d), cdm(%d), tla(%d), TDM(%d))",
				 RGXFWIF_REG_CFG_TYPE_PWR_ON,
				 RGXFWIF_REG_CFG_TYPE_DUST_CHANGE,
				 RGXFWIF_REG_CFG_TYPE_TA,
				 RGXFWIF_REG_CFG_TYPE_3D,
				 RGXFWIF_REG_CFG_TYPE_CDM,
				 RGXFWIF_REG_CFG_TYPE_TLA,
				 RGXFWIF_REG_CFG_TYPE_TDM);

	{
		IMG_UINT32 i;

		/* Write 32 bits in each iteration as required by PDUMP WRW command */
		for (i = 0; i < RGXFWIF_REG_CFG_TYPE_ALL; i += sizeof(IMG_UINT32))
		{
			DevmemPDumpLoadMemValue32(psDevInfo->psRGXFWIfRegCfgMemDesc,
									offsetof(RGXFWIF_REG_CFG, aui8NumRegsType[i]),
									0,
									PDUMP_FLAGS_CONTINUOUS);
		}
	}

	PDUMPCOMMENT("(Set registers here: address, mask, value)");
	DevmemPDumpLoadMemValue64(psDevInfo->psRGXFWIfRegCfgMemDesc,
							  offsetof(RGXFWIF_REG_CFG, asRegConfigs[0].ui64Addr),
							  0,
							  PDUMP_FLAGS_CONTINUOUS);
	DevmemPDumpLoadMemValue64(psDevInfo->psRGXFWIfRegCfgMemDesc,
							  offsetof(RGXFWIF_REG_CFG, asRegConfigs[0].ui64Mask),
							  0,
							  PDUMP_FLAGS_CONTINUOUS);
	DevmemPDumpLoadMemValue64(psDevInfo->psRGXFWIfRegCfgMemDesc,
							  offsetof(RGXFWIF_REG_CFG, asRegConfigs[0].ui64Value),
							  0,
							  PDUMP_FLAGS_CONTINUOUS);
#endif /* SUPPORT_USER_REGISTER_CONFIGURATION */
}
#endif /* defined(PDUMP) */

#if defined(MTK_GPU_BM_SUPPORT)
static PVRSRV_ERROR MTKJobStatusResources(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	DEVMEM_FLAGS_T     uiMemAllocFlags;
	PVRSRV_ERROR       eError = PVRSRV_OK;
	int                size = 4096;
	IMG_BOOL           valid;

	uiMemAllocFlags = PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(PMMETA_PROTECT)
				| PVRSRV_MEMALLOCFLAG_GPU_READABLE
				| PVRSRV_MEMALLOCFLAG_GPU_WRITEABLE
				| PVRSRV_MEMALLOCFLAG_CPU_READABLE
				| PVRSRV_MEMALLOCFLAG_CPU_WRITEABLE
				| PVRSRV_MEMALLOCFLAG_KERNEL_CPU_MAPPABLE
				| PVRSRV_MEMALLOCFLAG_UNCACHED;

	eError = DevmemFwAllocate(psDevInfo,
							size,
							uiMemAllocFlags,
							"FwJobStatusBuf",
							&psDevInfo->psJobStatusQOSBufMemDesc);

	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to allocate %d bytes for jobstatus (%u)",
				__func__, size, eError));
		goto fail;
	}

	RGXSetFirmwareAddress(&psDevInfo->psRGXFWIfRuntimeCfg->sJobStatsQOS,
						psDevInfo->psJobStatusQOSBufMemDesc,
						0, RFW_FWADDR_NOREF_FLAG);

	eError = DevmemAcquireCpuVirtAddr(psDevInfo->psJobStatusQOSBufMemDesc,
									  (void **)&psDevInfo->psJobStatusQOSBuf);

	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to acquire cpu_addr (%u)",
				__func__, eError));
		goto fail;
	}

	eError = PMR_CpuPhysAddr(
			psDevInfo->psJobStatusQOSBufMemDesc->psImport->hPMR,
			1, 1, 0,
			&psDevInfo->ui32JobStatusQOSPhyAddr,
			&valid);

	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to acquire phy_addr (%u)",
				__func__, eError));
		goto fail;
	}

	MTKGPUQoS_setup(
		(uint32_t *)psDevInfo->psJobStatusQOSBuf,
		psDevInfo->ui32JobStatusQOSPhyAddr.uiAddr, size);

fail:
	return eError;
}
#endif

/*!
*******************************************************************************
 @Function    RGXSetupFwSysData

 @Description Setups all system-wide firmware related data

 @Input       psDevInfo

 @Return      PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXSetupFwSysData(PVRSRV_DEVICE_NODE       *psDeviceNode,
									  IMG_BOOL                 bEnableSignatureChecks,
									  IMG_UINT32               ui32SignatureChecksBufSize,
									  IMG_UINT32               ui32HWPerfFWBufSizeKB,
									  IMG_UINT64               ui64HWPerfFilter,
									  IMG_UINT32               ui32RGXFWAlignChecksArrLength,
									  IMG_UINT32               *pui32RGXFWAlignChecks,
									  IMG_UINT32               ui32ConfigFlags,
									  IMG_UINT32               ui32ConfigFlagsExt,
									  IMG_UINT32               ui32LogType,
									  IMG_UINT32               ui32FilterFlags,
									  IMG_UINT32               ui32JonesDisableMask,
									  IMG_UINT32               ui32HWPerfCountersDataSize,
									  IMG_UINT32               *pui32TPUTrilinearFracMask,
									  RGX_RD_POWER_ISLAND_CONF eRGXRDPowerIslandConf,
									  FW_PERF_CONF             eFirmwarePerf)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	RGXFWIF_SYSINIT *psFwSysInitScratch = NULL;

	psFwSysInitScratch = OSAllocZMem(sizeof(*psFwSysInitScratch));
	PVR_LOG_GOTO_IF_NOMEM(psFwSysInitScratch, eError, fail);

	/* Sys Fw init data */
	eError = RGXSetupFwAllocation(psDevInfo,
								  (RGX_FWSHAREDMEM_ALLOCFLAGS |
								   PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
								   PVRSRV_MEMALLOCFLAG_FW_ALLOC_CONFIG) &
								   RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  sizeof(RGXFWIF_SYSINIT),
								  "FwSysInitStructure",
								  &psDevInfo->psRGXFWIfSysInitMemDesc,
								  NULL,
								  (void**) &psDevInfo->psRGXFWIfSysInit,
								  RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware Sys Init structure allocation", fail);

	/* Setup Fault read register */
	eError = RGXSetupFaultReadRegister(psDeviceNode, psFwSysInitScratch);
	PVR_LOG_GOTO_IF_ERROR(eError, "Fault read register setup", fail);

	/* RD Power Island */
	{
		RGX_DATA *psRGXData = (RGX_DATA*) psDeviceNode->psDevConfig->hDevData;
		IMG_BOOL bSysEnableRDPowIsland = psRGXData->psRGXTimingInfo->bEnableRDPowIsland;
		IMG_BOOL bEnableRDPowIsland = ((eRGXRDPowerIslandConf == RGX_RD_POWER_ISLAND_DEFAULT) && bSysEnableRDPowIsland) ||
										(eRGXRDPowerIslandConf == RGX_RD_POWER_ISLAND_FORCE_ON);

		ui32ConfigFlags |= bEnableRDPowIsland? RGXFWIF_INICFG_POW_RASCALDUST : 0;
	}

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	ui32ConfigFlags |= RGXFWIF_INICFG_WORKEST;
#if defined(SUPPORT_PDVFS)
    {
        RGXFWIF_PDVFS_OPP   *psPDVFSOPPInfo;
	    IMG_DVFS_DEVICE_CFG *psDVFSDeviceCfg;

        /* Pro-active DVFS depends on Workload Estimation */
        psPDVFSOPPInfo = &psFwSysInitScratch->sPDVFSOPPInfo;
        psDVFSDeviceCfg = &psDeviceNode->psDevConfig->sDVFS.sDVFSDeviceCfg;
        PVR_LOG_IF_FALSE(psDVFSDeviceCfg->pasOPPTable, "RGXSetupFwSysData: Missing OPP Table");

        if (psDVFSDeviceCfg->pasOPPTable != NULL)
        {
            if (psDVFSDeviceCfg->ui32OPPTableSize > ARRAY_SIZE(psPDVFSOPPInfo->asOPPValues))
            {
                PVR_DPF((PVR_DBG_ERROR,
                        "%s: OPP Table too large: Size = %u, Maximum size = %lu",
                        __func__,
                        psDVFSDeviceCfg->ui32OPPTableSize,
                        (unsigned long)(ARRAY_SIZE(psPDVFSOPPInfo->asOPPValues))));
                eError = PVRSRV_ERROR_INVALID_PARAMS;
                goto fail;
            }

            OSDeviceMemCopy(psPDVFSOPPInfo->asOPPValues,
                    psDVFSDeviceCfg->pasOPPTable,
                    sizeof(psPDVFSOPPInfo->asOPPValues));

            psPDVFSOPPInfo->ui32MaxOPPPoint = psDVFSDeviceCfg->ui32OPPTableSize - 1;

            ui32ConfigFlags |= RGXFWIF_INICFG_PDVFS;
        }
    }
#endif /* defined(SUPPORT_PDVFS) */
#endif /* defined(SUPPORT_WORKLOAD_ESTIMATION) */

	/* FW trace control structure */
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  sizeof(RGXFWIF_TRACEBUF),
								  "FwTraceCtlStruct",
								  &psDevInfo->psRGXFWIfTraceBufCtlMemDesc,
								  &psFwSysInitScratch->sTraceBufCtl,
								  (void**) &psDevInfo->psRGXFWIfTraceBufCtl,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	if (!psDeviceNode->bAutoVzFwIsUp)
	{
		/* Set initial firmware log type/group(s) */
		if (ui32LogType & ~RGXFWIF_LOG_TYPE_MASK)
		{
			eError = PVRSRV_ERROR_INVALID_PARAMS;
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Invalid initial log type (0x%X)",
			         __func__, ui32LogType));
			goto fail;
		}
		psDevInfo->psRGXFWIfTraceBufCtl->ui32LogType = ui32LogType;
	}

	/* When PDUMP is enabled, ALWAYS allocate on-demand trace buffer resource
	 * (irrespective of loggroup(s) enabled), given that logtype/loggroups can
	 * be set during PDump playback in logconfig, at any point of time,
	 * Otherwise, allocate only if required. */
#if !defined(PDUMP)
#if defined(SUPPORT_AUTOVZ)
	/* always allocate trace buffer for AutoVz Host drivers to allow
	 * deterministic addresses of all SysData structures */
	if ((PVRSRV_VZ_MODE_IS(HOST)) || (RGXTraceBufferIsInitRequired(psDevInfo)))
#else
	if (RGXTraceBufferIsInitRequired(psDevInfo))
#endif
#endif
	{
		eError = RGXTraceBufferInitOnDemandResources(psDevInfo,
													 RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS &
													 RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp));
	}
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXTraceBufferInitOnDemandResources", fail);

	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  sizeof(RGXFWIF_SYSDATA),
								  "FwSysData",
								  &psDevInfo->psRGXFWIfFwSysDataMemDesc,
								  &psFwSysInitScratch->sFwSysData,
								  (void**) &psDevInfo->psRGXFWIfFwSysData,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	/* GPIO validation setup */
	psFwSysInitScratch->eGPIOValidationMode = RGXFWIF_GPIO_VAL_OFF;
#if defined(SUPPORT_VALIDATION)
	{
		IMG_INT32 ui32AppHintDefault;
		IMG_INT32 ui32GPIOValidationMode;
		void      *pvAppHintState = NULL;

		/* Check AppHint for GPIO validation mode */
		OSCreateKMAppHintState(&pvAppHintState);
		ui32AppHintDefault = PVRSRV_APPHINT_GPIOVALIDATIONMODE;
		OSGetKMAppHintUINT32(pvAppHintState,
							 GPIOValidationMode,
							 &ui32AppHintDefault,
							 &ui32GPIOValidationMode);
		OSFreeKMAppHintState(pvAppHintState);
		pvAppHintState = NULL;

		if (ui32GPIOValidationMode >= RGXFWIF_GPIO_VAL_LAST)
		{
			PVR_DPF((PVR_DBG_ERROR,
			         "%s: Invalid GPIO validation mode: %d, only valid if smaller than %d. Disabling GPIO validation.",
			         __func__,
			         ui32GPIOValidationMode,
			         RGXFWIF_GPIO_VAL_LAST));
		}
		else
		{
			psFwSysInitScratch->eGPIOValidationMode = (RGXFWIF_GPIO_VAL_MODE) ui32GPIOValidationMode;
		}

		psFwSysInitScratch->eGPIOValidationMode = ui32GPIOValidationMode;
	}
#endif

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
	eError = RGXFWSetupCounterBuffer(psDevInfo,
									 &psDevInfo->psCounterBufferMemDesc,
									 PAGE_SIZE,
									 &psFwSysInitScratch->sCounterDumpCtl,
									 "CounterBuffer");
	PVR_LOG_GOTO_IF_ERROR(eError, "Counter Buffer allocation", fail);
#endif /* defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS) */

#if defined(SUPPORT_FIRMWARE_GCOV)
	eError = RGXFWSetupFirmwareGcovBuffer(psDevInfo,
										  &psDevInfo->psFirmwareGcovBufferMemDesc,
										  RGXFWIF_FIRMWARE_GCOV_BUFFER_SIZE,
										  &psFwSysInitScratch->sFirmwareGcovCtl,
										  "FirmwareGcovBuffer");
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware GCOV buffer allocation", fail);
	psDevInfo->ui32FirmwareGcovSize = RGXFWIF_FIRMWARE_GCOV_BUFFER_SIZE;
#endif /* defined(SUPPORT_FIRMWARE_GCOV) */

#if defined(PDUMP)
	/* Require a minimum amount of memory for the signature buffers */
	if (ui32SignatureChecksBufSize < RGXFW_SIG_BUFFER_SIZE_MIN)
	{
		ui32SignatureChecksBufSize = RGXFW_SIG_BUFFER_SIZE_MIN;
	}

	/* Setup Signature and Checksum Buffers for TA and 3D */
	eError = RGXFWSetupSignatureChecks(psDevInfo,
									   &psDevInfo->psRGXFWSigTAChecksMemDesc,
									   ui32SignatureChecksBufSize,
									   &psFwSysInitScratch->asSigBufCtl[RGXFWIF_DM_GEOM],
									   "TA");
	PVR_LOG_GOTO_IF_ERROR(eError, "TA Signature check setup", fail);
	psDevInfo->ui32SigTAChecksSize = ui32SignatureChecksBufSize;

	eError = RGXFWSetupSignatureChecks(psDevInfo,
									   &psDevInfo->psRGXFWSig3DChecksMemDesc,
									   ui32SignatureChecksBufSize,
									   &psFwSysInitScratch->asSigBufCtl[RGXFWIF_DM_3D],
									   "3D");
	PVR_LOG_GOTO_IF_ERROR(eError, "3D Signature check setup", fail);
	psDevInfo->ui32Sig3DChecksSize = ui32SignatureChecksBufSize;

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, TDM_PDS_CHECKSUM))
	{
		/* Buffer allocated only when feature present because, all known TDM
		 * signature registers are dependent on this feature being present */
		eError = RGXFWSetupSignatureChecks(psDevInfo,
										   &psDevInfo->psRGXFWSigTDM2DChecksMemDesc,
										   ui32SignatureChecksBufSize,
										   &psFwSysInitScratch->asSigBufCtl[RGXFWIF_DM_TDM],
										   "TDM");
		PVR_LOG_GOTO_IF_ERROR(eError, "TDM Signature check setup", fail);
		psDevInfo->ui32SigTDM2DChecksSize = ui32SignatureChecksBufSize;
	}
	else
	{
		psDevInfo->psRGXFWSigTDM2DChecksMemDesc = NULL;
		psDevInfo->ui32SigTDM2DChecksSize = 0;
	}
#endif

	if (!bEnableSignatureChecks)
	{
		psFwSysInitScratch->asSigBufCtl[RGXFWIF_DM_TDM].sBuffer.ui32Addr = 0x0;
		psFwSysInitScratch->asSigBufCtl[RGXFWIF_DM_GEOM].sBuffer.ui32Addr = 0x0;
		psFwSysInitScratch->asSigBufCtl[RGXFWIF_DM_3D].sBuffer.ui32Addr = 0x0;
	}

	eError = RGXFWSetupAlignChecks(psDeviceNode,
								   &psFwSysInitScratch->sAlignChecks,
								   pui32RGXFWAlignChecks,
								   ui32RGXFWAlignChecksArrLength);
	PVR_LOG_GOTO_IF_ERROR(eError, "Alignment checks setup", fail);

	psFwSysInitScratch->ui32FilterFlags = ui32FilterFlags;

	if (RGX_IS_BRN_SUPPORTED(psDevInfo, 65273))
	{
		/* Fill the remaining bits of fw the init data */
		psFwSysInitScratch->sPDSExecBase.uiAddr = RGX_PDSCODEDATA_BRN_65273_HEAP_BASE;
		psFwSysInitScratch->sUSCExecBase.uiAddr = RGX_USCCODE_BRN_65273_HEAP_BASE;
	}
	else
	{
		/* Fill the remaining bits of fw the init data */
		psFwSysInitScratch->sPDSExecBase.uiAddr = RGX_PDSCODEDATA_HEAP_BASE;
		psFwSysInitScratch->sUSCExecBase.uiAddr = RGX_USCCODE_HEAP_BASE;
	}

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, S7_TOP_INFRASTRUCTURE))
	{
		psFwSysInitScratch->ui32JonesDisableMask = ui32JonesDisableMask;
	}

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, SLC_VIVT))
	{
		eError = _AllocateSLC3Fence(psDevInfo, psFwSysInitScratch);
		PVR_LOG_GOTO_IF_ERROR(eError, "SLC3Fence memory allocation", fail);
	}

#if defined(SUPPORT_PDVFS)
	/* Core clock rate */
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  sizeof(IMG_UINT32),
								  "FwPDVFSCoreClkRate",
								  &psDevInfo->psRGXFWIFCoreClkRateMemDesc,
								  &psFwSysInitScratch->sCoreClockRate,
								  (void**) &psDevInfo->pui32RGXFWIFCoreClkRate,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "PDVFS core clock rate memory setup", fail);
#endif

#if defined(SUPPORT_TBI_INTERFACE)
#if !defined(PDUMP)
	/* allocate only if required */
	if (RGXTBIBufferIsInitRequired(psDevInfo))
#endif /* !defined(PDUMP) */
	{
		/* When PDUMP is enabled, ALWAYS allocate on-demand TBI buffer resource
		 * (irrespective of loggroup(s) enabled), given that logtype/loggroups
		 * can be set during PDump playback in logconfig, at any point of time
		 */
		eError = RGXTBIBufferInitOnDemandResources(psDevInfo);
		PVR_LOG_GOTO_IF_ERROR(eError, "RGXTBIBufferInitOnDemandResources", fail);
	}

	psFwSysInitScratch->sTBIBuf = psDevInfo->sRGXFWIfTBIBuffer;
#endif /* defined(SUPPORT_TBI_INTERFACE) */

	/* Allocate shared buffer for GPU utilisation */
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  sizeof(RGXFWIF_GPU_UTIL_FWCB),
								  "FwGPUUtilisationBuffer",
								  &psDevInfo->psRGXFWIfGpuUtilFWCbCtlMemDesc,
								  &psFwSysInitScratch->sGpuUtilFWCbCtl,
								  (void**) &psDevInfo->psRGXFWIfGpuUtilFWCb,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "GPU Utilisation Buffer ctl allocation", fail);

	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_GPU_RO_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  sizeof(RGXFWIF_RUNTIME_CFG),
								  "FwRuntimeCfg",
								  &psDevInfo->psRGXFWIfRuntimeCfgMemDesc,
								  &psFwSysInitScratch->sRuntimeCfg,
								  (void**) &psDevInfo->psRGXFWIfRuntimeCfg,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware runtime configuration memory allocation", fail);

#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  sizeof(RGXFWIF_REG_CFG),
								  "FwRegisterConfigStructure",
								  &psDevInfo->psRGXFWIfRegCfgMemDesc,
								  &psFwSysInitScratch->sRegCfg,
								  NULL,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware register user configuration structure allocation", fail);
#endif

	psDevInfo->ui32RGXFWIfHWPerfBufSize = GetHwPerfBufferSize(ui32HWPerfFWBufSizeKB);
	/* Second stage initialisation or HWPerf, hHWPerfLock created in first
	 * stage. See RGXRegisterDevice() call to RGXHWPerfInit(). */
	if (psDevInfo->ui64HWPerfFilter == 0)
	{
		psDevInfo->ui64HWPerfFilter = ui64HWPerfFilter;
		psFwSysInitScratch->ui64HWPerfFilter = ui64HWPerfFilter;
	}
	else
	{
		/* The filter has already been modified. This can happen if
		 * pvr/apphint/EnableFTraceGPU was enabled. */
		psFwSysInitScratch->ui64HWPerfFilter = psDevInfo->ui64HWPerfFilter;
	}

#if !defined(PDUMP)
	/* Allocate if HWPerf filter has already been set. This is possible either
	 * by setting a proper AppHint or enabling GPU ftrace events. */
	if (psDevInfo->ui64HWPerfFilter != 0)
#endif
	{
		/* When PDUMP is enabled, ALWAYS allocate on-demand HWPerf resources
		 * (irrespective of HWPerf enabled or not), given that HWPerf can be
		 * enabled during PDump playback via RTCONF at any point of time. */
		eError = RGXHWPerfInitOnDemandResources(psDevInfo);
		PVR_LOG_GOTO_IF_ERROR(eError, "RGXHWPerfInitOnDemandResources", fail);
	}

#if defined(MTK_GPU_BM_SUPPORT)
	MTKJobStatusResources(psDevInfo);
#endif

	RGXHWPerfInitAppHintCallbacks(psDeviceNode);

	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS &
								  RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp),
								  ui32HWPerfCountersDataSize,
								  "FwHWPerfControlStructure",
								  &psDevInfo->psRGXFWIfHWPerfCountersMemDesc,
								  &psFwSysInitScratch->sHWPerfCtl,
								  NULL,
								  RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware HW Perf control struct allocation", fail);

	psDevInfo->bPDPEnabled = (ui32ConfigFlags & RGXFWIF_INICFG_DISABLE_PDP_EN)
							  ? IMG_FALSE : IMG_TRUE;

	psFwSysInitScratch->eFirmwarePerf = eFirmwarePerf;

#if defined(PDUMP)
	/* default: no filter */
	psFwSysInitScratch->sPIDFilter.eMode = RGXFW_PID_FILTER_INCLUDE_ALL_EXCEPT;
	psFwSysInitScratch->sPIDFilter.asItems[0].uiPID = 0;
#endif

#if defined(SUPPORT_VALIDATION)
	{
		IMG_UINT32 dm;

		/* TPU trilinear rounding mask override */
		for (dm = 0; dm < RGXFWIF_TPU_DM_LAST; dm++)
		{
			psFwSysInitScratch->aui32TPUTrilinearFracMask[dm] = pui32TPUTrilinearFracMask[dm];
		}
	}
#endif

#if defined(SUPPORT_SECURITY_VALIDATION)
	PDUMPCOMMENT("Allocate non-secure buffer for security validation test");
	eError = DevmemFwAllocateExportable(psDeviceNode,
	                                    OSGetPageSize(),
	                                    OSGetPageSize(),
	                                    RGX_FWSHAREDMEM_ALLOCFLAGS,
	                                    "FwExNonSecureBuffer",
	                                    &psDevInfo->psRGXFWIfNonSecureBufMemDesc);
	PVR_LOG_GOTO_IF_ERROR(eError, "Non-secure buffer allocation", fail);

	eError = RGXSetFirmwareAddress(&psFwSysInitScratch->pbNonSecureBuffer,
	                      psDevInfo->psRGXFWIfNonSecureBufMemDesc,
	                      0, RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:1", fail);

	PDUMPCOMMENT("Allocate secure buffer for security validation test");
	eError = DevmemFwAllocateExportable(psDeviceNode,
	                                    OSGetPageSize(),
	                                    OSGetPageSize(),
	                                    RGX_FWSHAREDMEM_ALLOCFLAGS |
	                                    PVRSRV_MEMALLOCFLAG_VAL_SECURE_BUFFER,
	                                    "FwExSecureBuffer",
	                                    &psDevInfo->psRGXFWIfSecureBufMemDesc);
	PVR_LOG_GOTO_IF_ERROR(eError, "Secure buffer allocation", fail);

	eError = RGXSetFirmwareAddress(&psFwSysInitScratch->pbSecureBuffer,
	                      psDevInfo->psRGXFWIfSecureBufMemDesc,
	                      0, RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetFirmwareAddress:2", fail);
#endif /* SUPPORT_SECURITY_VALIDATION */

	/* Initialize FW started flag */
	psFwSysInitScratch->bFirmwareStarted = IMG_FALSE;
	psFwSysInitScratch->ui32MarkerVal = 1;

	if (!psDeviceNode->bAutoVzFwIsUp)
	{
		RGX_DATA *psRGXData = (RGX_DATA*) psDeviceNode->psDevConfig->hDevData;
		RGXFWIF_RUNTIME_CFG *psRuntimeCfg = psDevInfo->psRGXFWIfRuntimeCfg;

		/* Required info by FW to calculate the ActivePM idle timer latency */
		psFwSysInitScratch->ui32InitialCoreClockSpeed = psRGXData->psRGXTimingInfo->ui32CoreClockSpeed;
		psFwSysInitScratch->ui32ActivePMLatencyms = psRGXData->psRGXTimingInfo->ui32ActivePMLatencyms;

		/* Initialise variable runtime configuration to the system defaults */
		psRuntimeCfg->ui32CoreClockSpeed = psFwSysInitScratch->ui32InitialCoreClockSpeed;
		psRuntimeCfg->ui32ActivePMLatencyms = psFwSysInitScratch->ui32ActivePMLatencyms;
		psRuntimeCfg->bActivePMLatencyPersistant = IMG_TRUE;

		/* Initialize the DefaultDustsNumInit Field to Max Dusts */
		psRuntimeCfg->ui32DefaultDustsNumInit = psDevInfo->sDevFeatureCfg.ui32MAXDustCount;

		/* Setup FW coremem data */
		if (psDevInfo->psRGXFWIfCorememDataStoreMemDesc)
		{
			IMG_BOOL bMetaDMA = RGX_IS_FEATURE_SUPPORTED(psDevInfo, META_DMA);

			psFwSysInitScratch->sCorememDataStore.pbyFWAddr = psDevInfo->sFWCorememDataStoreFWAddr;

			if (bMetaDMA)
			{
				RGXSetMetaDMAAddress(&psFwSysInitScratch->sCorememDataStore,
						psDevInfo->psRGXFWIfCorememDataStoreMemDesc,
						&psFwSysInitScratch->sCorememDataStore.pbyFWAddr,
						0);
			}
		}

		psDevInfo->psRGXFWIfFwSysData->ui32ConfigFlags    = ui32ConfigFlags    & RGXFWIF_INICFG_ALL;
		psDevInfo->psRGXFWIfFwSysData->ui32ConfigFlagsExt = ui32ConfigFlagsExt & RGXFWIF_INICFG_EXT_ALL;

		/* Initialise GPU utilisation buffer */
		psDevInfo->psRGXFWIfGpuUtilFWCb->ui64LastWord =
				RGXFWIF_GPU_UTIL_MAKE_WORD(OSClockns64(),RGXFWIF_GPU_UTIL_STATE_IDLE);

		/* init HWPERF data */
		psDevInfo->psRGXFWIfFwSysData->ui32HWPerfRIdx = 0;
		psDevInfo->psRGXFWIfFwSysData->ui32HWPerfWIdx = 0;
		psDevInfo->psRGXFWIfFwSysData->ui32HWPerfWrapCount = 0;
		psDevInfo->psRGXFWIfFwSysData->ui32HWPerfSize = psDevInfo->ui32RGXFWIfHWPerfBufSize;
		psDevInfo->psRGXFWIfFwSysData->ui32HWPerfUt = 0;
		psDevInfo->psRGXFWIfFwSysData->ui32HWPerfDropCount = 0;
		psDevInfo->psRGXFWIfFwSysData->ui32FirstDropOrdinal = 0;
		psDevInfo->psRGXFWIfFwSysData->ui32LastDropOrdinal = 0;
#if defined(SUPPORT_POWMON_COMPONENT)
		psDevInfo->psRGXFWIfFwSysData->ui32PowMonEstimate = 0;
#endif

		/*Send through the BVNC Feature Flags*/
		eError = RGXServerFeatureFlagsToHWPerfFlags(psDevInfo, &psFwSysInitScratch->sBvncKmFeatureFlags);
		PVR_LOG_GOTO_IF_ERROR(eError, "RGXServerFeatureFlagsToHWPerfFlags", fail);

		/* populate the real FwOsInit structure with the values stored in the scratch copy */
		OSDeviceMemCopy(psDevInfo->psRGXFWIfSysInit, psFwSysInitScratch, sizeof(RGXFWIF_SYSINIT));
	}

	OSFreeMem(psFwSysInitScratch);

	return PVRSRV_OK;

fail:
	if (psFwSysInitScratch)
	{
		OSFreeMem(psFwSysInitScratch);
	}

	RGXFreeFwSysData(psDevInfo);

	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

/*!
*******************************************************************************
 @Function    RGXSetupFwOsData

 @Description Sets up all os-specific firmware related data

 @Input       psDevInfo

 @Return      PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXSetupFwOsData(PVRSRV_DEVICE_NODE       *psDeviceNode,
									 IMG_UINT32               ui32HWRDebugDumpLimit,
									 IMG_UINT32               ui32FwOsCfgFlags)
{
	PVRSRV_ERROR       eError;
	RGXFWIF_OSINIT     sFwOsInitScratch;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	IMG_UINT32         ui32kCCBSize = (!PVRSRV_VZ_MODE_IS(NATIVE) &&
	                                  !(psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_GPU_VIRTUALISATION_BIT_MASK)) ?
	                                  (RGXFWIF_KCCB_NUMCMDS_LOG2_GPUVIRT_WITHOUT_FEATURE) : (RGXFWIF_KCCB_NUMCMDS_LOG2_DEFAULT);

	OSCachedMemSet(&sFwOsInitScratch, 0, sizeof(RGXFWIF_OSINIT));

	/* Memory tracking the connection state should be non-volatile and
	 * is not cleared on allocation to prevent loss of pre-reset information */
	eError = RGXSetupFwAllocation(psDevInfo,
								  (RGX_FWSHAREDMEM_ALLOCFLAGS |
								   PVRSRV_MEMALLOCFLAG_FW_ALLOC_CONFIG)
								   & (~PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC),
								  sizeof(RGXFWIF_CONNECTION_CTL),
								  "FwConnectionCtl",
								  &psDevInfo->psRGXFWIfConnectionCtlMemDesc,
								  NULL,
								  (void**) &psDevInfo->psRGXFWIfConnectionCtl,
								  RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware Connection Control structure allocation", fail);

	eError = RGXSetupFwAllocation(psDevInfo,
								  (RGX_FWSHAREDMEM_ALLOCFLAGS |
								   PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED) |
								   PVRSRV_MEMALLOCFLAG_FW_ALLOC_CONFIG),
								  sizeof(RGXFWIF_OSINIT),
								  "FwOsInitStructure",
								  &psDevInfo->psRGXFWIfOsInitMemDesc,
								  NULL,
								  (void**) &psDevInfo->psRGXFWIfOsInit,
								  RFW_FWADDR_FLAG_NONE);
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware Os Init structure allocation", fail);

	/* init HWR frame info */
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS,
								  sizeof(RGXFWIF_HWRINFOBUF),
								  "FwHWRInfoBuffer",
								  &psDevInfo->psRGXFWIfHWRInfoBufCtlMemDesc,
								  &sFwOsInitScratch.sRGXFWIfHWRInfoBufCtl,
								  (void**) &psDevInfo->psRGXFWIfHWRInfoBufCtl,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "HWR Info Buffer allocation", fail);

	/* Might be uncached. Be conservative and use a DeviceMemSet */
	OSDeviceMemSet(psDevInfo->psRGXFWIfHWRInfoBufCtl, 0, sizeof(RGXFWIF_HWRINFOBUF));

	/* Allocate a sync for power management */
	eError = SyncPrimContextCreate(psDevInfo->psDeviceNode,
	                               &psDevInfo->hSyncPrimContext);
	PVR_LOG_GOTO_IF_ERROR(eError, "Sync primitive context allocation", fail);

	eError = SyncPrimAlloc(psDevInfo->hSyncPrimContext, &psDevInfo->psPowSyncPrim, "fw power ack");
	PVR_LOG_GOTO_IF_ERROR(eError, "Sync primitive allocation", fail);

	/* Set up kernel CCB */
	eError = RGXSetupCCB(psDevInfo,
						 &psDevInfo->psKernelCCBCtl,
						 &psDevInfo->psKernelCCBCtlMemDesc,
						 &psDevInfo->psKernelCCB,
						 &psDevInfo->psKernelCCBMemDesc,
						 &sFwOsInitScratch.psKernelCCBCtl,
						 &sFwOsInitScratch.psKernelCCB,
						 ui32kCCBSize,
						 sizeof(RGXFWIF_KCCB_CMD),
						 (RGX_FWSHAREDMEM_GPU_RO_ALLOCFLAGS |
						 PVRSRV_MEMALLOCFLAG_DEVICE_FLAG(FIRMWARE_CACHED)),
						 "FwKernelCCB");
	PVR_LOG_GOTO_IF_ERROR(eError, "Kernel CCB allocation", fail);

	/* KCCB additionally uses a return slot array for FW to be able to send back
	 * return codes for each required command
	 */
	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_ALLOCFLAGS,
								  (1U << ui32kCCBSize) * sizeof(IMG_UINT32),
								  "FwKernelCCBRtnSlots",
								  &psDevInfo->psKernelCCBRtnSlotsMemDesc,
								  &sFwOsInitScratch.psKernelCCBRtnSlots,
								  (void**) &psDevInfo->pui32KernelCCBRtnSlots,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "Kernel CCB return slot array allocation", fail);

	/* Set up firmware CCB */
	eError = RGXSetupCCB(psDevInfo,
						 &psDevInfo->psFirmwareCCBCtl,
						 &psDevInfo->psFirmwareCCBCtlMemDesc,
						 &psDevInfo->psFirmwareCCB,
						 &psDevInfo->psFirmwareCCBMemDesc,
						 &sFwOsInitScratch.psFirmwareCCBCtl,
						 &sFwOsInitScratch.psFirmwareCCB,
						 RGXFWIF_FWCCB_NUMCMDS_LOG2,
						 sizeof(RGXFWIF_FWCCB_CMD),
						 RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS,
						 "FwCCB");
	PVR_LOG_GOTO_IF_ERROR(eError, "Firmware CCB allocation", fail);

#if defined(PVRSRV_SYNC_CHECKPOINT_CCB)
	/* Set up checkpoint CCB */
	eError = RGXSetupCCB(psDevInfo,
						 &psDevInfo->psCheckpointCCBCtl,
						 &psDevInfo->psCheckpointCCBCtlMemDesc,
						 &psDevInfo->psCheckpointCCB,
						 &psDevInfo->psCheckpointCCBMemDesc,
						 &sFwOsInitScratch.psCheckpointCCBCtl,
						 &sFwOsInitScratch.psCheckpointCCB,
						 RGXFWIF_CHECKPOINTCCB_NUMCMDS_LOG2,
						 sizeof(PRGXFWIF_UFO_ADDR),
						 RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS,
						 "FwChkptCCB");
	PVR_LOG_GOTO_IF_ERROR(eError, "Checkpoint CCB allocation", fail);
#endif /* defined(PVRSRV_SYNC_CHECKPOINT_CCB) */

	eError = RGXSetupFwAllocation(psDevInfo,
								  RGX_FWSHAREDMEM_ALLOCFLAGS,
								  sizeof(RGXFWIF_OSDATA),
								  "FwOsData",
								  &psDevInfo->psRGXFWIfFwOsDataMemDesc,
								  &sFwOsInitScratch.sFwOsData,
								  (void**) &psDevInfo->psRGXFWIfFwOsData,
								  RFW_FWADDR_NOREF_FLAG);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXSetupFwAllocation", fail);

	psDevInfo->psRGXFWIfFwOsData->ui32FwOsConfigFlags = ui32FwOsCfgFlags & RGXFWIF_INICFG_OS_ALL;

	eError = SyncPrimGetFirmwareAddr(psDevInfo->psPowSyncPrim, &psDevInfo->psRGXFWIfFwOsData->sPowerSync.ui32Addr);
	PVR_LOG_GOTO_IF_ERROR(eError, "Get Sync Prim FW address", fail);

	sFwOsInitScratch.ui32HWRDebugDumpLimit = ui32HWRDebugDumpLimit;

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	/* Set up Workload Estimation firmware CCB */
	eError = RGXSetupCCB(psDevInfo,
						 &psDevInfo->psWorkEstFirmwareCCBCtl,
						 &psDevInfo->psWorkEstFirmwareCCBCtlMemDesc,
						 &psDevInfo->psWorkEstFirmwareCCB,
						 &psDevInfo->psWorkEstFirmwareCCBMemDesc,
						 &sFwOsInitScratch.psWorkEstFirmwareCCBCtl,
						 &sFwOsInitScratch.psWorkEstFirmwareCCB,
						 RGXFWIF_WORKEST_FWCCB_NUMCMDS_LOG2,
						 sizeof(RGXFWIF_WORKEST_FWCCB_CMD),
						 RGX_FWSHAREDMEM_CPU_RO_ALLOCFLAGS,
						 "FwWEstCCB");
	PVR_LOG_GOTO_IF_ERROR(eError, "Workload Estimation Firmware CCB allocation", fail);
#endif /* defined(SUPPORT_WORKLOAD_ESTIMATION) */

	/* Initialise the compatibility check data */
	RGXFWIF_COMPCHECKS_BVNC_INIT(sFwOsInitScratch.sRGXCompChecks.sFWBVNC);
	RGXFWIF_COMPCHECKS_BVNC_INIT(sFwOsInitScratch.sRGXCompChecks.sHWBVNC);

	/* populate the real FwOsInit structure with the values stored in the scratch copy */
	OSDeviceMemCopy(psDevInfo->psRGXFWIfOsInit, &sFwOsInitScratch, sizeof(RGXFWIF_OSINIT));

	KM_SET_OS_CONNECTION(READY, psDevInfo);

	return PVRSRV_OK;

fail:
	RGXFreeFwOsData(psDevInfo);

	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

/*!
*******************************************************************************
 @Function    RGXSetupFirmware

 @Description Setups all firmware related data

 @Input       psDevInfo

 @Return      PVRSRV_ERROR
******************************************************************************/
PVRSRV_ERROR RGXSetupFirmware(PVRSRV_DEVICE_NODE       *psDeviceNode,
							  IMG_BOOL                 bEnableSignatureChecks,
							  IMG_UINT32               ui32SignatureChecksBufSize,
							  IMG_UINT32               ui32HWPerfFWBufSizeKB,
							  IMG_UINT64               ui64HWPerfFilter,
							  IMG_UINT32               ui32RGXFWAlignChecksArrLength,
							  IMG_UINT32               *pui32RGXFWAlignChecks,
							  IMG_UINT32               ui32ConfigFlags,
							  IMG_UINT32               ui32ConfigFlagsExt,
							  IMG_UINT32               ui32FwOsCfgFlags,
							  IMG_UINT32               ui32LogType,
							  IMG_UINT32               ui32FilterFlags,
							  IMG_UINT32               ui32JonesDisableMask,
							  IMG_UINT32               ui32HWRDebugDumpLimit,
							  IMG_UINT32               ui32HWPerfCountersDataSize,
							  IMG_UINT32               *pui32TPUTrilinearFracMask,
							  RGX_RD_POWER_ISLAND_CONF eRGXRDPowerIslandConf,
							  FW_PERF_CONF             eFirmwarePerf)
{
	PVRSRV_ERROR eError;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	eError = RGXSetupFwOsData(psDeviceNode, ui32HWRDebugDumpLimit, ui32FwOsCfgFlags);
	PVR_LOG_GOTO_IF_ERROR(eError, "Setting up firmware os data", fail);

	if (PVRSRV_VZ_MODE_IS(GUEST))
	{
		/* Guest drivers do not configure system-wide firmware data */
		psDevInfo->psRGXFWIfSysInit = NULL;
	}
	else
	{
		/* Native and Host drivers must initialise the firmware's system data */
		eError = RGXSetupFwSysData(psDeviceNode,
								   bEnableSignatureChecks,
								   ui32SignatureChecksBufSize,
								   ui32HWPerfFWBufSizeKB,
								   ui64HWPerfFilter,
								   ui32RGXFWAlignChecksArrLength,
								   pui32RGXFWAlignChecks,
								   ui32ConfigFlags,
								   ui32ConfigFlagsExt,
								   ui32LogType,
								   ui32FilterFlags,
								   ui32JonesDisableMask,
								   ui32HWPerfCountersDataSize,
								   pui32TPUTrilinearFracMask,
								   eRGXRDPowerIslandConf,
								   eFirmwarePerf);
		PVR_LOG_GOTO_IF_ERROR(eError, "Setting up firmware system data", fail);
	}

	psDevInfo->bFirmwareInitialised = IMG_TRUE;

#if defined(PDUMP)
	RGXPDumpLoadFWInitData(psDevInfo,
					       ui32HWPerfCountersDataSize,
					       bEnableSignatureChecks);
#endif /* PDUMP */

fail:
	return eError;
}

/*!
*******************************************************************************
 @Function    RGXFreeFwSysData

 @Description Frees all system-wide firmware related data

 @Input       psDevInfo
******************************************************************************/
static void RGXFreeFwSysData(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	psDevInfo->bFirmwareInitialised = IMG_FALSE;

	if (psDevInfo->psRGXFWAlignChecksMemDesc)
	{
		RGXFWFreeAlignChecks(psDevInfo);
	}

#if defined(PDUMP)
	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, TDM_PDS_CHECKSUM) &&
	    psDevInfo->psRGXFWSigTDM2DChecksMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWSigTDM2DChecksMemDesc);
		psDevInfo->psRGXFWSigTDM2DChecksMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWSigTAChecksMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWSigTAChecksMemDesc);
		psDevInfo->psRGXFWSigTAChecksMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWSig3DChecksMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWSig3DChecksMemDesc);
		psDevInfo->psRGXFWSig3DChecksMemDesc = NULL;
	}
#endif

#if defined(SUPPORT_POWER_SAMPLING_VIA_DEBUGFS)
	if (psDevInfo->psCounterBufferMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psCounterBufferMemDesc);
		psDevInfo->psCounterBufferMemDesc = NULL;
	}
#endif

#if defined(SUPPORT_FIRMWARE_GCOV)
	if (psDevInfo->psFirmwareGcovBufferMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psFirmwareGcovBufferMemDesc);
		psDevInfo->psFirmwareGcovBufferMemDesc = NULL;
	}
#endif

	RGXSetupFaultReadRegisterRollback(psDevInfo);

	if (psDevInfo->psRGXFWIfGpuUtilFWCbCtlMemDesc)
	{
		if (psDevInfo->psRGXFWIfGpuUtilFWCb != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfGpuUtilFWCbCtlMemDesc);
			psDevInfo->psRGXFWIfGpuUtilFWCb = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfGpuUtilFWCbCtlMemDesc);
		psDevInfo->psRGXFWIfGpuUtilFWCbCtlMemDesc = NULL;
	}

	RGXHWPerfDeinit(psDevInfo);

	if (psDevInfo->psRGXFWIfRuntimeCfgMemDesc)
	{
		if (psDevInfo->psRGXFWIfRuntimeCfg != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfRuntimeCfgMemDesc);
			psDevInfo->psRGXFWIfRuntimeCfg = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfRuntimeCfgMemDesc);
		psDevInfo->psRGXFWIfRuntimeCfgMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWIfCorememDataStoreMemDesc)
	{
		psDevInfo->psRGXFWIfCorememDataStoreMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWIfTraceBufCtlMemDesc)
	{
		if (psDevInfo->psRGXFWIfTraceBufCtl != NULL)
		{
			/* first deinit/free the tracebuffer allocation */
			RGXTraceBufferDeinit(psDevInfo);

			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfTraceBufCtlMemDesc);
			psDevInfo->psRGXFWIfTraceBufCtl = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfTraceBufCtlMemDesc);
		psDevInfo->psRGXFWIfTraceBufCtlMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWIfFwSysDataMemDesc)
	{
		if (psDevInfo->psRGXFWIfFwSysData != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfFwSysDataMemDesc);
			psDevInfo->psRGXFWIfFwSysData = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfFwSysDataMemDesc);
		psDevInfo->psRGXFWIfFwSysDataMemDesc = NULL;
	}

#if defined(SUPPORT_TBI_INTERFACE)
	if (psDevInfo->psRGXFWIfTBIBufferMemDesc)
	{
		RGXTBIBufferDeinit(psDevInfo);
	}
#endif

#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
	if (psDevInfo->psRGXFWIfRegCfgMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfRegCfgMemDesc);
		psDevInfo->psRGXFWIfRegCfgMemDesc = NULL;
	}
#endif
	if (psDevInfo->psRGXFWIfHWPerfCountersMemDesc)
	{
		RGXUnsetFirmwareAddress(psDevInfo->psRGXFWIfHWPerfCountersMemDesc);
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfHWPerfCountersMemDesc);
		psDevInfo->psRGXFWIfHWPerfCountersMemDesc = NULL;
	}

#if defined(SUPPORT_SECURITY_VALIDATION)
	if (psDevInfo->psRGXFWIfNonSecureBufMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfNonSecureBufMemDesc);
		psDevInfo->psRGXFWIfNonSecureBufMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWIfSecureBufMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfSecureBufMemDesc);
		psDevInfo->psRGXFWIfSecureBufMemDesc = NULL;
	}
#endif

	if (RGX_IS_FEATURE_SUPPORTED(psDevInfo, SLC_VIVT))
	{
		_FreeSLC3Fence(psDevInfo);
	}

#if defined(SUPPORT_PDVFS)
	if (psDevInfo->psRGXFWIFCoreClkRateMemDesc)
	{
		if (psDevInfo->pui32RGXFWIFCoreClkRate != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIFCoreClkRateMemDesc);
			psDevInfo->pui32RGXFWIFCoreClkRate = NULL;
		}

		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIFCoreClkRateMemDesc);
		psDevInfo->psRGXFWIFCoreClkRateMemDesc = NULL;
	}
#endif
}

/*!
*******************************************************************************
 @Function    RGXFreeFwOsData

 @Description Frees all os-specific firmware related data

 @Input       psDevInfo
******************************************************************************/
static void RGXFreeFwOsData(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFreeCCBReturnSlots(psDevInfo,
	                      &psDevInfo->pui32KernelCCBRtnSlots,
	                      &psDevInfo->psKernelCCBRtnSlotsMemDesc);
	RGXFreeCCB(psDevInfo,
	           &psDevInfo->psKernelCCBCtl,
	           &psDevInfo->psKernelCCBCtlMemDesc,
	           &psDevInfo->psKernelCCB,
	           &psDevInfo->psKernelCCBMemDesc);

	RGXFreeCCB(psDevInfo,
	           &psDevInfo->psFirmwareCCBCtl,
	           &psDevInfo->psFirmwareCCBCtlMemDesc,
	           &psDevInfo->psFirmwareCCB,
	           &psDevInfo->psFirmwareCCBMemDesc);

#if defined(PVRSRV_SYNC_CHECKPOINT_CCB)
	RGXFreeCCB(psDevInfo,
	           &psDevInfo->psCheckpointCCBCtl,
	           &psDevInfo->psCheckpointCCBCtlMemDesc,
	           &psDevInfo->psCheckpointCCB,
	           &psDevInfo->psCheckpointCCBMemDesc);
#endif

#if defined(SUPPORT_WORKLOAD_ESTIMATION)
	RGXFreeCCB(psDevInfo,
	           &psDevInfo->psWorkEstFirmwareCCBCtl,
	           &psDevInfo->psWorkEstFirmwareCCBCtlMemDesc,
	           &psDevInfo->psWorkEstFirmwareCCB,
	           &psDevInfo->psWorkEstFirmwareCCBMemDesc);
#endif

	if (psDevInfo->psPowSyncPrim != NULL)
	{
		SyncPrimFree(psDevInfo->psPowSyncPrim);
		psDevInfo->psPowSyncPrim = NULL;
	}

	if (psDevInfo->hSyncPrimContext != (IMG_HANDLE) NULL)
	{
		SyncPrimContextDestroy(psDevInfo->hSyncPrimContext);
		psDevInfo->hSyncPrimContext = (IMG_HANDLE) NULL;
	}

	if (psDevInfo->psRGXFWIfHWRInfoBufCtlMemDesc)
	{
		if (psDevInfo->psRGXFWIfHWRInfoBufCtl != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfHWRInfoBufCtlMemDesc);
			psDevInfo->psRGXFWIfHWRInfoBufCtl = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfHWRInfoBufCtlMemDesc);
		psDevInfo->psRGXFWIfHWRInfoBufCtlMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWIfFwOsDataMemDesc)
	{
		if (psDevInfo->psRGXFWIfFwOsData != NULL)
		{
			DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfFwOsDataMemDesc);
			psDevInfo->psRGXFWIfFwOsData = NULL;
		}
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfFwOsDataMemDesc);
		psDevInfo->psRGXFWIfFwOsDataMemDesc = NULL;
	}
}

/*!
*******************************************************************************
 @Function    RGXFreeFirmware

 @Description Frees all the firmware-related allocations

 @Input       psDevInfo
******************************************************************************/
void RGXFreeFirmware(PVRSRV_RGXDEV_INFO	*psDevInfo)
{
	RGXFreeFwOsData(psDevInfo);

	if (psDevInfo->psRGXFWIfConnectionCtl)
	{
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfConnectionCtlMemDesc);
		psDevInfo->psRGXFWIfConnectionCtl = NULL;
	}

	if (psDevInfo->psRGXFWIfConnectionCtlMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfConnectionCtlMemDesc);
		psDevInfo->psRGXFWIfConnectionCtlMemDesc = NULL;
	}

	if (psDevInfo->psRGXFWIfOsInit)
	{
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfOsInitMemDesc);
		psDevInfo->psRGXFWIfOsInit = NULL;
	}

	if (psDevInfo->psRGXFWIfOsInitMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfOsInitMemDesc);
		psDevInfo->psRGXFWIfOsInitMemDesc = NULL;
	}

	RGXFreeFwSysData(psDevInfo);
	if (psDevInfo->psRGXFWIfSysInit)
	{
		DevmemReleaseCpuVirtAddr(psDevInfo->psRGXFWIfSysInitMemDesc);
		psDevInfo->psRGXFWIfSysInit = NULL;
	}

	if (psDevInfo->psRGXFWIfSysInitMemDesc)
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psRGXFWIfSysInitMemDesc);
		psDevInfo->psRGXFWIfSysInitMemDesc = NULL;
	}
}

/******************************************************************************
 FUNCTION	: RGXAcquireKernelCCBSlot

 PURPOSE	: Attempts to obtain a slot in the Kernel CCB

 PARAMETERS	: psCCB - the CCB
			: Address of space if available, NULL otherwise

 RETURNS	: PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXAcquireKernelCCBSlot(DEVMEM_MEMDESC *psKCCBCtrlMemDesc,
											RGXFWIF_CCB_CTL	*psKCCBCtl,
											IMG_UINT32		*pui32Offset)
{
	IMG_UINT32	ui32OldWriteOffset, ui32NextWriteOffset;

	ui32OldWriteOffset = psKCCBCtl->ui32WriteOffset;
	ui32NextWriteOffset = (ui32OldWriteOffset + 1) & psKCCBCtl->ui32WrapMask;

	/*
	 * Note: The MTS can queue up to 255 kicks (254 pending kicks and 1
	 * executing kick), hence the kernel CCB should not queue more than
	 * 254 commands.
	 */
	PVR_ASSERT(psKCCBCtl->ui32WrapMask < 255);

#if defined(PDUMP)
	/* Wait for sufficient CCB space to become available */
	PDUMPCOMMENTWITHFLAGS(0, "Wait for kCCB woff=%u", ui32NextWriteOffset);
	DevmemPDumpCBP(psKCCBCtrlMemDesc,
	               offsetof(RGXFWIF_CCB_CTL, ui32ReadOffset),
	               ui32NextWriteOffset,
	               1,
	               (psKCCBCtl->ui32WrapMask + 1));
#endif

	if (ui32NextWriteOffset == psKCCBCtl->ui32ReadOffset)
	{
		return PVRSRV_ERROR_KERNEL_CCB_FULL;
	}
	*pui32Offset = ui32NextWriteOffset;
	return PVRSRV_OK;
}

/******************************************************************************
 FUNCTION	: RGXPollKernelCCBSlot

 PURPOSE	: Poll for space in Kernel CCB

 PARAMETERS	: psCCB - the CCB
			: Address of space if available, NULL otherwise

 RETURNS	: PVRSRV_ERROR
******************************************************************************/
static PVRSRV_ERROR RGXPollKernelCCBSlot(DEVMEM_MEMDESC *psKCCBCtrlMemDesc,
										 RGXFWIF_CCB_CTL	*psKCCBCtl)
{
	IMG_UINT32	ui32OldWriteOffset, ui32NextWriteOffset;

	ui32OldWriteOffset = psKCCBCtl->ui32WriteOffset;
	ui32NextWriteOffset = (ui32OldWriteOffset + 1) & psKCCBCtl->ui32WrapMask;

	/*
	 * Note: The MTS can queue up to 255 kicks (254 pending kicks and 1
	 * executing kick), hence the kernel CCB should not queue more than
	 * 254 commands.
	 */
	PVR_ASSERT(psKCCBCtl->ui32WrapMask < 255);

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{

		if (ui32NextWriteOffset != psKCCBCtl->ui32ReadOffset)
		{
			return PVRSRV_OK;
		}
		{
			/*
			 * The following sanity check doesn't impact performance,
			 * since the CPU has to wait for the GPU anyway (full kernel CCB).
			 */
			if (PVRSRVGetPVRSRVData()->eServicesState != PVRSRV_SERVICES_STATE_OK)
			{
				return PVRSRV_ERROR_KERNEL_CCB_FULL;
			}
		}

		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	return PVRSRV_ERROR_KERNEL_CCB_FULL;
}

/******************************************************************************
 FUNCTION	: RGXGetCmdMemCopySize

 PURPOSE	: Calculates actual size of KCCB command getting used

 PARAMETERS	: eCmdType     Type of KCCB command

 RETURNS	: Returns actual size of KCCB command on success else zero
******************************************************************************/
static IMG_UINT32 RGXGetCmdMemCopySize(RGXFWIF_KCCB_CMD_TYPE eCmdType)
{
	/* First get offset of uCmdData inside the struct RGXFWIF_KCCB_CMD
	 * This will account alignment requirement of uCmdData union
	 *
	 * Then add command-data size depending on command type to calculate actual
	 * command size required to do mem copy
	 *
	 * NOTE: Make sure that uCmdData is the last member of RGXFWIF_KCCB_CMD struct.
	 */
	switch (eCmdType)
	{
		case RGXFWIF_KCCB_CMD_KICK:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_KCCB_CMD_KICK_DATA);
		}
		case RGXFWIF_KCCB_CMD_COMBINED_TA_3D_KICK:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_KCCB_CMD_COMBINED_TA_3D_KICK_DATA);
		}
		case RGXFWIF_KCCB_CMD_MMUCACHE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_MMUCACHEDATA);
		}
#if defined(SUPPORT_USC_BREAKPOINT)
		case RGXFWIF_KCCB_CMD_BP:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_BPDATA);
		}
#endif
		case RGXFWIF_KCCB_CMD_SLCFLUSHINVAL:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_SLCFLUSHINVALDATA);
		}
		case RGXFWIF_KCCB_CMD_CLEANUP:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_CLEANUP_REQUEST);
		}
		case RGXFWIF_KCCB_CMD_POW:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_POWER_REQUEST);
		}
		case RGXFWIF_KCCB_CMD_ZSBUFFER_BACKING_UPDATE:
		case RGXFWIF_KCCB_CMD_ZSBUFFER_UNBACKING_UPDATE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_ZSBUFFER_BACKING_DATA);
		}
		case RGXFWIF_KCCB_CMD_FREELIST_GROW_UPDATE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_FREELIST_GS_DATA);
		}
		case RGXFWIF_KCCB_CMD_FREELISTS_RECONSTRUCTION_UPDATE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_FREELISTS_RECONSTRUCTION_DATA);
		}
		case RGXFWIF_KCCB_CMD_NOTIFY_SIGNAL_UPDATE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_SIGNAL_UPDATE_DATA);
		}
		case RGXFWIF_KCCB_CMD_NOTIFY_WRITE_OFFSET_UPDATE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_WRITE_OFFSET_UPDATE_DATA);
		}
		case RGXFWIF_KCCB_CMD_FORCE_UPDATE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_KCCB_CMD_FORCE_UPDATE_DATA);
		}
#if defined(SUPPORT_USER_REGISTER_CONFIGURATION)
		case RGXFWIF_KCCB_CMD_REGCONFIG:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_REGCONFIG_DATA);
		}
#endif
		case RGXFWIF_KCCB_CMD_HWPERF_SELECT_CUSTOM_CNTRS:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_HWPERF_SELECT_CUSTOM_CNTRS);
		}
#if defined(SUPPORT_PDVFS)
		case RGXFWIF_KCCB_CMD_PDVFS_LIMIT_MAX_FREQ:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_PDVFS_MAX_FREQ_DATA);
		}
#endif
		case RGXFWIF_KCCB_CMD_OSID_PRIORITY_CHANGE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_OSID_PRIORITY_DATA);
		}
		case RGXFWIF_KCCB_CMD_HCS_SET_DEADLINE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_HCS_CTL);
		}
		case RGXFWIF_KCCB_CMD_OS_ISOLATION_GROUP_CHANGE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_OSID_ISOLATION_GROUP_DATA);
		}
		case RGXFWIF_KCCB_CMD_OS_ONLINE_STATE_CONFIGURE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_OS_STATE_CHANGE_DATA);
		}
		case RGXFWIF_KCCB_CMD_COUNTER_DUMP:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_COUNTER_DUMP_DATA);
		}
		case RGXFWIF_KCCB_CMD_HWPERF_UPDATE_CONFIG:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_HWPERF_CTRL);
		}
		case RGXFWIF_KCCB_CMD_HWPERF_CONFIG_ENABLE_BLKS:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_HWPERF_CONFIG_ENABLE_BLKS);
		}
		case RGXFWIF_KCCB_CMD_HWPERF_CTRL_BLKS:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_HWPERF_CTRL_BLKS);
		}
		case RGXFWIF_KCCB_CMD_CORECLKSPEEDCHANGE:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_CORECLKSPEEDCHANGE_DATA);
		}
		case RGXFWIF_KCCB_CMD_HEALTH_CHECK:
		case RGXFWIF_KCCB_CMD_HWPERF_CONFIG_ENABLE_BLKS_DIRECT:
		case RGXFWIF_KCCB_CMD_LOGTYPE_UPDATE:
		case RGXFWIF_KCCB_CMD_STATEFLAGS_CTRL:
		{
			/* No command specific data */
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData);
		}
		case RGXFWIF_KCCB_CMD_PHR_CFG:
		{
			return offsetof(RGXFWIF_KCCB_CMD, uCmdData) + sizeof(RGXFWIF_KCCB_CMD_PHR_CFG_DATA);
		}
		default:
		{
			/* Invalid (OR) Unused (OR) Newly added command type */
			return 0; /* Error */
		}
	}
}

PVRSRV_ERROR RGXWaitForKCCBSlotUpdate(PVRSRV_RGXDEV_INFO *psDevInfo,
                                      IMG_UINT32 ui32SlotNum,
									  IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVWaitForValueKM(
	              (IMG_UINT32 __iomem *)&psDevInfo->pui32KernelCCBRtnSlots[ui32SlotNum],
				  RGXFWIF_KCCB_RTN_SLOT_CMD_EXECUTED,
				  RGXFWIF_KCCB_RTN_SLOT_CMD_EXECUTED);
	PVR_LOG_RETURN_IF_ERROR(eError, "PVRSRVWaitForValueKM");

#if defined(PDUMP)
	/* PDumping conditions same as RGXSendCommandRaw for the actual command and poll command to go in harmony */
	if (PDumpIsContCaptureOn())
	{
		IMG_BOOL bIsInCaptureRange;

		PDumpIsCaptureFrameKM(&bIsInCaptureRange);

		if ((bIsInCaptureRange || PDUMP_IS_CONTINUOUS(ui32PDumpFlags)) && !PDUMPPOWCMDINTRANS())
		{
			PDUMPCOMMENT("Poll on KCCB slot %u for value %u (mask: 0x%x)", ui32SlotNum,
			             RGXFWIF_KCCB_RTN_SLOT_CMD_EXECUTED, RGXFWIF_KCCB_RTN_SLOT_CMD_EXECUTED);

			eError = DevmemPDumpDevmemPol32(psDevInfo->psKernelCCBRtnSlotsMemDesc,
											ui32SlotNum * sizeof(IMG_UINT32),
											RGXFWIF_KCCB_RTN_SLOT_CMD_EXECUTED,
											RGXFWIF_KCCB_RTN_SLOT_CMD_EXECUTED,
											PDUMP_POLL_OPERATOR_EQUAL,
											ui32PDumpFlags);
			PVR_LOG_IF_ERROR(eError, "DevmemPDumpDevmemPol32");
		}
	}
#else
	PVR_UNREFERENCED_PARAMETER(ui32PDumpFlags);
#endif

	return eError;
}

static PVRSRV_ERROR RGXSendCommandRaw(PVRSRV_RGXDEV_INFO  *psDevInfo,
									  RGXFWIF_KCCB_CMD    *psKCCBCmd,
									  IMG_UINT32          uiPdumpFlags,
									  IMG_UINT32          *pui32CmdKCCBSlot)
{
	PVRSRV_ERROR		eError;
	PVRSRV_DEVICE_NODE	*psDeviceNode = psDevInfo->psDeviceNode;
	RGXFWIF_CCB_CTL		*psKCCBCtl = psDevInfo->psKernelCCBCtl;
	IMG_UINT8			*pui8KCCB = psDevInfo->psKernelCCB;
	IMG_UINT32			ui32NewWriteOffset;
	IMG_UINT32			ui32OldWriteOffset = psKCCBCtl->ui32WriteOffset;
	IMG_UINT32			ui32CmdMemCopySize;

#if !defined(PDUMP)
	PVR_UNREFERENCED_PARAMETER(uiPdumpFlags);
#else
	IMG_BOOL bPdumpEnabled = IMG_FALSE;
	IMG_BOOL bPDumpPowTrans = PDUMPPOWCMDINTRANS();
	IMG_BOOL bContCaptureOn = PDumpIsContCaptureOn(); /* client connected or in pdump init phase */

	if (bContCaptureOn)
	{
		IMG_BOOL bIsInCaptureRange;

		PDumpIsCaptureFrameKM(&bIsInCaptureRange);
		bPdumpEnabled = (bIsInCaptureRange || PDUMP_IS_CONTINUOUS(uiPdumpFlags)) && !bPDumpPowTrans;

		/* in capture range */
		if (bPdumpEnabled)
		{
			if (!psDevInfo->bDumpedKCCBCtlAlready)
			{
				/* entering capture range */
				psDevInfo->bDumpedKCCBCtlAlready = IMG_TRUE;

				/* Wait for the live FW to catch up */
				PVR_DPF((PVR_DBG_MESSAGE, "%s: waiting on fw to catch-up, roff: %d, woff: %d",
						__func__,
						psKCCBCtl->ui32ReadOffset, ui32OldWriteOffset));
				PVRSRVPollForValueKM(psDevInfo->psDeviceNode,
				                     (IMG_UINT32 __iomem *)&psKCCBCtl->ui32ReadOffset,
									 ui32OldWriteOffset, 0xFFFFFFFF,
									 IMG_TRUE);

				/* Dump Init state of Kernel CCB control (read and write offset) */
				PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Initial state of kernel CCB Control, roff: %d, woff: %d",
						psKCCBCtl->ui32ReadOffset, psKCCBCtl->ui32WriteOffset);

				DevmemPDumpLoadMem(psDevInfo->psKernelCCBCtlMemDesc,
						0,
						sizeof(RGXFWIF_CCB_CTL),
						PDUMP_FLAGS_CONTINUOUS);
			}
		}
	}
#endif

#if defined(SUPPORT_AUTOVZ)
	if (!((KM_FW_CONNECTION_IS(READY, psDevInfo) && KM_OS_CONNECTION_IS(READY, psDevInfo)) ||
		(KM_FW_CONNECTION_IS(ACTIVE, psDevInfo) && KM_OS_CONNECTION_IS(ACTIVE, psDevInfo))))
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: The firmware-driver connection is invalid:"
								"driver state = %u / firmware state = %u;"
								"expected READY (%u/%u) or ACTIVE (%u/%u);",
								__func__, KM_GET_OS_CONNECTION(psDevInfo), KM_GET_FW_CONNECTION(psDevInfo),
								RGXFW_CONNECTION_OS_READY, RGXFW_CONNECTION_FW_READY,
								RGXFW_CONNECTION_OS_ACTIVE, RGXFW_CONNECTION_FW_ACTIVE));
		eError = PVRSRV_ERROR_PVZ_OSID_IS_OFFLINE;
		goto _RGXSendCommandRaw_Exit;
	}
#endif

	PVR_ASSERT(sizeof(RGXFWIF_KCCB_CMD) == psKCCBCtl->ui32CmdSize);
	if (!OSLockIsLocked(psDeviceNode->hPowerLock))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s called without power lock held!",
				__func__));
		PVR_ASSERT(OSLockIsLocked(psDeviceNode->hPowerLock));
	}

	/* Acquire a slot in the CCB */
	eError = RGXAcquireKernelCCBSlot(psDevInfo->psKernelCCBCtlMemDesc, psKCCBCtl, &ui32NewWriteOffset);
	if (eError != PVRSRV_OK)
	{
		goto _RGXSendCommandRaw_Exit;
	}

	/* Calculate actual size of command to optimize device mem copy */
	ui32CmdMemCopySize = RGXGetCmdMemCopySize(psKCCBCmd->eCmdType);
	PVR_LOG_RETURN_IF_FALSE(ui32CmdMemCopySize !=0, "RGXGetCmdMemCopySize failed", PVRSRV_ERROR_INVALID_CCB_COMMAND);

	/* Copy the command into the CCB */
	OSDeviceMemCopy(&pui8KCCB[ui32OldWriteOffset * psKCCBCtl->ui32CmdSize],
					psKCCBCmd, ui32CmdMemCopySize);

	/* If non-NULL pui32CmdKCCBSlot passed-in, return the kCCB slot in which the command was enqueued */
	if (pui32CmdKCCBSlot)
	{
		*pui32CmdKCCBSlot = ui32OldWriteOffset;

		/* Each such command enqueue needs to reset the slot value first. This is so that a caller
		 * doesn't get to see stale/false value in allotted slot */
		psDevInfo->pui32KernelCCBRtnSlots[ui32OldWriteOffset] = RGXFWIF_KCCB_RTN_SLOT_NO_RESPONSE;
#if defined(PDUMP)
		PDUMPCOMMENTWITHFLAGS(uiPdumpFlags, "Reset kCCB slot number %u", ui32OldWriteOffset);
		DevmemPDumpLoadMem(psDevInfo->psKernelCCBRtnSlotsMemDesc,
		                   ui32OldWriteOffset * sizeof(IMG_UINT32),
						   sizeof(IMG_UINT32),
						   uiPdumpFlags);
#endif
		PVR_DPF((PVR_DBG_MESSAGE, "%s: Device (%p) KCCB slot %u reset with value %u for command type %u",
		         __func__, psDevInfo, ui32OldWriteOffset, RGXFWIF_KCCB_RTN_SLOT_NO_RESPONSE, psKCCBCmd->eCmdType));
	}

	/* ensure kCCB data is written before the offsets */
	OSWriteMemoryBarrier();

	/* Move past the current command */
	psKCCBCtl->ui32WriteOffset = ui32NewWriteOffset;
	/* Force a read-back to memory to avoid posted writes on certain buses */
	(void) psKCCBCtl->ui32WriteOffset;


#if defined(PDUMP)
	if (bContCaptureOn)
	{
		/* in capture range */
		if (bPdumpEnabled)
		{
			/* Dump new Kernel CCB content */
			PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS, "Dump kCCB cmd woff = %d",
					ui32OldWriteOffset);
			DevmemPDumpLoadMem(psDevInfo->psKernelCCBMemDesc,
					ui32OldWriteOffset * psKCCBCtl->ui32CmdSize,
					ui32CmdMemCopySize,
					PDUMP_FLAGS_CONTINUOUS);

			/* Dump new kernel CCB write offset */
			PDUMPCOMMENTWITHFLAGS(uiPdumpFlags, "Dump kCCBCtl woff: %d",
					ui32NewWriteOffset);
			DevmemPDumpLoadMem(psDevInfo->psKernelCCBCtlMemDesc,
					offsetof(RGXFWIF_CCB_CTL, ui32WriteOffset),
					sizeof(IMG_UINT32),
					uiPdumpFlags);

			/* mimic the read-back of the write from above */
			DevmemPDumpDevmemPol32(psDevInfo->psKernelCCBCtlMemDesc,
					offsetof(RGXFWIF_CCB_CTL, ui32WriteOffset),
					ui32NewWriteOffset,
					0xFFFFFFFF,
					PDUMP_POLL_OPERATOR_EQUAL,
					uiPdumpFlags);

		}
		/* out of capture range */
		else
		{
			eError = RGXPdumpDrainKCCB(psDevInfo, ui32OldWriteOffset);
			PVR_LOG_GOTO_IF_ERROR(eError, "RGXPdumpDrainKCCB", _RGXSendCommandRaw_Exit);
		}
	}
#endif


	PDUMPCOMMENTWITHFLAGS(uiPdumpFlags, "MTS kick for kernel CCB");
	/*
	 * Kick the MTS to schedule the firmware.
	 */
	__MTSScheduleWrite(psDevInfo, MTS_SCHEDULE_DM_VAL & ~RGX_CR_MTS_SCHEDULE_DM_CLRMSK);

	PDUMPREG32(RGX_PDUMPREG_NAME, RGX_CR_MTS_SCHEDULE, MTS_SCHEDULE_DM_VAL & ~RGX_CR_MTS_SCHEDULE_DM_CLRMSK, uiPdumpFlags);

#if defined(NO_HARDWARE)
	/* keep the roff updated because fw isn't there to update it */
	psKCCBCtl->ui32ReadOffset = psKCCBCtl->ui32WriteOffset;
#endif

_RGXSendCommandRaw_Exit:
	return eError;
}

/******************************************************************************
 FUNCTION	: _AllocDeferredCommand

 PURPOSE	: Allocate a KCCB command and add it to KCCB deferred list

 PARAMETERS	: psDevInfo	RGX device info
			: eKCCBType		Firmware Command type
			: psKCCBCmd		Firmware Command
			: uiPdumpFlags	Pdump flags

 RETURNS	: PVRSRV_OK	If all went good, PVRSRV_ERROR_RETRY otherwise.
******************************************************************************/
static PVRSRV_ERROR _AllocDeferredCommand(PVRSRV_RGXDEV_INFO *psDevInfo,
                                          RGXFWIF_KCCB_CMD   *psKCCBCmd,
                                          IMG_UINT32         uiPdumpFlags)
{
	RGX_DEFERRED_KCCB_CMD *psDeferredCommand;
	OS_SPINLOCK_FLAGS uiFlags;

	psDeferredCommand = OSAllocMem(sizeof(*psDeferredCommand));

	if (!psDeferredCommand)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "Deferring a KCCB command failed: allocation failure: requesting retry"));
		return PVRSRV_ERROR_RETRY;
	}

	psDeferredCommand->sKCCBcmd = *psKCCBCmd;
	psDeferredCommand->uiPdumpFlags = uiPdumpFlags;
	psDeferredCommand->psDevInfo = psDevInfo;

	OSSpinLockAcquire(psDevInfo->hLockKCCBDeferredCommandsList, uiFlags);
	dllist_add_to_tail(&(psDevInfo->sKCCBDeferredCommandsListHead), &(psDeferredCommand->sListNode));
	psDevInfo->ui32KCCBDeferredCommandsCount++;
	OSSpinLockRelease(psDevInfo->hLockKCCBDeferredCommandsList, uiFlags);

	return PVRSRV_OK;
}

/******************************************************************************
 FUNCTION	: _FreeDeferredCommand

 PURPOSE	: Remove from the deferred list the sent deferred KCCB command

 PARAMETERS	: psNode			Node in deferred list
			: psDeferredKCCBCmd	KCCB Command to free

 RETURNS	: None
******************************************************************************/
static void _FreeDeferredCommand(DLLIST_NODE *psNode, RGX_DEFERRED_KCCB_CMD *psDeferredKCCBCmd)
{
	dllist_remove_node(psNode);
	psDeferredKCCBCmd->psDevInfo->ui32KCCBDeferredCommandsCount--;
	OSFreeMem(psDeferredKCCBCmd);
}

/******************************************************************************
 FUNCTION	: RGXSendCommandsFromDeferredList

 PURPOSE	: Try send KCCB commands in deferred list to KCCB
		  Should be called by holding PowerLock

 PARAMETERS	: psDevInfo	RGX device info
			: bPoll		Poll for space in KCCB

 RETURNS	: PVRSRV_OK	If all commands in deferred list are sent to KCCB,
			  PVRSRV_ERROR_KERNEL_CCB_FULL otherwise.
******************************************************************************/
PVRSRV_ERROR RGXSendCommandsFromDeferredList(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_BOOL bPoll)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	DLLIST_NODE *psNode, *psNext;
	RGX_DEFERRED_KCCB_CMD *psTempDeferredKCCBCmd;
	DLLIST_NODE sCommandList;
	OS_SPINLOCK_FLAGS uiFlags;

	PVR_ASSERT(PVRSRVPwrLockIsLockedByMe(psDevInfo->psDeviceNode));

	/* !!! Important !!!
	 *
	 * The idea of moving the whole list hLockKCCBDeferredCommandsList below
	 * to the temporary list is only valid under the principle that all of the
	 * operations are also protected by the power lock. It must be held
	 * so that the order of the commands doesn't get messed up while we're
	 * performing the operations on the local list.
	 *
	 * The necessity of releasing the hLockKCCBDeferredCommandsList comes from
	 * the fact that _FreeDeferredCommand() is allocating memory and it can't
	 * be done in atomic context (inside section protected by a spin lock).
	 *
	 * We're using spin lock here instead of mutex to quickly perform a check
	 * if the list is empty in MISR without a risk that the MISR is going
	 * to sleep due to a lock.
	 */

	/* move the whole list to a local list so it can be processed without lock */
	OSSpinLockAcquire(psDevInfo->hLockKCCBDeferredCommandsList, uiFlags);
	dllist_replace_head(&psDevInfo->sKCCBDeferredCommandsListHead, &sCommandList);
	OSSpinLockRelease(psDevInfo->hLockKCCBDeferredCommandsList, uiFlags);

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		if (dllist_is_empty(&sCommandList))
		{
			return PVRSRV_OK;
		}

		/* For every deferred KCCB command, try to send it*/
		dllist_foreach_node(&sCommandList, psNode, psNext)
		{
			psTempDeferredKCCBCmd = IMG_CONTAINER_OF(psNode, RGX_DEFERRED_KCCB_CMD, sListNode);
			eError = RGXSendCommandRaw(psTempDeferredKCCBCmd->psDevInfo,
			                           &psTempDeferredKCCBCmd->sKCCBcmd,
			                           psTempDeferredKCCBCmd->uiPdumpFlags,
			                           NULL /* We surely aren't interested in kCCB slot number of deferred command */);
			if (eError != PVRSRV_OK)
			{
				if (!bPoll)
				{
					eError = PVRSRV_ERROR_KERNEL_CCB_FULL;
					goto cleanup_;
				}
				break;
			}

			_FreeDeferredCommand(psNode, psTempDeferredKCCBCmd);
		}

		if (bPoll)
		{
			PVRSRV_ERROR eErrPollForKCCBSlot;

			/* Don't overwrite eError because if RGXPollKernelCCBSlot returns OK and the
			 * outer loop times-out, we'll still want to return KCCB_FULL to caller
			 */
			eErrPollForKCCBSlot = RGXPollKernelCCBSlot(psDevInfo->psKernelCCBCtlMemDesc,
			                                           psDevInfo->psKernelCCBCtl);
			if (eErrPollForKCCBSlot == PVRSRV_ERROR_KERNEL_CCB_FULL)
			{
				eError = PVRSRV_ERROR_KERNEL_CCB_FULL;
				goto cleanup_;
			}
		}
	} END_LOOP_UNTIL_TIMEOUT();

cleanup_:
	/* if the local list is not empty put it back to the deferred list head
	 * so that the old order of commands is retained */
	OSSpinLockAcquire(psDevInfo->hLockKCCBDeferredCommandsList, uiFlags);
	dllist_insert_list_at_head(&psDevInfo->sKCCBDeferredCommandsListHead, &sCommandList);
	OSSpinLockRelease(psDevInfo->hLockKCCBDeferredCommandsList, uiFlags);

	return eError;
}

PVRSRV_ERROR RGXSendCommandAndGetKCCBSlot(PVRSRV_RGXDEV_INFO  *psDevInfo,
										  RGXFWIF_KCCB_CMD    *psKCCBCmd,
										  IMG_UINT32          uiPdumpFlags,
										  IMG_UINT32          *pui32CmdKCCBSlot)
{
	IMG_BOOL     bPoll = (pui32CmdKCCBSlot != NULL);
	PVRSRV_ERROR eError;

	/*
	 * First try to Flush all the cmds in deferred list.
	 *
	 * We cannot defer an incoming command if the caller is interested in
	 * knowing the command's kCCB slot: it plans to poll/wait for a
	 * response from the FW just after the command is enqueued, so we must
	 * poll for space to be available.
	 */
	eError = RGXSendCommandsFromDeferredList(psDevInfo, bPoll);
	if (eError == PVRSRV_OK)
	{
		eError = RGXSendCommandRaw(psDevInfo,
								   psKCCBCmd,
								   uiPdumpFlags,
								   pui32CmdKCCBSlot);
	}

	/*
	 * If we don't manage to enqueue one of the deferred commands or the command
	 * passed as argument because the KCCB is full, insert the latter into the deferred commands list.
	 * The deferred commands will also be flushed eventually by:
	 *  - one more KCCB command sent for any DM
	 *  - RGX_MISRHandler_CheckFWActivePowerState
	 */
	if (eError == PVRSRV_ERROR_KERNEL_CCB_FULL)
	{
		if (pui32CmdKCCBSlot == NULL)
		{
			eError = _AllocDeferredCommand(psDevInfo, psKCCBCmd, uiPdumpFlags);
		}
		else
		{
			/* Let the caller retry. Otherwise if we deferred the command and returned OK,
			 * the caller can end up looking in a stale CCB slot.
			 */
			PVR_DPF((PVR_DBG_ERROR, "%s: Couldn't flush the deferred queue for a command (Type:%d) "
			                        "that needed the kCCB command slot number! Returning kCCB FULL",
			                        __func__, psKCCBCmd->eCmdType));
		}
	}

	return eError;
}

PVRSRV_ERROR RGXSendCommandWithPowLockAndGetKCCBSlot(PVRSRV_RGXDEV_INFO	*psDevInfo,
													 RGXFWIF_KCCB_CMD	*psKCCBCmd,
													 IMG_UINT32			ui32PDumpFlags,
													 IMG_UINT32         *pui32CmdKCCBSlot)
{
	PVRSRV_ERROR		eError;
	PVRSRV_DEVICE_NODE *psDeviceNode = psDevInfo->psDeviceNode;

	/* Ensure Rogue is powered up before kicking MTS */
	eError = PVRSRVPowerLock(psDeviceNode);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING,
				"%s: failed to acquire powerlock (%s)",
				__func__,
				PVRSRVGetErrorString(eError)));

		goto _PVRSRVPowerLock_Exit;
	}

	PDUMPPOWCMDSTART();
	eError = PVRSRVSetDevicePowerStateKM(psDeviceNode,
										 PVRSRV_DEV_POWER_STATE_ON,
										 IMG_FALSE);
	PDUMPPOWCMDEND();

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: failed to transition Rogue to ON (%s)",
				__func__,
				PVRSRVGetErrorString(eError)));

		goto _PVRSRVSetDevicePowerStateKM_Exit;
	}

	eError = RGXSendCommandAndGetKCCBSlot(psDevInfo,
										  psKCCBCmd,
										  ui32PDumpFlags,
	                                      pui32CmdKCCBSlot);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: failed to schedule command (%s)",
				__func__,
				PVRSRVGetErrorString(eError)));
#if defined(DEBUG)
		/* PVRSRVDebugRequest must be called without powerlock */
		PVRSRVPowerUnlock(psDeviceNode);
		PVRSRVDebugRequest(psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
		goto _PVRSRVPowerLock_Exit;
#endif
	}

_PVRSRVSetDevicePowerStateKM_Exit:
	PVRSRVPowerUnlock(psDeviceNode);

_PVRSRVPowerLock_Exit:
	return eError;
}

void RGXScheduleProcessQueuesKM(PVRSRV_CMDCOMP_HANDLE hCmdCompHandle)
{
	PVRSRV_DEVICE_NODE *psDeviceNode = (PVRSRV_DEVICE_NODE*) hCmdCompHandle;
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	OSScheduleMISR(psDevInfo->hProcessQueuesMISR);
}

/*!
*******************************************************************************

 @Function	RGX_MISRHandler_ScheduleProcessQueues

 @Description - Sends uncounted kick to all the DMs (the FW will process all
				the queue for all the DMs)
******************************************************************************/
static void RGX_MISRHandler_ScheduleProcessQueues(void *pvData)
{
	PVRSRV_DEVICE_NODE     *psDeviceNode = pvData;
	PVRSRV_RGXDEV_INFO     *psDevInfo = psDeviceNode->pvDevice;
	PVRSRV_ERROR           eError;
	PVRSRV_DEV_POWER_STATE ePowerState;

	eError = PVRSRVPowerLock(psDeviceNode);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: failed to acquire powerlock (%s)",
				__func__, PVRSRVGetErrorString(eError)));
		return;
	}

	/* Check whether it's worth waking up the GPU */
	eError = PVRSRVGetDevicePowerState(psDeviceNode, &ePowerState);

	if (!PVRSRV_VZ_MODE_IS(GUEST) &&
			(eError == PVRSRV_OK) && (ePowerState == PVRSRV_DEV_POWER_STATE_OFF))
	{
		/* For now, guest drivers will always wake-up the GPU */
		RGXFWIF_GPU_UTIL_FWCB  *psUtilFWCb = psDevInfo->psRGXFWIfGpuUtilFWCb;
		IMG_BOOL               bGPUHasWorkWaiting;

		bGPUHasWorkWaiting =
				(RGXFWIF_GPU_UTIL_GET_STATE(psUtilFWCb->ui64LastWord) == RGXFWIF_GPU_UTIL_STATE_BLOCKED);

		if (!bGPUHasWorkWaiting)
		{
			/* all queues are empty, don't wake up the GPU */
			PVRSRVPowerUnlock(psDeviceNode);
			return;
		}
	}

	PDUMPPOWCMDSTART();
	/* wake up the GPU */
	eError = PVRSRVSetDevicePowerStateKM(psDeviceNode,
										 PVRSRV_DEV_POWER_STATE_ON,
										 IMG_FALSE);
	PDUMPPOWCMDEND();

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: failed to transition Rogue to ON (%s)",
				__func__, PVRSRVGetErrorString(eError)));

		PVRSRVPowerUnlock(psDeviceNode);
		return;
	}

	/* uncounted kick to the FW */
	HTBLOGK(HTB_SF_MAIN_KICK_UNCOUNTED);
	__MTSScheduleWrite(psDevInfo, (MTS_SCHEDULE_DM_VAL & ~RGX_CR_MTS_SCHEDULE_DM_CLRMSK) | RGX_CR_MTS_SCHEDULE_TASK_NON_COUNTED);

	PVRSRVPowerUnlock(psDeviceNode);
}

PVRSRV_ERROR RGXInstallProcessQueuesMISR(IMG_HANDLE *phMISR, PVRSRV_DEVICE_NODE *psDeviceNode)
{
	return OSInstallMISR(phMISR,
			RGX_MISRHandler_ScheduleProcessQueues,
			psDeviceNode,
			"RGX_ScheduleProcessQueues");
}

PVRSRV_ERROR RGXScheduleCommandAndGetKCCBSlot(PVRSRV_RGXDEV_INFO  *psDevInfo,
                                              RGXFWIF_DM          eKCCBType,
											  RGXFWIF_KCCB_CMD    *psKCCBCmd,
											  IMG_UINT32          ui32CacheOpFence,
											  IMG_UINT32          ui32PDumpFlags,
											  IMG_UINT32          *pui32CmdKCCBSlot)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 uiMMUSyncUpdate;

	/* Don't send the command/power up request if the device is de-initialising.
	 * The de-init thread could destroy the device whilst the power up
	 * sequence below is accessing the HW registers.
	 */
	if (unlikely((psDevInfo == NULL) ||
	             (psDevInfo->psDeviceNode == NULL) ||
	             (psDevInfo->psDeviceNode->eDevState == PVRSRV_DEVICE_STATE_DEINIT)))
	{
		return PVRSRV_ERROR_INVALID_DEVICE;
	}

	eError = CacheOpFence(eKCCBType, ui32CacheOpFence);
	if (unlikely(eError != PVRSRV_OK)) goto RGXScheduleCommand_exit;

#if defined(SUPPORT_VALIDATION)
	/* For validation, force the core to different dust count states with each kick */
	if ((eKCCBType == RGXFWIF_DM_GEOM) || (eKCCBType == RGXFWIF_DM_CDM))
	{
		if (psDevInfo->ui32DeviceFlags & RGXKM_DEVICE_STATE_GPU_UNITS_POWER_CHANGE_EN)
		{
			IMG_UINT32 ui32NumDusts = RGXGetNextDustCount(&psDevInfo->sDustReqState, psDevInfo->sDevFeatureCfg.ui32MAXDustCount);
			PVRSRVDeviceGPUUnitsPowerChange(psDevInfo->psDeviceNode, ui32NumDusts);
		}
	}
#endif

	/* PVRSRVPowerLock guarantees atomicity between commands. This is helpful
	   in a scenario with several applications allocating resources. */
	eError = PVRSRVPowerLock(psDevInfo->psDeviceNode);
	if (unlikely(eError != PVRSRV_OK))
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: failed to acquire powerlock (%s)",
				__func__, PVRSRVGetErrorString(eError)));

		/* If system is found powered OFF, Retry scheduling the command */
		if (likely(eError == PVRSRV_ERROR_SYSTEM_STATE_POWERED_OFF))
		{
			eError = PVRSRV_ERROR_RETRY;
		}

		goto RGXScheduleCommand_exit;
	}

	if (unlikely(psDevInfo->psDeviceNode->eDevState == PVRSRV_DEVICE_STATE_DEINIT))
	{
		/* If we have the power lock the device is valid but the deinit
		 * thread could be waiting for the lock. */
		PVRSRVPowerUnlock(psDevInfo->psDeviceNode);
		return PVRSRV_ERROR_INVALID_DEVICE;
	}

	/* Ensure device is powered up before sending any commands */
	PDUMPPOWCMDSTART();
	eError = PVRSRVSetDevicePowerStateKM(psDevInfo->psDeviceNode,
			PVRSRV_DEV_POWER_STATE_ON,
			IMG_FALSE);
	PDUMPPOWCMDEND();
	if (unlikely(eError != PVRSRV_OK))
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: failed to transition RGX to ON (%s)",
				__func__, PVRSRVGetErrorString(eError)));
		goto _PVRSRVSetDevicePowerStateKM_Exit;
	}

	eError = RGXPreKickCacheCommand(psDevInfo, NULL, eKCCBType, &uiMMUSyncUpdate, IMG_FALSE);
	if (unlikely(eError != PVRSRV_OK)) goto _PVRSRVSetDevicePowerStateKM_Exit;

	eError = RGXSendCommandAndGetKCCBSlot(psDevInfo, psKCCBCmd, ui32PDumpFlags, pui32CmdKCCBSlot);
	if (unlikely(eError != PVRSRV_OK)) goto _PVRSRVSetDevicePowerStateKM_Exit;

_PVRSRVSetDevicePowerStateKM_Exit:
	PVRSRVPowerUnlock(psDevInfo->psDeviceNode);

RGXScheduleCommand_exit:
	return eError;
}

#if defined(PVRSRV_SYNC_CHECKPOINT_CCB)
/*
 * RGXCheckCheckpointCCB
 */
void RGXCheckCheckpointCCB(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	IMG_BOOL bSignal = IMG_FALSE;

	PRGXFWIF_UFO_ADDR *psFwUFOAddr;
	RGXFWIF_CCB_CTL *psChptCCBCtl = psDevInfo->psCheckpointCCBCtl;
	IMG_UINT8 *psChptCCB = psDevInfo->psCheckpointCCB;
	IMG_UINT32 ui32WriteOffset, ui32ReadOffset, ui32WrapMask = psChptCCBCtl->ui32WrapMask;
	IMG_UINT32 uiFwAddr;
	PVRSRV_SYNC_CHECKPOINT_STATE uiChptState;

	/*
	 * Check if the firmware has signalled a full sync state check.
	 */
	if (psDevInfo->psRGXFWIfFwOsData->ui32FWSyncCheckMark != psDevInfo->psRGXFWIfFwOsData->ui32HostSyncCheckMark)
	{
		/*
		 * Update the offsets first so that if the firmware tries to write
		 * another checkpoint it is not missed by the check state.
		 */
		psDevInfo->psRGXFWIfFwOsData->ui32HostSyncCheckMark = psDevInfo->psRGXFWIfFwOsData->ui32FWSyncCheckMark;
		psChptCCBCtl->ui32ReadOffset = psChptCCBCtl->ui32WriteOffset;

		PVR_DPF((PVR_DBG_MESSAGE, "%s: Checkpoint CCB full, performing full sync checkpoint state check", __func__));

		SyncCheckpointCheckState();
		bSignal = IMG_TRUE;

#if defined(SUPPORT_BUFFER_SYNC)
		pvr_buffer_sync_check_state();
#endif

		goto exit_signal;
	}

	/*
	 * Take a snapshot of the current CCB ctl pointers at the start of
	 * processing.
	 */
	ui32WriteOffset = psChptCCBCtl->ui32WriteOffset;
	ui32ReadOffset = psChptCCBCtl->ui32ReadOffset;
	ui32WrapMask = psChptCCBCtl->ui32WrapMask;

	while (ui32ReadOffset != ui32WriteOffset)
	{
		/* Point to the next checkpoint address */
		psFwUFOAddr = ((PRGXFWIF_UFO_ADDR *)psChptCCB) + ui32ReadOffset;

		/*
		 * State is encoded in bit 1 of ufo address
		 * 1 = signalled, 0 = errored
		 */
		uiChptState = PVRSRV_SYNC_CHECKPOINT_ERRORED;
		uiFwAddr = psFwUFOAddr->ui32Addr;

		if (uiFwAddr & 0x1U)
		{
			uiChptState = PVRSRV_SYNC_CHECKPOINT_SIGNALLED;
		}
		uiFwAddr |= 0x1U;

		if (SyncCheckpointUFOHasSignalled(psDeviceNode, uiFwAddr, uiChptState))
		{
			bSignal = IMG_TRUE;
		}
		else
#if defined(SUPPORT_BUFFER_SYNC)
		if (pvr_buffer_sync_checkpoint_ufo_has_signalled(uiFwAddr, uiChptState))
		{
			/* Buffer sync does not need a signal call. */
		}
		else
#endif
		{
			PVR_DPF((PVR_DBG_MESSAGE, "%s: Firmware signalled checkpoint (%#08X) with no host backing", __func__, uiFwAddr));
		}

		/* Update read offset */
		ui32ReadOffset = (ui32ReadOffset + 1) & ui32WrapMask;
	}

	psChptCCBCtl->ui32ReadOffset = ui32ReadOffset;

exit_signal:
	if (bSignal)
	{
		SyncCheckpointSignalWaiters();
	}
}
#endif /* defined(PVRSRV_SYNC_CHECKPOINT_CCB) */

/*
 * RGXCheckFirmwareCCB
 */
void RGXCheckFirmwareCCB(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_FWCCB_CMD *psFwCCBCmd;

	RGXFWIF_CCB_CTL *psFWCCBCtl = psDevInfo->psFirmwareCCBCtl;
	IMG_UINT8 *psFWCCB = psDevInfo->psFirmwareCCB;

	while (psFWCCBCtl->ui32ReadOffset != psFWCCBCtl->ui32WriteOffset)
	{
		/* Point to the next command */
		psFwCCBCmd = ((RGXFWIF_FWCCB_CMD *)psFWCCB) + psFWCCBCtl->ui32ReadOffset;

		HTBLOGK(HTB_SF_MAIN_FWCCB_CMD, psFwCCBCmd->eCmdType);
		switch (psFwCCBCmd->eCmdType)
		{
		case RGXFWIF_FWCCB_CMD_ZSBUFFER_BACKING:
		{
			if (psDevInfo->bPDPEnabled)
			{
				PDUMP_PANIC(ZSBUFFER_BACKING, "Request to add backing to ZSBuffer");
			}
			RGXProcessRequestZSBufferBacking(psDevInfo,
					psFwCCBCmd->uCmdData.sCmdZSBufferBacking.ui32ZSBufferID);
			break;
		}

		case RGXFWIF_FWCCB_CMD_ZSBUFFER_UNBACKING:
		{
			if (psDevInfo->bPDPEnabled)
			{
				PDUMP_PANIC(ZSBUFFER_UNBACKING, "Request to remove backing from ZSBuffer");
			}
			RGXProcessRequestZSBufferUnbacking(psDevInfo,
					psFwCCBCmd->uCmdData.sCmdZSBufferBacking.ui32ZSBufferID);
			break;
		}

		case RGXFWIF_FWCCB_CMD_FREELIST_GROW:
		{
			if (psDevInfo->bPDPEnabled)
			{
				PDUMP_PANIC(FREELIST_GROW, "Request to grow the free list");
			}
			RGXProcessRequestGrow(psDevInfo,
					psFwCCBCmd->uCmdData.sCmdFreeListGS.ui32FreelistID);
			break;
		}

		case RGXFWIF_FWCCB_CMD_FREELISTS_RECONSTRUCTION:
		{
			if (psDevInfo->bPDPEnabled)
			{
				PDUMP_PANIC(FREELISTS_RECONSTRUCTION, "Request to reconstruct free lists");
			}

			if (PVRSRV_VZ_MODE_IS(GUEST))
			{
				PVR_DPF((PVR_DBG_MESSAGE, "%s: Freelist reconstruction request (%d) for %d freelists",
						__func__,
						psFwCCBCmd->uCmdData.sCmdFreeListsReconstruction.ui32HwrCounter+1,
						psFwCCBCmd->uCmdData.sCmdFreeListsReconstruction.ui32FreelistsCount));
			}
			else
			{
				PVR_ASSERT(psDevInfo->psRGXFWIfHWRInfoBufCtl);
				PVR_DPF((PVR_DBG_MESSAGE, "%s: Freelist reconstruction request (%d/%d) for %d freelists",
						__func__,
						psFwCCBCmd->uCmdData.sCmdFreeListsReconstruction.ui32HwrCounter+1,
						psDevInfo->psRGXFWIfHWRInfoBufCtl->ui32HwrCounter+1,
						psFwCCBCmd->uCmdData.sCmdFreeListsReconstruction.ui32FreelistsCount));
			}

			RGXProcessRequestFreelistsReconstruction(psDevInfo,
					psFwCCBCmd->uCmdData.sCmdFreeListsReconstruction.ui32FreelistsCount,
					psFwCCBCmd->uCmdData.sCmdFreeListsReconstruction.aui32FreelistIDs);
			break;
		}

		case RGXFWIF_FWCCB_CMD_CONTEXT_RESET_NOTIFICATION:
		{
			DLLIST_NODE *psNode, *psNext;
			RGXFWIF_FWCCB_CMD_CONTEXT_RESET_DATA *psCmdContextResetNotification =
					&psFwCCBCmd->uCmdData.sCmdContextResetNotification;
			IMG_UINT32 ui32ServerCommonContextID =
					psCmdContextResetNotification->ui32ServerCommonContextID;
			RGX_SERVER_COMMON_CONTEXT *psServerCommonContext = NULL;

			OSWRLockAcquireRead(psDevInfo->hCommonCtxtListLock);
			dllist_foreach_node(&psDevInfo->sCommonCtxtListHead, psNode, psNext)
			{
				RGX_SERVER_COMMON_CONTEXT *psThisContext =
						IMG_CONTAINER_OF(psNode, RGX_SERVER_COMMON_CONTEXT, sListNode);

				if (psThisContext->ui32ContextID == ui32ServerCommonContextID)
				{
					psServerCommonContext = psThisContext;
					break;
				}
			}

			PVR_DPF((PVR_DBG_MESSAGE, "%s: Context 0x%p reset (ID=0x%08x, Reason=%d, JobRef=0x%08x)",
					__func__,
					psServerCommonContext,
					psCmdContextResetNotification->ui32ServerCommonContextID,
					(IMG_UINT32)(psCmdContextResetNotification->eResetReason),
					psCmdContextResetNotification->ui32ResetJobRef));

			if (psServerCommonContext != NULL)
			{
				psServerCommonContext->eLastResetReason    = psCmdContextResetNotification->eResetReason;
				psServerCommonContext->ui32LastResetJobRef = psCmdContextResetNotification->ui32ResetJobRef;
			}
			OSWRLockReleaseRead(psDevInfo->hCommonCtxtListLock);

			if (psCmdContextResetNotification->bPageFault)
			{
				DevmemIntPFNotify(psDevInfo->psDeviceNode,
						psCmdContextResetNotification->ui64PCAddress,
						psCmdContextResetNotification->sFaultAddress);
			}
			break;
		}

		case RGXFWIF_FWCCB_CMD_DEBUG_DUMP:
		{
			PVRSRV_ERROR eError;
			PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();
			OSAtomicWrite(&psDevInfo->psDeviceNode->eDebugDumpRequested, PVRSRV_DEVICE_DEBUG_DUMP_CAPTURE);
			eError = OSEventObjectSignal(psPVRSRVData->hDevicesWatchdogEvObj);
			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to signal FW Cmd debug dump event, dumping now instead", __func__));
				PVRSRVDebugRequest(psDevInfo->psDeviceNode, DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
			}
			break;
		}

		case RGXFWIF_FWCCB_CMD_UPDATE_STATS:
		{
#if defined(PVRSRV_ENABLE_PROCESS_STATS)
			IMG_PID pidTmp = psFwCCBCmd->uCmdData.sCmdUpdateStatsData.pidOwner;
			IMG_INT32 i32AdjustmentValue = psFwCCBCmd->uCmdData.sCmdUpdateStatsData.i32AdjustmentValue;

			switch (psFwCCBCmd->uCmdData.sCmdUpdateStatsData.eElementToUpdate)
			{
			case RGXFWIF_FWCCB_CMD_UPDATE_NUM_PARTIAL_RENDERS:
			{
				PVRSRVStatsUpdateRenderContextStats(i32AdjustmentValue,0,0,0,0,0,pidTmp);
				break;
			}
			case RGXFWIF_FWCCB_CMD_UPDATE_NUM_OUT_OF_MEMORY:
			{
				PVRSRVStatsUpdateRenderContextStats(0,i32AdjustmentValue,0,0,0,0,pidTmp);
				break;
			}
			case RGXFWIF_FWCCB_CMD_UPDATE_NUM_TA_STORES:
			{
				PVRSRVStatsUpdateRenderContextStats(0,0,i32AdjustmentValue,0,0,0,pidTmp);
				break;
			}
			case RGXFWIF_FWCCB_CMD_UPDATE_NUM_3D_STORES:
			{
				PVRSRVStatsUpdateRenderContextStats(0,0,0,i32AdjustmentValue,0,0,pidTmp);
				break;
			}
			case RGXFWIF_FWCCB_CMD_UPDATE_NUM_CDM_STORES:
			{
				PVRSRVStatsUpdateRenderContextStats(0,0,0,0,i32AdjustmentValue,0,pidTmp);
				break;
			}
			case RGXFWIF_FWCCB_CMD_UPDATE_NUM_TDM_STORES:
			{
				PVRSRVStatsUpdateRenderContextStats(0,0,0,0,0,i32AdjustmentValue,pidTmp);
				break;
			}
		}
#endif
			break;
		}
		case RGXFWIF_FWCCB_CMD_CORE_CLK_RATE_CHANGE:
		{
#if defined(SUPPORT_PDVFS)
			PDVFS_PROCESS_CORE_CLK_RATE_CHANGE(psDevInfo,
					psFwCCBCmd->uCmdData.sCmdCoreClkRateChange.ui32CoreClkRate);
#endif
			break;
		}

		case RGXFWIF_FWCCB_CMD_REQUEST_GPU_RESTART:
		{
			if (psDevInfo->psRGXFWIfFwSysData != NULL  &&
					psDevInfo->psRGXFWIfFwSysData->ePowState != RGXFWIF_POW_OFF)
			{
				PVRSRV_ERROR eError;

				/* Power down... */
				eError = PVRSRVSetDeviceSystemPowerState(psDevInfo->psDeviceNode,
						PVRSRV_SYS_POWER_STATE_OFF);
				if (eError == PVRSRV_OK)
				{
					/* Clear the FW faulted flags... */
					psDevInfo->psRGXFWIfFwSysData->ui32HWRStateFlags &= ~(RGXFWIF_HWR_FW_FAULT|RGXFWIF_HWR_RESTART_REQUESTED);

					/* Power back up again... */
					eError = PVRSRVSetDeviceSystemPowerState(psDevInfo->psDeviceNode,
							PVRSRV_SYS_POWER_STATE_ON);

					/* Send a dummy KCCB command to ensure the FW wakes up and checks the queues... */
					if (eError == PVRSRV_OK)
					{
						LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
						{
							eError = RGXFWHealthCheckCmd(psDevInfo);
							if (eError != PVRSRV_ERROR_RETRY)
							{
								break;
							}
							OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
						} END_LOOP_UNTIL_TIMEOUT();
					}
				}

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "%s: Failed firmware restart (%s)",
							__func__, PVRSRVGetErrorString(eError)));
				}
			}
			break;
		}

		default:
		{
			/* unknown command */
			PVR_DPF((PVR_DBG_WARNING, "%s: Unknown Command (eCmdType=0x%08x)",
			         __func__, psFwCCBCmd->eCmdType));
			/* Assert on magic value corruption */
			PVR_ASSERT((((IMG_UINT32)psFwCCBCmd->eCmdType & RGX_CMD_MAGIC_DWORD_MASK) >> RGX_CMD_MAGIC_DWORD_SHIFT) == RGX_CMD_MAGIC_DWORD);
		}
		}

		/* Update read offset */
		psFWCCBCtl->ui32ReadOffset = (psFWCCBCtl->ui32ReadOffset + 1) & psFWCCBCtl->ui32WrapMask;
	}
}

/*
 * PVRSRVRGXFrameworkCopyCommand
*/
PVRSRV_ERROR PVRSRVRGXFrameworkCopyCommand(DEVMEM_MEMDESC	*psFWFrameworkMemDesc,
		IMG_PBYTE		pbyGPUFRegisterList,
		IMG_UINT32		ui32FrameworkRegisterSize)
{
	PVRSRV_ERROR	eError;
	RGXFWIF_RF_REGISTERS	*psRFReg;

	eError = DevmemAcquireCpuVirtAddr(psFWFrameworkMemDesc,
			(void **)&psRFReg);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to map firmware render context state (%u)",
		         __func__, eError));
		return eError;
	}

	OSDeviceMemCopy(psRFReg, pbyGPUFRegisterList, ui32FrameworkRegisterSize);

	/* Release the CPU mapping */
	DevmemReleaseCpuVirtAddr(psFWFrameworkMemDesc);

	/*
	 * Dump the FW framework buffer
	 */
#if defined(PDUMP)
	PDUMPCOMMENT("Dump FWFramework buffer");
	DevmemPDumpLoadMem(psFWFrameworkMemDesc, 0, ui32FrameworkRegisterSize, PDUMP_FLAGS_CONTINUOUS);
#endif

	return PVRSRV_OK;
}

/*
 * PVRSRVRGXFrameworkCreateKM
*/
PVRSRV_ERROR PVRSRVRGXFrameworkCreateKM(PVRSRV_DEVICE_NODE	*psDeviceNode,
		DEVMEM_MEMDESC		**ppsFWFrameworkMemDesc,
		IMG_UINT32			ui32FrameworkCommandSize)
{
	PVRSRV_ERROR			eError;
	PVRSRV_RGXDEV_INFO		*psDevInfo = psDeviceNode->pvDevice;

	/*
		Allocate device memory for the firmware GPU framework state.
		Sufficient info to kick one or more DMs should be contained in this buffer
	 */
	PDUMPCOMMENT("Allocate Rogue firmware framework state");

	eError = DevmemFwAllocate(psDevInfo,
			ui32FrameworkCommandSize,
			RGX_FWCOMCTX_ALLOCFLAGS,
			"FwGPUFrameworkState",
			ppsFWFrameworkMemDesc);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to allocate firmware framework state (%u)",
		         __func__, eError));
		return eError;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR IMG_CALLCONV RGXPollForGPCommandCompletion(PVRSRV_DEVICE_NODE  *psDevNode,
												volatile IMG_UINT32	__iomem *pui32LinMemAddr,
												IMG_UINT32			ui32Value,
												IMG_UINT32			ui32Mask)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	RGXFWIF_CCB_CTL *psKCCBCtl;
	IMG_UINT32 ui32CurrentQueueLength, ui32MaxRetries;
	PVRSRV_RGXDEV_INFO	*psDevInfo = psDevNode->pvDevice;

	psKCCBCtl = psDevInfo->psKernelCCBCtl;
	ui32CurrentQueueLength = (psKCCBCtl->ui32WrapMask+1 +
					psKCCBCtl->ui32WriteOffset -
					psKCCBCtl->ui32ReadOffset) & psKCCBCtl->ui32WrapMask;
	ui32CurrentQueueLength += psDevInfo->ui32KCCBDeferredCommandsCount;

	for (ui32MaxRetries = ui32CurrentQueueLength + 1;
				ui32MaxRetries > 0;
				ui32MaxRetries--)
	{

		eError = PVRSRVPollForValueKM(psDevNode, pui32LinMemAddr, ui32Value, ui32Mask, IMG_FALSE);
		if (eError != PVRSRV_ERROR_TIMEOUT)
		{
			break;
		}

		RGXSendCommandsFromDeferredList(psDevInfo, IMG_FALSE);
	}

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed! Error(%s) CPU linear address(%p) Expected value(%u)",
		                        __func__, PVRSRVGetErrorString(eError),
								pui32LinMemAddr, ui32Value));
	}

	return eError;
}

PVRSRV_ERROR RGXStateFlagCtrl(PVRSRV_RGXDEV_INFO *psDevInfo,
		IMG_UINT32 ui32Config,
		IMG_UINT32 *pui32ConfigState,
		IMG_BOOL bSetNotClear)
{
	PVRSRV_ERROR eError;
	PVRSRV_DEV_POWER_STATE ePowerState;
	RGXFWIF_KCCB_CMD sStateFlagCmd = { 0 };
	PVRSRV_DEVICE_NODE *psDeviceNode;
	RGXFWIF_SYSDATA *psSysData;
	IMG_UINT32 ui32kCCBCommandSlot;
	IMG_BOOL bWaitForFwUpdate = IMG_FALSE;

	if (!psDevInfo)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psDeviceNode = psDevInfo->psDeviceNode;
	psSysData = psDevInfo->psRGXFWIfFwSysData;

	if (NULL == psSysData)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Fw Sys Config is not mapped into CPU space", __func__));
		return PVRSRV_ERROR_INVALID_CPU_ADDR;
	}

	/* apply change and ensure the new data is written to memory
	 * before requesting the FW to read it
	 */
	ui32Config = ui32Config & RGXFWIF_INICFG_ALL;
	if (bSetNotClear)
	{
		psSysData->ui32ConfigFlags |= ui32Config;
	}
	else
	{
		psSysData->ui32ConfigFlags &= ~ui32Config;
	}

	/* return current/new value to caller */
	if (pui32ConfigState)
	{
		*pui32ConfigState = psSysData->ui32ConfigFlags;
	}

	OSMemoryBarrier();

	eError = PVRSRVPowerLock(psDeviceNode);
	PVR_LOG_RETURN_IF_ERROR(eError, "PVRSRVPowerLock");

	/* notify FW to update setting */
	eError = PVRSRVGetDevicePowerState(psDeviceNode, &ePowerState);

	if ((eError == PVRSRV_OK) && (ePowerState != PVRSRV_DEV_POWER_STATE_OFF))
	{
		/* Ask the FW to update its cached version of the value */
		sStateFlagCmd.eCmdType = RGXFWIF_KCCB_CMD_STATEFLAGS_CTRL;

		eError = RGXSendCommandAndGetKCCBSlot(psDevInfo,
											  &sStateFlagCmd,
											  PDUMP_FLAGS_CONTINUOUS,
											  &ui32kCCBCommandSlot);
		PVR_LOG_GOTO_IF_ERROR(eError, "RGXSendCommandAndGetKCCBSlot", unlock);
		bWaitForFwUpdate = IMG_TRUE;
	}

unlock:
	PVRSRVPowerUnlock(psDeviceNode);
	if (bWaitForFwUpdate)
	{
		/* Wait for the value to be updated as the FW validates
		 * the parameters and modifies the ui32ConfigFlags
		 * accordingly
		 * (for completeness as registered callbacks should also
		 *  not permit invalid transitions)
		 */
		eError = RGXWaitForKCCBSlotUpdate(psDevInfo, ui32kCCBCommandSlot, PDUMP_FLAGS_CONTINUOUS);
		PVR_LOG_IF_ERROR(eError, "RGXWaitForKCCBSlotUpdate");
	}
	return eError;
}

static
PVRSRV_ERROR RGXScheduleCleanupCommand(PVRSRV_RGXDEV_INFO	*psDevInfo,
		RGXFWIF_DM			eDM,
		RGXFWIF_KCCB_CMD		*psKCCBCmd,
		RGXFWIF_CLEANUP_TYPE	eCleanupType,
		IMG_UINT32				ui32PDumpFlags)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32kCCBCommandSlot;

	psKCCBCmd->eCmdType = RGXFWIF_KCCB_CMD_CLEANUP;
	psKCCBCmd->uCmdData.sCleanupData.eCleanupType = eCleanupType;

	/*
		Send the cleanup request to the firmware. If the resource is still busy
		the firmware will tell us and we'll drop out with a retry.
	*/
	eError = RGXScheduleCommandAndGetKCCBSlot(psDevInfo,
	                                          eDM,
											  psKCCBCmd,
											  0,
											  ui32PDumpFlags,
											  &ui32kCCBCommandSlot);
	PVR_LOG_GOTO_IF_ERROR(eError, "RGXScheduleCommandAndGetKCCBSlot", fail_command);

	/* Wait for command kCCB slot to be updated by FW */
	PDUMPCOMMENT("Wait for the firmware to reply to the cleanup command");
	eError = RGXWaitForKCCBSlotUpdate(psDevInfo, ui32kCCBCommandSlot,
									  ui32PDumpFlags);
	/*
		If the firmware hasn't got back to us in a timely manner
		then bail and let the caller retry the command.
	 */
	if (eError == PVRSRV_ERROR_TIMEOUT)
	{
		PVR_DPF((PVR_DBG_WARNING,
		         "%s: RGXWaitForKCCBSlotUpdate timed out. Dump debug information.",
		         __func__));

		eError = PVRSRV_ERROR_RETRY;
#if defined(DEBUG)
		PVRSRVDebugRequest(psDevInfo->psDeviceNode,
				DEBUG_REQUEST_VERBOSITY_MAX, NULL, NULL);
#endif
		goto fail_poll;
	}
	else if (eError != PVRSRV_OK)
	{
		goto fail_poll;
	}

#if defined(PDUMP)
	/*
	 * The cleanup request to the firmware will tell us if a given resource is busy or not.
	 * If the RGXFWIF_KCCB_RTN_SLOT_CLEANUP_BUSY flag is set, this means that the resource is
	 * still in use. In this case we return a PVRSRV_ERROR_RETRY error to the client drivers
	 * and they will re-issue the cleanup request until it succeed.
	 *
	 * Since this retry mechanism doesn't work for pdumps, client drivers should ensure
	 * that cleanup requests are only submitted if the resource is unused.
	 * If this is not the case, the following poll will block infinitely, making sure
	 * the issue doesn't go unnoticed.
	 */
	PDUMPCOMMENT("Cleanup: If this poll fails, the following resource is still in use (DM=%u, type=%u, address=0x%08x), which is incorrect in pdumps",
			eDM,
			psKCCBCmd->uCmdData.sCleanupData.eCleanupType,
			psKCCBCmd->uCmdData.sCleanupData.uCleanupData.psContext.ui32Addr);
	eError = DevmemPDumpDevmemPol32(psDevInfo->psKernelCCBRtnSlotsMemDesc,
									ui32kCCBCommandSlot * sizeof(IMG_UINT32),
									0,
									RGXFWIF_KCCB_RTN_SLOT_CLEANUP_BUSY,
									PDUMP_POLL_OPERATOR_EQUAL,
									ui32PDumpFlags);
	PVR_LOG_IF_ERROR(eError, "DevmemPDumpDevmemPol32");
#endif

	/*
		If the command has was run but a resource was busy, then the request
		will need to be retried.
	*/
	if (unlikely(psDevInfo->pui32KernelCCBRtnSlots[ui32kCCBCommandSlot] & RGXFWIF_KCCB_RTN_SLOT_CLEANUP_BUSY))
	{
		if (psDevInfo->pui32KernelCCBRtnSlots[ui32kCCBCommandSlot] & RGXFWIF_KCCB_RTN_SLOT_POLL_FAILURE)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: FW poll on a HW operation failed", __func__));
		}
		eError = PVRSRV_ERROR_RETRY;
		goto fail_requestbusy;
	}

	return PVRSRV_OK;

fail_requestbusy:
fail_poll:
fail_command:
	PVR_ASSERT(eError != PVRSRV_OK);

	return eError;
}

/*
	RGXRequestCommonContextCleanUp
*/
PVRSRV_ERROR RGXFWRequestCommonContextCleanUp(PVRSRV_DEVICE_NODE *psDeviceNode,
		RGX_SERVER_COMMON_CONTEXT *psServerCommonContext,
		RGXFWIF_DM eDM,
		IMG_UINT32 ui32PDumpFlags)
{
	RGXFWIF_KCCB_CMD			sRCCleanUpCmd = {0};
	PVRSRV_ERROR				eError;
	PRGXFWIF_FWCOMMONCONTEXT	psFWCommonContextFWAddr;
	PVRSRV_RGXDEV_INFO			*psDevInfo = (PVRSRV_RGXDEV_INFO*)psDeviceNode->pvDevice;

	/* Force retry if this context's CCB is currently being dumped
	 * as part of the stalled CCB debug */
	if (psDevInfo->pvEarliestStalledClientCCB == (void*)psServerCommonContext->psClientCCB)
	{
		PVR_DPF((PVR_DBG_WARNING,
		         "%s: Forcing retry as psDevInfo->pvEarliestStalledClientCCB = psServerCommonContext->psClientCCB <%p>",
		         __func__,
		         (void*)psServerCommonContext->psClientCCB));
		return PVRSRV_ERROR_RETRY;
	}

	psFWCommonContextFWAddr = FWCommonContextGetFWAddress(psServerCommonContext);
#if defined(PDUMP)
	PDUMPCOMMENT("Common ctx cleanup Request DM%d [context = 0x%08x]",
			eDM, psFWCommonContextFWAddr.ui32Addr);
	PDUMPCOMMENT("Wait for CCB to be empty before common ctx cleanup");

	RGXCCBPDumpDrainCCB(FWCommonContextGetClientCCB(psServerCommonContext), ui32PDumpFlags);
#endif

	/* Setup our command data, the cleanup call will fill in the rest */
	sRCCleanUpCmd.uCmdData.sCleanupData.uCleanupData.psContext = psFWCommonContextFWAddr;

	/* Request cleanup of the firmware resource */
	eError = RGXScheduleCleanupCommand(psDeviceNode->pvDevice,
			eDM,
			&sRCCleanUpCmd,
			RGXFWIF_CLEANUP_FWCOMMONCONTEXT,
			ui32PDumpFlags);

	if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_RETRY))
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to schedule a memory context cleanup with error (%u)",
		         __func__, eError));
	}

	return eError;
}

/*
 * RGXFWRequestHWRTDataCleanUp
 */

PVRSRV_ERROR RGXFWRequestHWRTDataCleanUp(PVRSRV_DEVICE_NODE *psDeviceNode,
		PRGXFWIF_HWRTDATA psHWRTData)
{
	RGXFWIF_KCCB_CMD			sHWRTDataCleanUpCmd = {0};
	PVRSRV_ERROR				eError;

	PDUMPCOMMENT("HW RTData cleanup Request [HWRTData = 0x%08x]", psHWRTData.ui32Addr);

	sHWRTDataCleanUpCmd.uCmdData.sCleanupData.uCleanupData.psHWRTData = psHWRTData;

	eError = RGXScheduleCleanupCommand(psDeviceNode->pvDevice,
			RGXFWIF_DM_GP,
			&sHWRTDataCleanUpCmd,
			RGXFWIF_CLEANUP_HWRTDATA,
			PDUMP_FLAGS_NONE);

	if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_RETRY))
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to schedule a HWRTData cleanup with error (%u)",
		         __func__, eError));
	}

	return eError;
}

/*
	RGXFWRequestFreeListCleanUp
*/
PVRSRV_ERROR RGXFWRequestFreeListCleanUp(PVRSRV_RGXDEV_INFO *psDevInfo,
		PRGXFWIF_FREELIST psFWFreeList)
{
	RGXFWIF_KCCB_CMD			sFLCleanUpCmd = {0};
	PVRSRV_ERROR				eError;

	PDUMPCOMMENT("Free list cleanup Request [FreeList = 0x%08x]", psFWFreeList.ui32Addr);

	/* Setup our command data, the cleanup call will fill in the rest */
	sFLCleanUpCmd.uCmdData.sCleanupData.uCleanupData.psFreelist = psFWFreeList;

	/* Request cleanup of the firmware resource */
	eError = RGXScheduleCleanupCommand(psDevInfo,
			RGXFWIF_DM_GP,
			&sFLCleanUpCmd,
			RGXFWIF_CLEANUP_FREELIST,
			PDUMP_FLAGS_NONE);

	if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_RETRY))
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to schedule a memory context cleanup with error (%u)",
		         __func__, eError));
	}

	return eError;
}

/*
	RGXFWRequestZSBufferCleanUp
*/
PVRSRV_ERROR RGXFWRequestZSBufferCleanUp(PVRSRV_RGXDEV_INFO *psDevInfo,
		PRGXFWIF_ZSBUFFER psFWZSBuffer)
{
	RGXFWIF_KCCB_CMD			sZSBufferCleanUpCmd = {0};
	PVRSRV_ERROR				eError;

	PDUMPCOMMENT("ZS Buffer cleanup Request [ZS Buffer = 0x%08x]", psFWZSBuffer.ui32Addr);

	/* Setup our command data, the cleanup call will fill in the rest */
	sZSBufferCleanUpCmd.uCmdData.sCleanupData.uCleanupData.psZSBuffer = psFWZSBuffer;

	/* Request cleanup of the firmware resource */
	eError = RGXScheduleCleanupCommand(psDevInfo,
			RGXFWIF_DM_3D,
			&sZSBufferCleanUpCmd,
			RGXFWIF_CLEANUP_ZSBUFFER,
			PDUMP_FLAGS_NONE);

	if ((eError != PVRSRV_OK) && (eError != PVRSRV_ERROR_RETRY))
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Failed to schedule a memory context cleanup with error (%u)",
		         __func__, eError));
	}

	return eError;
}

PVRSRV_ERROR RGXFWSetHCSDeadline(PVRSRV_RGXDEV_INFO *psDevInfo,
		IMG_UINT32 ui32HCSDeadlineMs)
{
	PVRSRV_ERROR eError;
	RGXFWIF_KCCB_CMD	sSetHCSDeadline = { 0 };

	sSetHCSDeadline.eCmdType                            = RGXFWIF_KCCB_CMD_HCS_SET_DEADLINE;
	sSetHCSDeadline.uCmdData.sHCSCtrl.ui32HCSDeadlineMS = ui32HCSDeadlineMs;

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psDevInfo,
				RGXFWIF_DM_GP,
				&sSetHCSDeadline,
				0,
				PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	return eError;
}

PVRSRV_ERROR RGXFWHealthCheckCmd(PVRSRV_RGXDEV_INFO *psDevInfo)
{
	RGXFWIF_KCCB_CMD	sCmpKCCBCmd = { 0 };

	sCmpKCCBCmd.eCmdType = RGXFWIF_KCCB_CMD_HEALTH_CHECK;

	return	RGXScheduleCommand(psDevInfo,
							   RGXFWIF_DM_GP,
							   &sCmpKCCBCmd,
							   0,
							   PDUMP_FLAGS_CONTINUOUS);
}

PVRSRV_ERROR RGXFWSetOSIsolationThreshold(PVRSRV_RGXDEV_INFO *psDevInfo,
		IMG_UINT32 ui32IsolationPriorityThreshold)
{
	PVRSRV_ERROR eError;
	RGXFWIF_KCCB_CMD	sOSidIsoConfCmd = { 0 };

	sOSidIsoConfCmd.eCmdType = RGXFWIF_KCCB_CMD_OS_ISOLATION_GROUP_CHANGE;
	sOSidIsoConfCmd.uCmdData.sCmdOSidIsolationData.ui32IsolationPriorityThreshold = ui32IsolationPriorityThreshold;

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psDevInfo,
				RGXFWIF_DM_GP,
				&sOSidIsoConfCmd,
				0,
				PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	return eError;
}

PVRSRV_ERROR RGXFWSetFwOsState(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_UINT32 ui32OSid,
                               RGXFWIF_OS_STATE_CHANGE eOSOnlineState)
{
	PVRSRV_ERROR             eError = PVRSRV_OK;
	RGXFWIF_KCCB_CMD         sOSOnlineStateCmd = { 0 };
	RGXFWIF_SYSDATA          *psFwSysData = psDevInfo->psRGXFWIfFwSysData;
	PVRSRV_VZ_RET_IF_MODE(GUEST, PVRSRV_OK);

	sOSOnlineStateCmd.eCmdType = RGXFWIF_KCCB_CMD_OS_ONLINE_STATE_CONFIGURE;
	sOSOnlineStateCmd.uCmdData.sCmdOSOnlineStateData.ui32OSid = ui32OSid;
	sOSOnlineStateCmd.uCmdData.sCmdOSOnlineStateData.eNewOSState = eOSOnlineState;

	if (eOSOnlineState == RGXFWIF_OS_ONLINE)
	{
		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			eError = RGXScheduleCommand(psDevInfo,
					RGXFWIF_DM_GP,
					&sOSOnlineStateCmd,
					0,
					PDUMP_FLAGS_CONTINUOUS);
			if (eError != PVRSRV_ERROR_RETRY) break;

			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
	}
	else if (psFwSysData)
	{
		IMG_UINT32 ui32kCCBCommandSlot;
		volatile RGXFWIF_OS_RUNTIME_FLAGS *psFwRunFlags;

		psFwRunFlags = (volatile RGXFWIF_OS_RUNTIME_FLAGS*) &psFwSysData->asOsRuntimeFlagsMirror[ui32OSid];
		/* Attempt several times until the FW manages to offload the OS */
		LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
		{
			/* Send request */
			eError = RGXScheduleCommandAndGetKCCBSlot(psDevInfo,
			                                          RGXFWIF_DM_GP,
													  &sOSOnlineStateCmd,
													  0,
													  PDUMP_FLAGS_CONTINUOUS,
													  &ui32kCCBCommandSlot);
			if (unlikely(eError == PVRSRV_ERROR_RETRY)) continue;
			PVR_LOG_GOTO_IF_ERROR(eError, "RGXScheduleCommand", return_);

			/* Wait for FW to process the cmd */
			eError = RGXWaitForKCCBSlotUpdate(psDevInfo, ui32kCCBCommandSlot, PDUMP_FLAGS_CONTINUOUS);
			PVR_LOG_GOTO_IF_ERROR(eError, "RGXWaitForKCCBSlotUpdate", return_);

			/* read the OS state */
			OSMemoryBarrier();
			/* check if FW finished offloading the OSID and is stopped */
			if (psFwRunFlags->bfOsState == RGXFW_CONNECTION_FW_OFFLINE)
			{
				eError = PVRSRV_OK;
				break;
			}
			else
			{
				eError = PVRSRV_ERROR_TIMEOUT;
			}

			OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		} END_LOOP_UNTIL_TIMEOUT();
	}
	else
	{
		eError = PVRSRV_ERROR_NOT_INITIALISED;
	}

return_ :
	return eError;
}

PVRSRV_ERROR RGXFWChangeOSidPriority(PVRSRV_RGXDEV_INFO *psDevInfo,
		IMG_UINT32 ui32OSid,
		IMG_UINT32 ui32Priority)
{
	PVRSRV_ERROR eError;
	RGXFWIF_KCCB_CMD	sOSidPriorityCmd = { 0 };

	sOSidPriorityCmd.eCmdType = RGXFWIF_KCCB_CMD_OSID_PRIORITY_CHANGE;
	sOSidPriorityCmd.uCmdData.sCmdOSidPriorityData.ui32OSidNum = ui32OSid;
	sOSidPriorityCmd.uCmdData.sCmdOSidPriorityData.ui32Priority = ui32Priority;

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psDevInfo,
				RGXFWIF_DM_GP,
				&sOSidPriorityCmd,
				0,
				PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	return eError;
}

PVRSRV_ERROR ContextSetPriority(RGX_SERVER_COMMON_CONTEXT *psContext,
		CONNECTION_DATA *psConnection,
		PVRSRV_RGXDEV_INFO *psDevInfo,
		IMG_UINT32 ui32Priority,
		RGXFWIF_DM eDM)
{
	IMG_UINT32				ui32CmdSize;
	IMG_UINT8				*pui8CmdPtr;
	RGXFWIF_KCCB_CMD		sPriorityCmd = { 0 };
	RGXFWIF_CCB_CMD_HEADER	*psCmdHeader;
	RGXFWIF_CMD_PRIORITY	*psCmd;
	PVRSRV_ERROR			eError;
	RGX_CLIENT_CCB *psClientCCB = FWCommonContextGetClientCCB(psContext);

	/*
		Get space for command
	 */
	ui32CmdSize = RGX_CCB_FWALLOC_ALIGN(sizeof(RGXFWIF_CCB_CMD_HEADER) + sizeof(RGXFWIF_CMD_PRIORITY));

	eError = RGXAcquireCCB(psClientCCB,
			ui32CmdSize,
			(void **) &pui8CmdPtr,
			PDUMP_FLAGS_CONTINUOUS);
	if (eError != PVRSRV_OK)
	{
		if (eError != PVRSRV_ERROR_RETRY)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to acquire space for client CCB", __func__));
		}
		goto fail_ccbacquire;
	}

	/*
		Write the command header and command
	*/
	psCmdHeader = (RGXFWIF_CCB_CMD_HEADER *) pui8CmdPtr;
	psCmdHeader->eCmdType = RGXFWIF_CCB_CMD_TYPE_PRIORITY;
	psCmdHeader->ui32CmdSize = RGX_CCB_FWALLOC_ALIGN(sizeof(RGXFWIF_CMD_PRIORITY));
	pui8CmdPtr += sizeof(*psCmdHeader);

	psCmd = (RGXFWIF_CMD_PRIORITY *) pui8CmdPtr;
	psCmd->ui32Priority = ui32Priority;
	pui8CmdPtr += sizeof(*psCmd);

	/*
		We should reserved space in the kernel CCB here and fill in the command
		directly.
		This is so if there isn't space in the kernel CCB we can return with
		retry back to services client before we take any operations
	 */

	/*
		Submit the command
	 */
	RGXReleaseCCB(psClientCCB,
			ui32CmdSize,
			PDUMP_FLAGS_CONTINUOUS);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to release space in client CCB", __func__));
		return eError;
	}

	/* Construct the priority command. */
	sPriorityCmd.eCmdType = RGXFWIF_KCCB_CMD_KICK;
	sPriorityCmd.uCmdData.sCmdKickData.psContext = FWCommonContextGetFWAddress(psContext);
	sPriorityCmd.uCmdData.sCmdKickData.ui32CWoffUpdate = RGXGetHostWriteOffsetCCB(psClientCCB);
	sPriorityCmd.uCmdData.sCmdKickData.ui32CWrapMaskUpdate = RGXGetWrapMaskCCB(psClientCCB);
	sPriorityCmd.uCmdData.sCmdKickData.ui32NumCleanupCtl = 0;
	sPriorityCmd.uCmdData.sCmdKickData.ui32WorkEstCmdHeaderOffset = 0;

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psDevInfo,
				eDM,
				&sPriorityCmd,
				0,
				PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Failed to submit set priority command with error (%u)",
				__func__,
				eError));
	}

	return PVRSRV_OK;

fail_ccbacquire:
	PVR_ASSERT(eError != PVRSRV_OK);
	return eError;
}

PVRSRV_ERROR RGXFWConfigPHR(PVRSRV_RGXDEV_INFO *psDevInfo,
                            IMG_UINT32 ui32PHRMode)
{
	PVRSRV_ERROR eError;
	RGXFWIF_KCCB_CMD sCfgPHRCmd = { 0 };

	sCfgPHRCmd.eCmdType = RGXFWIF_KCCB_CMD_PHR_CFG;
	sCfgPHRCmd.uCmdData.sPeriodicHwResetCfg.ui32PHRMode = ui32PHRMode;

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		eError = RGXScheduleCommand(psDevInfo,
		                            RGXFWIF_DM_GP,
		                            &sCfgPHRCmd,
		                            0,
		                            PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();

	return eError;
}

/*
	RGXReadMETAAddr
*/
PVRSRV_ERROR RGXReadMETAAddr(PVRSRV_RGXDEV_INFO	*psDevInfo, IMG_UINT32 ui32METAAddr, IMG_UINT32 *pui32Value)
{
	IMG_UINT8 __iomem  *pui8RegBase = psDevInfo->pvRegsBaseKM;
	IMG_UINT32 ui32Value;

	/* Wait for Slave Port to be Ready */
	if (PVRSRVPollForValueKM(psDevInfo->psDeviceNode,
			(IMG_UINT32 __iomem *) (pui8RegBase + RGX_CR_META_SP_MSLVCTRL1),
			RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
			RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
			IMG_FALSE /* Also called from debug-dumping code, so IMG_FALSE */) != PVRSRV_OK)
	{
		return PVRSRV_ERROR_TIMEOUT;
	}

	/* Issue the Read */
	OSWriteHWReg32(
			psDevInfo->pvRegsBaseKM,
			RGX_CR_META_SP_MSLVCTRL0,
			ui32METAAddr | RGX_CR_META_SP_MSLVCTRL0_RD_EN);
	(void) OSReadHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_SP_MSLVCTRL0);

	/* Wait for Slave Port to be Ready: read complete */
	if (PVRSRVPollForValueKM(psDevInfo->psDeviceNode,
			(IMG_UINT32 __iomem *) (pui8RegBase + RGX_CR_META_SP_MSLVCTRL1),
			RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
			RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
			IMG_FALSE /* Also called from debug-dumping code, so IMG_FALSE */) != PVRSRV_OK)
	{
		return PVRSRV_ERROR_TIMEOUT;
	}

	/* Read the value */
	ui32Value = OSReadHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_SP_MSLVDATAX);

	*pui32Value = ui32Value;

	return PVRSRV_OK;
}

/*
	RGXWriteMETAAddr
*/
PVRSRV_ERROR RGXWriteMETAAddr(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_UINT32 ui32METAAddr, IMG_UINT32 ui32Value)
{
	IMG_UINT8 __iomem *pui8RegBase = psDevInfo->pvRegsBaseKM;

	/* Wait for Slave Port to be Ready */
	if (PVRSRVPollForValueKM(psDevInfo->psDeviceNode,
			(IMG_UINT32 __iomem *)(pui8RegBase + RGX_CR_META_SP_MSLVCTRL1),
			RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
			RGX_CR_META_SP_MSLVCTRL1_READY_EN|RGX_CR_META_SP_MSLVCTRL1_GBLPORT_IDLE_EN,
			IMG_FALSE /* Also called from debug-dumping code, so IMG_FALSE */) != PVRSRV_OK)
	{
		return PVRSRV_ERROR_TIMEOUT;
	}

	/* Issue the Write */
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_SP_MSLVCTRL0, ui32METAAddr);
	OSWriteHWReg32(psDevInfo->pvRegsBaseKM, RGX_CR_META_SP_MSLVDATAT, ui32Value);

	return PVRSRV_OK;
}

void RGXCheckForStalledClientContexts(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_BOOL bIgnorePrevious)
{
	/* Attempt to detect and deal with any stalled client contexts.
	 * bIgnorePrevious may be set by the caller if they know a context to be
	 * stalled, as otherwise this function will only identify stalled
	 * contexts which have not been previously reported.
	 */

	IMG_UINT32 ui32StalledClientMask = 0;

	if (!(OSTryLockAcquire(psDevInfo->hCCBStallCheckLock)))
	{
		PVR_LOG(("RGXCheckForStalledClientContexts: Failed to acquire hCCBStallCheckLock, returning..."));
		return;
	}

	ui32StalledClientMask |= CheckForStalledClientTransferCtxt(psDevInfo);

	ui32StalledClientMask |= CheckForStalledClientRenderCtxt(psDevInfo);

	ui32StalledClientMask |= CheckForStalledClientKickSyncCtxt(psDevInfo);

	if (psDevInfo->sDevFeatureCfg.ui64Features & RGX_FEATURE_COMPUTE_BIT_MASK)
	{
		ui32StalledClientMask |= CheckForStalledClientComputeCtxt(psDevInfo);
	}

	/* If at least one DM stalled bit is different than before */
	if (bIgnorePrevious || (psDevInfo->ui32StalledClientMask != ui32StalledClientMask))//(psDevInfo->ui32StalledClientMask ^ ui32StalledClientMask))
	{
		if (ui32StalledClientMask > 0)
		{
			static __maybe_unused const char *pszStalledAction =
#if defined(PVRSRV_STALLED_CCB_ACTION)
					"force";
#else
					"warn";
#endif
			/* Print all the stalled DMs */
			PVR_LOG(("Possible stalled client RGX contexts detected: %s%s%s%s%s%s%s%s%s",
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_GP),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_TDM_2D),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_TA),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_3D),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_CDM),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_RTU),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_SHG),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_TQ2D),
					 RGX_STRINGIFY_KICK_TYPE_DM_IF_SET(ui32StalledClientMask, RGX_KICK_TYPE_DM_TQ3D)));

			PVR_LOG(("Trying to identify stalled context...(%s) [%d]",
			         pszStalledAction, bIgnorePrevious));

			DumpStalledContextInfo(psDevInfo);
		}
		else
		{
			if (psDevInfo->ui32StalledClientMask> 0)
			{
				/* Indicate there are no stalled DMs */
				PVR_LOG(("No further stalled client contexts exist"));
			}
		}
		psDevInfo->ui32StalledClientMask = ui32StalledClientMask;
		psDevInfo->pvEarliestStalledClientCCB = NULL;
	}
	OSLockRelease(psDevInfo->hCCBStallCheckLock);
}

/*
	RGXUpdateHealthStatus
*/
PVRSRV_ERROR RGXUpdateHealthStatus(PVRSRV_DEVICE_NODE* psDevNode,
                                   IMG_BOOL bCheckAfterTimePassed)
{
	PVRSRV_DATA*                 psPVRSRVData = PVRSRVGetPVRSRVData();
	PVRSRV_DEVICE_HEALTH_STATUS  eNewStatus   = PVRSRV_DEVICE_HEALTH_STATUS_OK;
	PVRSRV_DEVICE_HEALTH_REASON  eNewReason   = PVRSRV_DEVICE_HEALTH_REASON_NONE;
	PVRSRV_RGXDEV_INFO*          psDevInfo;
	RGXFWIF_TRACEBUF*            psRGXFWIfTraceBufCtl;
	RGXFWIF_SYSDATA*             psFwSysData;
	RGXFWIF_CCB_CTL              *psKCCBCtl;
	IMG_UINT32                   ui32ThreadCount;
	IMG_BOOL                     bKCCBCmdsWaiting;

	PVR_ASSERT(psDevNode != NULL);
	psDevInfo = psDevNode->pvDevice;

	/* If the firmware is not yet initialised or has already deinitialised, stop here */
	if (psDevInfo  == NULL || !psDevInfo->bFirmwareInitialised || psDevInfo->pvRegsBaseKM == NULL ||
		psDevInfo->psDeviceNode == NULL || psDevInfo->psDeviceNode->eDevState == PVRSRV_DEVICE_STATE_DEINIT)
	{
		return PVRSRV_OK;
	}

	psRGXFWIfTraceBufCtl = psDevInfo->psRGXFWIfTraceBufCtl;
	psFwSysData = psDevInfo->psRGXFWIfFwSysData;

#if defined(SUPPORT_AUTOVZ)
	if (KM_FW_CONNECTION_IS(ACTIVE, psDevInfo) && KM_OS_CONNECTION_IS(ACTIVE, psDevInfo))
	{
		/* read and write back the alive token value to confirm to the
		 * virtualisation watchdog that this connection is healthy */
		KM_SET_OS_ALIVE_TOKEN(KM_GET_FW_ALIVE_TOKEN(psDevInfo), psDevInfo);
	}
#endif

	PVRSRV_VZ_RET_IF_MODE(GUEST, PVRSRV_OK);

	/* If this is a quick update, then include the last current value... */
	if (!bCheckAfterTimePassed)
	{
		eNewStatus = OSAtomicRead(&psDevNode->eHealthStatus);
		eNewReason = OSAtomicRead(&psDevNode->eHealthReason);
	}

	/* Decrement the SLR holdoff counter (if non-zero) */
	if (psDevInfo->ui32SLRHoldoffCounter > 0)
	{
		psDevInfo->ui32SLRHoldoffCounter--;
	}

	/* If Rogue is not powered on, just skip ahead and check for stalled client CCBs */
	if (PVRSRVIsDevicePowered(psDevNode))
	{
		if (psRGXFWIfTraceBufCtl != NULL)
		{
			/*
			   Firmware thread checks...
			 */
			for (ui32ThreadCount = 0; ui32ThreadCount < RGXFW_THREAD_NUM; ui32ThreadCount++)
			{
				IMG_CHAR* pszTraceAssertInfo = psRGXFWIfTraceBufCtl->sTraceBuf[ui32ThreadCount].sAssertBuf.szInfo;

				/*
				Check if the FW has hit an assert...
				*/
				if (*pszTraceAssertInfo != '\0')
				{
					PVR_DPF((PVR_DBG_WARNING, "%s: Firmware thread %d has asserted: %s (%s:%d)",
							__func__, ui32ThreadCount, pszTraceAssertInfo,
							psRGXFWIfTraceBufCtl->sTraceBuf[ui32ThreadCount].sAssertBuf.szPath,
							psRGXFWIfTraceBufCtl->sTraceBuf[ui32ThreadCount].sAssertBuf.ui32LineNum));
					eNewStatus = PVRSRV_DEVICE_HEALTH_STATUS_DEAD;
					eNewReason = PVRSRV_DEVICE_HEALTH_REASON_ASSERTED;
					{
						/* MTK: dump log */
						static int dump = 0;

						if (dump == 0)
						{
							MTKPP_TriggerAEE(0);
							dump = 1;
						}
					}
					goto _RGXUpdateHealthStatus_Exit;
				}

				/*
				   Check the threads to see if they are in the same poll locations as last time...
				*/
				if (bCheckAfterTimePassed)
				{
					if (psFwSysData->aui32CrPollAddr[ui32ThreadCount] != 0  &&
						psFwSysData->aui32CrPollCount[ui32ThreadCount] == psDevInfo->aui32CrLastPollCount[ui32ThreadCount])
					{
						PVR_DPF((PVR_DBG_WARNING, "%s: Firmware stuck on CR poll: T%u polling %s (reg:0x%08X mask:0x%08X)",
								__func__, ui32ThreadCount,
								((psFwSysData->aui32CrPollAddr[ui32ThreadCount] & RGXFW_POLL_TYPE_SET)?("set"):("unset")),
								psFwSysData->aui32CrPollAddr[ui32ThreadCount] & ~RGXFW_POLL_TYPE_SET,
								psFwSysData->aui32CrPollMask[ui32ThreadCount]));
						eNewStatus = PVRSRV_DEVICE_HEALTH_STATUS_NOT_RESPONDING;
						eNewReason = PVRSRV_DEVICE_HEALTH_REASON_POLL_FAILING;
						goto _RGXUpdateHealthStatus_Exit;
					}
					psDevInfo->aui32CrLastPollCount[ui32ThreadCount] = psFwSysData->aui32CrPollCount[ui32ThreadCount];
				}
			}

			/*
			Check if the FW has faulted...
			*/
			if (psFwSysData->ui32HWRStateFlags & RGXFWIF_HWR_FW_FAULT)
			{
				PVR_DPF((PVR_DBG_WARNING,
						"%s: Firmware has faulted and needs to restart",
						__func__));
				eNewStatus = PVRSRV_DEVICE_HEALTH_STATUS_FAULT;
				if (psFwSysData->ui32HWRStateFlags & RGXFWIF_HWR_RESTART_REQUESTED)
				{
					eNewReason = PVRSRV_DEVICE_HEALTH_REASON_RESTARTING;
				}
				else
				{
					eNewReason = PVRSRV_DEVICE_HEALTH_REASON_IDLING;
				}
				goto _RGXUpdateHealthStatus_Exit;
			}
		}

		/*
		   Event Object Timeouts check...
		*/
		if (!bCheckAfterTimePassed)
		{
			if (psDevInfo->ui32GEOTimeoutsLastTime > 1 && psPVRSRVData->ui32GEOConsecutiveTimeouts > psDevInfo->ui32GEOTimeoutsLastTime)
			{
				PVR_DPF((PVR_DBG_WARNING, "%s: Global Event Object Timeouts have risen (from %d to %d)",
						__func__,
						psDevInfo->ui32GEOTimeoutsLastTime, psPVRSRVData->ui32GEOConsecutiveTimeouts));
				eNewStatus = PVRSRV_DEVICE_HEALTH_STATUS_NOT_RESPONDING;
				eNewReason = PVRSRV_DEVICE_HEALTH_REASON_TIMEOUTS;
			}
			psDevInfo->ui32GEOTimeoutsLastTime = psPVRSRVData->ui32GEOConsecutiveTimeouts;
		}

		/*
		   Check the Kernel CCB pointer is valid. If any commands were waiting last time, then check
		   that some have executed since then.
		*/
		bKCCBCmdsWaiting = IMG_FALSE;
		psKCCBCtl = psDevInfo->psKernelCCBCtl;

		if (psKCCBCtl != NULL)
		{
			if (psKCCBCtl->ui32ReadOffset > psKCCBCtl->ui32WrapMask  ||
					psKCCBCtl->ui32WriteOffset > psKCCBCtl->ui32WrapMask)
			{
				PVR_DPF((PVR_DBG_WARNING, "%s: KCCB has invalid offset (ROFF=%d WOFF=%d)",
						__func__, psKCCBCtl->ui32ReadOffset, psKCCBCtl->ui32WriteOffset));
				eNewStatus = PVRSRV_DEVICE_HEALTH_STATUS_DEAD;
				eNewReason = PVRSRV_DEVICE_HEALTH_REASON_QUEUE_CORRUPT;
			}

			if (psKCCBCtl->ui32ReadOffset != psKCCBCtl->ui32WriteOffset)
			{
				bKCCBCmdsWaiting = IMG_TRUE;
			}
		}

		if (bCheckAfterTimePassed && psDevInfo->psRGXFWIfFwOsData != NULL)
		{
			IMG_UINT32 ui32KCCBCmdsExecuted = psDevInfo->psRGXFWIfFwOsData->ui32KCCBCmdsExecuted;

			if (psDevInfo->ui32KCCBCmdsExecutedLastTime == ui32KCCBCmdsExecuted)
			{
				/*
				   If something was waiting last time then the Firmware has stopped processing commands.
				*/
				if (psDevInfo->bKCCBCmdsWaitingLastTime)
				{
					PVR_DPF((PVR_DBG_WARNING, "%s: No KCCB commands executed since check!",
							__func__));
					eNewStatus = PVRSRV_DEVICE_HEALTH_STATUS_NOT_RESPONDING;
					eNewReason = PVRSRV_DEVICE_HEALTH_REASON_QUEUE_STALLED;
				}

				/*
				   If no commands are currently pending and nothing happened since the last poll, then
				   schedule a dummy command to ping the firmware so we know it is alive and processing.
				*/
				if (!bKCCBCmdsWaiting)
				{
					/* Protect the PDumpLoadMem. RGXScheduleCommand() cannot take the
					 * PMR lock itself, because some bridge functions will take the PMR lock
					 * before calling RGXScheduleCommand
					 */
					PVRSRV_ERROR eError = RGXFWHealthCheckCmd(psDevNode->pvDevice);

					if (eError != PVRSRV_OK)
					{
						PVR_DPF((PVR_DBG_WARNING, "%s: Cannot schedule Health Check command! (0x%x)",
								__func__, eError));
					}
					else
					{
						bKCCBCmdsWaiting = IMG_TRUE;
					}
				}
			}

			psDevInfo->bKCCBCmdsWaitingLastTime     = bKCCBCmdsWaiting;
			psDevInfo->ui32KCCBCmdsExecutedLastTime = ui32KCCBCmdsExecuted;
		}
	}

	/*
	   Interrupt counts check...
	*/
	if (bCheckAfterTimePassed  &&  psDevInfo->psRGXFWIfFwSysData != NULL)
	{
		IMG_UINT32  ui32LISRCount   = 0;
		IMG_UINT32  ui32FWCount     = 0;
		IMG_UINT32  ui32MissingInts = 0;

		/* Add up the total number of interrupts issued, sampled/received and missed... */
#if defined(RGX_FW_IRQ_OS_COUNTERS)
		/* Only the Host OS has a sample count, so only one counter to check. */
		ui32LISRCount += psDevInfo->aui32SampleIRQCount[RGXFW_HOST_OS];
		ui32FWCount   += OSReadHWReg32(psDevInfo->pvRegsBaseKM, gaui32FwOsIrqCntRegAddr[RGXFW_HOST_OS]);
#else
		IMG_UINT32  ui32Index;

		for (ui32Index = 0;  ui32Index < RGXFW_THREAD_NUM;  ui32Index++)
		{
			ui32LISRCount += psDevInfo->aui32SampleIRQCount[ui32Index];
			ui32FWCount   += psDevInfo->psRGXFWIfFwSysData->aui32InterruptCount[ui32Index];
		}
#endif /* RGX_FW_IRQ_OS_COUNTERS */

		if (ui32LISRCount < ui32FWCount)
		{
			ui32MissingInts = (ui32FWCount-ui32LISRCount);
		}

		if (ui32LISRCount == psDevInfo->ui32InterruptCountLastTime  &&
		    ui32MissingInts >= psDevInfo->ui32MissingInterruptsLastTime  &&
		    psDevInfo->ui32MissingInterruptsLastTime > 1)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: LISR has not received the last %d interrupts",
					__func__, ui32MissingInts));
			eNewStatus = PVRSRV_DEVICE_HEALTH_STATUS_NOT_RESPONDING;
			eNewReason = PVRSRV_DEVICE_HEALTH_REASON_MISSING_INTERRUPTS;

			/* Schedule the MISRs to help mitigate the problems of missing interrupts. */
			OSScheduleMISR(psDevInfo->pvMISRData);
			if (psDevInfo->pvAPMISRData != NULL)
			{
				OSScheduleMISR(psDevInfo->pvAPMISRData);
			}
		}
		psDevInfo->ui32InterruptCountLastTime    = ui32LISRCount;
		psDevInfo->ui32MissingInterruptsLastTime = ui32MissingInts;
	}

	/*
	   Stalled CCB check...
	*/
	if (bCheckAfterTimePassed && (PVRSRV_DEVICE_HEALTH_STATUS_OK==eNewStatus))
	{
		RGXCheckForStalledClientContexts(psDevInfo, IMG_FALSE);
	}

	/*
	   Finished, save the new status...
	*/
_RGXUpdateHealthStatus_Exit:
	OSAtomicWrite(&psDevNode->eHealthStatus, eNewStatus);
	OSAtomicWrite(&psDevNode->eHealthReason, eNewReason);
	RGXSRV_HWPERF_DEVICE_INFO(psDevInfo, RGX_HWPERF_DEV_INFO_EV_HEALTH, eNewStatus, eNewReason);

	/*
	 * Attempt to service the HWPerf buffer to regularly transport idle/periodic
	 * packets to host buffer.
	 */
	if (psDevNode->pfnServiceHWPerf != NULL)
	{
		PVRSRV_ERROR eError = psDevNode->pfnServiceHWPerf(psDevNode);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_WARNING, "%s: "
					"Error occurred when servicing HWPerf buffer (%d)",
					__func__, eError));
		}
	}

	/* Attempt to refresh timer correlation data */
	RGXTimeCorrRestartPeriodic(psDevNode);

	return PVRSRV_OK;
} /* RGXUpdateHealthStatus */

PVRSRV_ERROR CheckStalledClientCommonContext(RGX_SERVER_COMMON_CONTEXT *psCurrentServerCommonContext, RGX_KICK_TYPE_DM eKickTypeDM)
{
	if (psCurrentServerCommonContext == NULL)
	{
		/* the context has already been freed so there is nothing to do here */
		return PVRSRV_OK;
	}

	return CheckForStalledCCB(psCurrentServerCommonContext->psDevInfo->psDeviceNode,
	                          psCurrentServerCommonContext->psClientCCB,
	                          eKickTypeDM);
}

void DumpFWCommonContextInfo(RGX_SERVER_COMMON_CONTEXT *psCurrentServerCommonContext,
                             DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
                             void *pvDumpDebugFile,
                             IMG_UINT32 ui32VerbLevel)
{
	if (psCurrentServerCommonContext == NULL)
	{
		/* the context has already been freed so there is nothing to do here */
		return;
	}

	if (DD_VERB_LVL_ENABLED(ui32VerbLevel, DEBUG_REQUEST_VERBOSITY_HIGH))
	{
		/* If high verbosity requested, dump whole CCB */
		DumpCCB(psCurrentServerCommonContext->psDevInfo,
		        psCurrentServerCommonContext->sFWCommonContextFWAddr,
		        psCurrentServerCommonContext->psClientCCB,
		        pfnDumpDebugPrintf,
		        pvDumpDebugFile);
	}
	else
	{
		/* Otherwise, only dump first stalled command in the CCB */
		DumpStalledCCBCommand(psCurrentServerCommonContext->sFWCommonContextFWAddr,
		                      psCurrentServerCommonContext->psClientCCB,
		                      pfnDumpDebugPrintf,
		                      pvDumpDebugFile);
	}
}

PVRSRV_ERROR AttachKickResourcesCleanupCtls(PRGXFWIF_CLEANUP_CTL *apsCleanupCtl,
		IMG_UINT32 *pui32NumCleanupCtl,
		RGXFWIF_DM eDM,
		IMG_BOOL bKick,
		RGX_KM_HW_RT_DATASET           *psKMHWRTDataSet,
		RGX_ZSBUFFER_DATA              *psZSBuffer,
		RGX_ZSBUFFER_DATA              *psMSAAScratchBuffer)
{
	PVRSRV_ERROR eError;
	PRGXFWIF_CLEANUP_CTL *psCleanupCtlWrite = apsCleanupCtl;

	PVR_ASSERT((eDM == RGXFWIF_DM_GEOM) || (eDM == RGXFWIF_DM_3D));
	PVR_RETURN_IF_INVALID_PARAM((eDM == RGXFWIF_DM_GEOM) || (eDM == RGXFWIF_DM_3D));

	if (bKick)
	{
		if (psKMHWRTDataSet)
		{
			PRGXFWIF_CLEANUP_CTL psCleanupCtl;

			eError = RGXSetFirmwareAddress(&psCleanupCtl, psKMHWRTDataSet->psHWRTDataFwMemDesc,
					offsetof(RGXFWIF_HWRTDATA, sCleanupState),
					RFW_FWADDR_NOREF_FLAG);
			PVR_RETURN_IF_ERROR(eError);

			*(psCleanupCtlWrite++) = psCleanupCtl;
		}

		if (eDM == RGXFWIF_DM_3D)
		{
			RGXFWIF_PRBUFFER_TYPE eBufferType;
			RGX_ZSBUFFER_DATA *psBuffer = NULL;

			for (eBufferType = RGXFWIF_PRBUFFER_START; eBufferType < RGXFWIF_PRBUFFER_MAXSUPPORTED; eBufferType++)
			{
				switch (eBufferType)
				{
				case RGXFWIF_PRBUFFER_ZSBUFFER:
					psBuffer = psZSBuffer;
					break;
				case RGXFWIF_PRBUFFER_MSAABUFFER:
					psBuffer = psMSAAScratchBuffer;
					break;
				case RGXFWIF_PRBUFFER_MAXSUPPORTED:
					psBuffer = NULL;
					break;
				}
				if (psBuffer)
				{
					(psCleanupCtlWrite++)->ui32Addr = psBuffer->sZSBufferFWDevVAddr.ui32Addr +
							offsetof(RGXFWIF_PRBUFFER, sCleanupState);
					psBuffer = NULL;
				}
			}
		}
	}

	*pui32NumCleanupCtl = psCleanupCtlWrite - apsCleanupCtl;
	PVR_ASSERT(*pui32NumCleanupCtl <= RGXFWIF_KCCB_CMD_KICK_DATA_MAX_NUM_CLEANUP_CTLS);

	return PVRSRV_OK;
}

PVRSRV_ERROR RGXResetHWRLogs(PVRSRV_DEVICE_NODE *psDevNode)
{
	PVRSRV_RGXDEV_INFO       *psDevInfo;
	RGXFWIF_HWRINFOBUF       *psHWRInfoBuf;
	IMG_UINT32               i;

	if (psDevNode->pvDevice == NULL)
	{
		return PVRSRV_ERROR_INVALID_DEVINFO;
	}
	psDevInfo = psDevNode->pvDevice;

	psHWRInfoBuf = psDevInfo->psRGXFWIfHWRInfoBufCtl;

	for (i = 0 ; i < RGXFWIF_DM_MAX ; i++)
	{
		/* Reset the HWR numbers */
		psHWRInfoBuf->aui32HwrDmLockedUpCount[i] = 0;
		psHWRInfoBuf->aui32HwrDmFalseDetectCount[i] = 0;
		psHWRInfoBuf->aui32HwrDmRecoveredCount[i] = 0;
		psHWRInfoBuf->aui32HwrDmOverranCount[i] = 0;
	}

	for (i = 0 ; i < RGXFWIF_HWINFO_MAX ; i++)
	{
		psHWRInfoBuf->sHWRInfo[i].ui32HWRNumber = 0;
	}

	psHWRInfoBuf->ui32WriteIndex = 0;
	psHWRInfoBuf->ui32DDReqCount = 0;

	return PVRSRV_OK;
}

PVRSRV_ERROR RGXGetPhyAddr(PMR *psPMR,
		IMG_DEV_PHYADDR *psPhyAddr,
		IMG_UINT32 ui32LogicalOffset,
		IMG_UINT32 ui32Log2PageSize,
		IMG_UINT32 ui32NumOfPages,
		IMG_BOOL *bValid)
{

	PVRSRV_ERROR eError;

	eError = PMRLockSysPhysAddresses(psPMR);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: PMRLockSysPhysAddresses failed (%u)",
		         __func__,
		         eError));
		return eError;
	}

	eError = PMR_DevPhysAddr(psPMR,
			ui32Log2PageSize,
			ui32NumOfPages,
			ui32LogicalOffset,
			psPhyAddr,
			bValid);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: PMR_DevPhysAddr failed (%u)",
		         __func__,
		         eError));
		return eError;
	}


	eError = PMRUnlockSysPhysAddresses(psPMR);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: PMRUnLockSysPhysAddresses failed (%u)",
		         __func__,
		         eError));
		return eError;
	}

	return eError;
}

#if defined(PDUMP)
PVRSRV_ERROR RGXPdumpDrainKCCB(PVRSRV_RGXDEV_INFO *psDevInfo, IMG_UINT32 ui32WriteOffset)
{
	RGXFWIF_CCB_CTL	*psKCCBCtl = psDevInfo->psKernelCCBCtl;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (psDevInfo->bDumpedKCCBCtlAlready)
	{
		/* exiting capture range or pdump block */
		psDevInfo->bDumpedKCCBCtlAlready = IMG_FALSE;

		/* make sure previous cmd is drained in pdump in case we will 'jump' over some future cmds */
		PDUMPCOMMENTWITHFLAGS(PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWER,
				"kCCB(%p): Draining rgxfw_roff (0x%x) == woff (0x%x)",
				psKCCBCtl,
				ui32WriteOffset,
				ui32WriteOffset);
		eError = DevmemPDumpDevmemPol32(psDevInfo->psKernelCCBCtlMemDesc,
				offsetof(RGXFWIF_CCB_CTL, ui32ReadOffset),
				ui32WriteOffset,
				0xffffffff,
				PDUMP_POLL_OPERATOR_EQUAL,
				PDUMP_FLAGS_CONTINUOUS | PDUMP_FLAGS_POWER);

		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: problem pdumping POL for kCCBCtl (%d)", __func__, eError));
		}
	}

	return eError;

}
#endif

/*!
*******************************************************************************

 @Function	RGXClientConnectCompatCheck_ClientAgainstFW

 @Description

 Check compatibility of client and firmware (build options)
 at the connection time.

 @Input psDeviceNode - device node
 @Input ui32ClientBuildOptions - build options for the client

 @Return   PVRSRV_ERROR - depending on mismatch found

******************************************************************************/
PVRSRV_ERROR IMG_CALLCONV RGXClientConnectCompatCheck_ClientAgainstFW(PVRSRV_DEVICE_NODE *psDeviceNode, IMG_UINT32 ui32ClientBuildOptions)
{
#if !defined(NO_HARDWARE) || defined(PDUMP)
#if !defined(NO_HARDWARE)
	IMG_UINT32		ui32BuildOptionsMismatch;
	IMG_UINT32		ui32BuildOptionsFW;
#endif
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
#endif
	PVRSRV_VZ_RET_IF_MODE(GUEST, PVRSRV_OK);

#if !defined(NO_HARDWARE)
	if (psDevInfo == NULL || psDevInfo->psRGXFWIfOsInitMemDesc == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
		         "%s: Cannot acquire kernel fw compatibility check info, RGXFWIF_OSINIT structure not allocated.",
		         __func__));
		return PVRSRV_ERROR_NOT_INITIALISED;
	}

	LOOP_UNTIL_TIMEOUT(MAX_HW_TIME_US)
	{
		if (*((volatile IMG_BOOL *) &psDevInfo->psRGXFWIfOsInit->sRGXCompChecks.bUpdated))
		{
			/* No need to wait if the FW has already updated the values */
			break;
		}
		OSWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
	} END_LOOP_UNTIL_TIMEOUT();
#endif

#if defined(PDUMP)
	{
		PVRSRV_ERROR eError;

		PDUMPCOMMENT("Compatibility check: client and FW build options");
		eError = DevmemPDumpDevmemPol32(psDevInfo->psRGXFWIfOsInitMemDesc,
				offsetof(RGXFWIF_OSINIT, sRGXCompChecks) +
				offsetof(RGXFWIF_COMPCHECKS, ui32BuildOptions),
				ui32ClientBuildOptions,
				0xffffffff,
				PDUMP_POLL_OPERATOR_EQUAL,
				PDUMP_FLAGS_CONTINUOUS);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,
					"%s: problem pdumping POL for psRGXFWIfOsInitMemDesc (%d)",
					__func__,
					eError));
			return eError;
		}
	}
#endif

#if !defined(NO_HARDWARE)
	ui32BuildOptionsFW = psDevInfo->psRGXFWIfOsInit->sRGXCompChecks.ui32BuildOptions;
	ui32BuildOptionsMismatch = ui32ClientBuildOptions ^ ui32BuildOptionsFW;

	if (ui32BuildOptionsMismatch != 0)
	{
		if ((ui32ClientBuildOptions & ui32BuildOptionsMismatch) != 0)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in Firmware and client build options; "
					"extra options present in client: (0x%x). Please check rgx_options.h",
					ui32ClientBuildOptions & ui32BuildOptionsMismatch ));
		}

		if ((ui32BuildOptionsFW & ui32BuildOptionsMismatch) != 0)
		{
			PVR_LOG(("(FAIL) RGXDevInitCompatCheck: Mismatch in Firmware and client build options; "
					"extra options present in Firmware: (0x%x). Please check rgx_options.h",
					ui32BuildOptionsFW & ui32BuildOptionsMismatch ));
		}

		return PVRSRV_ERROR_BUILD_OPTIONS_MISMATCH;
	}
	else
	{
		PVR_DPF((PVR_DBG_MESSAGE, "%s: Firmware and client build options match. [ OK ]", __func__));
	}
#endif

	return PVRSRV_OK;
}

/*!
*******************************************************************************

 @Function	RGXFwRawHeapAllocMap

 @Description Register firmware heap for the specified guest OSID

 @Input psDeviceNode - device node
 @Input ui32OSID     - Guest OSID
 @Input sDevPAddr    - Heap address
 @Input ui64DevPSize - Heap size

 @Return   PVRSRV_ERROR - PVRSRV_OK if heap setup was successful.

******************************************************************************/
PVRSRV_ERROR RGXFwRawHeapAllocMap(PVRSRV_DEVICE_NODE *psDeviceNode,
								  IMG_UINT32 ui32OSID,
								  IMG_DEV_PHYADDR sDevPAddr,
								  IMG_UINT64 ui64DevPSize)
{
	PVRSRV_ERROR eError;
	IMG_CHAR szRegionRAName[PVRSRV_MAX_RA_NAME_LENGTH];
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;
	IMG_UINT32 ui32RawFwHeapAllocFlags = (RGX_FWSHAREDMEM_ALLOCFLAGS |
										  PVRSRV_MEMALLOCFLAG_FW_ALLOC_RAW |
										  PVRSRV_MEMALLOCFLAG_FW_RAW_ALLOC_OSID(ui32OSID));

	PVRSRV_VZ_RET_IF_NOT_MODE(HOST, PVRSRV_OK);

	OSSNPrintf(szRegionRAName, sizeof(szRegionRAName), RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT, ui32OSID);

	if (!ui64DevPSize ||
		!sDevPAddr.uiAddr ||
		ui32OSID >= RGX_NUM_OS_SUPPORTED ||
		ui64DevPSize != RGX_FIRMWARE_RAW_HEAP_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR, "Invalid parameters for %s", szRegionRAName));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eError = PVRSRVCreateRegionRA(psDeviceNode->psDevConfig,
								  &psDeviceNode->psKernelFwRawMemArena[ui32OSID],
								  psDeviceNode->szKernelFwRawRAName[ui32OSID],
								  0,
								  sDevPAddr.uiAddr,
								  RGX_FIRMWARE_RAW_HEAP_SIZE,
								  0,
								  szRegionRAName);
	PVR_LOG_RETURN_IF_ERROR(eError, "PVRSRVCreateRegionRA");

	PDUMPCOMMENT("Allocate and map raw firmware heap for OSID: [%d]", ui32OSID);

#if (RGX_NUM_OS_SUPPORTED > 1)
	/* don't clear the heap of other guests on allocation */
	ui32RawFwHeapAllocFlags &= (ui32OSID > RGXFW_HOST_OS) ? (~PVRSRV_MEMALLOCFLAG_ZERO_ON_ALLOC) : (~0);
#endif

	/* if the firmware is already powered up, consider the firmware heaps are pre-mapped. */
	if (psDeviceNode->bAutoVzFwIsUp)
	{
		ui32RawFwHeapAllocFlags &= RGX_AUTOVZ_KEEP_FW_DATA_MASK(psDeviceNode->bAutoVzFwIsUp);
		DevmemHeapSetPremapStatus(psDevInfo->psGuestFirmwareRawHeap[ui32OSID], IMG_TRUE);
	}

	eError = DevmemFwAllocate(psDevInfo,
							  RGX_FIRMWARE_RAW_HEAP_SIZE,
							  ui32RawFwHeapAllocFlags,
							  psDevInfo->psGuestFirmwareRawHeap[ui32OSID]->pszName,
							  &psDevInfo->psGuestFirmwareRawMemDesc[ui32OSID]);
	PVR_LOG_RETURN_IF_ERROR(eError, "DevmemFwAllocate");

	/* Mark this devmem heap as premapped so allocations will not require device mapping. */
	DevmemHeapSetPremapStatus(psDevInfo->psGuestFirmwareRawHeap[ui32OSID], IMG_TRUE);

	if (ui32OSID == RGXFW_HOST_OS)
	{
		/* if the Host's raw fw heap is premapped, mark its main & config sub-heaps accordingly
		 * No memory allocated from these sub-heaps will be individually mapped into the device's
		 * address space so they can remain marked permanently as premapped. */
		DevmemHeapSetPremapStatus(psDevInfo->psFirmwareMainHeap, IMG_TRUE);
		DevmemHeapSetPremapStatus(psDevInfo->psFirmwareConfigHeap, IMG_TRUE);
	}

	return eError;
}

/*!
*******************************************************************************

 @Function	RGXFwRawHeapUnmapFree

 @Description Unregister firmware heap for the specified guest OSID

 @Input psDeviceNode - device node
 @Input ui32OSID     - Guest OSID

******************************************************************************/
void RGXFwRawHeapUnmapFree(PVRSRV_DEVICE_NODE *psDeviceNode,
						   IMG_UINT32 ui32OSID)
{
	PVRSRV_RGXDEV_INFO *psDevInfo = psDeviceNode->pvDevice;

	/* remove the premap status, so the heap can be unmapped and freed */
	if (psDevInfo->psGuestFirmwareRawHeap[ui32OSID])
	{
		DevmemHeapSetPremapStatus(psDevInfo->psGuestFirmwareRawHeap[ui32OSID], IMG_FALSE);
	}

	if (psDevInfo->psGuestFirmwareRawMemDesc[ui32OSID])
	{
		DevmemFwUnmapAndFree(psDevInfo, psDevInfo->psGuestFirmwareRawMemDesc[ui32OSID]);
		psDevInfo->psGuestFirmwareRawMemDesc[ui32OSID] = NULL;
	}

	if (psDeviceNode->psKernelFwRawMemArena[ui32OSID])
	{
		RA_Delete(psDeviceNode->psKernelFwRawMemArena[ui32OSID]);
	}

	psDeviceNode->psKernelFwRawMemArena[ui32OSID] = NULL;
}

/******************************************************************************
 End of file (rgxfwutils.c)
******************************************************************************/
