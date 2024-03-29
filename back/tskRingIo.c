/** ============================================================================
 *  @file   tskRingIo.c
 *
 *  @path   $(DSPLINK)/dsp/src/samples/ring_io/
 *
 *  @desc   This is a TSK based application that demonstrates the usage of the
 *          RingIO component.
 *          There are two RingIO objects in the system. One is created by the
 *          GPP and one by the DSP. The first one is opened by the DSP in reader
 *          mode, and the second in writer mode.
 *          The DSP reads data from the RINGIO1 and writes it into RINGIO2 after
 *          processing the data with a scaling factor.
 *          The scaling factor is received on the DSP-side as a variable
 *          attribute attached to the RINGIO1 data buffer, and is used for
 *          the data buffer processing.
 *          The scaling factor(varibale attribute)  is sent to GPP on RINGIO2
 *          and then the processed data buffer till the completion of the data
 *          transfer.
 *
 *  @ver    1.65.00.02
 *  ============================================================================
 *  Copyright (C) 2002-2009, Texas Instruments Incorporated -
 *  http://www.ti.com/
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  ============================================================================
 */

/* ---------------------------- DSP/BIOS Headers ---------------------------- */
#include <std.h>
#include <log.h>
#include <swi.h>
#include <sys.h>
#include <sio.h>
#include <tsk.h>
#include <pool.h>
#include <gbl.h>

/*  --------------------------- DSP/BIOS LINK Headers ----------------------- */
#include <failure.h>
#include <dsplink.h>
#include <platform.h>
#include <notify.h>

/*  --------------------------- RingIO Headers ----------------------------- */
#include <ringio.h>
#if defined (DSP_BOOTMODE_NOBOOT)
#include <sma_pool.h>
#endif
/*  --------------------------- Sample Headers ---------------------------- */
#include <ring_io_config.h>
#include <tskRingIo.h>

/** ============================================================================
 *  @const  FILEID
 *
 *  @desc   FILEID is used by SET_FAILURE_REASON macro.
 *  ============================================================================
 */
#define FILEID  FID_APP_C

/*  ============================================================================
 *  @name   OP_FACTOR
 *
 *  @desc   The value used by dsp to perform mulification and division on
 *          received data.
 *  ============================================================================
 */
#define OP_FACTOR            2u

/*  ============================================================================
 *  @name   OP_MULTIPLY
 *
 *  @desc   Macro to indicates multiplication  needs to be performed on the
 *          received data with OP_FACTOR by DSP.
 *
 *  ============================================================================
 */
#define OP_MULTIPLY         1u

/*  ============================================================================
 *  @name   OP_DIVIDE
 *
 *  @desc   Macro to indicates division  needs to be performed on the
 *          received data with OP_FACTOR by DSP.
 *  ============================================================================
 */
#define OP_DIVIDE           2u

/** ============================================================================
 *  @const  RINGIO_WRITE_ACQ_SIZE
 *
 *  @desc   Acquire size of the   RingIO  created by DSP as the writer.
 *  ============================================================================
 */
#define RINGIO_WRITE_ACQ_SIZE     640u

/** ============================================================================
 *  @const  FILEID
 *
 *  @desc   Acquire size of the   RingIO  opened by DSP  in read mode.
 *  ============================================================================
 */
#define RINGIO_READ_ACQ_SIZE     512u

/*  ============================================================================
 *  @const   RINGIO_DATA_START
 *
 *  @desc    Fixed attribute type indicates  start of the data in the RingIO
 *  ============================================================================
 */
#define RINGIO_DATA_START       1u

/*  ============================================================================
 *  @const   NOTIFY_DATA_START
 *
 *  @desc    Message id to send data start notification  to  DSP.
 *  ============================================================================
 */
#define NOTIFY_DATA_START       2u

/*  ============================================================================
 *  @const   RINGIO_DATA_END
 *
 *  @desc    Fixed attribute type indicates  start of the data in the RingIO
 *  ============================================================================
 */
#define RINGIO_DATA_END         3u

/*  ============================================================================
 *  @const   NOTIFY_DATA_END
 *
 *  @desc    Message id to send data start notification  to  DSP.
 *  ============================================================================
 */
#define NOTIFY_DATA_END         4u



/*  ============================================================================
 *  @const   RINGIO_DSP_END
 *
 *  @desc     Fixed attribute type indicates  end of the dsp 
 *  ============================================================================
 */
#define RINGIO_DSP_END         5u


/*  ============================================================================
 *  @const   NOTIFY_DSP_END
 *
 *  @desc     Notification message  to  DSP.Indicates DSP end
 *  ============================================================================
 */
#define NOTIFY_DSP_END         6u





/** ============================================================================
 *  @name   RING_IO_xferBufSize
 *
 *  @desc   Size of the buffer for TSK based RING_IO transfers.
 *  ============================================================================
 */
extern Uint16 RING_IO_xferBufSize;

/** ============================================================================
 *  @name   RING_IO_numTransfers
 *
 *  @desc   Iterations of data transfer to be done by the application.
 *  ============================================================================
 */
extern Uint16 RING_IO_numTransfers;

/** ============================================================================
 *  @name   RING_IO_dataBufSize
 *
 *  @desc   Size of the data buffer to be allocated for the RingIO.
 *  ============================================================================
 */
extern Uint32 RING_IO_dataBufSize1;
extern Uint32 RING_IO_dataBufSize2;

extern Uint32 RING_IO_dataBufSize3;
extern Uint32 RING_IO_dataBufSize4;

/** ============================================================================
 *  @name   RING_IO_AttrBufSize
 *
 *  @desc   Size of the attribute buffer to be allocated for the RingIO.
 *  ============================================================================
 */
extern Uint16 RING_IO_attrBufSize;

/** ============================================================================
 *  @name   RING_IO_FootBufSize
 *
 *  @desc   Size of the foot buffer buffer to be allocated for the RingIO.
 *  ============================================================================
 */
extern Uint16 RING_IO_footBufSize;

/** ============================================================================
 *  @name   MAX_VATTR_NUM
 *
 *  @desc   length of the buffer to hold variable attribute.
 *  ============================================================================
 */
#define MAX_VATTR_NUM       1u

/** ============================================================================
 *  @name   attrs
 *
 *  @desc   buffer  to receive Variable attribute.
 *  ============================================================================
 */
Uint32 attrs[MAX_VATTR_NUM];

/** ----------------------------------------------------------------------------
 *  @func   TSKRING_IO_writer_notify
 *
 *  @desc   This function implements the notification callback for the RingIO
 *          opened by the DSP in writer or reader mode.
 *
 *  @arg    handle
 *              Handle to the RingIO.
 *  @arg    param
 *              Parameter used while registering the notification.
 *  @arg    msg
 *               Message passed along with notification.
 *
 *  @ret    None
 *
 *  @enter  None
 *
 *  @leave  None
 *
 *  @see    None
 *  ----------------------------------------------------------------------------
 */
