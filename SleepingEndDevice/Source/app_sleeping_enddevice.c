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

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#ifndef DEBUG_APP
	#define TRACE_APP 	FALSE
#else
	#define TRACE_APP 	TRUE
#endif

#define RESTART_TIME    APP_TIME_MS(1000)


/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE void vStartup(void);
PRIVATE void vWaitForNetworkDiscovery(ZPS_tsAfEvent sStackEvent);
PRIVATE void vWaitForNetworkJoin(ZPS_tsAfEvent sStackEvent);
PRIVATE void vHandleStackEvent(ZPS_tsAfEvent sStackEvent);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
PUBLIC pwrm_tsWakeTimerEvent sWake;

extern bool_t configPressed_sed;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE uint64 currentEpid = 0;
PRIVATE bool_t networkWakeup = TRUE;
PRIVATE bool_t networkFlex = FALSE;

PRIVATE uint64 blacklistEpids[BLACKLIST_MAX] = { 0 };
PRIVATE uint8  blacklistIndex = 0;
PRIVATE tsBeaconFilterType discoverFilter;


PUBLIC tsDeviceDesc s_eDeviceState;
PUBLIC uint8 au8DefaultTCLinkKey[16]    = {0x5a, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6c,
                                           0x6c, 0x69, 0x61, 0x6e, 0x63, 0x65, 0x30, 0x39};
/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
extern void AppWakeRoutine();

void AuthNetwork();
void BlacklistNetwork();
void JoinHandler();
void LeaveNetwork();
void RestartNetwork();

void AuthNetwork()
{
    /* save the EPID for rejoins */
    currentEpid = ZPS_u64AplZdoGetNetworkExtendedPanId();
    ZPS_eAplAibSetApsUseExtendedPanId( currentEpid );

    PDM_eSaveRecordData(
            PDM_APP_ID_EPID,
            &currentEpid,
            sizeof(currentEpid) );


    s_eDeviceState.eNodeState = E_RUNNING;

    // successfully connected to a network, clear FLEX flag
    networkFlex = FALSE;

    /* Save the application state to flash. Note that all records may be saved at any time by the PDM:
     * if a module has called PDM_vSaveRecord(), but there is insufficient spare memory, an erase is performed
     * followed by a write of all records.
     */
    PDM_eSaveRecordData( PDM_ID_APP_SED,
                         &s_eDeviceState,
                         sizeof(s_eDeviceState) );
}

