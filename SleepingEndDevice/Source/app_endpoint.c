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
#include "ad8231.h"
#include "ltc1661.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define AUTH_TIMEOUT    			6	// seconds

#ifndef DEBUG_APP
	#define TRACE_APP 				FALSE
#else
	#define TRACE_APP 				TRUE
#endif

#define NO_NETWORK_SLEEP_DUR        10   // seconds
#define CONFIG_STATE_SLEEP_DUR      10   // seconds
#define SAMPLE_RATE_DEFAULT         10   // seconds
#define CHANNEL_A_VALUE_DEFAULT		2000 // raw value
#define CHANNEL_B_VALUE_DEFAULT		2000 // raw value
#define GAIN_VALUE_DEFAULT			16	 // times

#define DIO17						17
#define ENABLE_3VLN() 				vAHI_DioSetDirection(0x0,(1 << DIO17)); vAHI_DioSetOutput((1 << DIO17), 0x0);
#define DISABLE_3VLN() 				vAHI_DioSetDirection(0x0,(1 << DIO17)); vAHI_DioSetOutput(0x0, (1 << DIO17));

#define DIO12						12
#define ENABLE_POWERSAVE() 			vAHI_DioSetDirection(0x0,(1 << DIO12)); vAHI_DioSetOutput(0x0, (1 << DIO12));
#define DISABLE_POWERSAVE() 		vAHI_DioSetDirection(0x0,(1 << DIO12)); vAHI_DioSetOutput((1 << DIO12), 0x0);

#define DIO11						11
#define ENABLE_WB() 				vAHI_DioSetDirection(0x0,(1 << DIO11)); vAHI_DioSetOutput(0x0, (1 << DIO11));
#define DISABLE_WB() 				vAHI_DioSetDirection(0x0,(1 << DIO11)); vAHI_DioSetOutput((1 << DIO11), 0x0);

#define SECS_TO_TICKS( seconds )	seconds * 32768

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

bool_t configured = FALSE;
bool_t wakeup = TRUE;

