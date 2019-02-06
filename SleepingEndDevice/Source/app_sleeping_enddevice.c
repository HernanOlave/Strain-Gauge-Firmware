/*****************************************************************************
 *
 * MODULE:				JN-AN-1184 ZigBeePro Application Template
 *
 * COMPONENT:			app_sleeping_enddevice.c
 *
 * DESCRIPTION:			Sleeping EndDevice Application
 *
 *****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved. We grant You,
 * and any third parties, a license to use this software solely and
 * exclusively on NXP products [NXP Microcontrollers such as JN5169, JN5168,
 * JN5164, JN5161].
 * You, and any third parties must reproduce the copyright and warranty notice
 * and any other legend of ownership on each copy or partial copy of the
 * software.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright NXP B.V. 2015. All rights reserved
 *
 ****************************************************************************/
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

#define NO_NETWORK_SLEEP_DUR        10   // seconds
#define SECS_TO_TICKS( seconds )	seconds * 32768

#define CONFIG_BUTTON_PIN			13

#define DIO17						17
#define ENABLE_3VLN() 				vAHI_DioSetDirection(0x0,(1 << DIO17)); vAHI_DioSetOutput((1 << DIO17), 0x0);
#define DISABLE_3VLN() 				vAHI_DioSetDirection(0x0,(1 << DIO17)); vAHI_DioSetOutput(0x0, (1 << DIO17));

#define DIO12						12
#define ENABLE_POWERSAVE() 			vAHI_DioSetDirection(0x0,(1 << DIO12)); vAHI_DioSetOutput(0x0, (1 << DIO12));
#define DISABLE_POWERSAVE() 		vAHI_DioSetDirection(0x0,(1 << DIO12)); vAHI_DioSetOutput((1 << DIO12), 0x0);

#define DIO11						11
#define ENABLE_WB() 				vAHI_DioSetDirection(0x0,(1 << DIO11)); vAHI_DioSetOutput(0x0, (1 << DIO11));
#define DISABLE_WB() 				vAHI_DioSetDirection(0x0,(1 << DIO11)); vAHI_DioSetOutput((1 << DIO11), 0x0);

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef struct
{
	networkStates_t		currentState;
	uint64				currentEpid;
	bool				isConnected;
	uint8				ackStrikes;
	uint8				noNwkStrikes;
	uint8				rejoinStrikes;
} networkDesc_t;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void vHandleNetwork(ZPS_tsAfEvent sStackEvent);
PRIVATE frameReturnValues_t vHandleIncomingFrame(ZPS_tsAfEvent sStackEvent);
PRIVATE void sendBroadcast(void);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

PUBLIC pwrm_tsWakeTimerEvent sWake;
PUBLIC uint8 au8DefaultTCLinkKey[16] = "ZigBeeAlliance09";

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE seDeviceDesc_t s_eDevice;
PRIVATE networkDesc_t s_network;

tszQueue APP_msgStrainGaugeEvents;
tszQueue APP_msgZpsEvents;