static Void
TSKRING_IO_writer_notify(RingIO_Handle handle, RingIO_NotifyParam param,
		RingIO_NotifyMsg msg);

/** ----------------------------------------------------------------------------
 *  @func   TSKRING_IO_reader_notify
 *
 *  @desc   This function implements the notification callback for the RingIO
 *          opened by the DSP in writer or reader mode.
 *
 *  @arg    handle
 *              Handle to the RingIO.
 *  @arg    param
 *              Parameter used while registering the notification.
 *  @arg    msg
 *               Message passed along with notification.
 *
 *  @ret    None
 *
 *  @enter  None
 *
 *  @leave  None
 *
 *  @see    None
 *  ----------------------------------------------------------------------------
 */
static Void
TSKRING_IO_reader_notify(RingIO_Handle handle, RingIO_NotifyParam param,
		RingIO_NotifyMsg msg);

/** ----------------------------------------------------------------------------
 *  @func   RING_IO_apply
 *
 *  @desc   This function multiples or divides the data in the buffer by a value
 *          specified in the factor variable.(inplace processing)
 *  @field buffer
 *       data buffer that needs to be processed.
 *  @field factor
 *        scale value
 *  @field opCode
 *         specifies the operation to be performed
 *       (OP_MULTIPLY/OP_DIVIDE)
 *  @field size
 *        size of the buffer that needs to be processed.
 *
 *  @modif  buffer
 *  ----------------------------------------------------------------------------
 */
static Void
		RING_IO_apply(RingIO_BufPtr * buffer, Uint32 factor, Uint32 opCode,
				Uint32 size);

#if defined (DSP_BOOTMODE_NOBOOT)
/** ============================================================================
 *  @name   smaPoolObj
 *
 *  @desc   Global object for pool parameters for the dynamic POOL_open call
 *  ============================================================================
 */
SMAPOOL_Params smaPoolObj;
#endif
/** ============================================================================
 *  @func   TSKRING_IO_create
 *
 *  @desc   Create phase function for the RING_IO application. Initializes the
 *          TSKRING_IO_TransferInfo structure with the information that will be
 *          used by the other phases of the application.
 *
 *  @modif  None.
 *  ============================================================================
 */
Int TSKRING_IO_create1(TSKRING_IO_TransferInfo ** infoPtr) {
	Int status = SYS_OK;
	TSKRING_IO_TransferInfo * info = NULL;
	RingIO_Attrs ringIoAttrs;
	Uint32 flags;
	RingIO_Handle writerHandle;
	RingIO_Handle readerHandle;

	/*
	 *  Create the RingIO to be used with DSP as the writer.
	 */
	if (status == SYS_OK) {
		ringIoAttrs.transportType = RINGIO_TRANSPORT_GPP_DSP;
		ringIoAttrs.ctrlPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.dataPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.attrPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.lockPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.dataBufSize = RING_IO_dataBufSize1;
		ringIoAttrs.footBufSize = RING_IO_footBufSize;
		ringIoAttrs.attrBufSize = RING_IO_attrBufSize;

#if defined (DSPLINK_LEGACY_SUPPORT)
		status = RingIO_create (RING_IO_WRITER_NAME1, &ringIoAttrs);
#else
		status = RingIO_create(GBL_getProcId(), RING_IO_WRITER_NAME1,
				&ringIoAttrs);
#endif /* if defined (DSPLINK_LEGACY_SUPPORT) */
		if (status != SYS_OK) {
			SET_FAILURE_REASON(status);
		}
	}

	/*
	 *  Open the RingIO to be used with DSP as the writer.
	 */
	if (status == SYS_OK) {
		/* Value of the flags indicates:
		 *     Cache coherence required for: Control structure
		 *                                   Data buffer
		 *                                   Attribute buffer
		 *     Exact size requirement.
		 */
		flags = RINGIO_DATABUF_CACHEUSE | RINGIO_ATTRBUF_CACHEUSE
				| RINGIO_CONTROL_CACHEUSE | RINGIO_NEED_EXACT_SIZE;
		do {
			writerHandle = RingIO_open(RING_IO_WRITER_NAME1, RINGIO_MODE_WRITE,
					flags);
			if (writerHandle == NULL) {
				status = RINGIO_EFAILURE;
				SET_FAILURE_REASON(status);
			}
		} while (writerHandle == NULL);
	}

	/*
	 *  Open the RingIO to be used with DSP as the reader.
	 */
	if (status == SYS_OK) {
		/* Wait till the RingIO is created by the GPP. */
		do {
			/* Value of the flags indicates:
			 *     Cache coherence required for: Control structure
			 *                                   Data buffer
			 *                                   Attribute buffer
			 *     Exact size requirement false.
			 */
			flags = (RINGIO_DATABUF_CACHEUSE | RINGIO_ATTRBUF_CACHEUSE
					| RINGIO_CONTROL_CACHEUSE);

			readerHandle = RingIO_open(RING_IO_READER_NAME1, RINGIO_MODE_READ,
					flags);
		} while (readerHandle == NULL);
	}

	/* Allocate TSKRING_IO_TransferInfo structure that will be initialized
	 * and passed to other phases of the application */
	if (status == SYS_OK) {
		*infoPtr = MEM_calloc(DSPLINK_SEGID, sizeof(TSKRING_IO_TransferInfo),
				DSPLINK_BUF_ALIGN);
		if (*infoPtr == NULL) {
			status = SYS_EALLOC;
			SET_FAILURE_REASON(status);
		} else {
			info = *infoPtr;
		}
	}

	/* Fill up the transfer info structure */
	if (status == SYS_OK) {
		info->writerHandle = writerHandle;
		info->readerHandle = readerHandle;
		SEM_new(&(info->writerSemObj), 0);
		SEM_new(&(info->readerSemObj), 0);
		info->readerRecvSize = 0;
		info->writerRecvSize = 0;
		/* Set the flags to false */
		info->freadStart = FALSE;
		info->freadEnd = FALSE;
		info->exitflag = FALSE;
	}

	return (status);
}

