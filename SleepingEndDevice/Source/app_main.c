/**
 * @file app_main.c
 * @brief
 *
 * @author Wisely SpA
 * @date 22-Sep-19
 *
 */

/****************************************************************************/
/***        Libraries                                                     ***/
/****************************************************************************/

#include "app_main.h"

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

PRIVATE void vfExtendedStatusCallBack (ZPS_teExtendedStatus eExtendedStatus);

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void vAppMain(void)
{
	APP_vSetUpHardware();

    DBG_vPrintf(TRACE_APP, "\n\nAPP: Power Up\n");
    DBG_vPrintf(TRACE_APP, "Device Type: %d\n", DEVICE_TYPE);
    DBG_vPrintf(TRACE_APP, "FW Version: %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
    DBG_vPrintf(TRACE_APP, "Built: %s %s\n\n", __DATE__, __TIME__);

    /*
     * Initialize the stack overflow exception to trigger if the end of the
     * stack is reached. See the linker command file to adjust the allocated
     * stack size.
     */
    vAHI_SetStackOverflow(TRUE, (uint32)&_stack_low_water_mark);

    /* Catch resets due to watchdog timer expiry. */
    if (bAHI_WatchdogResetEvent())
    {
        DBG_vPrintf(TRACE_APP, "APP: Watchdog timer has reset device!\n\r");
    }

    /* Initialize the Z timer module */
    ZTIMER_eInit(asTimers, sizeof(asTimers) / sizeof(ZTIMER_tsTimer));

    /* Create Z timers */
    ZTIMER_eOpen(&u8TimerWatchdog, APP_cbTimerWatchdog,  NULL, ZTIMER_FLAG_PREVENT_SLEEP);

    /* Create Queues */
    ZQ_vQueueCreate(&zps_msgMlmeDcfmInd,         MLME_QUEQUE_SIZE,      sizeof(MAC_tsMlmeVsDcfmInd), (uint8*)asMacMlmeVsDcfmInd);
    ZQ_vQueueCreate(&zps_msgMcpsDcfmInd,         MCPS_QUEUE_SIZE,       sizeof(MAC_tsMcpsVsDcfmInd), (uint8*)asMacMcpsDcfmInd);
    ZQ_vQueueCreate(&zps_TimeEvents,             TIMER_QUEUE_SIZE,      sizeof(zps_tsTimeEvent),     (uint8*)asTimeEvent);
    ZQ_vQueueCreate(&APP_msgZpsEvents,           ZPS_QUEUE_SIZE,        sizeof(ZPS_tsAfEvent),       (uint8*)asStackEvents);
    ZQ_vQueueCreate(&APP_msgStrainGaugeEvents,   APP_QUEUE_SIZE,        sizeof(ZPS_tsAfEvent),       (uint8*)asAppEvents);
	ZQ_vQueueCreate(&zps_msgMcpsDcfm,            MCPS_DCFM_QUEUE_SIZE,  sizeof(MAC_tsMcpsVsCfmData),(uint8*)asMacMcpsDcfm);

    /* Initialize JenOS modules. Initialize Power Manager even on non-sleeping nodes
     * as it allows the device to doze when in the idle task
     */
    PWRM_vInit(E_AHI_SLEEP_OSCON_RAMON);

    /*
     *  Initialise the PDM, use an application supplied key (g_sKey),
     *  The key value can be set to the desired value here, or the key in eFuse can be used.
     *  To use the key stored in eFuse set the pointer to the key to Null, and remove the
     *  key structure here.
     */
	#ifdef PDM_EEPROM
	PDM_eInitialise(63);
	PDM_vRegisterSystemCallback(vPdmEventHandlerCallback);
	#else
	PDM_vInit(7, 1, 64 * 1024 , NULL, NULL, NULL, &g_sKey);
	#endif

    /* Initialize Protocol Data Unit Manager */
    PDUM_vInit();

    /* Register callback that provides information about stack errors */
    ZPS_vExtendedStatusSetCallback(vfExtendedStatusCallBack);

    /* Initialize application API */
    nd005_init();

    /* Enter main loop */
    app_vMainloop();
}

void vAppRegisterPWRMCallbacks(void)
{
    PWRM_vRegisterPreSleepCallback(PreSleep);
    PWRM_vRegisterWakeupCallback(Wakeup);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

PRIVATE void APP_vSetUpHardware(void)
{
	/* Wait until FALSE i.e. on XTAL, otherwise uart data will be at wrong speed */
	while (bAHI_GetClkSource() == TRUE);
	/* Now we are running on the XTAL, optimise the flash memory wait states. */
	vAHI_OptimiseWaitStates();

	/* Initialise the debug diagnostics module to use UART0 at 115K Baud */
	DBG_vUartInit(DBG_E_UART_0, DBG_E_UART_BAUD_RATE_115200);

	vAppApiSetHighPowerMode(APP_API_MODULE_HPM06, TRUE);

	TARGET_INITIALISE();
	/* clear interrupt priority level  */
	SET_IPL(0);
	portENABLE_INTERRUPTS();
}

PRIVATE PWRM_CALLBACK(PreSleep)
{
    vAppApiSaveMacSettings();
    ZTIMER_vSleep();
}

PRIVATE PWRM_CALLBACK(Wakeup)
{
	APP_vSetUpHardware();
    vMAC_RestoreSettings();
    ZTIMER_vWake();
}

PRIVATE void vWakeCallBack(void)
{
	DBG_vPrintf(TRACE_APP, "\n\r\n\r*** WAKE UP ROUTINE ***\n\r");
	DBG_vPrintf(TRACE_APP, "APP: WAKE_UP_STATE\n\r");
	app_currentState = WAKE_UP_STATE;
}

PRIVATE void vfExtendedStatusCallBack (ZPS_teExtendedStatus eExtendedStatus)
{
	DBG_vPrintf(TRACE_APP, "ERROR: Extended status 0x%x\n", eExtendedStatus);
    DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
}

/****************************************************************************/
/***        MAIN LOOP	                                                  ***/
/****************************************************************************/

PRIVATE void app_vMainloop(void)
{
	while (TRUE)
	{
		zps_taskZPS();
		nwk_taskHandler();
		ZTIMER_vTask();
		/* kick the watchdog timer */
		vAHI_WatchdogRestart();
		PWRM_vManagePower();

		/* State machine watchdog */
		if(app_currentState != app_previousState)
		{
			if (app_currentState != SLEEP_STATE)
			{
					ZTIMER_eStop(u8TimerWatchdog);
					ZTIMER_eStart(u8TimerWatchdog, STATE_MACHINE_WDG_TIME);
			}
			app_previousState = app_currentState;
		}

		//if (s_eDevice.systemStrikes >= 5) vAHI_SwReset();

		/* Main State Machine */
		switch (app_currentState)
		{
			case POLL_DATA_STATE:
			{
				uint8 pollStatus = nwk_getPollStatus();

				if(pollStatus == NWK_NEW_MESSAGE)
				{
					DBG_vPrintf(TRACE_APP, "\n\rAPP: HANDLE_DATA_STATE\n\r");
					app_currentState = HANDLE_DATA_STATE;
				}
				else if (pollStatus == NWK_NO_MESSAGE)
				{
					DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");
					app_currentState = PREP_TO_SLEEP_STATE;
				}
			}
			break;

			case HANDLE_DATA_STATE:
			{
				DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");
				app_currentState = PREP_TO_SLEEP_STATE;
			}
			break;

			case SEND_DATA_STATE:
			{

			}
			break;

			case PREP_TO_SLEEP_STATE:
			{
				ZTIMER_eStop(u8TimerWatchdog);

				DBG_vPrintf
				(
					TRACE_APP,
					"APP: Sleep for %d seconds\n\r",
					5
				);

				/* Set wakeup time */
				PWRM_eScheduleActivity
				(
					&sWake,
					SECS_TO_TICKS(5),
					vWakeCallBack
				);

				app_currentState = SLEEP_STATE;
			}
			break;

			case SLEEP_STATE:
			{
				/* Waits until OS sends the device to sleep */
			}
			break;

			case WAKE_UP_STATE:
			{
				ZTIMER_eStart(u8TimerWatchdog, STATE_MACHINE_WDG_TIME);

				/* Poll data from Stack */
				ZPS_eAplZdoPoll();

				DBG_vPrintf(TRACE_APP, "\n\rAPP: POLL_DATA_STATE\n\r");
				app_currentState = POLL_DATA_STATE;

			}
			break;

			default:
			{
				//TODO: Handle error
				DBG_vPrintf
				(
					TRACE_APP,
					"APP: Unhandled State : %d\n\r",
					app_currentState
				);
			}
			break;
		}
	}
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
