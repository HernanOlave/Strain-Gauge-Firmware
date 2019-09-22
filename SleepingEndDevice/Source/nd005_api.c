/**
 * @file nd005_api.c
 * @brief
 *
 * @author Wisely SpA
 * @date 22-Sep-19
 *
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <appZpsBeaconHandler.h>
#include <jendefs.h>
#include <string.h>
#include <dbg.h>
#include <zps_apl_af.h>
#include <zps_apl_aib.h>
#include <pwrm.h>
#include <pdm.h>
#include <PDM_IDs.h>
#include "app_common.h"
#include "app_sleeping_enddevice.h"
#include <ZTimer.h>
#include "pdum_gen.h"
#include "pdm_app_ids.h"

#include "mcp3204.h"
#include "ad8231.h"
#include "ltc1661.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#ifndef DEBUG_APP
	#define TRACE_APP 	FALSE
#else
	#define TRACE_APP 	TRUE
#endif

#define MAX_SYSTEM_STRIKES			5	// times
#define MAX_AUTH_STRIKES			3	// times
#define SECS_TO_TICKS(seconds)		seconds * 32768
#define STATE_MACHINE_WDG_TIME		ZTIMER_TIME_MSEC(5000)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

PUBLIC pwrm_tsWakeTimerEvent sWake;
PUBLIC uint8 u8TimerWatchdog;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE seDeviceDesc_t s_eDevice;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void vWakeCallBack(void)
{
	DBG_vPrintf(TRACE_APP, "\n\r\n\r*** WAKE UP ROUTINE ***\n\r");
	DBG_vPrintf(TRACE_APP, "APP: WAKE_UP_STATE\n\r");
	s_eDevice.currentState = WAKE_UP_STATE;
}

/****************************************************************************
 *
 * NAME: APP_cbTimerButtonScan
 *
 * DESCRIPTION:
 * Timer callback to debounce the button presses
 *
 * PARAMETER:
 *
 * RETURNS:
 *
 ****************************************************************************/
PUBLIC void APP_cbTimerWatchdog(void *pvParam)
{
	s_eDevice.systemStrikes++;
	DBG_vPrintf
	(
		TRACE_APP,
		"APP: State machine timed out, strike = %d\n\r",
		s_eDevice.systemStrikes
	);
	s_eDevice.currentState = PREP_TO_SLEEP_STATE;
}