Int TSKRING_IO_create2(TSKRING_IO_TransferInfo ** infoPtr) {
	Int status = SYS_OK;
	TSKRING_IO_TransferInfo * info = NULL;
	RingIO_Attrs ringIoAttrs;
	Uint32 flags;
	RingIO_Handle writerHandle;
	RingIO_Handle readerHandle;

	/*
	 *  Create the RingIO to be used with DSP as the writer.
	 */
	if (status == SYS_OK) {
		ringIoAttrs.transportType = RINGIO_TRANSPORT_GPP_DSP;
		ringIoAttrs.ctrlPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.dataPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.attrPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.lockPoolId = SAMPLE_POOL_ID;
		ringIoAttrs.dataBufSize = RING_IO_dataBufSize2;
		ringIoAttrs.footBufSize = RING_IO_footBufSize;
		ringIoAttrs.attrBufSize = RING_IO_attrBufSize;

#if defined (DSPLINK_LEGACY_SUPPORT)
		status = RingIO_create (RING_IO_WRITER_NAME2, &ringIoAttrs);
#else
		status = RingIO_create(GBL_getProcId(), RING_IO_WRITER_NAME2,
				&ringIoAttrs);
#endif /* if defined (DSPLINK_LEGACY_SUPPORT) */
		if (status != SYS_OK) {
			SET_FAILURE_REASON(status);
		}
	}

	/*
	 *  Open the RingIO to be used with DSP as the writer.
	 */
	if (status == SYS_OK) {
		/* Value of the flags indicates:
		 *     Cache coherence required for: Control structure
		 *                                   Data buffer
		 *                                   Attribute buffer
		 *     Exact size requirement.
		 */
		flags = RINGIO_DATABUF_CACHEUSE | RINGIO_ATTRBUF_CACHEUSE
				| RINGIO_CONTROL_CACHEUSE | RINGIO_NEED_EXACT_SIZE;
		do {
			writerHandle = RingIO_open(RING_IO_WRITER_NAME2, RINGIO_MODE_WRITE,
					flags);
			if (writerHandle == NULL) {
				status = RINGIO_EFAILURE;
				SET_FAILURE_REASON(status);
			}
		} while (writerHandle == NULL);
	}

	/*
	 *  Open the RingIO to be used with DSP as the reader.
	 */
	if (status == SYS_OK) {
		/* Wait till the RingIO is created by the GPP. */
		do {
			/* Value of the flags indicates:
			 *     Cache coherence required for: Control structure
			 *                                   Data buffer
			 *                                   Attribute buffer
			 *     Exact size requirement false.
			 */
			flags = (RINGIO_DATABUF_CACHEUSE | RINGIO_ATTRBUF_CACHEUSE
					| RINGIO_CONTROL_CACHEUSE);

			readerHandle = RingIO_open(RING_IO_READER_NAME2, RINGIO_MODE_READ,
					flags);
		} while (readerHandle == NULL);
	}

	/* Allocate TSKRING_IO_TransferInfo structure that will be initialized
	 * and passed to other phases of the application */
	if (status == SYS_OK) {
		*infoPtr = MEM_calloc(DSPLINK_SEGID, sizeof(TSKRING_IO_TransferInfo),
				DSPLINK_BUF_ALIGN);
		if (*infoPtr == NULL) {
			status = SYS_EALLOC;
			SET_FAILURE_REASON(status);
		} else {
			info = *infoPtr;
		}
	}

	/* Fill up the transfer info structure */
	if (status == SYS_OK) {
		info->writerHandle = writerHandle;
		info->readerHandle = readerHandle;
		SEM_new(&(info->writerSemObj), 0);
		SEM_new(&(info->readerSemObj), 0);
		info->readerRecvSize = 0;
		info->writerRecvSize = 0;
		/* Set the flags to false */
		info->freadStart = FALSE;
		info->freadEnd = FALSE;
		info->exitflag = FALSE;
	}

	return (status);
}

/** ============================================================================
 *  @func   TSKRING_IO_execute
 *
 *  @desc   Execute phase function for the RingIO sample application. Application
 *          receives the data from the input RingIO and  processes data
 *          as specified by the attributes from GPP.Sends the same data
 *          back on output RingIO .
 *
 *  @modif  None.
 *  ============================================================================
 */
