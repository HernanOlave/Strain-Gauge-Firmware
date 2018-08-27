/*****************************************************************************
 *
 * MODULE:				JN-AN-1184 ZigBeePro Application Template
 *
 * COMPONENT:			app_endpoint.c
 *
 * DESCRIPTION:			End Point Event Handler
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
#include <jendefs.h>
#include <dbg.h>
#include <pdm.h>
#include <pwrm.h>
#include <zps_apl_af.h>
#include "app_common.h"
#include "pdum_gen.h"
#include "pdm_app_ids.h"
#include "mcp3204.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define AUTH_TIMEOUT    6       // seconds

#ifndef DEBUG_APP
	#define TRACE_APP 	FALSE
#else
	#define TRACE_APP 	TRUE
#endif

#define NO_NETWORK_SLEEP_DUR        10  // seconds
#define CONFIG_STATE_SLEEP_DUR      4   // seconds
#define SAMPLE_RATE_DEFAULT         1   // seconds

#define SECS_TO_TICKS( seconds )		seconds * 32768

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

// stack message handling functions for this endpoint
PRIVATE void vRunning           ();
PRIVATE void vWaitForAuthCode   ();

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
extern tsDeviceDesc s_eDeviceState;
extern pwrm_tsWakeTimerEvent sWake;

extern bool_t configPressed_ep;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
PUBLIC uint8 authTimer;

tszQueue APP_msgStrainGaugeEvents;
tszQueue APP_msgZpsEvents;

bool_t configured = TRUE; //TODO Change to TRUE after testing
bool_t wakeup = TRUE;

uint16 samplePeriod = SAMPLE_RATE_DEFAULT;

enum {
	EP_STATE_INIT,
	EP_STATE_WAIT_NETWORK,
	EP_STATE_AUTH,
	EP_STATE_CONFIG,
	EP_STATE_RUNNING,
} endPointState = EP_STATE_INIT;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
extern void vWakeCallBack();
extern void AuthNetwork();
extern void BlacklistNetwork();
extern void LeaveNetwork();

void AuthTimerCallback( void* params )
{
    // auth code response TIMEOUT
    DBG_vPrintf(TRACE_APP, "AUTH: AUTH Code NOT Received\n");
    ZTIMER_eClose( authTimer );

    BlacklistNetwork();
}

/****************************************************************************
 *
 * NAME: AppWakeRoutine
 *
 * DESCRIPTION:
 * End Point (Application) routine to run upon every wake
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void AppWakeRoutine()
{
    wakeup = TRUE;
}

/****************************************************************************
 *
 * NAME: SendData
 *
 * DESCRIPTION:
 * acquire and transmit data
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void SendAuthReq()
{
    PDUM_teStatus status;

    DBG_vPrintf(TRACE_APP, "AUTH: Sending Auth Code Request\n");

    // allocate memory for APDU buffer with preconfigured "type"
    PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance( apduMyData );
    if( data == PDUM_INVALID_HANDLE )
    {
        // problem allocating APDU instance memory
        DBG_vPrintf(TRACE_APP, "AUTH: Unable to allocate APDU memory\n");
    }
    else
    {
        // load payload data into APDU
        uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
                data,   // APDU instance handle
                0,      // APDU position for data
                "b",    // data format string
                '!'
        );
        if( byteCount == 0 )
        {
            // no data was written to the APDU instance
            DBG_vPrintf(TRACE_APP, "AUTH: No data written to APDU\n");
        }
        else
        {
            PDUM_eAPduInstanceSetPayloadSize( data, byteCount );

            DBG_vPrintf(TRACE_APP, "AUTH: Data written to APDU: %d\n", byteCount);

            // request data send to destination
            status = ZPS_eAplAfUnicastDataReq(
                    data,                   // APDU instance handle
                    0xffff,                 // cluster ID
                    1,                      // source endpoint
                    1,                      // destination endpoint
                    0x0000,                 // destination network address
                    ZPS_E_APL_AF_UNSECURE,  // security mode
                    0,                      // radius
                    NULL                    // sequence number pointer
            );
            if( status != ZPS_E_SUCCESS )
            {
                // problem with request
                DBG_vPrintf(TRACE_APP, "AUTH: DataReq not successful. Return: 0x%x\n", status);
            }
            else
            {
                // TX data request successful

            }
        }
    }
}

void SendData()
{
    if( s_eDeviceState.eNodeState == E_RUNNING )
    {
        PDUM_teStatus status;

        // allocate memory for APDU buffer with preconfigured "type"
        PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance( apduMyData );
        if( data == PDUM_INVALID_HANDLE )
        {
            // problem allocating APDU instance memory
            DBG_vPrintf(TRACE_APP, "APP: Unable to allocate APDU memory\n");
        }
        else
        {
            // read ADC values
            int16 sensorValue = 0x5678;
            int16 batteryValue = 0x9ABC;

            // load payload data into APDU
            uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
                    data,	// APDU instance handle
                    0,		// APDU position for data
                    "bhh",	// data format string
                    '*',
                    sensorValue,
                    batteryValue
            );
            if( byteCount == 0 )
            {
                // no data was written to the APDU instance
                DBG_vPrintf(TRACE_APP, "APP: No data written to APDU\n");
            }
            else
            {
                PDUM_eAPduInstanceSetPayloadSize( data, byteCount );

                DBG_vPrintf(TRACE_APP, "APP: Data written to APDU: %d\n", byteCount);

                // request data send to destination
                status = ZPS_eAplAfUnicastDataReq(
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
                    // problem with request
                    DBG_vPrintf(TRACE_APP, "APP: AckDataReq not successful. Return: 0x%x\n", status);
                    DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
                    DBG_vPrintf(TRACE_APP, "ERROR: Node State:  %d\n",s_eDeviceState.eNodeState);
                }
                else
                {
                    // TX data request successful

                }
            }
        }
    }
}

/****************************************************************************
 *
 * NAME: vWaitForAuthCode
 *
 * DESCRIPTION:
 * Check for and act upon stack events while waiting for the auth code.
 * This state is only used if NOT attempting rejoin of previous network
 *
 * PARAMETERS:      Name            RW  Usage
 *                  sStackEvent     R   Contains details of stack event
 * RETURNS:
 * void
 *
 ****************************************************************************/