uint32 timeout;

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
    }

    /* Load default values on startup */
    s_network.currentEpid = 0;
    s_network.isConnected = FALSE;
    s_network.ackStrikes = 0;
    s_network.noNwkStrikes = 0;
    s_network.rejoinStrikes = 0;

    s_eDevice.systemStrikes = 0;
    s_eDevice.isConfigured = FALSE;
    s_eDevice.samplePeriod = DEFAULT_SAMPLE_PERIOD;
    s_eDevice.channelAValue = CHANNEL_A_DEFAULT_VALUE;
    s_eDevice.channelBValue = CHANNEL_B_DEFAULT_VALUE;
    s_eDevice.gainValue = GAIN_DEFAULT_VALUE;
    s_eDevice.sleepTime = DEFAULT_SLEEP_TIME;

    timeout = 0;

    /* Restore any application data previously saved to flash
     * All Application records must be loaded before the call to
     * ZPS_eAplAfInit
     */
    uint16 u16DataBytesRead;

    DBG_vPrintf(TRACE_APP, "APP: Restoring application data from flash\n\r");

    PDM_eReadDataFromRecord
    (
    	PDM_APP_ID_EPID,
        &s_network.currentEpid,
        sizeof(s_network.currentEpid),
        &u16DataBytesRead
    );

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

    /* Initialize ZBPro stack */
    ZPS_eAplAfInit();

    /* Set security keys */
    ZPS_vAplSecSetInitialSecurityState
    (
    	ZPS_ZDO_PRECONFIGURED_LINK_KEY,
        au8DefaultTCLinkKey,
        0x00,
        ZPS_APS_GLOBAL_LINK_KEY
    );

    DBG_vPrintf(TRACE_APP, "APP: Device Information:\n\r");
    DBG_vPrintf(TRACE_APP, "  MAC: 0x%016llx\n\r", ZPS_u64AplZdoGetIeeeAddr());
    DBG_vPrintf(TRACE_APP, "  EPID: 0x%016llx\n\r", s_network.currentEpid);
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

    /* Always start on NETWORK STATE */
    DBG_vPrintf(TRACE_APP, "\n\rAPP: NETWORK_STATE\n\r");
    s_eDevice.currentState = NETWORK_STATE;

    /* Always start on NETWORK STARTUP STATE */
    DBG_vPrintf(TRACE_APP, "  NWK: NWK_STARTUP_STATE\n\r");
    s_network.currentState = NWK_STARTUP_STATE;
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
 ****************************************************************************/