Int TSKRING_IO_execute1(TSKRING_IO_TransferInfo * info) {
	Int status = SYS_OK;
	Int wrRingStatus = RINGIO_SUCCESS;
	Int rdRingStatus = RINGIO_SUCCESS;
	Bool semStatus = TRUE;
	Bool exitFlag = FALSE;
	Uint32 i;
	Uint32 j;
	Uint16 type;
	Uint32 param;
	Uint32 writerWaterMark;
	Uint32 writeAcqSize;
	Uint32 readerAcqSize;
	Uint32 size;
	Uint32 totalRcvbytes = 0;
	Char * Buffer;
	Uint32 bytesTransfered = 0;
	Uint8 * ptr8;

	RING_IO_dataBufSize3 = 1024;   // the DSP reader size 

	/*
	 *  Set the notification for Writer.
	 */
	//writerWaterMark
	//		= (RING_IO_dataBufSize1 < RINGIO_WRITE_ACQ_SIZE) ? RING_IO_dataBufSize1
	//				: RINGIO_WRITE_ACQ_SIZE;
	writerWaterMark = RING_IO_dataBufSize1;

	do {
		status = RingIO_setNotifier(info->writerHandle,
				RINGIO_NOTIFICATION_ONCE, writerWaterMark,
				&TSKRING_IO_writer_notify, (RingIO_NotifyParam) info);
	} while (status != SYS_OK);

	/*
	 *  Set the notification for Reader.
	 */
	// readerAcqSize = (RING_IO_dataBufSize3 < RINGIO_READ_ACQ_SIZE)?
	//                 RING_IO_dataBufSize3 : RINGIO_READ_ACQ_SIZE ;


	readerAcqSize = RING_IO_dataBufSize3;
	Buffer = MEM_calloc(DSPLINK_SEGID, readerAcqSize, DSPLINK_BUF_ALIGN);
	if(NULL == Buffer) {
		status = RINGIO_EFAILURE;
		SET_FAILURE_REASON(status);
	}

	do {
		status = RingIO_setNotifier(info->readerHandle,
				RINGIO_NOTIFICATION_ONCE, 0 /* readerWaterMark */,
				&TSKRING_IO_reader_notify, (RingIO_NotifyParam) info);
	} while (status != SYS_OK);

	writeAcqSize = writerWaterMark;

	//while (1) {
	while (!info->exitflag) {

		/* Wait for the start notification from gpp */
		semStatus = SEM_pend(&(info->readerSemObj), SYS_FOREVER);
		if (semStatus == FALSE) {
			status = RINGIO_EFAILURE;
			SET_FAILURE_REASON(status);
		} else {
			status = SYS_OK;
		}

		if ((info->freadStart == TRUE)&&(!info->exitflag)) {

			info->freadStart = FALSE;

			/* Got  data transfer start notification from DSP*/
			do {

				/* Get the start attribute from dsp */
				status = RingIO_getAttribute(info->readerHandle, &type, &param);
				if ((status == RINGIO_SUCCESS) || (status
						== RINGIO_SPENDINGATTRIBUTE)) {

					if (type == (Uint16) RINGIO_DATA_START) {
						/*RING_IO_0Print ("GPP<--DSP1:Received Data Transfer"
						 "Start Attribute\n") ;*/
						break;
					}

				}

			} while ((status != RINGIO_SUCCESS) && (status
					!= RINGIO_SPENDINGATTRIBUTE) &&(!info->exitflag));
		}

		info->readerRecvSize = readerAcqSize; //the size of RingIO_acquire
		info->scaleSize = readerAcqSize; //the size of the rest of the RingIO_acquire
		while ((exitFlag == FALSE) && (!info->exitflag)) {
			rdRingStatus = RingIO_acquire(info->readerHandle,
					(RingIO_BufPtr *) &(info->readerBuf),
					&(info->readerRecvSize));

			if ((rdRingStatus == RINGIO_EFAILURE) || (rdRingStatus
					== RINGIO_EBUFEMPTY)) {
				/* Wait for the read buffer to be available */
				semStatus = SEM_pend(&(info->readerSemObj), SYS_FOREVER);
				if (semStatus == FALSE) {
					status = RINGIO_EFAILURE;
					SET_FAILURE_REASON(status);
				} else {
					status = SYS_OK;
				}
			} else if ((rdRingStatus == RINGIO_SUCCESS)
					|| ((info->readerRecvSize > 0) && ((rdRingStatus
							== RINGIO_ENOTCONTIGUOUSDATA) || (rdRingStatus
							== RINGIO_EBUFWRAP) || (rdRingStatus
							== RINGIO_SPENDINGATTRIBUTE)))) {
				/* Acquired Read buffer.Copy input received data
				 *to output buffer and  process the buffer as
				 *specified in the received  variable attribute
				 */
				info->scaleSize -= info->readerRecvSize;

				if (Buffer && info->readerBuf) {
					if ((totalRcvbytes + info->readerRecvSize) <= readerAcqSize)
					//if ((totalRcvbytes + info->readerRecvSize) < readerAcqSize)
						memcpy((Buffer + totalRcvbytes), info->readerBuf,
								info->readerRecvSize);
				}
				totalRcvbytes += info->readerRecvSize;

				/* Release the input buffer(reader buffer) */
				rdRingStatus = RingIO_release(info->readerHandle,
						info->readerRecvSize);
				if (RINGIO_SUCCESS != rdRingStatus) {
					SET_FAILURE_REASON(rdRingStatus);

				}
				/* Set the acqSize for the next acquire */
				if (info->scaleSize == 0) {
					/* Reset  the rcvSize to  size of the full buffer  */
					info->scaleSize = RING_IO_dataBufSize3;
					info->readerRecvSize = RING_IO_dataBufSize3;
				} else {
					/*Acquire the partial buffer  in next acquire */
					info->readerRecvSize = info->scaleSize;
				}

			}

			else if (rdRingStatus == RINGIO_SPENDINGATTRIBUTE) {

				rdRingStatus = RingIO_getAttribute(info->readerHandle, &type,
						&param);
				if ((RINGIO_SUCCESS == rdRingStatus)
						|| (RINGIO_SPENDINGATTRIBUTE == rdRingStatus)) {

					/* Got the fixed attribute */
					if (type == RINGIO_DATA_END) {
						/* End of data transfer from DSP */

						exitFlag = TRUE;
					}
				} else if (rdRingStatus == RINGIO_EVARIABLEATTRIBUTE) {
					j = sizeof(attrs);
					rdRingStatus = RingIO_getvAttribute(info->readerHandle,
							&type, &i, attrs, &j);
					if ((RINGIO_SUCCESS == rdRingStatus)
							|| (RINGIO_SPENDINGATTRIBUTE == rdRingStatus)) {

						/* got the variable attribute */
						readerAcqSize = attrs[0];

						//info->scaleSize = attrs[0];
						info->scaleSize =readerAcqSize;
						info->readerRecvSize = info->scaleSize;
					} else if (RINGIO_EVARIABLEATTRIBUTE == rdRingStatus) {

						/* This case should not arise.
						 * as we have provided the sufficient buffer
						 * to receive variable Attribute
						 */
						SET_FAILURE_REASON(rdRingStatus);
					} else {
						/* For RINGIO_EPENDINGDATA, RINGIO_EFAILURE
						 * nothing to be done. go and  read data again
						 */
					}
				} else {
					/* For other return status
					 * (RINGIO_EPENDINGDATA,RINGIO_EFAILURE)
					 * no thing o be done. go and read the data again
					 */

				}
			} else {
				/* For Any other  wrRingStatus,Consider it as failure */
				status = RINGIO_EFAILURE;
				SET_FAILURE_REASON(status);
			}
			/* Reset the acquired size if it is changed to zero by the
			 * failed acquire call
			 */
			if (info->readerRecvSize == 0) {
				info->readerRecvSize = info->scaleSize;
			}

		}

		totalRcvbytes = 0;
		info->readerRecvSize = readerAcqSize; //the size of RingIO_acquire
		info->scaleSize = readerAcqSize; //the size of the rest of the RingIO_acquire
		info->freadEnd = FALSE;
		exitFlag = FALSE;

		///////////////////////////////////////////////////////////////////////////////
		//End  the read  task
		///////////////////////////////////////////////////////////////////////////////


		///////////////////////////////////////////////////////////////////////////////
		//To do the algorithms with the Buffer (RING_IO_dataBufSize3)
		///////////////////////////////////////////////////////////////////////////////
		//if(&& (!info->exitflag))


		///////////////////////////////////////////////////////////////////////////////
		//END To do the algorithms with the Buffer (RING_IO_dataBufSize3)
		///////////////////////////////////////////////////////////////////////////////


		///////////////////////////////////////////////////////////////////////////////
		//start  the write  task
		///////////////////////////////////////////////////////////////////////////////

		if ((RINGIO_SUCCESS == wrRingStatus) && (!info->exitflag)) {
			type = (Uint16) RINGIO_DATA_START;
			/* Set the attribute start attribute to output */
			wrRingStatus = RingIO_setAttribute(info->writerHandle, 0, type, 0);
			if (wrRingStatus != RINGIO_SUCCESS) {
				SET_FAILURE_REASON(wrRingStatus);
			} else {
				/* Sending the Hard Notification to gpp reader */
				do {
					wrRingStatus = RingIO_sendNotify(info->writerHandle,
							(RingIO_NotifyMsg) NOTIFY_DATA_START);
					if (wrRingStatus != RINGIO_SUCCESS) {
						SET_FAILURE_REASON(wrRingStatus);
					}
				} while ((wrRingStatus != RINGIO_SUCCESS) && (!info->exitflag));
			}
		}

		if ((RINGIO_SUCCESS == wrRingStatus) && (!info->exitflag)) {
			//	while ((RING_IO_dataBufSize1 == 0) || (bytesTransfered
			while ((bytesTransfered < RING_IO_dataBufSize1) && (!info->exitflag) ) {
				/* Update the attrs to send variable attribute to DSP*/
				attrs[0] = RING_IO_dataBufSize1;
				wrRingStatus = RingIO_setvAttribute(info->writerHandle, 0, 0,
						0, attrs, sizeof(attrs));

				if (wrRingStatus == RINGIO_EWRONGSTATE) {
				} else {
					/* Acquire writer bufs and initialize and release them. */
					info->writerRecvSize = RING_IO_dataBufSize1;
					wrRingStatus = RingIO_acquire(info->writerHandle,
							(RingIO_BufPtr *) &(info->writerBuf),
							&(info->writerRecvSize));

					if ((wrRingStatus == RINGIO_EFAILURE) || (wrRingStatus
							== RINGIO_EBUFFULL)) {
						/* Wait for Writer notification */
						semStatus
								= SEM_pend(&(info->writerSemObj), SYS_FOREVER);
						if (semStatus == FALSE) {
							status = RINGIO_EFAILURE;
							SET_FAILURE_REASON(status);
						} else {
							status = SYS_OK;
						}
					} else if (wrRingStatus == RINGIO_SUCCESS) {

						/* Successfully acquired the output buffer of size equal to
						 * writeAcqSize
						 */
						//Initial the write buffer
						ptr8 = (Uint8 *) (info->writerBuf);
						if (info->writerBuf != NULL) {
							for (i = 0; i < (info->writerRecvSize); i++) {

								*ptr8 = Buffer[1] * 2;
								//*ptr8 = 10;
								ptr8++;

							}
						}
						if ((RING_IO_dataBufSize1 != 0) && ((bytesTransfered
								+ info->writerRecvSize) > RING_IO_dataBufSize1)) {

							/* we have acquired more buffer than the rest of data
							 * bytes to be transferred */
							if (bytesTransfered != RING_IO_dataBufSize1) {

								wrRingStatus
										= RingIO_release(info->writerHandle,
												(RING_IO_dataBufSize1
														- bytesTransfered));
								if (RINGIO_SUCCESS != wrRingStatus) {
									SET_FAILURE_REASON(wrRingStatus);
								}
							}

							/* Cancel the  rest of the buffer */
							wrRingStatus = RingIO_cancel(info->writerHandle);
							if (RINGIO_SUCCESS != wrRingStatus) {
								SET_FAILURE_REASON(wrRingStatus);
							}
							bytesTransfered = RING_IO_dataBufSize1;

						} else {
							wrRingStatus = RingIO_release(info->writerHandle,
									info->writerRecvSize);
							if (RINGIO_SUCCESS != wrRingStatus) {
								SET_FAILURE_REASON(wrRingStatus);
							} else {
								bytesTransfered += info->writerRecvSize;
							}
						}

					}

				}
			}

			bytesTransfered = 0;
			if ((RINGIO_SUCCESS == wrRingStatus) && (!info->exitflag)) {

				/* Send  End of  data transfer attribute to DSP */
				type = (Uint16) RINGIO_DATA_END;
				do {
					wrRingStatus = RingIO_setAttribute(info->writerHandle, 0, type,
							0);
					if (wrRingStatus != RINGIO_SUCCESS) {
						SET_FAILURE_REASON(wrRingStatus);
					} else {
						status = RINGIO_SUCCESS;
						
					}
				} while ((RINGIO_SUCCESS != wrRingStatus) && (!info->exitflag));
			}
			if ((RINGIO_SUCCESS == wrRingStatus) && (!info->exitflag)) {

				/* Send Notification  to  the reader (DSP)
				 * This allows DSP  application to come out from blocked state  if
				 * it is waiting for Data buffer and  GPP sent only data end
				 * attribute.
				 */
				wrRingStatus = RingIO_sendNotify(info->writerHandle,
						(RingIO_NotifyMsg) NOTIFY_DATA_END);
				if (wrRingStatus != RINGIO_SUCCESS) {
					SET_FAILURE_REASON(wrRingStatus);
				} else {
					status = RINGIO_SUCCESS;
					TSK_yield();
					
				}
			}
		}
	}
	status = MEM_free(DSPLINK_SEGID, Buffer, RING_IO_dataBufSize3);
	


	return (status);
}

