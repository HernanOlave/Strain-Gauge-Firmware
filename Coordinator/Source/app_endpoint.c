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
#include <zps_apl_af.h>
#include "AppHardwareApi.h"
#include "ZQueue.h"
#include "app_common.h"
#include "pdum_gen.h"
#include "zps_gen.h"

#include <stdlib.h>
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

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
tszQueue APP_msgStrainGaugeEvents;
tszQueue APP_msgZpsEvents;
/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

extern void PermitJoining();

uint16 TranslateMacStubToNetwork( uint16 macStub )
{
    uint8 i;
    uint64 macAddr;
    bool_t matchFound = FALSE;
    uint16 netAddr = 0;
    for( i = 0; i < ZPS_MAC_ADDRESS_TABLE_SIZE; i++ )
    {
        macAddr = ZPS_u64NwkNibGetMappedIeeeAddr( ZPS_pvAplZdoGetNwkHandle(), i );
        if( macStub == (macAddr & 0xFFFF) )
        {
            matchFound = TRUE;
            break;
        }
    }

    if( matchFound )
    {
        netAddr = ZPS_u16AplZdoLookupAddr( macAddr );
        DBG_vPrintf( TRACE_APP,
                "Address Translation: %04x -> %08x %08x (%04x)\n",
                macStub,
                (uint32)(macAddr >> 32),
                (uint32)(macAddr >>  0),
                netAddr );
    }
    else
    {
        // no matching MAC address found
        // return 0 as it is not a valid translated address (address of network Coordinator)
        netAddr = 0;
        DBG_vPrintf( TRACE_APP, "Address Translation FAILED (%04x)\n",
                macStub );
    }

    return netAddr;
}

uint8 u8ToDec( char * buffer, uint8 value )
{
    uint8 remain = value;
    uint8 i = 0;
    if( value >= 100 )
    {
        buffer[i++] = (remain / 100) + 0x30;
        remain %= 100;
    }
    if( value >= 10 )
    {
        buffer[i++] = (remain / 10) + 0x30;
        remain %= 10;
    }
    buffer[i++] = remain + 0x30;

    return i;
}

uint8 u16ToHex( char * buffer, uint16 value )
{
    uint8 i;
    uint8 digits = 4;
    for( i = 0; i < digits; i++ )
    {
        uint8 digitValue = ((value >> 4 * (digits - (i + 1))) & 0x000F);
        if( digitValue <= 9 )
        {
            digitValue += 0x30;
        }
        else
        {
            digitValue += 0x41 - 10;
        }

        buffer[i] = digitValue;
    }

    return i;
}