PUBLIC void APP_vtaskSleepingEndDevice()
{
    ZPS_tsAfEvent sStackEvent;
    sStackEvent.eType = ZPS_EVENT_NONE;

    /* State machine watchdog */
    if(s_eDevice.currentState != s_eDevice.previousState)
    {
    	timeout = 0;
    	s_eDevice.previousState = s_eDevice.currentState;
    }
    else
    {
    	//TODO: Change timeout to a timer
    	if (s_eDevice.currentState != SLEEP_STATE) timeout++;
    	if (timeout >= 20000)
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
    }

	if (s_eDevice.systemStrikes >= 5)
	{
		vAHI_SwReset();
	}

    /* Main State Machine */
    switch (s_eDevice.currentState)
    {
        case NETWORK_STATE:
        {
        	/* Check if there is any event on the stack */
			if (ZQ_bQueueReceive(&APP_msgZpsEvents, &sStackEvent))
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: New event on the stack APP_msgZpsEvents = %d\n\r",
					sStackEvent.eType
				);
			}

        	vHandleNetwork(sStackEvent);

        	if(s_network.isConnected)
        	{
        		DBG_vPrintf(TRACE_APP, "APP: Device is connected\n\r");

        		DBG_vPrintf(TRACE_APP, "\n\rAPP: WAIT_CONFIRM_STATE\n\r");
        		s_eDevice.currentState = WAIT_CONFIRM_STATE;
        	}
        }
        break;

        case POLL_DATA_STATE:
        {
        	/* Check if there is any event on the stack */
			if (ZQ_bQueueReceive(&APP_msgZpsEvents, &sStackEvent))
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: New event on the stack APP_msgZpsEvents = %d\n\r",
					sStackEvent.eType
				);
			}

			else if (ZQ_bQueueReceive(&APP_msgStrainGaugeEvents, &sStackEvent))
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: New event on the stack APP_msgStrainGaugeEvents = %d\n\r",
					sStackEvent.eType
				);
			}

        	switch (sStackEvent.eType)
			{
				case ZPS_EVENT_NONE: break;

				/* Poll request completed */
				case ZPS_EVENT_NWK_POLL_CONFIRM:
				{
					uint8 eStatus;
					eStatus = sStackEvent.uEvent.sNwkPollConfirmEvent.u8Status;

					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: ZPS_EVENT_NEW_POLL_COMPLETE, status = %d\n\r",
						eStatus
					);

					/* No new data */
					if(eStatus == MAC_ENUM_NO_DATA)
					{
						DBG_vPrintf(TRACE_APP,"  NWK: No new Data\n\r");

						if(s_eDevice.isConfigured)
						{
							DBG_vPrintf(TRACE_APP, "\n\rAPP: SEND_DATA_STATE\n\r");
							s_eDevice.currentState = SEND_DATA_STATE;
						}
						else
						{
							s_eDevice.currentState = PREP_TO_SLEEP_STATE;
						}
					}

					/* Success */
					else if(eStatus == MAC_ENUM_SUCCESS)
					{
						DBG_vPrintf(TRACE_APP,"  NWK: MAC_ENUM_SUCCESS\n\r");
					}

					/* No acknowledge */
					else if(eStatus == MAC_ENUM_NO_ACK)
					{
						/* add 1 strike */
						s_network.noNwkStrikes++;
						DBG_vPrintf
						(
							TRACE_APP,
							"  NWK: No Acknowledge received, strike = %d\n\r",
							s_network.noNwkStrikes
						);

						/* if 3 strikes node loses connection */
						if(s_network.noNwkStrikes >= 3)
						{
							s_network.noNwkStrikes = 0;
							s_network.isConnected = FALSE;
							DBG_vPrintf(TRACE_APP,"  NWK: Connection lost\n\r");
						}

						s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					}
					else /* unexpected status */
					{
						DBG_vPrintf
						(
							TRACE_APP,
							"  NWK: Unexpected poll complete, status = %d\n\r",
							eStatus
						);
						//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
						//TODO: Hanlde error
					}
				}
				break;

				case ZPS_EVENT_APS_DATA_INDICATION:
				{
					DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_APS_DATA_INDICATION\n");

					/* Process incoming cluster messages for this endpoint... */
					DBG_vPrintf(TRACE_APP, "  Data Indication:\r\n");
					DBG_vPrintf(TRACE_APP, "    Status  :%d\r\n",sStackEvent.uEvent.sApsDataIndEvent.eStatus);
					DBG_vPrintf(TRACE_APP, "    Profile :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId);
					DBG_vPrintf(TRACE_APP, "    Cluster :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId);
					DBG_vPrintf(TRACE_APP, "    EndPoint:%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);
					DBG_vPrintf(TRACE_APP, "    LQI     :%d\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8LinkQuality);

					uint8 status = vHandleIncomingFrame(sStackEvent);

					if (status != FRAME_SUCCESS)
					{
						DBG_vPrintf(TRACE_APP, "  APP: Frame processing error, status = %d\n\r", status);
						s_eDevice.currentState = PREP_TO_SLEEP_STATE;
						//TODO: Handle Frame ERROR
					}
					else
					{
						/* everything OK, now we wait for ZPS_EVENT_APS_DATA_CONFIRM */
						DBG_vPrintf(TRACE_APP, "\n\rAPP: WAIT_CONFIRM_STATE\n\r");
						s_eDevice.currentState = WAIT_CONFIRM_STATE;
					}
				}
				break;

				default:
				{
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: Poll request unhandled event - %d\n\r",
						sStackEvent.eType
					);
					//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Handle error
				}
				break;
			}
        }
        break;

        case WAIT_CONFIRM_STATE:
        {
        	/* Check if there is any event on the stack */
			if (ZQ_bQueueReceive(&APP_msgStrainGaugeEvents, &sStackEvent))
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: New event on the stack APP_msgStrainGaugeEvents = %d\n\r",
					sStackEvent.eType
				);
			}

			switch (sStackEvent.eType)
			{
				case ZPS_EVENT_NONE: break;

				case ZPS_EVENT_APS_DATA_CONFIRM:
				{
					/* Acknowledge data was sent */
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: event ZPS_EVENT_APS_DATA_CONFIRM, status = %d, Address = 0x%04x\n",
						sStackEvent.uEvent.sApsDataConfirmEvent.u8Status,
						sStackEvent.uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr
					);

					s_eDevice.currentState = PREP_TO_SLEEP_STATE;
				}
				break;

				default:
				{
					DBG_vPrintf(TRACE_APP, "  NWK: unhandled event %d\n", sStackEvent.eType);
					//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Handle Error
				}
				break;
			}
        }
        break;

        case SEND_DATA_STATE:
        {
        	static int results[10];
        	int i, j, Imin, temp;
        	uint16 byteCount;
        	PDUM_teStatus status;

        	/* Enable peripherals for sensing */
        	DISABLE_POWERSAVE();
			ENABLE_WB();
			ad8231_init();
			ad8231_enable();
			ad8231_setGain(s_eDevice.gainValue);
			ltc1661_init();
			ltc1661_setDAC_A(s_eDevice.channelAValue);
			ltc1661_setDAC_B(s_eDevice.channelBValue);

			/* Start ADC Conversion */
			MCP3204_init(0);

			for (i = 0; i < 10; i++) results[i] = MCP3204_convert(0, 2);

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

			/* Low power Config */
			ad8231_disable();
			ltc1661_sleep();
			DISABLE_WB();
			ENABLE_POWERSAVE();

			DBG_vPrintf(TRACE_APP, "  APP: sorted Values = ");
			for (i = 0; i < 10; i++) DBG_vPrintf(TRACE_APP,"%d, ", results[i]);
			DBG_vPrintf(TRACE_APP, "\n\r");

			s_eDevice.sensorValue = results[3];
			s_eDevice.sensorValue += results[4];
			s_eDevice.sensorValue += results[5];
			s_eDevice.sensorValue /= 3;

			s_eDevice.temperatureValue = MCP3204_convert(0, 1);
			s_eDevice.batteryLevel = MCP3204_convert(0, 0);

			DBG_vPrintf(TRACE_APP, "  APP: sensorValue = %d\n\r", s_eDevice.sensorValue);
			DBG_vPrintf(TRACE_APP, "  APP: temperatureValue = %d\n\r", s_eDevice.temperatureValue);
			DBG_vPrintf(TRACE_APP, "  APP: batteryValue = %d\n\r", s_eDevice.batteryLevel);

			DBG_vPrintf(TRACE_APP, "  APP: Sending data to Coordinator\n\r");
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
				/* Load payload data into APDU */
				byteCount = PDUM_u16APduInstanceWriteNBO
				(
					data,	// APDU instance handle
					0,		// APDU position for data
					"bhhh",	// data format string
					'*',
					s_eDevice.sensorValue,
					s_eDevice.temperatureValue,
					s_eDevice.batteryLevel
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
						//TODO: Add strike count and handle error
					}

					/* everything OK, now we wait for ZPS_EVENT_APS_DATA_CONFIRM */
					DBG_vPrintf(TRACE_APP, "\n\rAPP: WAIT_CONFIRM_STATE\n\r");
					s_eDevice.currentState = WAIT_CONFIRM_STATE;
					break;
				}
			}
        	//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
        }
        break;

        case PREP_TO_SLEEP_STATE:
        {
        	DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");

			if(s_eDevice.isConfigured && s_network.isConnected)
				s_eDevice.sleepTime = s_eDevice.samplePeriod;
			else s_eDevice.sleepTime = DEFAULT_SLEEP_TIME;

        	DBG_vPrintf
			(
				TRACE_APP,
				"APP: Sleep for %d seconds\n\r",
				s_eDevice.sleepTime
			);

        	/* Set wakeup time */
        	PWRM_eScheduleActivity
			(
				&sWake,
				SECS_TO_TICKS(s_eDevice.sleepTime),
				vWakeCallBack
			);

        	s_eDevice.currentState = SLEEP_STATE;
        }
        break;

        case SLEEP_STATE:
		{
			/* Waits until OS sends the device to sleep */
		}
		break;

        case WAKE_UP_STATE:
		{
			if(s_network.isConnected)
			{
				/* Poll data from Stack */
				ZPS_eAplZdoPoll();

				DBG_vPrintf(TRACE_APP, "\n\rAPP: POLL_DATA_STATE\n\r");
				s_eDevice.currentState = POLL_DATA_STATE;
			}
			else
			{
				DBG_vPrintf(TRACE_APP, "\n\rAPP: NETWORK_STATE\n\r");
				s_eDevice.currentState = NETWORK_STATE;

				DBG_vPrintf(TRACE_APP, "  NWK: NWK_STARTUP_STATE\n\r");
				s_network.currentState = NWK_STARTUP_STATE;
			}
		}
		break;

        default:
        {
        	//TODO: Handle error
            DBG_vPrintf
            (
            	TRACE_APP,
            	"APP: Unhandled State : %d\n\r",
            	s_eDevice.currentState
            );
        }
        break;
    }
}

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
PRIVATE void vHandleNetwork(ZPS_tsAfEvent sStackEvent)
{
	ZPS_teStatus eStatus;

	switch(s_network.currentState)
	{
		case NWK_STARTUP_STATE:
		{
		    /* If network parameters were restored, Rejoin */
		    if(s_network.currentEpid)
		    {
		    	DBG_vPrintf
		    	(
		    		TRACE_APP,
		    		"  NWK: Trying to rejoin network 0x%016llx\n\r",
		    		s_network.currentEpid
		    	);

		    	ZPS_eAplAibSetApsUseExtendedPanId(s_network.currentEpid);

		    	/* Rejoin stored network without a discovery process */
		    	eStatus = ZPS_eAplZdoRejoinNetwork(FALSE);

		    	if (ZPS_E_SUCCESS != eStatus)
				{
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: Failed rejoin request, status = %d\n\r",
						eStatus
					);
					//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Handle errors
				}
		    	else
		    	{
		    		DBG_vPrintf(TRACE_APP, "\n\r  NWK: NWK_REJOIN_STATE\n\r");
		    		s_network.currentState = NWK_REJOIN_STATE;
		    	}
		    }
		    else /* Discovery */
		    {
				/* Reset nwk params */
				void * pvNwk = ZPS_pvAplZdoGetNwkHandle();
				//ZPS_vNwkNibSetPanId(pvNwk, 0);
				ZPS_vNwkNibSetExtPanId(pvNwk, 0);
				ZPS_eAplAibSetApsUseExtendedPanId(0);

		    	/* Start the network stack as a end device */
				DBG_vPrintf(TRACE_APP, "  NWK: Starting ZPS\n\r");
				eStatus = ZPS_eAplZdoStartStack();

				if (ZPS_E_SUCCESS != eStatus)
				{
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: Failed to Start Stack, status = %d\n\r",
						eStatus
					);
					//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Handle error
				}
				else
				{
					DBG_vPrintf(TRACE_APP, "\n\r  NWK: NWK_DISC_STATE\n\r");
					s_network.currentState = NWK_DISC_STATE;
				}
		    }
		}
		break;

		case NWK_DISC_STATE:
	    {
	    	switch (sStackEvent.eType)
	    	{
	    		case ZPS_EVENT_NONE: break;

	    		/* Discovery process complete */
				case ZPS_EVENT_NWK_DISCOVERY_COMPLETE:
				{
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: Network discovery complete\n\r"
					);

					/* If there is any error in the discovery process stops */
					if(sStackEvent.uEvent.sNwkDiscoveryEvent.eStatus != MAC_ENUM_SUCCESS)
					{
						DBG_vPrintf
						(
							TRACE_APP,
							"  NWK: Network discovery failed, status = %d\n\r",
							sStackEvent.uEvent.sNwkDiscoveryEvent.eStatus
						);
						//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
						//TODO: Handle error
					}
					else /* Discovery process successful */
					{
						/* If no network is found stops */
						if(sStackEvent.uEvent.sNwkDiscoveryEvent.u8NetworkCount == 0)
						{
							DBG_vPrintf
							(
								TRACE_APP,
								"  NWK: No network found\n\r"
							);
						}
						else /* Networks found */
						{
							DBG_vPrintf
							(
								TRACE_APP,
								"  NWK: Found %d networks\n\r",
								sStackEvent.uEvent.sNwkDiscoveryEvent.u8NetworkCount
							);

							/* Get index of recommended network to join */
							uint8 networkIndex = sStackEvent.uEvent.sNwkDiscoveryEvent.u8SelectedNetwork;

							/* Create network descriptor */
							ZPS_tsNwkNetworkDescr *psNwkDescr = &sStackEvent.uEvent.sNwkDiscoveryEvent.psNwkDescriptors[networkIndex];

							/* Join request */
							eStatus = ZPS_eAplZdoJoinNetwork(psNwkDescr);
							if (eStatus == ZPS_E_SUCCESS)
							{
								DBG_vPrintf(TRACE_APP, "  NWK: Joining network\n\r");
								DBG_vPrintf(TRACE_APP, "  NWK: Ext PAN ID = 0x%016llx\n", psNwkDescr->u64ExtPanId);
								DBG_vPrintf(TRACE_APP, "\n\r  NWK: NWK_JOIN_STATE\n\r");
								s_network.currentState = NWK_JOIN_STATE;
							}
							else if (eStatus == ZPS_NWK_ENUM_NOT_PERMITTED)
							{
								DBG_vPrintf
								(
									TRACE_APP,
									"  NWK: Join not permitted, status = %d\n\r",
									eStatus
								);
								s_eDevice.currentState = PREP_TO_SLEEP_STATE;
							}
							else if (eStatus == ZPS_NWK_ENUM_INVALID_REQUEST)
							{
								vAHI_SwReset();
							}
							else
							{
								DBG_vPrintf
								(
									TRACE_APP,
									"  NWK: Failed to request network join, status = %d\n\r",
									eStatus
								);
								//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
								//TODO: Handle ERROR
							}
						}
					}
				}
				break;

				/* Unwanted rejoin */
				case ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE:
				{
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: Node rejoined network with Address 0x%04x\n",
						sStackEvent.uEvent.sNwkJoinedEvent.u16Addr
					);

					s_network.currentEpid = ZPS_u64NwkNibGetEpid(ZPS_pvAplZdoGetNwkHandle());

					PDM_eSaveRecordData
					(
						PDM_APP_ID_EPID,
						&s_network.currentEpid,
						sizeof(s_network.currentEpid)
					);

					//TODO: Request AUTH and go to AUTH State
					s_network.isConnected = TRUE;

					sendBroadcast();
				}
				break;

				default:
				{
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: Discovery unexpected event - %d\n\r",
						sStackEvent.eType
					);
					// s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Handle error
				}
				break;
	    	}
	    }
	    break;

		case NWK_JOIN_STATE:
		{
			/* If there is no event breaks */
			if(sStackEvent.eType == ZPS_EVENT_NONE) break;

			/* Node joined as end device */
			else if(sStackEvent.eType == ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE)
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Node joined network with Address 0x%04x\n",
				    sStackEvent.uEvent.sNwkJoinedEvent.u16Addr
				);

				s_network.currentEpid = ZPS_u64NwkNibGetEpid(ZPS_pvAplZdoGetNwkHandle());

				PDM_eSaveRecordData
				(
					PDM_APP_ID_EPID,
				    &s_network.currentEpid,
				    sizeof(s_network.currentEpid)
				);

				//TODO: Request AUTH and go to AUTH State
				s_network.rejoinStrikes = 0;
				s_network.isConnected = TRUE;

				sendBroadcast();
			}

			/* Node failed to join */
			else if(sStackEvent.eType == ZPS_EVENT_NWK_FAILED_TO_JOIN)
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Node failed to join network, status = %d\n\r",
					sStackEvent.uEvent.sNwkJoinFailedEvent.u8Status
				);
				//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
				//TODO: Handle error
			}
			else /* Unexpected event */
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Join unexpected event - %d\n\r",
					sStackEvent.eType
				);
				//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
				//TODO: Handle error
			}
		}
		break;

		case NWK_AUTH_STATE:
		{
			//TODO: all of this
			DBG_vPrintf(TRACE_APP,"  NWK: AUTH State\n\r");
		}
		break;

		case NWK_LEAVE_STATE:
		{

		}
		break;

		case NWK_REJOIN_STATE:
		{
			/* If there is no event breaks */
			if(sStackEvent.eType == ZPS_EVENT_NONE) break;

			/* Node rejoined as end device */
			else if(sStackEvent.eType == ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE)
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Node rejoined network with Address 0x%04x\n",
					sStackEvent.uEvent.sNwkJoinedEvent.u16Addr
				);

				/* Device reconnected successfully */
				s_network.rejoinStrikes = 0;
				s_network.isConnected = TRUE;

				sendBroadcast();
			}

			/* Node failed rejoin */
			else if(sStackEvent.eType == ZPS_EVENT_NWK_FAILED_TO_JOIN)
			{
				uint8 eStatus;
				eStatus = sStackEvent.uEvent.sNwkJoinFailedEvent.u8Status;

				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Node failed to rejoin network, status = %d\n\r",
					eStatus
				);
				/* Can't find saved network */
				if(eStatus == ZPS_NWK_ENUM_NO_NETWORKS)
				{
					s_network.rejoinStrikes++;
					DBG_vPrintf
					(
						TRACE_APP,
						"  NWK: Can't find network, strike = %d\n\r",
						s_network.rejoinStrikes
					);

					if(s_network.rejoinStrikes >= 5)
					{
						DBG_vPrintf(TRACE_APP, "  NWK: Deleting network parameters..\n\r");
						/* Reset nwk params */
						void * pvNwk = ZPS_pvAplZdoGetNwkHandle();
						//ZPS_vNwkNibSetPanId(pvNwk, 0);
						ZPS_vNwkNibSetExtPanId(pvNwk, 0);
						ZPS_eAplAibSetApsUseExtendedPanId(0);

						s_network.currentEpid = 0;
						PDM_vDeleteDataRecord(PDM_APP_ID_EPID);

						s_network.rejoinStrikes = 0;
					}
				}
				s_eDevice.currentState = PREP_TO_SLEEP_STATE;
				//TODO: Handle error
			}
			else /* Unexpected event */
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Rejoin unexpected event - %d\n\r",
					sStackEvent.eType
				);
				//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
				//TODO: Handle error
			}
		}
		break;

		default:
		{
			DBG_vPrintf
			(
				TRACE_APP,
				"  NWK: Unhandled State : %d\n",
				s_network.currentState
			);
			//s_eDevice.currentState = PREP_TO_SLEEP_STATE;
			//TODO: Handle error
		}
		break;
	}
}

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
			data,	// APDU instance handle
			0,		// APDU position for data
			"b",	// data format string
			'&'
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

			/* everything OK, now we wait for ZPS_EVENT_APS_DATA_CONFIRM */
		}
	}
}