/****************************************************************************
 *
 * NAME: APP_vInitialiseSleepingEndDevice
 *
 * DESCRIPTION:
 * Initializes the Sleeping End Device application
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void APP_vInitialiseSleepingEndDevice(void)
{
	DBG_vPrintf(TRACE_APP, "APP: Startup\n\r");

    vAHI_DioSetDirection((1 << CONFIG_BUTTON_PIN), 0x0);	// set as input (default)
    vAHI_DioSetPullup((1 << CONFIG_BUTTON_PIN), 0x0);		// enable pull-up (default)

    /* Delete the network context from flash if a button is being held down
     * at reset
     */
    if (!((1 << CONFIG_BUTTON_PIN) & u32AHI_DioReadInput()))
    {
        DBG_vPrintf(TRACE_APP, "APP: Deleting all records from flash\n\r");

        PDM_vDeleteDataRecord(PDM_APP_ID_SAMPLE_PERIOD);
        PDM_vDeleteDataRecord(PDM_APP_ID_CONFIGURED);
        PDM_vDeleteDataRecord(PDM_APP_ID_EPID);
        PDM_vDeleteDataRecord(PDM_APP_ID_BLACKLIST);
        PDM_vDeleteDataRecord(PDM_APP_ID_CHANNEL_A);
        PDM_vDeleteDataRecord(PDM_APP_ID_CHANNEL_B);
        PDM_vDeleteDataRecord(PDM_APP_ID_GAIN);

        /* Clear blacklist */
		for( blacklistIndex = 0; blacklistIndex < BLACKLIST_MAX; blacklistIndex++ )
		{
			blacklistEpids[blacklistIndex] = 0;
		}
		blacklistIndex = 0;
    }

    s_eDevice.systemStrikes = 0;
    s_eDevice.isConfigured = FALSE;
    s_eDevice.samplePeriod = DEFAULT_SAMPLE_PERIOD;
    s_eDevice.channelAValue = CHANNEL_A_DEFAULT_VALUE;
    s_eDevice.channelBValue = CHANNEL_B_DEFAULT_VALUE;
    s_eDevice.gainValue = GAIN_DEFAULT_VALUE;
    s_eDevice.sleepTime = DEFAULT_SLEEP_TIME;

    /* Restore any application data previously saved to flash
     * All Application records must be loaded before the call to
     * ZPS_eAplAfInit
     */
    uint16 u16DataBytesRead;
    uint64 epidBuffer;

    DBG_vPrintf(TRACE_APP, "APP: Restoring application data from flash\n\r");

    PDM_eReadDataFromRecord
    (
    	PDM_APP_ID_EPID,
        &epidBuffer,
        sizeof(epidBuffer),
        &u16DataBytesRead
    );

    nwk_api_setEpid(epidBuffer);

    PDM_eReadDataFromRecord
	(
		PDM_APP_ID_CONFIGURED,
		&s_eDevice.isConfigured,
		sizeof(s_eDevice.isConfigured),
		&u16DataBytesRead
	);

    /* If configured flag is set, then restore analog parameters */
    if(s_eDevice.isConfigured)
    {
    	PDM_eReadDataFromRecord
		(
			PDM_APP_ID_SAMPLE_PERIOD,
			&s_eDevice.samplePeriod,
			sizeof(s_eDevice.samplePeriod),
			&u16DataBytesRead
		);
    	PDM_eReadDataFromRecord
		(
			PDM_APP_ID_CHANNEL_A,
			&s_eDevice.channelAValue,
			sizeof(s_eDevice.channelAValue),
			&u16DataBytesRead
		);
    	PDM_eReadDataFromRecord
		(
			PDM_APP_ID_CHANNEL_B,
			&s_eDevice.channelBValue,
			sizeof(s_eDevice.channelBValue),
			&u16DataBytesRead
		);
    	PDM_eReadDataFromRecord
		(
			PDM_APP_ID_GAIN,
			&s_eDevice.gainValue,
			sizeof(s_eDevice.gainValue),
			&u16DataBytesRead
		);
    }

    /* Initialize network API */
    nwk_api_init();

    DBG_vPrintf(TRACE_APP, "APP: Device Information:\n\r");
    DBG_vPrintf(TRACE_APP, "  MAC: 0x%016llx\n\r", ZPS_u64AplZdoGetIeeeAddr());
    DBG_vPrintf(TRACE_APP, "  EPID: 0x%016llx\n\r", nwk_api_getEpid());
    DBG_vPrintf(TRACE_APP, "  Sample Period: %d\n\r", s_eDevice.samplePeriod);
    DBG_vPrintf(TRACE_APP, "  Configured Flag: %d\n\r", s_eDevice.isConfigured);
    DBG_vPrintf(TRACE_APP, "  Channel A: %d\n\r", s_eDevice.channelAValue);
    DBG_vPrintf(TRACE_APP, "  Channel B: %d\n\r", s_eDevice.channelBValue);
    DBG_vPrintf(TRACE_APP, "  Gain: %d\n\r", s_eDevice.gainValue);

    /* Send DAC and OPAMP to low power */
	ENABLE_3VLN();
	ad8231_init();
	ad8231_enable();
	ltc1661_init();
	ad8231_disable();
	ltc1661_sleep();
	ENABLE_POWERSAVE();
	DISABLE_WB();

	ZTIMER_eStart(u8TimerWatchdog, STATE_MACHINE_WDG_TIME);

    /* Always start on NETWORK STATE */
    DBG_vPrintf(TRACE_APP, "\n\rAPP: NETWORK_STATE\n\r");
    s_eDevice.currentState = NETWORK_STATE;
}