Int TSKRING_IO_execute2(TSKRING_IO_TransferInfo * info) {
	Int status = SYS_OK;
	Int wrRingStatus = RINGIO_SUCCESS;
	Int rdRingStatus = RINGIO_SUCCESS;
	Bool semStatus = TRUE;
	Bool exitFlag = FALSE;
	Uint32 i;
	Uint32 j;
	Uint16 type;
	Uint32 param;
	Uint32 writerWaterMark;
	Uint32 writeAcqSize;
	Uint32 readerAcqSize;
	Uint32 size;
	Uint32 totalRcvbytes = 0;
	Char * Buffer;
	Uint32 bytesTransfered = 0;
	Uint8 * ptr8;

	RING_IO_dataBufSize4 = 2048;  //The DSP reader Size

	/*
	 *  Set the notification for Writer.
	 */
	//writerWaterMark
	//		= (RING_IO_dataBufSize2 < RINGIO_WRITE_ACQ_SIZE) ? RING_IO_dataBufSize2
	//				: RINGIO_WRITE_ACQ_SIZE;
	writerWaterMark= RING_IO_dataBufSize2;

	do {
		status = RingIO_setNotifier(info->writerHandle,
				RINGIO_NOTIFICATION_ONCE, writerWaterMark,
				&TSKRING_IO_writer_notify, (RingIO_NotifyParam) info);
	} while (status != SYS_OK);

	/*
	 *  Set the notification for Reader.
	 */
	// readerAcqSize = (RING_IO_dataBufSize3 < RINGIO_READ_ACQ_SIZE)?
	//                 RING_IO_dataBufSize3 : RINGIO_READ_ACQ_SIZE ;


	readerAcqSize = RING_IO_dataBufSize4;
	Buffer = MEM_calloc(DSPLINK_SEGID, readerAcqSize, DSPLINK_BUF_ALIGN);
	if(NULL == Buffer) {
		status = RINGIO_EFAILURE;
		SET_FAILURE_REASON(status);
	}

	do {
		status = RingIO_setNotifier(info->readerHandle,
				RINGIO_NOTIFICATION_ONCE, 0 /* readerWaterMark */,
				&TSKRING_IO_reader_notify, (RingIO_NotifyParam) info);
	} while (status != SYS_OK);

	writeAcqSize = writerWaterMark;

	//while (1) {
	while (!info->exitflag) {


		/* Wait for the start notification from gpp */
		semStatus = SEM_pend(&(info->readerSemObj), SYS_FOREVER);
		if (semStatus == FALSE) {
			status = RINGIO_EFAILURE;
			SET_FAILURE_REASON(status);
		} else {
			status = SYS_OK;
		}

		if ((info->freadStart == TRUE) && (!info->exitflag)) {

			info->freadStart = FALSE;

			/* Got  data transfer start notification from DSP*/
			do {

				/* Get the start attribute from dsp */
				status = RingIO_getAttribute(info->readerHandle, &type, &param);
				if ((status == RINGIO_SUCCESS) || (status
						== RINGIO_SPENDINGATTRIBUTE)) {

					if (type == (Uint16) RINGIO_DATA_START) {
						/*RING_IO_0Print ("GPP<--DSP1:Received Data Transfer"
						 "Start Attribute\n") ;*/
						break;
					}

				}

			} while ((status != RINGIO_SUCCESS) && (status
					!= RINGIO_SPENDINGATTRIBUTE)  && (!info->exitflag));
		}

		info->readerRecvSize = readerAcqSize; //the size of RingIO_acquire
		info->scaleSize = readerAcqSize; //the size of the rest of the RingIO_acquire
		while ((exitFlag == FALSE)  && (!info->exitflag)) {
			rdRingStatus = RingIO_acquire(info->readerHandle,
					(RingIO_BufPtr *) &(info->readerBuf),
					&(info->readerRecvSize));

			if ((rdRingStatus == RINGIO_EFAILURE) || (rdRingStatus
					== RINGIO_EBUFEMPTY)) {
				/* Wait for the read buffer to be available */
				semStatus = SEM_pend(&(info->readerSemObj), SYS_FOREVER);
				if (semStatus == FALSE) {
					status = RINGIO_EFAILURE;
					SET_FAILURE_REASON(status);
				} else {
					status = SYS_OK;
				}
			} else if ((rdRingStatus == RINGIO_SUCCESS)
					|| ((info->readerRecvSize > 0) && ((rdRingStatus
							== RINGIO_ENOTCONTIGUOUSDATA) || (rdRingStatus
							== RINGIO_EBUFWRAP) || (rdRingStatus
							== RINGIO_SPENDINGATTRIBUTE)))) {
				/* Acquired Read buffer.Copy input received data
				 *to output buffer and  process the buffer as
				 *specified in the received  variable attribute
				 */
				info->scaleSize -= info->readerRecvSize;




/*
				//debug
						
							do {
								wrRingStatus = RingIO_sendNotify(info->writerHandle,
										(RingIO_NotifyMsg)info->readerBuf[0] );
								if (wrRingStatus != RINGIO_SUCCESS) {
									SET_FAILURE_REASON(wrRingStatus);
								}
							} while (wrRingStatus != RINGIO_SUCCESS);
						





				//debug*/
				

				if (Buffer && info->readerBuf) {
					if ((totalRcvbytes + info->readerRecvSize) <= readerAcqSize)
						memcpy((Buffer + totalRcvbytes), info->readerBuf,
								info->readerRecvSize);

					//debug

							/* Sending the Hard Notification to gpp reader */
							do {
								wrRingStatus = RingIO_sendNotify(info->writerHandle,
										(RingIO_NotifyMsg)Buffer[0] );
								if (wrRingStatus != RINGIO_SUCCESS) {
									SET_FAILURE_REASON(wrRingStatus);
								}
							} while (wrRingStatus != RINGIO_SUCCESS);


					//debug




					
				}
				totalRcvbytes += info->readerRecvSize;

				/* Release the input buffer(reader buffer) */
				rdRingStatus = RingIO_release(info->readerHandle,
						info->readerRecvSize);
				if (RINGIO_SUCCESS != rdRingStatus) {
					SET_FAILURE_REASON(rdRingStatus);

				}
				/* Set the acqSize for the next acquire */
				if (info->scaleSize == 0) {
					/* Reset  the rcvSize to  size of the full buffer  */
					info->scaleSize = RING_IO_dataBufSize4;
					info->readerRecvSize = RING_IO_dataBufSize4;
				} else {
					/*Acquire the partial buffer  in next acquire */
					info->readerRecvSize = info->scaleSize;
				}

			}

			else if (rdRingStatus == RINGIO_SPENDINGATTRIBUTE) {

				rdRingStatus = RingIO_getAttribute(info->readerHandle, &type,
						&param);
				if ((RINGIO_SUCCESS == rdRingStatus)
						|| (RINGIO_SPENDINGATTRIBUTE == rdRingStatus)) {

					/* Got the fixed attribute */
					if (type == RINGIO_DATA_END) {
						/* End of data transfer from DSP */

						exitFlag = TRUE;
					}
				} else if (rdRingStatus == RINGIO_EVARIABLEATTRIBUTE) {
					j = sizeof(attrs);
					rdRingStatus = RingIO_getvAttribute(info->readerHandle,
							&type, &i, attrs, &j);
					if ((RINGIO_SUCCESS == rdRingStatus)
							|| (RINGIO_SPENDINGATTRIBUTE == rdRingStatus)) {

						/* got the variable attribute */
						readerAcqSize = attrs[0];

						//info->scaleSize = attrs[0];
						info->scaleSize = readerAcqSize;
						info->readerRecvSize = info->scaleSize;
					} else if (RINGIO_EVARIABLEATTRIBUTE == rdRingStatus) {

						/* This case should not arise.
						 * as we have provided the sufficient buffer
						 * to receive variable Attribute
						 */
						SET_FAILURE_REASON(rdRingStatus);
					} else {
						/* For RINGIO_EPENDINGDATA, RINGIO_EFAILURE
						 * nothing to be done. go and  read data again
						 */
					}
				} else {
					/* For other return status
					 * (RINGIO_EPENDINGDATA,RINGIO_EFAILURE)
					 * no thing o be done. go and read the data again
					 */

				}
			} else {
				/* For Any other  wrRingStatus,Consider it as failure */
				status = RINGIO_EFAILURE;
				SET_FAILURE_REASON(status);
			}
			/* Reset the acquired size if it is changed to zero by the
			 * failed acquire call
			 */
			if (info->readerRecvSize == 0) {
				info->readerRecvSize = info->scaleSize;
			}

		}

		totalRcvbytes = 0;
		info->readerRecvSize = readerAcqSize; //the size of RingIO_acquire
		info->scaleSize = readerAcqSize; //the size of the rest of the RingIO_acquire
		info->freadEnd = FALSE;
		exitFlag = FALSE;

		///////////////////////////////////////////////////////////////////////////////
		//End  the read  task
		///////////////////////////////////////////////////////////////////////////////


		///////////////////////////////////////////////////////////////////////////////
		//To do the algorithms with the Buffer (RING_IO_dataBufSize3)
		///////////////////////////////////////////////////////////////////////////////
		//if( && (!info->exitflag))


		///////////////////////////////////////////////////////////////////////////////
		//END To do the algorithms with the Buffer (RING_IO_dataBufSize3)
		///////////////////////////////////////////////////////////////////////////////


		///////////////////////////////////////////////////////////////////////////////
		//start  the write  task
		///////////////////////////////////////////////////////////////////////////////


		if ((RINGIO_SUCCESS == wrRingStatus)  && (!info->exitflag)) {
			type = (Uint16) RINGIO_DATA_START;
			/* Set the attribute start attribute to output */
			wrRingStatus = RingIO_setAttribute(info->writerHandle, 0, type, 0);
			if (wrRingStatus != RINGIO_SUCCESS) {
				SET_FAILURE_REASON(wrRingStatus);
			} else {
				/* Sending the Hard Notification to gpp reader */
				do {
					wrRingStatus = RingIO_sendNotify(info->writerHandle,
							(RingIO_NotifyMsg) NOTIFY_DATA_START);
					if (wrRingStatus != RINGIO_SUCCESS) {
						SET_FAILURE_REASON(wrRingStatus);
					}
				} while (wrRingStatus != RINGIO_SUCCESS);
			}
		}

		if ((RINGIO_SUCCESS == wrRingStatus)  && (!info->exitflag)) {
			//while ((RING_IO_dataBufSize2 == 0) || (bytesTransfered
			while ((bytesTransfered < RING_IO_dataBufSize2)  && (!info->exitflag)) {
				/* Update the attrs to send variable attribute to DSP*/
				attrs[0] = RING_IO_dataBufSize2;
				wrRingStatus = RingIO_setvAttribute(info->writerHandle, 0, 0,
						0, attrs, sizeof(attrs));

				if (wrRingStatus == RINGIO_EWRONGSTATE) {
				} else {
					/* Acquire writer bufs and initialize and release them. */
					info->writerRecvSize = RING_IO_dataBufSize2;
					wrRingStatus = RingIO_acquire(info->writerHandle,
							(RingIO_BufPtr *) &(info->writerBuf),
							&(info->writerRecvSize));

					if ((wrRingStatus == RINGIO_EFAILURE) || (wrRingStatus
							== RINGIO_EBUFFULL)) {
						/* Wait for Writer notification */
						semStatus
								= SEM_pend(&(info->writerSemObj), SYS_FOREVER);
						if (semStatus == FALSE) {
							status = RINGIO_EFAILURE;
							SET_FAILURE_REASON(status);
						} else {
							status = SYS_OK;
						}
					} else if (wrRingStatus == RINGIO_SUCCESS) {

						/* Successfully acquired the output buffer of size equal to
						 * writeAcqSize
						 */
						//Initial the write buffer
						ptr8 = (Uint8 *) (info->writerBuf);
						if (info->writerBuf != NULL) {
							for (i = 0; i < (info->writerRecvSize); i++) {

								*ptr8 = Buffer[0] * 2;
								//*ptr8 = 9;
								ptr8++;

							}
						}
						if ((RING_IO_dataBufSize2 != 0) && ((bytesTransfered
								+ info->writerRecvSize) > RING_IO_dataBufSize2)) {

							/* we have acquired more buffer than the rest of data
							 * bytes to be transferred */
							if (bytesTransfered != RING_IO_dataBufSize2) {

								wrRingStatus
										= RingIO_release(info->writerHandle,
												(RING_IO_dataBufSize2
														- bytesTransfered));
								if (RINGIO_SUCCESS != wrRingStatus) {
									SET_FAILURE_REASON(wrRingStatus);
								}
							}

							/* Cancel the  rest of the buffer */
							wrRingStatus = RingIO_cancel(info->writerHandle);
							if (RINGIO_SUCCESS != wrRingStatus) {
								SET_FAILURE_REASON(wrRingStatus);
							}
							bytesTransfered = RING_IO_dataBufSize2;

						} else {
							wrRingStatus = RingIO_release(info->writerHandle,
									info->writerRecvSize);
							if (RINGIO_SUCCESS != wrRingStatus) {
								SET_FAILURE_REASON(wrRingStatus);
							} else {
								bytesTransfered += info->writerRecvSize;
							}
						}

					}

				}
			}

			bytesTransfered = 0;
			if ((RINGIO_SUCCESS == wrRingStatus)  && (!info->exitflag)) {

				/* Send  End of  data transfer attribute to DSP */
				type = (Uint16) RINGIO_DATA_END;
				do {
					wrRingStatus = RingIO_setAttribute(info->writerHandle, 0, type,
							0);
					if (wrRingStatus != RINGIO_SUCCESS) {
						SET_FAILURE_REASON(wrRingStatus);
					} else {
						status = RINGIO_SUCCESS;
						
					}
				} while (RINGIO_SUCCESS != wrRingStatus);
			}
			if ((RINGIO_SUCCESS == wrRingStatus)  && (!info->exitflag)) {

				/* Send Notification  to  the reader (DSP)
				 * This allows DSP  application to come out from blocked state  if
				 * it is waiting for Data buffer and  GPP sent only data end
				 * attribute.
				 */
				wrRingStatus = RingIO_sendNotify(info->writerHandle,
						(RingIO_NotifyMsg) NOTIFY_DATA_END);
				if (wrRingStatus != RINGIO_SUCCESS) {
					SET_FAILURE_REASON(wrRingStatus);
				} else {
					status = RINGIO_SUCCESS;
					TSK_yield();
					
				}
			}
		}
	}

	status = MEM_free(DSPLINK_SEGID, Buffer, RING_IO_dataBufSize4);

	return (status);
}

