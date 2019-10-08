/**
 * @file app_main.h
 * @brief
 *
 * @author Wisely SpA
 * @date 22-Sep-19
 *
 */

#ifndef APP_MAIN_H_
#define APP_MAIN_H_

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

#include <zps_nwk_pub.h>
#include "portmacro.h"
#include "mac_vs_sap.h"
#include "AppHardwareApi.h"
#include <AHI_ModuleConfiguration.h>

#include "ZTimer.h"
#include "ZQueue.h"

#include "nwk_api.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/*
 * DEVICE_TYPES
 * 0 = NONE
 * 1 = QB1C
 * 2 = HB2C/QB2C
 */
#define DEVICE_TYPE				1

#define VERSION_MAJOR			3
#define VERSION_MINOR			0

#define TIMER_QUEUE_SIZE		8
#define MLME_QUEQUE_SIZE		4
#define MCPS_QUEUE_SIZE			24
#define ZPS_QUEUE_SIZE			2
#define APP_QUEUE_SIZE			2
#define MCPS_DCFM_QUEUE_SIZE	8

#define POLLING_PERIOD			30
#define FAST_POLLING_PERIOD		5

#define SECS_TO_TICKS(seconds)	seconds * 32768
#define STATE_MACHINE_WDG_TIME	ZTIMER_TIME_MSEC(5000)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef enum
{
	CONNECTING_NWK_STATE,
    POLL_DATA_STATE,
    HANDLE_DATA_STATE,
    WAIT_CONFIRM_STATE,
    SEND_DATA_STATE,
    PREP_TO_SLEEP_STATE,
    SLEEP_STATE,
    WAKE_UP_STATE
} sleepingEndDeviceStates_t;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE MAC_tsMcpsVsCfmData asMacMcpsDcfm[MCPS_DCFM_QUEUE_SIZE];
PRIVATE zps_tsTimeEvent asTimeEvent[TIMER_QUEUE_SIZE];
PRIVATE MAC_tsMcpsVsDcfmInd asMacMcpsDcfmInd[MCPS_QUEUE_SIZE];
PRIVATE MAC_tsMlmeVsDcfmInd  asMacMlmeVsDcfmInd[MLME_QUEQUE_SIZE];
PRIVATE ZPS_tsAfEvent asAppEvents[APP_QUEUE_SIZE];
PRIVATE ZPS_tsAfEvent asStackEvents[ZPS_QUEUE_SIZE];

PRIVATE ZTIMER_tsTimer asTimers[1];

PRIVATE bool lockFlag = FALSE;

PRIVATE sleepingEndDeviceStates_t app_currentState;
PRIVATE sleepingEndDeviceStates_t app_previousState;

PRIVATE uint16 APP_rxBuffer[50];
PRIVATE uint16 APP_txBuffer[50];

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

PRIVATE void APP_vMainloop(void);
PRIVATE void APP_vSetUpHardware(void);
PRIVATE void vfExtendedStatusCallBack (ZPS_teExtendedStatus eExtendedStatus);
PRIVATE void APP_stateMachine(void);
PRIVATE void APP_handleData(uint16 * data_ptr);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

PUBLIC uint8 u8TimerWatchdog;

PUBLIC pwrm_tsWakeTimerEvent sPoll;
PUBLIC pwrm_tsWakeTimerEvent sData;

extern tszQueue zps_msgMlmeDcfmInd;
extern tszQueue zps_msgMcpsDcfm;
extern tszQueue zps_msgMcpsDcfmInd;
extern tszQueue zps_TimeEvents;
extern tszQueue APP_msgStrainGaugeEvents;
extern tszQueue APP_msgZpsEvents;
extern uint8 u8App_tmr1sec;
extern void *_stack_low_water_mark;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void vAHI_WatchdogRestart(void);
PUBLIC void PWRM_vManagePower(void);
PUBLIC void zps_taskZPS(void);
PUBLIC void APP_cbTimerWatchdog(void *pvParam);

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

#endif /*APP_MAIN_H_*/