/****************************************************************************
 *
 * NAME: APP_vGenCallback
 *
 * DESCRIPTION:
 * Stack callback
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void APP_vGenCallback(uint8 u8Endpoint, ZPS_tsAfEvent *psStackEvent)
{
    if ( u8Endpoint == 0 )
    {
    	ZQ_bQueueSend(&APP_msgZpsEvents, (void*) psStackEvent);
    }
    else
    {
    	ZQ_bQueueSend(&APP_msgStrainGaugeEvents, (void*) psStackEvent);
    }
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/


/* TODO: errores registrados
 *
 * Despues de reprogamar el concentrador, todos los nodos previamente asociados "pierden" conexion. Despues de 3 intentos
 * el nodo "deja" la red y no puede volver a conectarse con rejoin. Hay que ver que pasa si se elimina el EPID.
 * Una teoria es que el feature de seguridad "frame counter" es el que rechaza mensajes del nodo.
 *
 **********************************************************************************************************************************
 *
 * Despues de ingresar a una red se genera uno o mas eventos ZPS_EVENT_NWK_POLL_CONFIRM con supuestamente data valida (MAC_ENUM_SUCCESS).
 * pero no se genera el evento ZPS_EVENT_APS_DATA_INDICATION, por lo cual no hay data que procesar en el endpoint. Por otro lado,
 * cuando un nodo ingresa a la red, el coordinador siempre arroja la siguiente secuencia:
 *
 * APP: No event to process
 * APP: vCheckStackEvent: vCheckStackEvent: ZPS_EVENT_NEW_NODE_HAS_JOINED, Nwk Addr=0x542b
 * APP: No event to process
 * APP: vCheckStackEvent: ZPS_EVENT_AF_DATA_INDICATION
 *        Profile :0
 *        Cluster :13
 *        EndPoint:0
 * APP: No event to process
 * APP: vCheckStackEvent: ZPS_EVENT_ROUTE_DISCOVERY_CFM
 *
 **********************************************************************************************************************************
 *
 * Cuando se realiza un pareamiento masivo, algunos nodos logran conectarse a la red, pero no enviar el comando broadcast.
 * De estos ultimos, algunos se recuperan y envian el comando broadcast despues de un reset, los otros aparecen como mensaje
 * de que se han unido a la red (puerta dbg coordinador) pero en la puerta dbg del nodo aparece:
 *
 * *** WAKE UP ROUTINE ***
 * APP: WAKE_UP_STATE
 *
 * APP: NETWORK_STATE
 *   NWK: NWK_STARTUP_STATE
 *   NWK: Starting ZPS
 *   NWK: Failed to Start Stack, status = 139
 *
 *   NWK: NWK_DISC_STATE
 *
 * APP: PREP_TO_SLEEP_STATE
 * APP: Sleep for 5 seconds
 *
 * el cual implica: ZPS_APL_ZDP_E_NOT_PERMITTED (0x8B) The device is not in the proper state to support the requested operation.
 * Al realizar un reset, se obtiene:
 *
 * APP: Startup
 * APP: Restoring application data from flash
 * APP: Device Information:
 *   MAC: 0x00158d0002a0c881
 *   EPID: 0x0000000000000000
 *   Sample Period: 10
 *   Configured Flag: 0
 *   Channel A: 2048
 *   Channel B: 2048
 *   Gain: 32
 *
 * APP: NETWORK_STATE
 *   NWK: NWK_STARTUP_STATE
 *   NWK: Starting ZPS
 *
 *   NWK: NWK_DISC_STATE
 *   NWK: New event on the stack APP_msgZpsEvents = 6
 *   NWK: Discovery unexpected event - 6
 *
 * Despues de iniciar un DISCOVERY, se genera evento ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE.
 * Lo cual corresponde a un reingreso a la red, pero el dispositivo no fue capaz de guardar la EPID en flash
 *
 * Cuando el nodo intenta conectarse por primera vez, presenta el siguiente texto:
 *
 *   NWK: NWK_JOIN_STATE
 *   NWK: New event on the stack APP_msgZpsEvents = 15
 *   NWK: Join unexpected event - 15
 *
 *   15 = ZPS_EVENT_NWK_POLL_CONFIRM
 *
 * Posterior a esto, en los siguientes WAKEUP el dispositivo no es capaz de iniciar el Stack:
 *
 *   NWK: NWK_STARTUP_STATE
 *   NWK: New event on the stack APP_msgZpsEvents = 15
 *   NWK: Starting ZPS
 *   NWK: Failed to Start Stack, status = 139
 *
 *   status 139 = ZPS_APL_ZDP_E_NOT_PERMITTED (0x8B)
 *   event 15 = ZPS_EVENT_NWK_POLL_CONFIRM (0x0F)
 *
 * Mas info respecto a lo anterior:
 *
 * *** WAKE UP ROUTINE ***
APP: WAKE_UP_STATE

APP: NETWORK_STATE
  NWK: NWK_STARTUP_STATE
  NWK: Starting ZPS

  NWK: NWK_DISC_STATE
  NWK: New event on the stack APP_msgZpsEvents = 10
  NWK: Network discovery complete
  NWK: Found 1 networks
  NWK: Joining network
  NWK: Ext PAN ID = 0x00158d0001ba807a

  NWK: NWK_JOIN_STATE
  NWK: New event on the stack APP_msgZpsEvents = 8
  NWK: Node failed to join network, status = 235
APP: State machine timed out, strike = 1

APP: PREP_TO_SLEEP_STATE
APP: Sleep for 5 seconds


*** WAKE UP ROUTINE ***
APP: WAKE_UP_STATE

APP: NETWORK_STATE
  NWK: NWK_STARTUP_STATE
  NWK: Starting ZPS

  NWK: NWK_DISC_STATE
  NWK: New event on the stack APP_msgZpsEvents = 10
  NWK: Network discovery complete
  NWK: Network discovery failed, status = 234
APP: State machine timed out, strike = 2

APP: PREP_TO_SLEEP_STATE
APP: Sleep for 5 seconds
 *
 * status 234 = MAC_ENUM_NO_BEACON (0xEA)   Scan  failed to find any beacons
 * status 235 = MAC_ENUM_NO_DATA (0xEB)   No response data after a data request
 *
 * por otro lado, el coordinador responde lo siguiente:
 *
 * APP: vCheckStackEvent: ZPS_EVENT_NWK_STATUS_INDICATION
    Address: 0xf8f0
    Status : 17
 *
 **********************************************************************************************************************************
 *
 *
 *
 *
 */