void ProcessUART()
{
    // check for a command terminated with \n
    static char uartRxBuffer[32];
    static uint8 uartRxBufferIndex = 0;
    static bool_t commandReady = FALSE;
    {
        uint8 status = u8AHI_UartReadLineStatus( E_AHI_UART_1 );
        if( status & E_AHI_UART_LS_ERROR )
        {
            // parity or framing error or break indication received
            vAHI_UartReset( E_AHI_UART_1, FALSE, TRUE );	// reset RX
            uartRxBufferIndex = 0;
            commandReady = FALSE;
        }
        if( status & E_AHI_UART_LS_TEMT )
        {
            // TX shift register empty

        }
        if( status & E_AHI_UART_LS_THRE )
        {
            // TX FIFO empty

        }
        if( status & E_AHI_UART_LS_BI )
        {
            // break indication received
            vAHI_UartReset( E_AHI_UART_1, FALSE, TRUE );	// reset RX
            uartRxBufferIndex = 0;
            commandReady = FALSE;

            DBG_vPrintf( TRACE_APP, "UART Break" );
        }
        if( status & E_AHI_UART_LS_FE )
        {
            // framing error
            vAHI_UartReset( E_AHI_UART_1, FALSE, TRUE );	// reset RX
            uartRxBufferIndex = 0;
            commandReady = FALSE;

            DBG_vPrintf( TRACE_APP, "UART Framing Error" );
        }
        if( status & E_AHI_UART_LS_PE )
        {
            // parity error
            vAHI_UartReset( E_AHI_UART_1, FALSE, TRUE );	// reset RX
            uartRxBufferIndex = 0;
            commandReady = FALSE;

            DBG_vPrintf( TRACE_APP, "UART Parity Error" );
        }
        if( status & E_AHI_UART_LS_OE )
        {
            // RX FIFO buffer over-run
            vAHI_UartReset( E_AHI_UART_1, FALSE, TRUE );	// reset RX
            uartRxBufferIndex = 0;
            commandReady = FALSE;

            DBG_vPrintf( TRACE_APP, "UART RX Buffer Over-run" );
        }
        if( status & E_AHI_UART_LS_DR )
        {
            // RX FIFO data ready
        }
    }
    while( u16AHI_UartReadRxFifoLevel( E_AHI_UART_1 ) > 0
            && commandReady == FALSE )
    {
        char byte = u8AHI_UartReadData( E_AHI_UART_1 );
        if( byte == '\n' )
        {
            commandReady = TRUE;
            uartRxBuffer[uartRxBufferIndex] = 0;		// null termination
        }
        else
        {
            uartRxBuffer[uartRxBufferIndex++] = byte;
        }
    }
    if( commandReady )
    {
        // process the received command
        DBG_vPrintf( TRACE_APP, "UART Command: %s\n", uartRxBuffer );

        switch( uartRxBuffer[0] ) {
        case '~':
        {
            // sample period command
            //	~aaaa,pppp\n (a = last 4 of End Device MAC address, p = sample period in HEX)
            uint16 destMacAddrStub = strtol( &uartRxBuffer[1], NULL, 16 );
            uint16 period = strtol( &uartRxBuffer[6], NULL, 16 );

            uint16 destNetAddr;

            destNetAddr = TranslateMacStubToNetwork( destMacAddrStub );

            if( destNetAddr != 0 )
            {
                PDUM_teStatus status;

                // allocate memory for APDU buffer with preconfigured "type"
                PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance(
                apduMyData );
                if( data == PDUM_INVALID_HANDLE )
                {
                    // problem allocating APDU instance memory
                    DBG_vPrintf( TRACE_APP,
                            "APP: Unable to allocate APDU memory\n" );
                }
                else
                {
                    // load payload data into APDU
                    uint16 byteCount = PDUM_u16APduInstanceWriteNBO( data,	// APDU instance handle
                            0,		// APDU position for data
                            "bh",	// data format string
                            '~', period );
                    if( byteCount == 0 )
                    {
                        // no data was written to the APDU instance
                        DBG_vPrintf( TRACE_APP,
                                "APP: No data written to APDU\n" );
                    }
                    else
                    {
                        PDUM_eAPduInstanceSetPayloadSize( data, byteCount );

                        DBG_vPrintf( TRACE_APP,
                                "APP: Data written to APDU: %d\n", byteCount );

                        // request data send to destination
                        status = ZPS_eAplAfUnicastAckDataReq( data,	// APDU instance handle
                                0xFFFF,					// cluster ID
                                1,						// source endpoint
                                1,						// destination endpoint
                                destNetAddr,	        // destination network address
                                ZPS_E_APL_AF_UNSECURE,	// security mode
                                0,						// radius
                                NULL				    // sequence number pointer
                                );
                        if( status != ZPS_E_SUCCESS )
                        {
                            // problem with request
                            DBG_vPrintf( TRACE_APP,
                                    "APP: AckDataReq not successful. Return: 0x%x\n",
                                    status );
                        }
                        else
                        {
                            // TX data request successful

                        }
                    }
                }
            }
            break;
        }
        case '$':
        {
            // GO command
            //	$aaaa,GO\n (a = last 4 of End Device MAC address)
            // send: $GO to addressed device

            uint16 destMacAddrStub = strtol( &uartRxBuffer[1], NULL, 16 );

            // could read "GO" string, but no need currently

            uint16 destNetAddr;

            destNetAddr = TranslateMacStubToNetwork( destMacAddrStub );

            if( destNetAddr != 0 )
            {
                PDUM_teStatus status;

                // allocate memory for APDU buffer with preconfigured "type"
                PDUM_thAPduInstance data = PDUM_hAPduAllocateAPduInstance(
                apduMyData );
                if( data == PDUM_INVALID_HANDLE )
                {
                    // problem allocating APDU instance memory
                    DBG_vPrintf( TRACE_APP,
                            "APP: Unable to allocate APDU memory\n" );
                }
                else
                {
                    // load payload data into APDU
                    uint16 byteCount = PDUM_u16APduInstanceWriteNBO( data,	// APDU instance handle
                            0,		// APDU position for data
                            "bbb",	// data format string
                            '$', 'G', 'O' );
                    if( byteCount == 0 )
                    {
                        // no data was written to the APDU instance
                        DBG_vPrintf( TRACE_APP,
                                "APP: No data written to APDU\n" );
                    }
                    else
                    {
                        PDUM_eAPduInstanceSetPayloadSize( data, byteCount );

                        DBG_vPrintf( TRACE_APP,
                                "APP: Data written to APDU: %d\n", byteCount );

                        // request data send to destination
                        status = ZPS_eAplAfUnicastAckDataReq(
                                data,	                // APDU instance handle
                                0xffff,					// cluster ID
                                1,						// source endpoint
                                1,						// destination endpoint
                                destNetAddr,	        // destination network address
                                ZPS_E_APL_AF_UNSECURE,	// security mode
                                0,						// radius
                                NULL				    // sequence number pointer
                                );
                        if( status != ZPS_E_SUCCESS )
                        {
                            // problem with request
                            DBG_vPrintf( TRACE_APP,
                                    "APP: AckDataReq not successful. Return: 0x%x\n",
                                    status );
                        }
                        else
                        {
                            // TX data request successful

                        }
                    }
                }
            }

            break;
        }
        case '%':
        {
            // Permit Joining command
            // allow node joining for X seconds

            PermitJoining();

            break;
        }
        default:
            DBG_vPrintf( TRACE_APP, "UART Unrecognized Command" );
        }

        // free up the buffer for the next command
        uartRxBufferIndex = 0;
        commandReady = FALSE;
    }
}


