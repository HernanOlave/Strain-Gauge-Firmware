/**
 * @file nwk_api.c
 * @brief
 *
 * @author Wisely SpA
 * @date 21-Sep-19
 *
 */

/****************************************************************************/
/***        Libraries                                                     ***/
/****************************************************************************/

#include <jendefs.h>
#include <app_pdm.h>

#include "pwrm.h"
#include "pdum_nwk.h"
#include "pdum_apl.h"
#include "pdum_gen.h"
#include "PDM.h"
#include "dbg.h"
#include "dbg_uart.h"
#include "rnd_pub.h"
#include "zps_gen.h"
#include "zps_apl.h"
#include "zps_apl_af.h"
#include "zps_apl_zdo.h"
#include "zps_tsv.h"
#include "AppApi.h"

#include "nwk_api.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#ifndef DEBUG_APP
	#define TRACE_APP 		FALSE
#else
	#define TRACE_APP 		TRUE
#endif

#define BLACKLIST_MAX   	32		// max number of blacklist EPIDs
#define MAX_NO_NWK_STRIKES	3		// times (fixed by ZPS_Config Editor)
#define RX_BUFFER_SIZE		50		//

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef struct
{
	uint64				currentEpid;
	bool				isConnected;
	bool				isAuthenticated;
	uint8				ackStrikes;
	uint8				noNwkStrikes;
	uint8				rejoinStrikes;
	uint8				authStrikes;
	pollReturnValues_t	pollStatus;
} networkDesc_t;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE networkDesc_t s_network;
PRIVATE uint8 au8DefaultTCLinkKey[16] = "ZigBeeAlliance09";
PRIVATE tszQueue APP_msgStrainGaugeEvents;
PRIVATE tszQueue APP_msgZpsEvents;
PRIVATE uint64 blacklistEpids[BLACKLIST_MAX] = { 0 };
PRIVATE uint8  blacklistIndex = 0;
PRIVATE tsBeaconFilterType discoverFilter;
PRIVATE uint16 rxBuffer[RX_BUFFER_SIZE];

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void networkDiscovery_handler(ZPS_tsAfEvent sStackEvent);
PRIVATE void networkPoll_handler(ZPS_tsAfEvent sStackEvent);
PRIVATE void networkData_handler(ZPS_tsAfEvent sStackEvent);
PRIVATE void blacklistNetwork(void);

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

PRIVATE void networkDiscovery_handler(ZPS_tsAfEvent sStackEvent)
{
	ZPS_teStatus eStatus;

	/* If there is any error in the discovery process stops */
	if(sStackEvent.uEvent.sNwkDiscoveryEvent.eStatus != MAC_ENUM_SUCCESS)
	{
		DBG_vPrintf
		(
			TRACE_APP,
			"  NWK: Network discovery failed, status = %02X\n\r",
			sStackEvent.uEvent.sNwkDiscoveryEvent.eStatus
		);
		//TODO: Hanlde error
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
			}
			else if (eStatus == ZPS_NWK_ENUM_NOT_PERMITTED)
			{
				DBG_vPrintf
				(
					TRACE_APP,
					"  NWK: Join not permitted, status = %02X\n\r",
					eStatus
				);
				//TODO: Handle error
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
					"  NWK: Failed to request network join, status = %02X\n\r",
					eStatus
				);
				//TODO: Handle ERROR
			}
		}
	}
}

PRIVATE void networkPoll_handler(ZPS_tsAfEvent sStackEvent)
{
	ZPS_teStatus eStatus;

	eStatus = sStackEvent.uEvent.sNwkPollConfirmEvent.u8Status;
	DBG_vPrintf(TRACE_APP, "  NWK: Status = %02X\n\r", eStatus);

	/* No new data */
	if(eStatus == MAC_ENUM_NO_DATA)
	{
		DBG_vPrintf(TRACE_APP,"  NWK: No new Data\n\r");
		s_network.pollStatus = NWK_NO_MESSAGE;
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
		if(s_network.noNwkStrikes >= MAX_NO_NWK_STRIKES)
		{
			s_network.noNwkStrikes = 0;
			s_network.isConnected = FALSE;
			DBG_vPrintf(TRACE_APP,"  NWK: Connection lost\n\r");
		}
	}
	else /* unexpected status */
	{
		DBG_vPrintf
		(
			TRACE_APP,
			"  NWK: Unexpected poll complete, status = %02X\n\r",
			eStatus
		);
	}
}

