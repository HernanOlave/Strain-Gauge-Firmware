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

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void vHandleNetwork(ZPS_tsAfEvent sStackEvent);
PUBLIC uint8 au8DefaultTCLinkKey[16] = "ZigBeeAlliance09";

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE seDeviceDesc_t s_eDevice;
PRIVATE networkStates_t networkState;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/


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
    bool_t bDeleteRecords = FALSE;

    /* Delete the network context from flash if a button is being held down
     * at reset
     */
    //TODO: Implement this feature
    if (bDeleteRecords)
    {
        DBG_vPrintf(TRACE_APP, "APP: Deleting all records from flash\n");
        PDM_vDeleteAllDataRecords();
    }

    /* Load default values on startup */
    s_eDevice.currentEpid = 0;
    s_eDevice.isConfigured = FALSE;
    s_eDevice.isConnected = FALSE;
    s_eDevice.channelAValue = CHANNEL_A_DEFAULT_VALUE;
    s_eDevice.channelBValue = CHANNEL_B_DEFAULT_VALUE;
    s_eDevice.gainValue = GAIN_DEFAULT_VALUE;

    /* Restore any application data previously saved to flash
     * All Application records must be loaded before the call to
     * ZPS_eAplAfInit
     */
    uint16 u16DataBytesRead;

    DBG_vPrintf(TRACE_APP, "APP: Restoring application data from flash\n\r");

    PDM_eReadDataFromRecord
    (
    	PDM_APP_ID_EPID,
        &s_eDevice.currentEpid,
        sizeof(s_eDevice.currentEpid),
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

    DBG_vPrintf(TRACE_APP, "APP: EPID: %d", s_eDevice.currentEpid);
    DBG_vPrintf(TRACE_APP, "APP: CONFIGURED: %d", s_eDevice.isConfigured);
    DBG_vPrintf(TRACE_APP, "APP: CHANNEL_A: %d", s_eDevice.channelAValue);
    DBG_vPrintf(TRACE_APP, "APP: CHANNEL_B: %d", s_eDevice.channelBValue);
    DBG_vPrintf(TRACE_APP, "APP: GAIN: %d", s_eDevice.gainValue);

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

    /* Initialize other software modules
     * HERE
     */

    /* Always initialize any peripherals used by the application
     * HERE
     */

    /* Always start on NETWORK STATE */
    s_eDevice.currentState = NETWORK_STATE;

    /* Always start on NETWORK STARTUP STATE */
    networkState = NWK_STARTUP_STATE;
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

        	if(s_eDevice.isConnected)
        	{
        		DBG_vPrintf(TRACE_APP, "APP: Device is connected\r\n");
        		s_eDevice.currentState = POLL_DATA_STATE;
        	}
        	else
        	{
        		DBG_vPrintf(TRACE_APP, "APP: Device is NOT connected\r\n");
        		s_eDevice.currentState = PREP_TO_SLEEP_STATE;
        	}
        }
        break;

        case POLL_DATA_STATE:
        {

        }
        break;

        case HANDLE_DATA_STATE:
        {

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

        }
        break;

        case WAKE_UP_STATE:
		{

		}
		break;

        default:
        {
        	//TODO: Handle error
            DBG_vPrintf
            (
            	TRACE_APP,
            	"APP: Unhandled State : %d\n",
            	s_eDeviceState.eNodeState
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
	switch(networkState)
	{
		case NWK_STARTUP_STATE:
		{
		    /* Start the network stack as a end device */
		    DBG_vPrintf(TRACE_APP, "NWK: Starting ZPS\n");
		    ZPS_teStatus eStatus = ZPS_eAplZdoStartStack();

		    if (ZPS_E_SUCCESS != eStatus)
		    {
		    	DBG_vPrintf
		    	(
		    		TRACE_APP,
		    		"NWK: Failed to Start Stack. Status: 0x%02x\n",
		    		eStatus
		    	);
		    	//TODO: Handle errors
		    	s_eDevice.isConnected = FALSE;
		    	return;
		    }

		    /* If network parameters were restored, Rejoin */
		    if(s_eDevice.currentEpid)
		    {
		    	DBG_vPrintf(TRACE_APP, "NWK: NWK_REJOIN_STATE\n\r");
		    	networkState = NWK_REJOIN_STATE;
		    }
		    else /* Discovery */
		    {
		    	DBG_vPrintf(TRACE_APP, "NWK: NWK_DISC_STATE\n\r");
		    	networkState = NWK_DISC_STATE;
		    }
		}
		break;

		case NWK_DISC_STATE:
	    {

	    }
	    break;

		case NWK_JOIN_STATE:
		{

		}
		break;

		case NWK_AUTH_STATE:
		{

		}
		break;

		case NWK_LEAVE_STATE:
		{

		}
		break;

		case NWK_REJOIN_STATE:
		{

		}
		break;

		default:
		{
			//TODO: Handle error
			DBG_vPrintf
			(
				TRACE_APP,
				"NWK: Unhandled State : %d\n",
				networkState
			);
		}
		break;
	}
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