void SendAuthCode( uint16 destination )
{
    PDUM_teStatus status;

    DBG_vPrintf(TRACE_APP, "AUTH: Sending Auth Code\n");

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
                "w",    // data format string
                AUTH_CODE
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
                    destination,            // destination network address
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
void APP_vtaskMyEndPoint( void )
{
    ZPS_tsAfEvent sStackEvent;
    sStackEvent.eType = ZPS_EVENT_NONE;

    /* check if any messages to collect */
    ZQ_bQueueReceive( &APP_msgStrainGaugeEvents, &sStackEvent );

    if( ZPS_EVENT_NONE != sStackEvent.eType )
    {
        switch( sStackEvent.eType )
        {
        case ZPS_EVENT_APS_DATA_INDICATION:
        {
            DBG_vPrintf( TRACE_APP, "APP: APP_taskEndPoint: ZPS_EVENT_AF_DATA_INDICATION\n" );

            /* Process incoming cluster messages for this endpoint... */
            DBG_vPrintf( TRACE_APP, "    Data Indication:\n" );
            DBG_vPrintf( TRACE_APP, "        Status  : %x\n",
                    sStackEvent.uEvent.sApsDataIndEvent.eStatus );
            DBG_vPrintf( TRACE_APP, "        Profile : %x\n",
                    sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId );
            DBG_vPrintf( TRACE_APP, "        Cluster : %x\n",
                    sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId );
            DBG_vPrintf( TRACE_APP, "        EndPoint: %x\n",
                    sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint );

            uint8 lqi = sStackEvent.uEvent.sApsDataIndEvent.u8LinkQuality;
            DBG_vPrintf( TRACE_APP, "        LQI     : %d\n", lqi );

            uint64 macAddress = ZPS_u64AplZdoLookupIeeeAddr(
                    sStackEvent.uEvent.sApsDataIndEvent.uSrcAddress.u16Addr );
            DBG_vPrintf( TRACE_APP, "        MAC Address: %08x%08x (%04x)\n",
                    (uint32 )(macAddress >> 32), (uint32 )macAddress,
                    sStackEvent.uEvent.sApsDataIndEvent.uSrcAddress.u16Addr );

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
                case '*':
                {
                    DBG_vPrintf( TRACE_APP, "    ADC Values:\n" );

                    struct
                    {
                        uint16 sampleValue;
                        uint16 batteryValue;
                    } values = { 0, 0 };

                    byteCount = PDUM_u16APduInstanceReadNBO(
                            sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
                            1,
                            "hh",
                            &values
                            );
                    if( byteCount == 4 )
                    {
                        DBG_vPrintf( TRACE_APP, "        sampleValue  = 0x%04x\n",
                                values.sampleValue );
                        DBG_vPrintf( TRACE_APP, "        batteryValue = 0x%04x\n",
                                values.batteryValue );
#if SBC_UART_DISABLE == 0
                        char dataString[24] = { 0 };
                        uint8 i = 0;
                        i += u16ToHex( &dataString[i], (uint16) (macAddress & 0xFFFF) );
                        dataString[i++] = ',';
                        i += u16ToHex( &dataString[i], (uint16) lqi );
                        dataString[i++] = ':';
                        dataString[i++] = '*';
                        i += u16ToHex( &dataString[i], values.sampleValue );
                        dataString[i++] = ',';
                        i += u16ToHex( &dataString[i], values.batteryValue );
                        dataString[i++] = '\n';

                        u16AHI_UartBlockWriteData( E_AHI_UART_1, (uint8 *) dataString,
                                strlen( dataString ) );
#endif
                    }
                    else
                    {
                        // unexpected number of read bytes

                    }

                    break;
                }
                case '!':
                {
                    DBG_vPrintf( TRACE_APP, "\n    Auth Code Request\n" );

                    SendAuthCode( sStackEvent.uEvent.sApsDataIndEvent.uSrcAddress.u16Addr );

                    break;
                }
                default:
                    DBG_vPrintf( TRACE_APP, "Unrecognized Packet ID: 0x%x\n", idByte );
                    break;
                }
            }

            /* free the application protocol data unit (APDU) once it has been dealt with */
            PDUM_eAPduFreeAPduInstance( sStackEvent.uEvent.sApsDataIndEvent.hAPduInst );

            break;
        }
        case ZPS_EVENT_APS_DATA_CONFIRM:
        {
            DBG_vPrintf( TRACE_APP,
                    "APP: APP_taskEndPoint: ZPS_EVENT_APS_DATA_CONFIRM Status %d, Address 0x%04x\n",
                    sStackEvent.uEvent.sApsDataConfirmEvent.u8Status,
                    sStackEvent.uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr );

            break;
        }
        case ZPS_EVENT_APS_DATA_ACK:
        {
            DBG_vPrintf( TRACE_APP,
                    "APP: APP_taskEndPoint: ZPS_EVENT_APS_DATA_ACK Status %d, Address 0x%04x\n",
                    sStackEvent.uEvent.sApsDataAckEvent.u8Status,
                    sStackEvent.uEvent.sApsDataAckEvent.u16DstAddr );

            break;
        }
        default:
        {
            DBG_vPrintf( TRACE_APP, "APP: APP_taskEndPoint: unhandled event %d\n",
                    sStackEvent.eType );

            break;
        }
        }
    }

#if SBC_UART_DISABLE == 0
    ProcessUART();
#endif
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
PUBLIC void APP_vGenCallback( uint8 u8Endpoint, ZPS_tsAfEvent *psStackEvent )
{
    if( u8Endpoint == 0 )
    {
        ZQ_bQueueSend( &APP_msgZpsEvents, (void*) psStackEvent );
    }
    else
    {
        ZQ_bQueueSend( &APP_msgStrainGaugeEvents, (void*) psStackEvent );
    }
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