/****************************************************************************
 *
 * NAME: APP_taskSleepingEndDevice
 *
 * DESCRIPTION:
 * Main state machine
 *
 * RETURNS:
 * void
 *


/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: vHandleNetwork
 *
 * DESCRIPTION:
 * Handles all events related to discover, join, rejoin and leave a network
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/


/****************************************************************************
 *
 * NAME: vHandleIncomingFrame
 *
 * DESCRIPTION:
 * Process incoming frame
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE frameReturnValues_t vHandleIncomingFrame(ZPS_tsAfEvent sStackEvent)
{
	uint8 idByte = 0;
	uint16 byteCount;
	uint8 status;
	PDM_teStatus pdmStatus;

	byteCount = PDUM_u16APduInstanceReadNBO
	(
		sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
	    0,
	    "b",
	    &idByte
	);

	/* Size mismatch */
	if(byteCount == 0)
	{
		/* free the application protocol data unit (APDU) once it has been dealt with */
		PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

		DBG_vPrintf(TRACE_APP, "  APP: Frame error, size = 0\n\r");
		return FRAME_BAD_SIZE;
	}

	switch(idByte)
	{
		/* configuration parameters */
		case '~':
		{
			DBG_vPrintf(TRACE_APP, "  APP: Configuration frame\n\r");

			struct
			{
				uint16 inSamplePeriod;
				uint16 inChannelAValue;
				uint16 inChannelBValue;
				uint16 inGainValue;
			} values = { 0 };

			byteCount = PDUM_u16APduInstanceReadNBO
			(
				sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
			    1,
			    "hhhh",
			     &values
			);

			/* Size mismatch */
			if(byteCount != 8)
			{
				/* free the application protocol data unit (APDU) once it has been dealt with */
				PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

				DBG_vPrintf(TRACE_APP, "  APP: Frame error, size = %d\r\n", byteCount);
				return FRAME_BAD_SIZE;
			}
			/* size OK */
			else
			{
				s_eDevice.samplePeriod = values.inSamplePeriod;
				s_eDevice.channelAValue = values.inChannelAValue;
				s_eDevice.channelBValue = values.inChannelBValue;
				s_eDevice.gainValue = values.inGainValue;

				DBG_vPrintf(TRACE_APP, "  APP: Configuration values received:\r\n");
				DBG_vPrintf(TRACE_APP, "    samplePeriod = 0x%04x\n", s_eDevice.samplePeriod);
				DBG_vPrintf(TRACE_APP, "    channelA = 0x%04x\n", s_eDevice.channelAValue);
				DBG_vPrintf(TRACE_APP, "    channelB = 0x%04x\n", s_eDevice.channelBValue);
				DBG_vPrintf(TRACE_APP, "    gainValue = 0x%04x\n", s_eDevice.gainValue);

				/* Store parameters in flash */
				pdmStatus = PDM_eSaveRecordData(PDM_APP_ID_SAMPLE_PERIOD, &s_eDevice.samplePeriod, sizeof(s_eDevice.samplePeriod));
				if(pdmStatus != PDM_E_STATUS_OK) DBG_vPrintf(TRACE_APP, "  APP: PDM_APP_ID_SAMPLE_PERIOD save error, status = %d\n", pdmStatus);

				pdmStatus = PDM_eSaveRecordData(PDM_APP_ID_CHANNEL_A, &s_eDevice.channelAValue, sizeof(s_eDevice.channelAValue));
				if(pdmStatus != PDM_E_STATUS_OK) DBG_vPrintf(TRACE_APP, "  APP: PDM_APP_ID_CHANNEL_A save error, status = %d\n", pdmStatus);

				pdmStatus = PDM_eSaveRecordData(PDM_APP_ID_CHANNEL_B, &s_eDevice.channelBValue, sizeof(s_eDevice.channelBValue));
				if(pdmStatus != PDM_E_STATUS_OK) DBG_vPrintf(TRACE_APP, "  APP: PDM_APP_ID_CHANNEL_B save error, status = %d\n", pdmStatus);

				pdmStatus = PDM_eSaveRecordData(PDM_APP_ID_GAIN, &s_eDevice.gainValue, sizeof(s_eDevice.gainValue));
				if(pdmStatus != PDM_E_STATUS_OK) DBG_vPrintf(TRACE_APP, "  APP: PDM_APP_ID_GAIN save error, status = %d\n", pdmStatus);

				/* free the application protocol data unit (APDU) once it has been dealt with */
				PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

				DBG_vPrintf(TRACE_APP, "  APP: Sending response to Coordinator\n\r");
				/* Allocate memory for APDU buffer with preconfigured "type" */
				PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance(apduMyData);
				if(data == PDUM_INVALID_HANDLE)
				{
					/* Problem allocating APDU instance memory */
					DBG_vPrintf(TRACE_APP, "  APP: Unable to allocate APDU memory\n\r");
					return FRAME_UNK_ERROR;
				}
				else
				{
					/* Load payload data into APDU */
					byteCount = PDUM_u16APduInstanceWriteNBO
					(
						data,		// APDU instance handle
						0,			// APDU position for data
						"bhhhh",	// data format string
						'~',
						s_eDevice.samplePeriod,
						s_eDevice.channelAValue,
						s_eDevice.channelBValue,
						s_eDevice.gainValue
					);

					if( byteCount == 0 )
					{
						/* No data was written to the APDU instance */
						DBG_vPrintf(TRACE_APP, "  APP: No data written to APDU\n\r");
						return FRAME_UNK_ERROR;
					}
					else
					{
						PDUM_eAPduInstanceSetPayloadSize(data, byteCount);
						DBG_vPrintf(TRACE_APP, "  APP: Data written to APDU: %d\n\r", byteCount);

						/* Request data send to destination */
						status = ZPS_eAplAfUnicastDataReq
						(
							data,					// APDU instance handle
							0xFFFF,					// cluster ID
							1,						// source endpoint
							1,						// destination endpoint
							0x0000,					// destination network address
							ZPS_E_APL_AF_UNSECURE,	// security mode
							0,						// radius
							NULL					// sequence number pointer
						);

						if( status != ZPS_E_SUCCESS )
						{
							/* Problem with request */
							DBG_vPrintf(TRACE_APP, "  APP: AckDataReq not successful, status = %d\n\r", status);
							return FRAME_UNK_ERROR;
							//TODO: Add strike count and handle error
						}

						/* everything OK, now we wait for ZPS_EVENT_APS_DATA_CONFIRM */
						return FRAME_SUCCESS;
					}
				}
			}
		}
		break;

		/* GO command */
		case '$':
		{
			DBG_vPrintf(TRACE_APP, "  APP: GO command frame\n\r");

			s_eDevice.isConfigured = TRUE;

			/* Save configured flag in flash */
			pdmStatus = PDM_eSaveRecordData(PDM_APP_ID_CONFIGURED, &s_eDevice.isConfigured, sizeof(s_eDevice.isConfigured));
			if(pdmStatus != PDM_E_STATUS_OK) DBG_vPrintf(TRACE_APP, "  APP: PDM_APP_ID_CONFIGURED save error, status = %d\n", pdmStatus);

			/* free the application protocol data unit (APDU) once it has been dealt with */
			PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

			DBG_vPrintf(TRACE_APP, "  APP: Sending response to Coordinator\n\r");
			/* Allocate memory for APDU buffer with preconfigured "type" */
			PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance(apduMyData);
			if(data == PDUM_INVALID_HANDLE)
			{
				/* Problem allocating APDU instance memory */
				DBG_vPrintf(TRACE_APP, "  APP: Unable to allocate APDU memory\n\r");
				return FRAME_UNK_ERROR;
			}
			else
			{
				/* Load payload data into APDU */
				byteCount = PDUM_u16APduInstanceWriteNBO
				(
					data,	// APDU instance handle
					0,		// APDU position for data
					"bbb",	// data format string
					'$',
					'G',
					'O'
				);

				if( byteCount == 0 )
				{
					/* No data was written to the APDU instance */
					DBG_vPrintf(TRACE_APP, "  APP: No data written to APDU\n\r");
					return FRAME_UNK_ERROR;
				}
				else
				{
					PDUM_eAPduInstanceSetPayloadSize(data, byteCount);
					DBG_vPrintf(TRACE_APP, "  APP: Data written to APDU: %d\n\r", byteCount);

					/* Request data send to destination */
					status = ZPS_eAplAfUnicastDataReq
					(
						data,					// APDU instance handle
						0xFFFF,					// cluster ID
						1,						// source endpoint
						1,						// destination endpoint
						0x0000,					// destination network address
						ZPS_E_APL_AF_UNSECURE,	// security mode
						0,						// radius
						NULL					// sequence number pointer
					);

					if( status != ZPS_E_SUCCESS )
					{
						/* Problem with request */
						DBG_vPrintf(TRACE_APP, "  APP: AckDataReq not successful, status = %d\n\r", status);
						return FRAME_UNK_ERROR;
						//TODO: Add strike count and handle error
					}

					/* everything OK, now we wait for ZPS_EVENT_APS_DATA_CONFIRM */
					return FRAME_SUCCESS;
				}
			}
		}
		break;

		/* Broadcast request */
		case '&':
		{
			DBG_vPrintf(TRACE_APP, "  APP: Broadcast request frame\n\r");

			/* free the application protocol data unit (APDU) once it has been dealt with */
			PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

			sendBroadcast();

			/* everything OK, now we wait for ZPS_EVENT_APS_DATA_CONFIRM */
			return FRAME_SUCCESS;
		}
		break;

		default:
		{
			/* free the application protocol data unit (APDU) once it has been dealt with */
			PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

			DBG_vPrintf(TRACE_APP, "  APP: Frame format error\n\r");
			return FRAME_BAD_FORMAT;
		}
		break;
	}

	/* free the application protocol data unit (APDU) once it has been dealt with */
	PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

	DBG_vPrintf(TRACE_APP, "  APP: Frame unknown error\n\r");
	return FRAME_UNK_ERROR;
}