/** ============================================================================
 *  @func   TSKRING_IO_delete
 *
 *  @desc   Delete phase function for the application. It deallocates
 *          all the resources  allocated during create phase of the
 *          application.
 *
 *  @modif  None.
 *  ============================================================================
 */
Int TSKRING_IO_delete1(TSKRING_IO_TransferInfo * info) {
	Int status = SYS_OK;
	Int tmpStatus = SYS_OK;
	Bool freeStatus = FALSE;
	Uint32 size = 0;

	/*
	 *  Close the RingIO to be used with DSP as the writer.
	 */
	if (info->writerHandle != NULL) {
		do {
			size = RingIO_getValidAttrSize(info->writerHandle);
		} while (size != 0);

		/* Ensure that gpp has read all the data */
		tmpStatus = RingIO_close(info->writerHandle);
		if (tmpStatus != SYS_OK) {
			status = tmpStatus;
			SET_FAILURE_REASON(status);
		} else {
			info->writerHandle = NULL;
		}
	}

	/*
	 *  Delete the RingIO to be used with DSP as the writer.
	 */
	do {
#if defined (DSPLINK_LEGACY_SUPPORT)
		tmpStatus = RingIO_delete (RING_IO_WRITER_NAME1);
#else
		tmpStatus = RingIO_delete(GBL_getProcId(), RING_IO_WRITER_NAME1);
#endif /* if defined (DSPLINK_LEGACY_SUPPORT) */
		if (tmpStatus != SYS_OK) {
		}
	} while (tmpStatus != SYS_OK);

	/*
	 *  Close the RingIO to be used with DSP as the reader.
	 */
	if (info->readerHandle != NULL) {
		do {

			status = RingIO_close(info->readerHandle);
			if (status != SYS_OK) {
				SET_FAILURE_REASON(status);
			} else {
				info->readerHandle = NULL;
			}
		} while (status != SYS_OK);
	}

	/* Free the info structure */
	freeStatus = MEM_free(DSPLINK_SEGID, info, sizeof(TSKRING_IO_TransferInfo));

	if ((status == SYS_OK) && (freeStatus != TRUE)) {
		status = SYS_EFREE;
		SET_FAILURE_REASON(status);
	}

	return (status);
}