uint16 samplePeriod = 	SAMPLE_RATE_DEFAULT;
uint16 channelAValue = 	CHANNEL_A_VALUE_DEFAULT;
uint16 channelBValue = 	CHANNEL_B_VALUE_DEFAULT;
uint16 gainValue = 		GAIN_VALUE_DEFAULT;

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
        	int sensorValue, temperatureValue, batteryValue;
        	static int results[10];
        	int i, j, Imin, temp;

        	DISABLE_POWERSAVE();
        	ENABLE_WB();

        	ad8231_init();
        	ad8231_enable();
        	ad8231_setGain(gainValue);

        	ltc1661_init();
        	ltc1661_setDAC_A(channelAValue);
        	ltc1661_setDAC_B(channelBValue);

        	// Start ADC Conversion
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

        	DBG_vPrintf(TRACE_APP, "APP: sorted Values = ");
        	for (i = 0; i < 10; i++) DBG_vPrintf(TRACE_APP,"%d, ", results[i]);
        	DBG_vPrintf(TRACE_APP, "\n\r");

        	sensorValue = results[3];
        	sensorValue += results[4];
        	sensorValue += results[5];
        	sensorValue /= 3;

        	temperatureValue = MCP3204_convert(0, 1);
        	batteryValue = MCP3204_convert(0, 0);

        	DBG_vPrintf(TRACE_APP, "APP: sensorValue = %d - %x\n", sensorValue, sensorValue);
        	DBG_vPrintf(TRACE_APP, "APP: temperatureValue = %d - %x\n", temperatureValue, temperatureValue);
        	DBG_vPrintf(TRACE_APP, "APP: batteryValue = %d - %x\n", batteryValue, batteryValue);

        	ad8231_disable();
        	ltc1661_sleep();

        	DISABLE_WB();
        	ENABLE_POWERSAVE();

            // load payload data into APDU
            uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
                    data,	// APDU instance handle
                    0,		// APDU position for data
                    "bhhh",	// data format string
                    '*',
                    sensorValue,
                    temperatureValue,
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

                // "Broadcast" command
				DBG_vPrintf(TRACE_APP, "    Broadcast command received\n");

				// allocate memory for APDU buffer with preconfigured "type"
				PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance( apduMyData );
				if( data == PDUM_INVALID_HANDLE )
				{
					// problem allocating APDU instance memory
					DBG_vPrintf(TRACE_APP, "APP: Unable to allocate APDU memory\n");
				}
				else
				{
					// load payload data into APDU
					uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
									   data,	// APDU instance handle
									   0,		// APDU position for data
									   "b",	// data format string
									   '&');

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
						PDM_teStatus status = ZPS_eAplAfUnicastDataReq(
								 data,					// APDU instance handle
								 0xFFFF,					// cluster ID
								 1,						// source endpoint
								 1,						// destination endpoint
								 0x0000,					// destination network address
								 ZPS_E_APL_AF_UNSECURE,	// security mode
								 0,						// radius
								 NULL);					// sequence number pointer

						if( status != ZPS_E_SUCCESS )
						{
							// problem with request
							DBG_vPrintf(TRACE_APP, "APP: AckDataReq not successful. Return: 0x%x\n", status);
							DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
							DBG_vPrintf(TRACE_APP, "ERROR: Node State:  %d\n",s_eDeviceState.eNodeState);
						}
					}
				}

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

                    struct
					{
						uint16 inSamplePeriod;
						uint16 inChannelAValue;
						uint16 inChannelBValue;
						uint16 inGainValue;
					} values = { 0 };

                    byteCount = PDUM_u16APduInstanceReadNBO(
                            sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
                            1,
                            "hhhh",
                            &values);

                    if( byteCount == 8 )
                    {

                    	samplePeriod = values.inSamplePeriod;
						channelAValue = values.inChannelAValue;
						channelBValue = values.inChannelBValue;
						gainValue = values.inGainValue;

                        DBG_vPrintf(TRACE_APP, "        samplePeriod = 0x%04x\n", samplePeriod);
                        DBG_vPrintf(TRACE_APP, "        channelA = 0x%04x\n", channelAValue);
                        DBG_vPrintf(TRACE_APP, "        channelB = 0x%04x\n", channelBValue);
                        DBG_vPrintf(TRACE_APP, "        gainValue = 0x%04x\n", gainValue);

                        // save new values
                        PDM_teStatus status = PDM_eSaveRecordData(
                                PDM_APP_ID_SAMPLE_PERIOD,
                                &samplePeriod,
                                sizeof(samplePeriod)
                        );

                        if( status != PDM_E_STATUS_OK )
                            DBG_vPrintf(TRACE_APP, "APP: Data RX: sample period save error: 0x%x\n", status);

                        status = PDM_eSaveRecordData(
								PDM_APP_ID_CHANNEL_A,
								&channelAValue,
								sizeof(channelAValue)
						);

						if( status != PDM_E_STATUS_OK )
							DBG_vPrintf(TRACE_APP, "APP: Data RX: channel A value save error: 0x%x\n", status);

						status = PDM_eSaveRecordData(
								PDM_APP_ID_CHANNEL_B,
								&channelBValue,
								sizeof(channelBValue)
						);

						if( status != PDM_E_STATUS_OK )
							DBG_vPrintf(TRACE_APP, "APP: Data RX: channel B value save error: 0x%x\n", status);

						status = PDM_eSaveRecordData(
								PDM_APP_ID_GAIN,
								&gainValue,
								sizeof(gainValue)
						);

						if( status != PDM_E_STATUS_OK )
							DBG_vPrintf(TRACE_APP, "APP: Data RX: gain value save error: 0x%x\n", status);

                        // allocate memory for APDU buffer with preconfigured "type"
                        PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance( apduMyData );
                        if( data == PDUM_INVALID_HANDLE )
                        {
                        	// problem allocating APDU instance memory
                            DBG_vPrintf(TRACE_APP, "APP: Unable to allocate APDU memory\n");
                        }
                        else
                        {
                        	// load payload data into APDU
                        	uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
                        	                   data,	// APDU instance handle
                        	                   0,		// APDU position for data
                        	                   "bhhhh",	// data format string
                        	                   '~',
                        	                   samplePeriod,
                        	                   channelAValue,
                        	                   channelBValue,
                        	                   gainValue);

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
                        	             NULL);					// sequence number pointer

                        	    if( status != ZPS_E_SUCCESS )
                        	    {
                        	    	// problem with request
                        	        DBG_vPrintf(TRACE_APP, "APP: AckDataReq not successful. Return: 0x%x\n", status);
                        	        DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
                        	        DBG_vPrintf(TRACE_APP, "ERROR: Node State:  %d\n",s_eDeviceState.eNodeState);
                        	    }
                        	}
                        }
                    }
                    else
                    {
                    	DBG_vPrintf(TRACE_APP, "APP: unexpected number of read bytes = %d\n", byteCount);
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

                    // allocate memory for APDU buffer with preconfigured "type"
					PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance( apduMyData );
					if( data == PDUM_INVALID_HANDLE )
					{
						// problem allocating APDU instance memory
						DBG_vPrintf(TRACE_APP, "APP: Unable to allocate APDU memory\n");
					}
					else
					{
						// load payload data into APDU
						uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
										   data,	// APDU instance handle
										   0,		// APDU position for data
										   "bbb",	// data format string
										   '$',
										   'G',
										   'O');

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
									 NULL);					// sequence number pointer

							if( status != ZPS_E_SUCCESS )
							{
								// problem with request
								DBG_vPrintf(TRACE_APP, "APP: AckDataReq not successful. Return: 0x%x\n", status);
								DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
								DBG_vPrintf(TRACE_APP, "ERROR: Node State:  %d\n",s_eDeviceState.eNodeState);
							}
						}
					}
                    break;
                }
                case '&':
				{
					// "Broadcast" command
					DBG_vPrintf(TRACE_APP, "    Broadcast command received\n");

					// allocate memory for APDU buffer with preconfigured "type"
					PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance( apduMyData );
					if( data == PDUM_INVALID_HANDLE )
					{
						// problem allocating APDU instance memory
						DBG_vPrintf(TRACE_APP, "APP: Unable to allocate APDU memory\n");
					}
					else
					{
						// load payload data into APDU
						uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
										   data,	// APDU instance handle
										   0,		// APDU position for data
										   "b",	// data format string
										   '&');

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
							PDM_teStatus status = ZPS_eAplAfUnicastDataReq(
									 data,					// APDU instance handle
									 0xFFFF,					// cluster ID
									 1,						// source endpoint
									 1,						// destination endpoint
									 0x0000,					// destination network address
									 ZPS_E_APL_AF_UNSECURE,	// security mode
									 0,						// radius
									 NULL);					// sequence number pointer

							if( status != ZPS_E_SUCCESS )
							{
								// problem with request
								DBG_vPrintf(TRACE_APP, "APP: AckDataReq not successful. Return: 0x%x\n", status);
								DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
								DBG_vPrintf(TRACE_APP, "ERROR: Node State:  %d\n",s_eDeviceState.eNodeState);
							}
						}
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
    	DBG_vPrintf(TRACE_APP, "\nENDPOINT: CONFIG Reset Detected\n\n");

        configPressed_ep = FALSE;
        PDM_vDeleteDataRecord( PDM_APP_ID_SAMPLE_PERIOD );
        PDM_vDeleteDataRecord( PDM_APP_ID_CONFIGURED );
        PDM_vDeleteDataRecord( PDM_APP_ID_CHANNEL_A );
        PDM_vDeleteDataRecord( PDM_APP_ID_CHANNEL_B );
        PDM_vDeleteDataRecord( PDM_APP_ID_GAIN );
        configured = FALSE;

        DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_INIT\n");
        endPointState = EP_STATE_INIT;
    }

	switch( endPointState )
	{
	case EP_STATE_INIT:
	{
		//send DAC and OPAMP to low power
		ENABLE_3VLN();
		ad8231_init();
		ad8231_enable();
		ltc1661_init();
		ad8231_disable();
		ltc1661_sleep();
		ENABLE_POWERSAVE();
		DISABLE_WB();

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

	    {
			// if channel A value NV record doesn't exist,
			// create one with the defaul value
			uint16 byteCount;
			bool_t dataExists = PDM_bDoesDataExist( PDM_APP_ID_CHANNEL_A, &byteCount );
			if( !dataExists || (byteCount != sizeof(channelAValue)) )
			{
				if( byteCount != sizeof(channelAValue) )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: channel A NV size mismatch (NV size = %d)\n", byteCount);
				}

				// the record does NOT exist in NV memory or size mismatch
				// create a (new) record with the default value
				PDM_teStatus status;
				status = PDM_eSaveRecordData(
						PDM_APP_ID_CHANNEL_A,
						&channelAValue,
						sizeof(channelAValue)
				);

				if( status != PDM_E_STATUS_OK )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: channel A NV save error: 0x%x\n", status);
				}
			}

			else
			{
				// the record does exist in NV memory
				// read the record and set the variable
				uint16 bytesRead;
				PDM_teStatus status;
				status = PDM_eReadDataFromRecord(
						PDM_APP_ID_CHANNEL_A,
						&channelAValue,
						sizeof(channelAValue),
						&bytesRead
				);

				if( status != PDM_E_STATUS_OK )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: channel A NV load error: 0x%x\n", status);
				}
			}
		}

	    {
			// if channel B value NV record doesn't exist,
			// create one with the default value
			uint16 byteCount;
			bool_t dataExists = PDM_bDoesDataExist( PDM_APP_ID_CHANNEL_B, &byteCount );
			if( !dataExists || (byteCount != sizeof(channelBValue)) )
			{
				if( byteCount != sizeof(channelBValue) )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: channel B NV size mismatch (NV size = %d)\n", byteCount);
				}

				// the record does NOT exist in NV memory or size mismatch
				// create a (new) record with the default value
				PDM_teStatus status;
				status = PDM_eSaveRecordData(
						PDM_APP_ID_CHANNEL_B,
						&channelBValue,
						sizeof(channelBValue)
				);

				if( status != PDM_E_STATUS_OK )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: channel B NV save error: 0x%x\n", status);
				}
			}

			else
			{
				// the record does exist in NV memory
				// read the record and set the variable
				uint16 bytesRead;
				PDM_teStatus status;
				status = PDM_eReadDataFromRecord(
						PDM_APP_ID_CHANNEL_B,
						&channelBValue,
						sizeof(channelBValue),
						&bytesRead
				);

				if( status != PDM_E_STATUS_OK )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: channel B NV load error: 0x%x\n", status);
				}
			}
		}

	    {
			// if gain value NV record doesn't exist,
			// create one with the default value
			uint16 byteCount;
			bool_t dataExists = PDM_bDoesDataExist( PDM_APP_ID_GAIN, &byteCount );
			if( !dataExists || (byteCount != sizeof(gainValue)) )
			{
				if( byteCount != sizeof(gainValue) )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: gain NV size mismatch (NV size = %d)\n", byteCount);
				}

				// the record does NOT exist in NV memory or size mismatch
				// create a (new) record with the default value
				PDM_teStatus status;
				status = PDM_eSaveRecordData(
						PDM_APP_ID_GAIN,
						&gainValue,
						sizeof(gainValue)
				);

				if( status != PDM_E_STATUS_OK )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: gain NV save error: 0x%x\n", status);
				}
			}

			else
			{
				// the record does exist in NV memory
				// read the record and set the variable
				uint16 bytesRead;
				PDM_teStatus status;
				status = PDM_eReadDataFromRecord(
						PDM_APP_ID_GAIN,
						&gainValue,
						sizeof(gainValue),
						&bytesRead
				);

				if( status != PDM_E_STATUS_OK )
				{
					DBG_vPrintf(TRACE_APP, "APP_INIT: gain NV load error: 0x%x\n", status);
				}
			}
		}

	    if( s_eDeviceState.eNodeState == E_RUNNING )
	    {
	    	// "Broadcast" command
			DBG_vPrintf(TRACE_APP, "    sending Broadcast command\n");

			// allocate memory for APDU buffer with preconfigured "type"
			PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance( apduMyData );
			if( data == PDUM_INVALID_HANDLE )
			{
				// problem allocating APDU instance memory
				DBG_vPrintf(TRACE_APP, "APP: Unable to allocate APDU memory\n");
			}
			else
			{
				// load payload data into APDU
				uint16 byteCount = PDUM_u16APduInstanceWriteNBO(
								   data,	// APDU instance handle
								   0,		// APDU position for data
								   "b",	// data format string
								   '&');

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
					PDM_teStatus status = ZPS_eAplAfUnicastDataReq(
							 data,					// APDU instance handle
							 0xFFFF,					// cluster ID
							 1,						// source endpoint
							 1,						// destination endpoint
							 0x0000,					// destination network address
							 ZPS_E_APL_AF_UNSECURE,	// security mode
							 0,						// radius
							 NULL);					// sequence number pointer

					if( status != ZPS_E_SUCCESS )
					{
						// problem with request
						DBG_vPrintf(TRACE_APP, "APP: AckDataReq not successful. Return: 0x%x\n", status);
						DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
						DBG_vPrintf(TRACE_APP, "ERROR: Node State:  %d\n",s_eDeviceState.eNodeState);
					}
				}
			}
	    }

	    // Initialize External ADC Here (if needed)
	    // ADC_INIT

		DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_WAIT_NETWORK\n");
		endPointState = EP_STATE_WAIT_NETWORK;
		break;
	}
	case EP_STATE_WAIT_NETWORK:
	{
		if( s_eDeviceState.eNodeState == E_AUTH_REQ )
		{
		    // initialize AUTH CODE state

			DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_AUTH\n");
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
			DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_CONFIG 1\n");
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
	    	DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_WAIT_NETWORK\n");
            endPointState = EP_STATE_WAIT_NETWORK;
            break;
        }
	    else if( s_eDeviceState.eNodeState == E_RUNNING )
        {
	    	DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_CONFIG 2\n");
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
        	DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_WAIT_NETWORK\n");
            endPointState = EP_STATE_WAIT_NETWORK;
        }
        else if( configured )
		{
        	DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_RUNNING\n");
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
	    if( s_eDeviceState.eNodeState != E_RUNNING )
		{
	    	DBG_vPrintf(TRACE_APP, "APP_STATE: EP_STATE_WAIT_NETWORK\n");
			endPointState = EP_STATE_WAIT_NETWORK;
		}
	    else if( wakeup )
		{
			wakeup = FALSE;

			DBG_vPrintf(TRACE_APP, "APP_RUN: Sleep Time: %d seconds\n", samplePeriod );
			PWRM_eScheduleActivity(&sWake, SECS_TO_TICKS(samplePeriod), vWakeCallBack);
			SendData();
		}
	    else vRunning();

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