/****************************************************************************
 *
 * NAME: sendBroadcast
 *
 * DESCRIPTION:
 * Sends a broadcast message (&) to the coordinator
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void sendBroadcast(void)
{
	uint16 byteCount, status;

	/* Allocate memory for APDU buffer with preconfigured "type" */
	PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance(apduMyData);
	if(data == PDUM_INVALID_HANDLE)
	{
		/* Problem allocating APDU instance memory */
		DBG_vPrintf(TRACE_APP, "  APP: Unable to allocate APDU memory\n\r");
		//TODO: Handle error
	}
	else
	{
		DBG_vPrintf(TRACE_APP, "  APP: Sending response to Coordinator\n\r");
		/* Load payload data into APDU */
		byteCount = PDUM_u16APduInstanceWriteNBO
		(
			data,			// APDU instance handle
			0,				// APDU position for data
			"bbbb",			// data format string
			'&',			// frame ID
			DEVICE_TYPE,	// Device Type
			VERSION_MAJOR,	// FW Version (major)
			VERSION_MINOR	// FW Version (minor)
		);

		if( byteCount == 0 )
		{
			/* No data was written to the APDU instance */
			DBG_vPrintf(TRACE_APP, "  APP: No data written to APDU\n\r");
			//TODO: Handle error
		}
		else
		{
			PDUM_eAPduInstanceSetPayloadSize(data, byteCount);
			DBG_vPrintf(TRACE_APP, "  APP: Data written to APDU: %d\n\r", byteCount);

			/* Request data send to destination */
			status = ZPS_eAplAfUnicastDataReq
			(
				data,					// APDU instance handle
				0xFFFF,					// cluster ID
				1,						// source endpoint
				1,						// destination endpoint
				0x0000,					// destination network address
				ZPS_E_APL_AF_UNSECURE,	// security mode
				0,						// radius
				NULL					// sequence number pointer
			);

			if( status != ZPS_E_SUCCESS )
			{
				/* Problem with request */
				DBG_vPrintf(TRACE_APP, "  APP: AckDataReq not successful, status = %d\n\r", status);
				//TODO: Handle error
			}
		}
	}
}