PRIVATE void networkData_handler(ZPS_tsAfEvent sStackEvent)
{
	/* Process incoming cluster messages ... */
	DBG_vPrintf(TRACE_APP, "    Profile :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ProfileId);
	DBG_vPrintf(TRACE_APP, "    Cluster :%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u16ClusterId);
	DBG_vPrintf(TRACE_APP, "    EndPoint:%x\r\n",sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);

	uint16 payloadSize, byteCount;

	payloadSize = PDUM_u16APduInstanceGetPayloadSize(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);

	byteCount = PDUM_u16APduInstanceReadNBO
	(
		sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
		0,
		"b",
		&rxBuffer
	);

	DBG_vPrintf( TRACE_APP, "  NWK: byteCount  = %d - idByte = %c\n\r", byteCount, rxBuffer[0]);
	DBG_vPrintf( TRACE_APP, "  NWK: payloadSize  = %d\n\r", payloadSize);

	uint16 offset, index;

	offset = 1;
	index = 1;

	while (offset <= payloadSize - 2)
	{

		offset += PDUM_u16APduInstanceReadNBO
		(
			sStackEvent.uEvent.sApsDataIndEvent.hAPduInst,
			offset,
			"h",
			&rxBuffer[index++]
		);
	}

	/* free the application protocol data unit (APDU) once it has been dealt with */
	PDUM_eAPduFreeAPduInstance(sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);
}

PRIVATE void blacklistNetwork(void)
{
    /* Add EPID to blacklist */
	DBG_vPrintf(TRACE_APP, "  APP: Blacklisting Network: 0x%016llx\n\r", ZPS_u64AplZdoGetNetworkExtendedPanId());
    blacklistEpids[ blacklistIndex++ ] = ZPS_u64AplZdoGetNetworkExtendedPanId();

    ZPS_eAplAibSetApsUseExtendedPanId(0);

    s_network.isConnected = FALSE;
    s_network.isAuthenticated = FALSE;

    /* Leave request */
    ZPS_teStatus status = ZPS_eAplZdoLeaveNetwork
    (
    	0,
        FALSE,
        FALSE
    );
    if( status != ZPS_E_SUCCESS )
    {
        DBG_vPrintf(TRACE_APP, " APP: LeaveNetwork Request Failed, status = %02X\n\r", status);
    }
}

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void nwk_init(void)
{
	DBG_vPrintf(TRACE_APP, "  NWK: Initializing network API\n\r");

	s_network.isAuthenticated = FALSE;
	s_network.isConnected = FALSE;

	s_network.ackStrikes = 0;
	s_network.noNwkStrikes = 0;
	s_network.rejoinStrikes = 0;
	s_network.authStrikes = 0;

	s_network.pollStatus = NWK_NO_EVENT;

	/* Initialize ZBPro stack */
	ZPS_eAplAfInit();

	nwk_api_discovery();
}

PUBLIC void nwk_taskHandler(void)
{
	ZPS_tsAfEvent sStackEvent;
	sStackEvent.eType = ZPS_EVENT_NONE;

	/* Check if there is any event on the stack */
	if (ZQ_bQueueReceive(&APP_msgZpsEvents, &sStackEvent))
	{
		DBG_vPrintf
		(
			TRACE_APP,
			"  NWK: New event on the stack APP_msgZpsEvents = %02X\n\r",
			sStackEvent.eType
		);
	}
	else if (ZQ_bQueueReceive(&APP_msgStrainGaugeEvents, &sStackEvent))
	{
		DBG_vPrintf
		(
			TRACE_APP,
			"  NWK: New event on the stack APP_msgStrainGaugeEvents = %02X\n\r",
			sStackEvent.eType
		);
	}

	/* Stack handler */
	switch (sStackEvent.eType)
	{
		case ZPS_EVENT_NONE:
		break;

		case ZPS_EVENT_APS_DATA_INDICATION:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_APS_DATA_INDICATION\n\r");
			s_network.pollStatus = NWK_NEW_MESSAGE;
			networkData_handler(sStackEvent);
		}
		break;

		case ZPS_EVENT_APS_DATA_CONFIRM:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_APS_DATA_CONFIRM, status = %02X\n\r",
					sStackEvent.uEvent.sApsDataConfirmEvent.u8Status);
		}
		break;

		case ZPS_EVENT_APS_DATA_ACK:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_APS_DATA_ACK, status = %02X\n\r",
					sStackEvent.uEvent.sApsDataAckEvent.u8Status);
		}
		break;

		case ZPS_EVENT_NWK_LEAVE_INDICATION:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_NWK_LEAVE_INDICATION\n\r");
			s_network.isConnected = FALSE;
		}
		break;

		case ZPS_EVENT_NWK_LEAVE_CONFIRM:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_NWK_LEAVE_CONFIRM\n\r");
			s_network.isConnected = FALSE;
		}
		break;

		case ZPS_EVENT_NWK_POLL_CONFIRM:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_NWK_POLL_CONFIRM\n\r");
			networkPoll_handler(sStackEvent);
		}
		break;

		case ZPS_EVENT_NWK_FAILED_TO_JOIN:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_NWK_FAILED_TO_JOIN\n\r");
		}
		break;

		case ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE\n\r");
			DBG_vPrintf(TRACE_APP, "  NWK: Node joined network with Address 0x%04x\n",
				sStackEvent.uEvent.sNwkJoinedEvent.u16Addr);
			s_network.isConnected = TRUE;
		}
		break;

		case ZPS_EVENT_NWK_DISCOVERY_COMPLETE:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: ZPS_EVENT_NWK_DISCOVERY_COMPLETE\n\r");
			networkDiscovery_handler(sStackEvent);
		}
		break;

		default:
		{
			DBG_vPrintf(TRACE_APP, "  NWK: Unhandled event %02X\n", sStackEvent.eType);
		}
		break;
	}
}