Int TSKRING_IO_delete2(TSKRING_IO_TransferInfo * info) {
	Int status = SYS_OK;
	Int tmpStatus = SYS_OK;
	Bool freeStatus = FALSE;
	Uint32 size = 0;

	/*
	 *  Close the RingIO to be used with DSP as the writer.
	 */
	if (info->writerHandle != NULL) {
		do {
			size = RingIO_getValidAttrSize(info->writerHandle);
		} while (size != 0);

		/* Ensure that gpp has read all the data */
		tmpStatus = RingIO_close(info->writerHandle);
		if (tmpStatus != SYS_OK) {
			status = tmpStatus;
			SET_FAILURE_REASON(status);
		} else {
			info->writerHandle = NULL;
		}
	}

	/*
	 *  Delete the RingIO to be used with DSP as the writer.
	 */
	do {
#if defined (DSPLINK_LEGACY_SUPPORT)
		tmpStatus = RingIO_delete (RING_IO_WRITER_NAME2);
#else
		tmpStatus = RingIO_delete(GBL_getProcId(), RING_IO_WRITER_NAME2);
#endif /* if defined (DSPLINK_LEGACY_SUPPORT) */
		if (tmpStatus != SYS_OK) {
		}
	} while (tmpStatus != SYS_OK);

	/*
	 *  Close the RingIO to be used with DSP as the reader.
	 */
	if (info->readerHandle != NULL) {
		do {

			status = RingIO_close(info->readerHandle);
			if (status != SYS_OK) {
				SET_FAILURE_REASON(status);
			} else {
				info->readerHandle = NULL;
			}
		} while (status != SYS_OK);
	}

	/* Free the info structure */
	freeStatus = MEM_free(DSPLINK_SEGID, info, sizeof(TSKRING_IO_TransferInfo));

	if ((status == SYS_OK) && (freeStatus != TRUE)) {
		status = SYS_EFREE;
		SET_FAILURE_REASON(status);
	}

	return (status);
}