void BlacklistNetwork()
{
    // add EPID to blacklist
    DBG_vPrintf(TRACE_APP, "AUTH: Blacklisting Network: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
    blacklistEpids[ blacklistIndex++ ] = ZPS_u64AplZdoGetNetworkExtendedPanId();

    ZPS_eAplAibSetApsUseExtendedPanId(0);

    LeaveNetwork();
}

void JoinHandler()
{
    if( currentEpid == 0 )
    {
        s_eDeviceState.eNodeState = E_AUTH_REQ;

        DBG_vPrintf(
                TRACE_APP,
                "!" );
        ZPS_eAplZdoPoll();
    }
    else
    {
        // currentEpid previously set signalling a rejoin

        s_eDeviceState.eNodeState = E_RUNNING;

        // successfully connected to a network, clear FLEX flag
        networkFlex = FALSE;

        /* Save the application state to flash. Note that all records may be saved at any time by the PDM:
         * if a module has called PDM_vSaveRecord(), but there is insufficient spare memory, an erase is performed
         * followed by a write of all records.
         */
        PDM_eSaveRecordData(
                PDM_ID_APP_SED,
                &s_eDeviceState,
                sizeof(s_eDeviceState) );
    }
}

void LeaveNetwork()
{
    ZPS_teStatus status = ZPS_eAplZdoLeaveNetwork(
            0,
            FALSE,
            FALSE );
    if( status != ZPS_E_SUCCESS )
    {
        DBG_vPrintf(TRACE_APP, "AUTH: LeaveNetwork Request Failed. Status: 0x%x\n", status);
    }
}

void RestartNetwork()
{
    s_eDeviceState.eNodeState = E_STARTUP;
}


PUBLIC void vWakeCallBack(void)
{
	networkWakeup = TRUE;

    AppWakeRoutine();
}

PRIVATE void vWaitForAuthCode( ZPS_tsAfEvent sStackEvent )
{
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

    case ZPS_EVENT_APS_DATA_INDICATION:
    {
        DBG_vPrintf(TRACE_APP, "AUTH: ZPS_EVENT_AF_DATA_INDICATION\n");

        /* Process incoming cluster messages ... */
        DBG_vPrintf(TRACE_APP, "        Profile :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId);
        DBG_vPrintf(TRACE_APP, "        Cluster :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId);
        DBG_vPrintf(TRACE_APP, "        EndPoint:%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);

        /* free the application protocol data unit (APDU) once it has been dealt with */
        PDUM_eAPduFreeAPduInstance( sStackEvent.uEvent.sApsDataIndEvent.hAPduInst );
    }
    break;

    case ZPS_EVENT_NWK_LEAVE_CONFIRM:
    {
        DBG_vPrintf(TRACE_APP, "AUTH: ZPS_EVENT_NWK_LEAVE_CONFIRM, status = %d\n",
                sStackEvent.uEvent.sNwkLeaveConfirmEvent.eStatus );
        // we have left the network, now we can restart

        RestartNetwork();
    }
    break;

    case ZPS_EVENT_NWK_POLL_CONFIRM:
    {
        uint8 pollStatus = sStackEvent.uEvent.sNwkPollConfirmEvent.u8Status;

        DBG_vPrintf(TRACE_APP, "AUTH: ZPS_EVENT_NEW_POLL_COMPLETE, status = %d\n",
                pollStatus );

        DBG_vPrintf(TRACE_APP, "!");
        ZPS_eAplZdoPoll();
    }
    break;

    default:
    {
        DBG_vPrintf(TRACE_APP, "APP_AUTH: Unexpected event - %d\n", sStackEvent.eType);
    }
    break;

    }
}

/****************************************************************************
 *
 * NAME: APP_vInitialiseSleepingEndDevice
 *
 * DESCRIPTION:
 * Initialises the Sleeping End Device application
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void APP_vInitialiseSleepingEndDevice(void)
{
    bool_t bDeleteRecords = /*TRUE*/ FALSE;
    uint16 u16DataBytesRead;

    /* If required, at this point delete the network context from flash, perhaps upon some condition
     * For example, check if a button is being held down at reset, and if so request the Persistent
     * Data Manager to delete all its records:
     * e.g. bDeleteRecords = vCheckButtons();
     * Alternatively, always call PDM_vDelete() if context saving is not required.
     */
    if (bDeleteRecords)
    {
        DBG_vPrintf(TRACE_APP, "APP: Deleting all records from flash\n");
        PDM_vDeleteAllDataRecords();
    }


    /* Restore any application data previously saved to flash
     * All Application records must be loaded before the call to
     * ZPS_eAplAfInit
     */
    s_eDeviceState.eNodeState = E_STARTUP;
    PDM_eReadDataFromRecord(PDM_ID_APP_SED,
                    		&s_eDeviceState,
                    		sizeof(s_eDeviceState),
                    		&u16DataBytesRead);


    currentEpid = 0;
    PDM_eReadDataFromRecord(
            PDM_APP_ID_EPID,
            &currentEpid,
            sizeof(currentEpid),
            &u16DataBytesRead
    );


    /* Initialise ZBPro stack */
    ZPS_eAplAfInit();

    ZPS_vAplSecSetInitialSecurityState(ZPS_ZDO_PRECONFIGURED_LINK_KEY,
                                       au8DefaultTCLinkKey,
                                       0x00,
                                       ZPS_APS_GLOBAL_LINK_KEY);

    /* Initialise other software modules
     * HERE
     */

    /* Always initialise any peripherals used by the application
     * HERE
     */

    /* If the device state has been restored from flash, re-start the stack
     * and set the application running again. Note that if there is more than 1 state
     * where the network has already joined, then the other states should also be included
     * in the test below
     * E.g. E_RUNNING_1, E_RUNNING_2......
     * if (E_RUNNING_1 == s_eDeviceState || E_RUNNING_2 == s_eDeviceState)
     */
    if (E_RUNNING == s_eDeviceState.eNodeState)
    {
        ZPS_teStatus eStatus = ZPS_eAplZdoStartStack();
        DBG_vPrintf(TRACE_APP, "APP: Re-starting Stack....\r\n");
        if (ZPS_E_SUCCESS != eStatus)
        {
            DBG_vPrintf(TRACE_APP, "APP: ZPS_eZdoStartStack() failed error %d", eStatus);
        }
    }
    else /* perform any actions require on initial start-up */
    {
        /* Return the device to the start-up state if it was reset during the network formation stage */
        s_eDeviceState.eNodeState = E_STARTUP;
    }
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
    if( configPressed_sed )
    {
        configPressed_sed = FALSE;
        networkFlex = TRUE;
    }

    ZPS_tsAfEvent sStackEvent;

    sStackEvent.eType = ZPS_EVENT_NONE;
    ZQ_bQueueReceive(&APP_msgZpsEvents, &sStackEvent);

    switch (s_eDeviceState.eNodeState)
    {
        case E_STARTUP:
        {
            vStartup();
        }
        break;

        case E_DISCOVERY:
        {
        	vWaitForNetworkDiscovery(sStackEvent);
        }
        break;

        case E_JOINING_NETWORK:
        {
        	vWaitForNetworkJoin(sStackEvent);
        }
        break;

        case E_AUTH_REQ:
        {
            vWaitForAuthCode(sStackEvent);
        }
        break;

        case E_RUNNING:
        {
        	vHandleStackEvent(sStackEvent);
        }
        break;

        default:
        {
        	/* Do nothing */
            DBG_vPrintf(TRACE_APP, "SED: Unhandled State : %d\n", s_eDeviceState.eNodeState);
        }
        break;
    }
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/****************************************************************************
 *
 * NAME: vStartup
 *
 * DESCRIPTION:
 * Start the process of network discovery
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void vStartup(void)
{
    /* The Startup state is used in the following cases:
     *      - before a successful connection to a network
     *      - when connection to a network is lost (after X unsuccessful communications)
     */

    /* Start the network stack as a end device */
    DBG_vPrintf(TRACE_APP, "STARTUP: Starting ZPS\n");


    if( currentEpid != 0 )
    {
        // currentEpid is set to target network
        DBG_vPrintf(TRACE_APP, "STARTUP: Attempting Rejoin of 0x%016llx\n", currentEpid);
    }
    else
    {
        // no target network
        DBG_vPrintf(TRACE_APP, "STARTUP: Attempting Discovery of New Network\n");

        // create Beacon filter
        discoverFilter.pu64ExtendPanIdList = blacklistEpids;
        discoverFilter.u8ListSize = blacklistIndex;
        discoverFilter.u16FilterMap = ( BF_BITMAP_BLACKLIST );

        ZPS_bAppAddBeaconFilter( &discoverFilter );
        // according to the ZigBee PRO Stack UG (JN-UG-3101), the u16FilterMap is cleared
        // after a join which effectively disables the beacon filter.
        // Therefore, there should be no need to manually disable the filter later on.
    }

    ZPS_teStatus eStatus = ZPS_eAplZdoStartStack();

    if (ZPS_E_SUCCESS == eStatus)
    {
        s_eDeviceState.eNodeState = E_DISCOVERY;
    }
    else
    {
        DBG_vPrintf(TRACE_APP, "STARTUP: Failed to Start Stack. Status: 0x%02x\n", eStatus);
    }


}

void ResetNetwork()
{
    PDM_vDeleteDataRecord( PDM_APP_ID_EPID );
    currentEpid = 0;

    // clear blacklist
    for( blacklistIndex = 0; blacklistIndex < BLACKLIST_MAX; blacklistIndex++ )
    {
        blacklistEpids[blacklistIndex] = 0;
    }
    blacklistIndex = 0;

    ZPS_eAplAibSetApsUseExtendedPanId( 0 );
}

/****************************************************************************
 *
 * NAME: vWaitForNetworkDiscovery
 *
 * DESCRIPTION:
 * Check for and act upon stack events during network discovery.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  sStackEvent     R   Contains details of stack event
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void vWaitForNetworkDiscovery(ZPS_tsAfEvent sStackEvent)
{
    if (ZPS_EVENT_NONE != sStackEvent.eType)
    {
        if (ZPS_EVENT_NWK_DISCOVERY_COMPLETE == sStackEvent.eType)
        {
            DBG_vPrintf(TRACE_APP, "DISCOVER: Network discovery complete\n");

            if ((ZPS_E_SUCCESS == sStackEvent.uEvent.sNwkDiscoveryEvent.eStatus) ||
                (ZPS_NWK_ENUM_NEIGHBOR_TABLE_FULL == sStackEvent.uEvent.sNwkDiscoveryEvent.eStatus))
            {
                DBG_vPrintf(TRACE_APP, "DISCOVER: Found %d networks\n", sStackEvent.uEvent.sNwkDiscoveryEvent.u8NetworkCount);
            }
            else
            {
                DBG_vPrintf(TRACE_APP, "DISCOVER: Network discovery failed with error %d\n",sStackEvent.uEvent.sNwkDiscoveryEvent.eStatus);
            }
            if (0 == sStackEvent.uEvent.sNwkDiscoveryEvent.u8NetworkCount
                || 0xff == sStackEvent.uEvent.sNwkDiscoveryEvent.u8SelectedNetwork)
            {
                // no network found and no more channels to scan
                DBG_vPrintf(TRACE_APP, "DISCOVER: Exhausted channels to scan\n");

                if( networkFlex )
                {
                    networkFlex = FALSE;

                    // no networks found AND Config button was pressed
                    ResetNetwork();
                }

                RestartNetwork();
            }
            else
            {
                uint8 networkIndex = sStackEvent.uEvent.sNwkDiscoveryEvent.u8SelectedNetwork;
                ZPS_tsNwkNetworkDescr *psNwkDescr = &sStackEvent.uEvent.sNwkDiscoveryEvent.psNwkDescriptors[ networkIndex ];

                DBG_vPrintf(TRACE_APP, "DISCOVER: Unscanned channels 0x%08x\n", sStackEvent.uEvent.sNwkDiscoveryEvent.u32UnscannedChannels);
                DBG_vPrintf(TRACE_APP, "\tDISCOVER: Ext PAN ID = 0x%016llx\n", psNwkDescr->u64ExtPanId);
                DBG_vPrintf(TRACE_APP, "\tDISCOVER: Channel = %d\n", psNwkDescr->u8LogicalChan);
                DBG_vPrintf(TRACE_APP, "\tDISCOVER: Stack Profile = %d\n", psNwkDescr->u8StackProfile);
                DBG_vPrintf(TRACE_APP, "\tDISCOVER: Zigbee Version = %d\n", psNwkDescr->u8ZigBeeVersion);
                DBG_vPrintf(TRACE_APP, "\tDISCOVER: Permit Joining = %d\n", psNwkDescr->u8PermitJoining);
                DBG_vPrintf(TRACE_APP, "\tDISCOVER: Router Capacity = %d\n", psNwkDescr->u8RouterCapacity);
                DBG_vPrintf(TRACE_APP, "\tDISCOVER: End Device Capacity = %d\n", psNwkDescr->u8EndDeviceCapacity);


                ZPS_teStatus eStatus = ZPS_eAplZdoJoinNetwork(psNwkDescr);
                if (ZPS_E_SUCCESS == eStatus)
                {
                    DBG_vPrintf(TRACE_APP, "DISCOVER: Joining network\n");
                    s_eDeviceState.eNodeState = E_JOINING_NETWORK;
                }
                else
                {
                    /* start scan again */
                    DBG_vPrintf(TRACE_APP, "DISCOVER: Failed to request network join : 0x%02x\n", eStatus);
                    RestartNetwork();
                }

            }
        }
        else if (ZPS_EVENT_NWK_FAILED_TO_JOIN == sStackEvent.eType)
        {
            // REJOIN failed

            DBG_vPrintf(TRACE_APP, "DISCOVER: Network discovery failed error= 0x%02x\n", sStackEvent.uEvent.sNwkJoinFailedEvent.u8Status);
            if( networkFlex )
            {
                networkFlex = FALSE;

                // no networks found AND Config button was pressed
                ResetNetwork();
            }

            RestartNetwork();
        }
        else if (ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE == sStackEvent.eType)
        {
            // REJOIN successful

            DBG_vPrintf(TRACE_APP, "DISCOVER: Node rejoined network with Addr 0x%04x\n",
                        sStackEvent.uEvent.sNwkJoinedEvent.u16Addr);

            JoinHandler();
        }
        else
        {
            DBG_vPrintf(TRACE_APP, "DISCOVER: Unexpected event - 0x%04x\n", sStackEvent.eType);
        }
    }
}