PUBLIC void nwk_discovery(void)
{
	ZPS_teStatus eStatus;

	/* If network parameters were restored, Rejoin */
	if(s_network.currentEpid)
	{
		DBG_vPrintf(TRACE_APP,"  NWK: Trying to rejoin network 0x%016llx\n\r", s_network.currentEpid);

		ZPS_eAplAibSetApsUseExtendedPanId(s_network.currentEpid);

		/* Rejoin stored network without a discovery process */
		eStatus = ZPS_eAplZdoRejoinNetwork(FALSE);

		if (eStatus != ZPS_E_SUCCESS)
		{
			DBG_vPrintf
			(TRACE_APP, "  NWK: Failed rejoin request, status = %02X\n\r", eStatus);
			//TODO: Handle errors
		}
		else
		{
			//TODO: Review this
			s_network.isAuthenticated = TRUE;
		}
	}
	else /* Discovery */
	{
		/* Create Beacon filter */
		discoverFilter.pu64ExtendPanIdList = blacklistEpids;
		discoverFilter.u8ListSize = blacklistIndex;
		discoverFilter.u16FilterMap = (BF_BITMAP_BLACKLIST);

		ZPS_bAppAddBeaconFilter(&discoverFilter);

		/* Reset nwk params */
		void * pvNwk = ZPS_pvAplZdoGetNwkHandle();
		ZPS_vNwkNibSetExtPanId(pvNwk, 0);
		ZPS_eAplAibSetApsUseExtendedPanId(0);

		/* Set security keys */
		ZPS_vAplSecSetInitialSecurityState
		(
			ZPS_ZDO_PRECONFIGURED_LINK_KEY,
			au8DefaultTCLinkKey,
			0x00,
			ZPS_APS_GLOBAL_LINK_KEY
		);

		/* Start the network stack as a end device */
		DBG_vPrintf(TRACE_APP, "  NWK: Starting ZPS\n\r");
		eStatus = ZPS_eAplZdoStartStack();

		if (ZPS_E_SUCCESS != eStatus)
		{
			DBG_vPrintf
			(
				TRACE_APP,
				"  NWK: Failed to Start Stack, status = %02X\n\r",
				eStatus
			);
			//TODO: Handle error
		}
	}
}

PUBLIC void nwk_setEpid(uint64 epid)
{
	s_network.currentEpid = epid;
}

PUBLIC uint64 nwk_getEpid(void)
{
	return s_network.currentEpid;
}

PUBLIC pollReturnValues_t nwk_getPollStatus(void)
{
	uint8 temp = s_network.pollStatus;
	s_network.pollStatus = NWK_NO_EVENT;
	return temp;
}

PUBLIC void nwk_sendData(uint16 * data_ptr, uint16 size)
{
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
		uint16 byteCount, index;
		PDUM_teStatus eStatus;

		for(index = 0; index < size; index++)
		{
			/* Load payload data into APDU */
			byteCount = PDUM_u16APduInstanceWriteNBO
			(
				data,	// APDU instance handle
				index,	// APDU position for data
				"h",	// data format string
				data_ptr[index]
			);
		}

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
			eStatus = ZPS_eAplAfUnicastDataReq
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

			if(eStatus != ZPS_E_SUCCESS)
			{
				/* Problem with request */
				DBG_vPrintf(TRACE_APP, "  APP: AckDataReq not successful, status = %d\n\r", eStatus);
				//TODO: Add strike count and handle error
			}
		}
	}
}

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