/****************************************************************************
 *
 * NAME: SendAuthReq
 *
 * DESCRIPTION:
 * Send authentication command request
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void sendAuthReq(void)
{
	uint16 byteCount, status;

	/* Allocate memory for APDU buffer with preconfigured "type" */
	PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance(apduMyData);
	if(data == PDUM_INVALID_HANDLE)
	{
		/* Problem allocating APDU instance memory */
		DBG_vPrintf(TRACE_APP, "  APP: Unable to allocate APDU memory\n\r");
		//TODO: Handle error
	}
	else
	{
		DBG_vPrintf(TRACE_APP, "  APP: Sending response to Coordinator\n\r");
		/* Load payload data into APDU */
		byteCount = PDUM_u16APduInstanceWriteNBO
		(
			data,	// APDU instance handle
			0,		// APDU position for data
			"b",	// data format string
			'!'
		);

		if( byteCount == 0 )
		{
			/* No data was written to the APDU instance */
			DBG_vPrintf(TRACE_APP, "  APP: No data written to APDU\n\r");
			//TODO: Handle error
		}
		else
		{
			PDUM_eAPduInstanceSetPayloadSize(data, byteCount);
			DBG_vPrintf(TRACE_APP, "  APP: Data written to APDU: %d\n\r", byteCount);

			/* Request data send to destination */
			status = ZPS_eAplAfUnicastDataReq
			(
				data,					// APDU instance handle
				0xFFFF,					// cluster ID
				1,						// source endpoint
				1,						// destination endpoint
				0x0000,					// destination network address
				ZPS_E_APL_AF_UNSECURE,	// security mode
				0,						// radius
				NULL					// sequence number pointer
			);

			if( status != ZPS_E_SUCCESS )
			{
				/* Problem with request */
				DBG_vPrintf(TRACE_APP, "  APP: AckDataReq not successful, status = %d\n\r", status);
				//TODO: Handle error
			}
		}
	}
}

/****************************************************************************
 *
 * NAME: getMedianAvg
 *
 * DESCRIPTION:
 * Returns the average of the three median values from a list.
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE uint16 getMedianAvg(uint8 adcChannel, uint8 samples)
{
	uint16 i, j, Imin, temp;
	uint16 results[samples+1];

	if (samples < 3) return 0;

	DBG_vPrintf(TRACE_APP, "  APP: getting %d values from channel %d\n\r", samples, adcChannel);

	for (i = 0; i < samples; i++) results[i] = MCP3204_convert(0, adcChannel);

	for(i = 0; i < 10-1; i++)
	{
		Imin = i;
		for(j = i + 1; j < 10; j++)
		{
			if(results[j] < results[Imin])
			{
				Imin = j;
			}
		}
		temp = results[Imin];
		results[Imin] = results[i];
		results[i] = temp;
	}

	DBG_vPrintf(TRACE_APP, "  APP: sorted Values = ");
	for (i = 0; i < samples; i++) DBG_vPrintf(TRACE_APP,"%d, ", results[i]);
	DBG_vPrintf(TRACE_APP, "\n\r");

	temp = results[3];
	temp += results[4];
	temp += results[5];
	temp /= 3;

	return temp;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