/****************************************************************************
 *
 * NAME: vWaitForNetworkJoin
 *
 * DESCRIPTION:
 * Check for and act upon stack events during network join.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  sStackEvent     R   Contains details of stack event
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void vWaitForNetworkJoin(ZPS_tsAfEvent sStackEvent)
{
    switch( sStackEvent.eType )
    {

    case ZPS_EVENT_NONE:
    {
    }
    break;

    case ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE:
    {
        DBG_vPrintf(TRACE_APP, "JOIN: Node joined network with Addr 0x%04x\n",
                    sStackEvent.uEvent.sNwkJoinedEvent.u16Addr);

        JoinHandler();
    }
    break;

    case ZPS_EVENT_NWK_POLL_CONFIRM:
    {
        uint8 pollStatus = sStackEvent.uEvent.sNwkPollConfirmEvent.u8Status;

        DBG_vPrintf(TRACE_APP, "JOIN: ZPS_EVENT_NEW_POLL_COMPLETE, status = %d\n",
                pollStatus );
    }
    break;

    case ZPS_EVENT_NWK_LEAVE_INDICATION:
    {
        uint64 nodeAddress = sStackEvent.uEvent.sNwkLeaveIndicationEvent.u64ExtAddr;

        DBG_vPrintf(TRACE_APP, "JOIN: Network Leave Indication : 0x%llx\n", nodeAddress);
    }
    break;

    case ZPS_EVENT_NWK_FAILED_TO_JOIN:
    {
        DBG_vPrintf(TRACE_APP, "JOIN: Node failed to join network. Status: 0x%x\n",
                sStackEvent.uEvent.sNwkJoinFailedEvent.u8Status );
        RestartNetwork();
    }
    break;

    default:
    {
        DBG_vPrintf(TRACE_APP, "JOIN: Unexpected event - %d\n", sStackEvent.eType);
    }
    break;

    }
}



/****************************************************************************
 *
 * NAME: vHandleStackEvent
 *
 * DESCRIPTION:
 * Check for and act upon any valid stack event. This function should be called
 * after node has formed network.
 *
 * PARAMETERS:      Name            RW  Usage
 *                  sStackEvent     R   Contains details of stack event
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void vHandleStackEvent(ZPS_tsAfEvent sStackEvent)
{
	if( networkWakeup )
	{
		networkWakeup = FALSE;
		DBG_vPrintf(TRACE_APP, "#");
		ZPS_eAplZdoPoll();
	}

    if (ZPS_EVENT_NONE != sStackEvent.eType)
    {
        switch (sStackEvent.eType)
        {
            case ZPS_EVENT_APS_DATA_INDICATION:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_AF_DATA_INDICATION\n");

                /* Process incoming cluster messages ... */
                DBG_vPrintf(TRACE_APP, "\tProfile :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId);
                DBG_vPrintf(TRACE_APP, "\tCluster :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId);
                DBG_vPrintf(TRACE_APP, "\tEndPoint:%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);

                /* free the application protocol data unit (APDU) once it has been dealt with */
                PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);
            }
            break;

            case ZPS_EVENT_APS_DATA_CONFIRM:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_APS_DATA_CONFIRM Status %d, Address 0x%04x\n",
                            sStackEvent.uEvent.sApsDataConfirmEvent.u8Status,
                            sStackEvent.uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr);
            }
            break;

            case ZPS_EVENT_APS_DATA_ACK:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_APS_DATA_ACK : Status 0x%02x, Address 0x%04x\n",
                            sStackEvent.uEvent.sApsDataAckEvent.u8Status,
                            sStackEvent.uEvent.sApsDataAckEvent.u16DstAddr);
            }
            break;

            case ZPS_EVENT_NWK_NEW_NODE_HAS_JOINED:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_NEW_NODE_HAS_JOINED, Nwk Addr=0x%04x\n",
                            sStackEvent.uEvent.sNwkJoinIndicationEvent.u16NwkAddr);
            }
            break;

            case ZPS_EVENT_NWK_LEAVE_INDICATION:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_LEAVE_INDICATION\n");
            }
            break;

            case ZPS_EVENT_NWK_LEAVE_CONFIRM:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_LEAVE_CONFIRM\n");
            }
            break;

            case ZPS_EVENT_NWK_STATUS_INDICATION:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_NWK_STATUS_INDICATION : Status 0x%02x, Address 0x%04x\n",
                                            sStackEvent.uEvent.sNwkStatusIndicationEvent.u8Status,
                                            sStackEvent.uEvent.sNwkStatusIndicationEvent.u16NwkAddr);
            }
            break;

            case ZPS_EVENT_NWK_ROUTE_DISCOVERY_CONFIRM:
            {
                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_ROUTE_DISCOVERY_CFM\n");
            }
            break;

            case ZPS_EVENT_ERROR:
            {
                DBG_vPrintf(TRACE_APP, "RUN: Monitor Sensors ZPS_EVENT_ERROR\n");
                DBG_vPrintf(TRACE_APP, "    Error Code %d\n", sStackEvent.uEvent.sAfErrorEvent.eError);

                if (ZPS_ERROR_OS_MESSAGE_QUEUE_OVERRUN == sStackEvent.uEvent.sAfErrorEvent.eError)
                {
                    DBG_vPrintf(TRACE_APP, "    Queue handle %d\n", sStackEvent.uEvent.sAfErrorEvent.uErrorData.sAfErrorOsMessageOverrun.hMessage);
                }
            }
            break;

            case ZPS_EVENT_NWK_POLL_CONFIRM:
            {
                uint8 pollStatus = sStackEvent.uEvent.sNwkPollConfirmEvent.u8Status;

                DBG_vPrintf(TRACE_APP, "RUN: ZPS_EVENT_NEW_POLL_COMPLETE, status = %d\n",
                        pollStatus );

                if( (pollStatus == ZPS_NWK_ENUM_SUCCESS) || (pollStatus == MAC_ENUM_NO_DATA) )
                {
                    // successfully poll, clear FLEX flag
                    networkFlex = FALSE;
                }
            }
            break;

            case ZPS_EVENT_NWK_FAILED_TO_JOIN:
            {
                RestartNetwork();
            }
            break;

            case ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE:
            case ZPS_EVENT_NWK_STARTED:
            case ZPS_EVENT_NWK_FAILED_TO_START:
            case ZPS_EVENT_NWK_DISCOVERY_COMPLETE:
                /* Deliberate fall through */
            default:
            {
                DBG_vPrintf(TRACE_APP, "RUN: unhandled event 0x%02x\n", sStackEvent.eType);
            }
            break;
        }
    }
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