/** ----------------------------------------------------------------------------
 *  @func   RING_IO_apply
 *
 *  @desc   This function multiples or divides the data in the buffer by a value
 *          specified in the factor variable.(inplace processing)
 *  @field buffer
 *       data buffer that needs to be processed.
 *  @field factor
 *        scale value
 *  @field opCode
 *         specifies the operation to be performed
 *       (OP_MULTIPLY/OP_DIVIDE)
 *  @field size
 *        size of the buffer that needs to be processed.
 *
 *  @modif  buffer
 *  ----------------------------------------------------------------------------
 */
static Void RING_IO_apply(RingIO_BufPtr * buffer, Uint32 factor, Uint32 opCode,
		Uint32 size) {
	Uint32 i = 0;
	Uint8 maduSize = DSP_MAUSIZE;
	Uint8 * ptr8;
	Uint16 * ptr16;

	if (buffer != NULL) {
		ptr8 = (Uint8 *) (buffer);
		ptr16 = (Uint16 *) (buffer);

		for (i = 0; i < (size / DSP_MAUSIZE); i++) {
			switch (opCode) {
			case OP_MULTIPLY:
				if (maduSize == 1u) {
					*ptr8 = (*ptr8) * factor;
					ptr8++;
				} else {
					/* DSP_MAUSIZE == 2 */
					*ptr16 = (*ptr16) * factor;
					ptr16++;
				}
				break;

			case OP_DIVIDE:
				if (maduSize == 1u) {
					*ptr8 = (*ptr8) / factor;
					ptr8++;
				} else {
					/* DSP_MAUSIZE == 2 */
					*ptr16 = (*ptr16) / factor;
					ptr16++;
				}
				break;

			default:
				break;

			}
		}
	}
}

/** ----------------------------------------------------------------------------
 *  @func   TSKRING_IO_reader_notify
 *
 *  @desc   This function implements the notification callback for the RingIO1
 *          opened by the DSP in  reader mode.
 *
 *  @modif  None
 *  ----------------------------------------------------------------------------
 */
static Void TSKRING_IO_reader_notify(RingIO_Handle handle,
		RingIO_NotifyParam param, RingIO_NotifyMsg msg) {
	TSKRING_IO_TransferInfo * info;
	(Void) handle; /* To avoid compiler warning */
	(Void) msg; /* To avoid compiler warning */

	if (param != NULL) {
		info = (TSKRING_IO_TransferInfo *) param;

		switch (msg) {
		case NOTIFY_DATA_START:
			/* Got data transfer start notification  From GPP
			 * (RINGIO1)
			 */
			info->freadStart = TRUE;
			break;

		case NOTIFY_DATA_END:
			/* Got data transfer end  notification  From GPP
			 * (RINGIO1)
			 */
			info->freadEnd = TRUE;
			break;
		case NOTIFY_DSP_END:
			
			info->exitflag = TRUE;
			/*RingIO_sendNotify(info->writerHandle,
							(RingIO_NotifyMsg)(8));*/
			break;
		default:
			break;

		}

		/* Post the semaphore. */
		SEM_post((SEM_Handle) & (info->readerSemObj));
	}
}

/** ----------------------------------------------------------------------------
 *  @func   TSKRING_IO_writer_notify
 *
 *  @desc   This function implements the notification callback for the RingIO2
 *          opened by the DSP in writer  mode.
 *
 *  @modif  None
 *  ----------------------------------------------------------------------------
 */
static Void TSKRING_IO_writer_notify(RingIO_Handle handle,
		RingIO_NotifyParam param, RingIO_NotifyMsg msg) {
	TSKRING_IO_TransferInfo * info;
	(Void) handle; /* To avoid compiler warning */
	(Void) msg; /* To avoid compiler warning */

	if (param != NULL) {
		info = (TSKRING_IO_TransferInfo *) param;
		/* Post the semaphore. */
		SEM_post((SEM_Handle) & (info->writerSemObj));
	}
}