void vWaitForAuthCode()
{
    ZPS_tsAfEvent sStackEvent;
    sStackEvent.eType = ZPS_EVENT_NONE;

    /* check if any messages to collect */
    ZQ_bQueueReceive (&APP_msgStrainGaugeEvents, &sStackEvent);

    switch( sStackEvent.eType )
    {

    case ZPS_EVENT_NONE:
    {
    }
    break;

    case ZPS_EVENT_APS_DATA_ACK:
    {
        DBG_vPrintf(TRACE_APP, "AUTH: ZPS_EVENT_APS_DATA_ACK Status %d, Address 0x%04x\n",
                    sStackEvent.uEvent.sApsDataAckEvent.u8Status,
                    sStackEvent.uEvent.sApsDataAckEvent.u16DstAddr);
    }
    break;

    case ZPS_EVENT_APS_DATA_CONFIRM:
    {
        DBG_vPrintf(TRACE_APP, "AUTH: ZPS_EVENT_APS_DATA_CONFIRM Status %d, Address 0x%04x\n",
                    sStackEvent.uEvent.sApsDataConfirmEvent.u8Status,
                    sStackEvent.uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr);
    }
    break;

    case ZPS_EVENT_APS_DATA_INDICATION:
    {
        DBG_vPrintf(TRACE_APP, "AUTH: ZPS_EVENT_AF_DATA_INDICATION\n");

        /* Process incoming cluster messages ... */
        DBG_vPrintf(TRACE_APP, "        Profile :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId);
        DBG_vPrintf(TRACE_APP, "        Cluster :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId);
        DBG_vPrintf(TRACE_APP, "        EndPoint:%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);


        uint32 authCode = 0;
        uint16 byteCount = PDUM_u16APduInstanceReadNBO(
                sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
                0,
                "w",
                &authCode
        );

        /* free the application protocol data unit (APDU) once it has been dealt with */
        PDUM_eAPduFreeAPduInstance( sStackEvent.uEvent.sApsDataIndEvent.hAPduInst );

        if( byteCount == 4 )
        {
            if( authCode == AUTH_CODE )
            {
                DBG_vPrintf(TRACE_APP, "AUTH: AUTH Code Confirmed\n");

                ZTIMER_eClose( authTimer );

                AuthNetwork();
            }
            else
            {
                // auth code incorrect
                // add EPID to blacklist

                ZTIMER_eClose( authTimer );

                BlacklistNetwork();
            }
        }
        else
        {
            // auth code size incorrect
            // add EPID to blacklist

            ZTIMER_eClose( authTimer );

            BlacklistNetwork();
        }
    }
    break;

    default:
    {
        DBG_vPrintf(TRACE_APP, "AUTH: Unexpected event - %d\n", sStackEvent.eType);
    }
    break;

    }
}

void vRunning()
{
    ZPS_tsAfEvent sStackEvent;
    sStackEvent.eType = ZPS_EVENT_NONE;

    /* check if any messages to collect */
    ZQ_bQueueReceive (&APP_msgStrainGaugeEvents, &sStackEvent);

    switch (sStackEvent.eType)
    {
        case ZPS_EVENT_NONE:
        {

        }
        break;

        case ZPS_EVENT_APS_ZGP_DATA_INDICATION:
        {
            DBG_vPrintf(TRACE_APP, "APP: event  ZPS_EVENT_APS_ZGP_DATA_INDICATION\n");
            if (sStackEvent.uEvent.sApsZgpDataIndEvent.hAPduInst)
            {
                PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsZgpDataIndEvent.hAPduInst);
            }
        }
        break;

        case ZPS_EVENT_APS_DATA_INDICATION:
        {
            DBG_vPrintf(TRACE_APP, "APP: APP_taskEndPoint: ZPS_EVENT_AF_DATA_INDICATION\n");

            /* Process incoming cluster messages for this endpoint... */
            DBG_vPrintf(TRACE_APP, "    Data Indication:\n");
            DBG_vPrintf(TRACE_APP, "        Status  : %x\n", sStackEvent.uEvent.sApsDataIndEvent.eStatus);
            DBG_vPrintf(TRACE_APP, "        Profile : %x\n", sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId);
            DBG_vPrintf(TRACE_APP, "        Cluster : %x\n", sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId);
            DBG_vPrintf(TRACE_APP, "        EndPoint: %x\n", sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);

            uint8 lqi = sStackEvent.uEvent.sApsDataIndEvent.u8LinkQuality;
            DBG_vPrintf(TRACE_APP, "        LQI     : %d\n", lqi);

            uint64 macAddress = ZPS_u64AplZdoLookupIeeeAddr(
                    sStackEvent.uEvent.sApsDataIndEvent.uSrcAddress.u16Addr );
            DBG_vPrintf(TRACE_APP, "        MAC Address: %08x%08x (%04x)\n",
                    (uint32)(macAddress >> 32), (uint32)macAddress,
                    sStackEvent.uEvent.sApsDataIndEvent.uSrcAddress.u16Addr);

            uint8 idByte = 0;

            uint16 byteCount = PDUM_u16APduInstanceReadNBO(
                    sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
                    0,
                    "b",
                    &idByte
            );

            if( byteCount == 1 )
            {
                switch( idByte )
                {
                case '~':
                {
                    DBG_vPrintf(TRACE_APP, "    Data Values:\n");

                    uint16 periodValue;

                    byteCount = PDUM_u16APduInstanceReadNBO(
                            sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
                            1,
                            "h",
                            &periodValue
                    );
                    if( byteCount == 2 )
                    {
                        DBG_vPrintf(TRACE_APP, "        periodValue = 0x%04x\n", periodValue);
                        samplePeriod = periodValue;

                        // save new value
                        PDM_teStatus status = PDM_eSaveRecordData(
                                PDM_APP_ID_SAMPLE_PERIOD,
                                &samplePeriod,
                                sizeof(samplePeriod)
                        );

                        if( status != PDM_E_STATUS_OK )
                        {
                            DBG_vPrintf(TRACE_APP, "APP: Data RX: sample period save error: 0x%x\n", status);
                        }
                    }
                    else
                    {
                        // unexpected number of read bytes

                    }

                    break;
                }
                case '$':
                {
                    // "GO" command
                    DBG_vPrintf(TRACE_APP, "    GO command received\n");
                    configured = TRUE;

                    // save new value
                    PDM_teStatus status = PDM_eSaveRecordData(
                            PDM_APP_ID_CONFIGURED,
                            &configured,
                            sizeof(configured)
                    );

                    if( status != PDM_E_STATUS_OK )
                    {
                        DBG_vPrintf(TRACE_APP, "APP: Data RX: configured save error: 0x%x\n", status);
                    }

                    break;
                }
                default:
                    DBG_vPrintf(TRACE_APP, "Unrecognized Packet ID: 0x%x\n", idByte);
                    break;
                }
            }

            /* free the application protocol data unit (APDU) once it has been dealt with */
            PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);
        }
        break;

        case ZPS_EVENT_APS_DATA_CONFIRM:
        {
            DBG_vPrintf(TRACE_APP, "APP: APP_taskEndPoint: ZPS_EVENT_APS_DATA_CONFIRM Status %d, Address 0x%04x\n",
                        sStackEvent.uEvent.sApsDataConfirmEvent.u8Status,
                        sStackEvent.uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr);
        }
        break;

        case ZPS_EVENT_APS_DATA_ACK:
        {
            DBG_vPrintf(TRACE_APP, "APP: APP_taskEndPoint: ZPS_EVENT_APS_DATA_ACK Status %d, Address 0x%04x\n",
                        sStackEvent.uEvent.sApsDataAckEvent.u8Status,
                        sStackEvent.uEvent.sApsDataAckEvent.u16DstAddr);
        }
        break;

        default:
        {
            DBG_vPrintf(TRACE_APP, "APP: APP_taskEndPoint: unhandled event %d\n", sStackEvent.eType);
        }
        break;
    }
}


/****************************************************************************
 *
 * NAME: APP_taskEndpoint
 *
 * DESCRIPTION:
 * End Point event handling
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void APP_vtaskMyEndPoint (void)
{
    if( configPressed_ep )
    {
        configPressed_ep = FALSE;
        PDM_vDeleteDataRecord( PDM_APP_ID_SAMPLE_PERIOD );
        PDM_vDeleteDataRecord( PDM_APP_ID_CONFIGURED );
        configured = TRUE;  // TODO change to FALSE after testing

        endPointState = EP_STATE_INIT;
    }


	switch( endPointState )
	{
	case EP_STATE_INIT:
	{
		// load NV configuration
	    {
            // if sample period NV record doesn't exist,
            // create one with the default value
            uint16 byteCount;
            bool_t dataExists = PDM_bDoesDataExist( PDM_APP_ID_SAMPLE_PERIOD, &byteCount );
            if( !dataExists || (byteCount != sizeof(samplePeriod)) )
            {
                if( dataExists && (byteCount != sizeof(samplePeriod)) )
                {
                    // record exists, but size is wrong
                    DBG_vPrintf(TRACE_APP, "APP_INIT: sample period NV size mismatch (NV size = %d)\n", byteCount);
                }

                // the record does NOT exist in NV memory or size mismatch
                // create a (new) record with the default value
                PDM_teStatus status;
                status = PDM_eSaveRecordData(
                        PDM_APP_ID_SAMPLE_PERIOD,
                        &samplePeriod,
                        sizeof(samplePeriod)
                );

                if( status != PDM_E_STATUS_OK )
                {
                    DBG_vPrintf(TRACE_APP, "APP_INIT: sample period save error: 0x%x\n", status);
                }
            }
            else
            {
                // the record does exist in NV memory
                // read the record and set the variable
                uint16 bytesRead;
                PDM_teStatus status;
                status = PDM_eReadDataFromRecord(
                        PDM_APP_ID_SAMPLE_PERIOD,
                        &samplePeriod,
                        sizeof(samplePeriod),
                        &bytesRead
                );

                if( status != PDM_E_STATUS_OK )
                {
                    DBG_vPrintf(TRACE_APP, "APP_INIT: sample period load error: 0x%x\n", status);
                }
            }
        }

	    {
            // if configured (RUN flag) NV record doesn't exist,
            // create one with a FALSE value
            uint16 byteCount;
            bool_t dataExists = PDM_bDoesDataExist( PDM_APP_ID_CONFIGURED, &byteCount );
            if( !dataExists || (byteCount != sizeof(configured)) )
            {
                if( byteCount != sizeof(configured) )
                {
                    DBG_vPrintf(TRACE_APP, "APP_INIT: configured NV size mismatch (NV size = %d)\n", byteCount);
                }

                // the record does NOT exist in NV memory or size mismatch
                // create a (new) record with the default value
                PDM_teStatus status;
                status = PDM_eSaveRecordData(
                        PDM_APP_ID_CONFIGURED,
                        &configured,
                        sizeof(configured)
                );

                if( status != PDM_E_STATUS_OK )
                {
                    DBG_vPrintf(TRACE_APP, "APP_INIT: configured save error: 0x%x\n", status);
                }
            }

            else
            {
                // the record does exist in NV memory
                // read the record and set the variable
                uint16 bytesRead;
                PDM_teStatus status;
                status = PDM_eReadDataFromRecord(
                        PDM_APP_ID_CONFIGURED,
                        &configured,
                        sizeof(configured),
                        &bytesRead
                );

                if( status != PDM_E_STATUS_OK )
                {
                    DBG_vPrintf(TRACE_APP, "APP_INIT: configured load error: 0x%x\n", status);
                }
            }
	    }

	    // Initialize External ADC Here (if needed)
	    // ADC_INIT

		endPointState = EP_STATE_WAIT_NETWORK;
		break;
	}
	case EP_STATE_WAIT_NETWORK:
	{
		if( s_eDeviceState.eNodeState == E_AUTH_REQ )
		{
		    // initialize AUTH CODE state

			endPointState = EP_STATE_AUTH;

			SendAuthReq();

			ZTIMER_eOpen(
                    &authTimer,
                    AuthTimerCallback,
                    NULL,
                    ZTIMER_FLAG_PREVENT_SLEEP );
            ZTIMER_eStart ( authTimer, ZTIMER_TIME_SEC( AUTH_TIMEOUT ) );
		}
		else if( s_eDeviceState.eNodeState == E_RUNNING )
        {
            endPointState = EP_STATE_CONFIG;
        }
		else if( wakeup )
		{
			wakeup = FALSE;
			DBG_vPrintf(TRACE_APP, "APP_WAIT_NET: Sleep Time: %d seconds\n", NO_NETWORK_SLEEP_DUR );
			PWRM_eScheduleActivity(&sWake, SECS_TO_TICKS(NO_NETWORK_SLEEP_DUR), vWakeCallBack);
		}

		break;
	}
	case EP_STATE_AUTH:
	{
	    if( (s_eDeviceState.eNodeState != E_RUNNING) && (s_eDeviceState.eNodeState != E_AUTH_REQ) )
        {
            endPointState = EP_STATE_WAIT_NETWORK;
            break;
        }
	    else if( s_eDeviceState.eNodeState == E_RUNNING )
        {
            endPointState = EP_STATE_CONFIG;
        }
	    else
	    {
	        vWaitForAuthCode();
	    }

	    break;
	}
	case EP_STATE_CONFIG:
        if( s_eDeviceState.eNodeState != E_RUNNING )
        {
            endPointState = EP_STATE_WAIT_NETWORK;
        }
        else if( configured )
		{
			endPointState = EP_STATE_RUNNING;
		}
        else if( wakeup )
        {
            wakeup = FALSE;
            DBG_vPrintf(TRACE_APP, "APP_CONFIG: Sleep Time: %d seconds\n", CONFIG_STATE_SLEEP_DUR );
            PWRM_eScheduleActivity(&sWake, SECS_TO_TICKS(CONFIG_STATE_SLEEP_DUR), vWakeCallBack);

            vRunning();
        }
        else
        {
            vRunning();
        }

		break;
	case EP_STATE_RUNNING:
	{
	    static bool_t adcDone = TRUE;   // temporary until actual ADC code is added

	    if( s_eDeviceState.eNodeState != E_RUNNING )
		{
			endPointState = EP_STATE_WAIT_NETWORK;
		}
	    else if( wakeup )
		{
			wakeup = FALSE;
			uint8_t i;

			// Start ADC Conversion
			DBG_vPrintf(TRACE_APP, "APP: MCP3204_init...");
			MCP3204_init(0, 3.3);
			DBG_vPrintf(TRACE_APP, "OK\n");

			for (i = 0; i < 4; i++)
			{
				MCP3204_convert(0, i);
				DBG_vPrintf(TRACE_APP, "APP: CH%d = %d - %x\n", i, MCP3204_getValue(), MCP3204_getValue());
			}
			// ADC_CONVERT
			adcDone = TRUE;

			DBG_vPrintf(TRACE_APP, "APP_RUN: Sleep Time: %d seconds\n", samplePeriod );
			PWRM_eScheduleActivity(&sWake, SECS_TO_TICKS(samplePeriod), vWakeCallBack);

			vRunning();
		}
	    else
	    {
	        if( adcDone )  // poll for "ADC done" ADC_POLL
            {
                adcDone = FALSE;
                // Read ADC Values
                // ADC_READ

                SendData();
            }

	        vRunning();
	    }

	    break;
	}
	default:
		DBG_vPrintf(TRACE_APP, "Unknown EndPoint State: %d", endPointState);
		endPointState = EP_STATE_INIT;
	}

}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
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
