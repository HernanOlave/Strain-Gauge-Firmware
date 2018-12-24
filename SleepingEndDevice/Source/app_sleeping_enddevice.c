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
} networkDesc_t;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void vHandleNetwork(ZPS_tsAfEvent sStackEvent);
PRIVATE void vHandleIncomingFrame(ZPS_tsAfEvent sStackEvent);

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

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void vWakeCallBack(void)
{
	DBG_vPrintf(TRACE_APP, "\n\r*** WAKE UP ROUTINE ***\n\r");
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

    bool_t bDeleteRecords = FALSE;

    /* Delete the network context from flash if a button is being held down
     * at reset
     */
    //TODO: Implement this feature
    if (bDeleteRecords)
    {
        DBG_vPrintf(TRACE_APP, "APP: Deleting all records from flash\n\r");
        PDM_vDeleteAllDataRecords();
    }

    /* Load default values on startup */
    s_network.currentEpid = 0;
    s_network.isConnected = FALSE;
    s_network.ackStrikes = 0;
    s_network.noNwkStrikes = 0;

    s_eDevice.isConfigured = FALSE;
    s_eDevice.samplePeriod = DEFAULT_SLEEP_TIME; //TODO: Create macro for this
    s_eDevice.channelAValue = CHANNEL_A_DEFAULT_VALUE;
    s_eDevice.channelBValue = CHANNEL_B_DEFAULT_VALUE;
    s_eDevice.gainValue = GAIN_DEFAULT_VALUE;
    s_eDevice.sleepTime = DEFAULT_SLEEP_TIME;

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
    DBG_vPrintf(TRACE_APP, "  Configured Flag: %d\n\r", s_eDevice.isConfigured);
    DBG_vPrintf(TRACE_APP, "  Channel A: %d\n\r", s_eDevice.channelAValue);
    DBG_vPrintf(TRACE_APP, "  Channel B: %d\n\r", s_eDevice.channelBValue);
    DBG_vPrintf(TRACE_APP, "  Gain: %d\n\r", s_eDevice.gainValue);

    /* Initialize other software modules
     * HERE
     */

    /* Always initialize any peripherals used by the application
     * HERE
     */

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

    /* Check if there is any event on the stack */
    ZQ_bQueueReceive(&APP_msgZpsEvents, &sStackEvent);

    switch (s_eDevice.currentState)
    {
        case NETWORK_STATE:
        {
        	vHandleNetwork(sStackEvent);

        	if(s_network.isConnected)
        	{
        		DBG_vPrintf(TRACE_APP, "APP: Device is connected\n\r");

        		/* Poll data from Stack */
        		ZPS_eAplZdoPoll();

        		DBG_vPrintf(TRACE_APP, "\n\rAPP: POLL_DATA_STATE\n\r");
        		s_eDevice.currentState = POLL_DATA_STATE;
        	}
        }
        break;

        case POLL_DATA_STATE:
        {
        	/* If there is no event breaks */
        	if(sStackEvent.eType == ZPS_EVENT_NONE) break;

        	/* Poll request completed */
        	else if(sStackEvent.eType == ZPS_EVENT_NWK_POLL_CONFIRM)
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
					s_eDevice.currentState = PREP_TO_SLEEP_STATE;
				}
				/* New Data */
				else if(eStatus == MAC_ENUM_SUCCESS)
				{
					DBG_vPrintf(TRACE_APP,"  NWK: New Data\n\r");

					DBG_vPrintf(TRACE_APP, "\n\rAPP: HANDLE_DATA_STATE\n\r");
					s_eDevice.currentState = HANDLE_DATA_STATE;
				}
				else if(eStatus == MAC_ENUM_NO_ACK) /* No acknowledge */
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
					s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Hanlde error
				}
			}
        	else /* unexpected event */
        	{
        		DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Poll request unexpected event - %d\n\r",
					sStackEvent.eType
				);
        		s_eDevice.currentState = PREP_TO_SLEEP_STATE;
        		//TODO: Handle error
        	}

			break;

        }
        break;

        case HANDLE_DATA_STATE:
        {
        	//TODO: Handle data
        	/* check if any messages to collect */
			if ( ZQ_bQueueReceive(&APP_msgStrainGaugeEvents, &sStackEvent))
			{
				DBG_vPrintf(TRACE_APP, "APP: New event in the stack\n\r");
			}

			switch (sStackEvent.eType)
			{
				case ZPS_EVENT_NONE:
				break;
				case ZPS_EVENT_APS_INTERPAN_DATA_INDICATION:
				{
					 DBG_vPrintf(TRACE_APP, "  NWK: event ZPS_EVENT_APS_INTERPAN_DATA_INDICATION\n");
					 PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsInterPanDataIndEvent.hAPduInst);
				}
				break;

				case ZPS_EVENT_APS_ZGP_DATA_INDICATION:
				{
					DBG_vPrintf(TRACE_APP, "  NWK: event ZPS_EVENT_APS_ZGP_DATA_INDICATION\n");
					if (sStackEvent.uEvent.sApsZgpDataIndEvent.hAPduInst)
					{
						PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsZgpDataIndEvent.hAPduInst);
					}
				}
				break;

				case ZPS_EVENT_APS_DATA_INDICATION:
				{
					DBG_vPrintf(TRACE_APP, "  NWK: APP_taskEndPoint: ZPS_EVENT_AF_DATA_INDICATION\n");

					/* Process incoming cluster messages for this endpoint... */
					DBG_vPrintf(TRACE_APP, "  Data Indication:\r\n");
					DBG_vPrintf(TRACE_APP, "    Status  :%d\r\n",sStackEvent.uEvent.sApsDataIndEvent.eStatus);
					DBG_vPrintf(TRACE_APP, "    Profile :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId);
					DBG_vPrintf(TRACE_APP, "    Cluster :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId);
					DBG_vPrintf(TRACE_APP, "    EndPoint:%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);
					DBG_vPrintf(TRACE_APP, "    LQI     :%d\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8LinkQuality);

					vHandleIncomingFrame(sStackEvent);

					/* free the application protocol data unit (APDU) once it has been dealt with */
					PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);
				}
				break;

				case ZPS_EVENT_APS_DATA_CONFIRM:
				{
					DBG_vPrintf(TRACE_APP, "  NWK: APP_taskEndPoint: ZPS_EVENT_APS_DATA_CONFIRM Status %d, Address 0x%04x\n",
								sStackEvent.uEvent.sApsDataConfirmEvent.u8Status,
								sStackEvent.uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr);
				}
				break;

				case ZPS_EVENT_APS_DATA_ACK:
				{
					DBG_vPrintf(TRACE_APP, "  NWK: APP_taskEndPoint: ZPS_EVENT_APS_DATA_ACK Status %d, Address 0x%04x\n",
								sStackEvent.uEvent.sApsDataAckEvent.u8Status,
								sStackEvent.uEvent.sApsDataAckEvent.u16DstAddr);
				}
				break;

				default:
				{
					DBG_vPrintf(TRACE_APP, "  NWK: APP_taskEndPoint: unhandled event %d\n", sStackEvent.eType);
				}
				break;
			}
			//TODO: Change this
			s_eDevice.currentState = PREP_TO_SLEEP_STATE;
        }
        break;

        case READ_SENSOR_STATE:
        {

        }
        break;

        case SEND_DATA_STATE:
        {

        }
        break;

        case PREP_TO_SLEEP_STATE:
        {
        	DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");

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
					s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Handle errors
				}

		    	DBG_vPrintf(TRACE_APP, "\n\r  NWK: NWK_REJOIN_STATE\n\r");
		    	s_network.currentState = NWK_REJOIN_STATE;
		    }
		    else /* Discovery */
		    {
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
					s_eDevice.currentState = PREP_TO_SLEEP_STATE;
					//TODO: Handle error
				}

		    	DBG_vPrintf(TRACE_APP, "\n\r  NWK: NWK_DISC_STATE\n\r");
		    	s_network.currentState = NWK_DISC_STATE;
		    }
		}
		break;

		case NWK_DISC_STATE:
	    {
	    	/* If there is no event breaks */
	    	if(sStackEvent.eType == ZPS_EVENT_NONE) break;

	    	/* Discovery process complete */
	    	else if(sStackEvent.eType == ZPS_EVENT_NWK_DISCOVERY_COMPLETE)
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
	    			s_eDevice.currentState = PREP_TO_SLEEP_STATE;
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
						else
						{
							DBG_vPrintf
							(
								TRACE_APP,
								"  NWK: Failed to request network join, status = %d\n\r",
								eStatus
							);
							s_eDevice.currentState = PREP_TO_SLEEP_STATE;
							//TODO: Handle ERROR
						}
	    			}
	    		}

	    	}
	    	else
	    	{
	    		DBG_vPrintf
	    		(
	    			TRACE_APP,
	    			"  NWK: Discovery unexpected event - %d\n\r",
	    			sStackEvent.eType
	    		);
	    		s_eDevice.currentState = PREP_TO_SLEEP_STATE;
	    		//TODO: Handle error
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
				s_network.isConnected = TRUE;
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
				s_eDevice.currentState = PREP_TO_SLEEP_STATE;
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
				s_eDevice.currentState = PREP_TO_SLEEP_STATE;
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
				s_network.isConnected = TRUE;
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
					DBG_vPrintf(TRACE_APP,"  NWK: Can't find network\n\r");
					//TODO: after X strikes, delete network parameters
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
				s_eDevice.currentState = PREP_TO_SLEEP_STATE;
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
			s_eDevice.currentState = PREP_TO_SLEEP_STATE;
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
PRIVATE void vHandleIncomingFrame(ZPS_tsAfEvent sStackEvent)
{
	uint8 idByte = 0;
	uint16 byteCount;

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
		DBG_vPrintf(TRACE_APP, "  APP: Frame error, size = 0\n\r");
		return;
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

			if(byteCount == 8)
			{
				s_eDevice.samplePeriod = values.inSamplePeriod;
				channelAValue = values.inChannelAValue;
				channelBValue = values.inChannelBValue;
				gainValue = values.inGainValue;

				DBG_vPrintf(TRACE_APP, "        samplePeriod = 0x%04x\n", samplePeriod);
				DBG_vPrintf(TRACE_APP, "        channelA = 0x%04x\n", channelAValue);
				DBG_vPrintf(TRACE_APP, "        channelB = 0x%04x\n", channelBValue);
				DBG_vPrintf(TRACE_APP, "        gainValue = 0x%04x\n", gainValue);
			}

		}
		break;

		/* GO command */
		case '$':
		{
			DBG_vPrintf(TRACE_APP, "  APP: GO command frame\n\r");
		}
		break;

		/* Broadcast request */
		case '&':
		{
			DBG_vPrintf(TRACE_APP, "  APP: Broadcast request frame\n\r");
		}
		break;
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
 * Despues de 3 poll de data y no recibir ack, el nodo "deja" la red y se queda pegado en POLL_DATA_STATE
 * con un reset, intenta reingresar a la red pero se obtiene error "NWK: Node failed to rejoin network. Status: 235"
 * con otro reset el nodo logra conectarse a traves de un rejoin.
 *
 * Despues de reprogamar el concentrador, todos los nodos previamente asociados "pierden" conexion. Despues de 3 intentos
 * el nodo "deja" la red y no puede volver a conectarse con rejoin. Hay que ver que pasa si se elimina el EPID.
 * Una teoria es que el feature de seguridad "frame counter" es el que rechaza mensajes del nodo.
 *
 * Despues de ingresar a una red se genera un POLL event con supuestamente data valida. El handler de Data no detecta ningun
 * evento en la version actual, hace 2 commits aprox arrojaba evento 15 equivalente a ZPS_EVENT_NWK_POLL_CONFIRM. Por otro lado,
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
 *
 *
 *
 */
