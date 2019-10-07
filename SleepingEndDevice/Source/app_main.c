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
#include "nd005_api.h"

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
/***        Local Variables                                               ***/
/****************************************************************************/

PRIVATE PWRM_DECLARE_CALLBACK_DESCRIPTOR(Wakeup);
PRIVATE PWRM_DECLARE_CALLBACK_DESCRIPTOR(PreSleep);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void vAppMain(void)
{
	/* Wait until FALSE i.e. on XTAL  - otherwise uart data will be at wrong speed */
    while (bAHI_GetClkSource() == TRUE);
    /* Now we are running on the XTAL, optimise the flash memory wait states. */
    vAHI_OptimiseWaitStates();

    /* Initialise the debug diagnostics module to use UART0 at 115K Baud;
     * Do not use UART 1 if LEDs are used, as it shares DIO with the LEDS
     */
    DBG_vUartInit(DBG_E_UART_0, DBG_E_UART_BAUD_RATE_115200);

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
    ZTIMER_eStart(u8TimerWatchdog, STATE_MACHINE_WDG_TIME);

    /* Create Queues */
    ZQ_vQueueCreate(&zps_msgMlmeDcfmInd,         MLME_QUEQUE_SIZE,      sizeof(MAC_tsMlmeVsDcfmInd), (uint8*)asMacMlmeVsDcfmInd);
    ZQ_vQueueCreate(&zps_msgMcpsDcfmInd,         MCPS_QUEUE_SIZE,       sizeof(MAC_tsMcpsVsDcfmInd), (uint8*)asMacMcpsDcfmInd);
    ZQ_vQueueCreate(&zps_TimeEvents,             TIMER_QUEUE_SIZE,      sizeof(zps_tsTimeEvent),     (uint8*)asTimeEvent);
    ZQ_vQueueCreate(&APP_msgZpsEvents,           ZPS_QUEUE_SIZE,        sizeof(ZPS_tsAfEvent),       (uint8*)asStackEvents);
    ZQ_vQueueCreate(&APP_msgStrainGaugeEvents,   APP_QUEUE_SIZE,        sizeof(ZPS_tsAfEvent),       (uint8*)asAppEvents);
	ZQ_vQueueCreate(&zps_msgMcpsDcfm,            MCPS_DCFM_QUEUE_SIZE,  sizeof(MAC_tsMcpsVsCfmData),(uint8*)asMacMcpsDcfm);

	APP_vSetUpHardware();

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
	PDM_eInitialise(63);
	PDM_vRegisterSystemCallback(vPdmEventHandlerCallback);

    /* Initialize Protocol Data Unit Manager */
    PDUM_vInit();

    /* Register callback that provides information about stack errors */
    ZPS_vExtendedStatusSetCallback(vfExtendedStatusCallBack);

    /* Initialize application API */
    nd005_init();

    /* Initialize network API */
    nwk_init();

    /* Setup High power module */
    vAppApiSetHighPowerMode(APP_API_MODULE_HPM06, TRUE);

    /* On startup, first state is CONNECTING_NWK_STATE */
    DBG_vPrintf(TRACE_APP, "\n\rAPP: CONNECTING_NWK_STATE\n\r");
    app_currentState = CONNECTING_NWK_STATE;

    /* Enter main loop */
    APP_vMainloop();
}

void vAppRegisterPWRMCallbacks(void)
{
    PWRM_vRegisterPreSleepCallback(PreSleep);
    PWRM_vRegisterWakeupCallback(Wakeup);
}

/****************************************************************************/
/***        MAIN LOOP	                                                  ***/
/****************************************************************************/

PRIVATE void APP_vMainloop(void)
{
	while (TRUE)
	{
		zps_taskZPS();
		nwk_taskHandler();
		APP_stateMachine();
		ZTIMER_vTask();
		/* kick the watchdog timer */
		vAHI_WatchdogRestart();
		PWRM_vManagePower();
	}
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

PRIVATE void APP_vSetUpHardware(void)
{
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
	/* Wait until FALSE i.e. on XTAL  - otherwise uart data will be at wrong speed */
    while (bAHI_GetClkSource() == TRUE);
    /* Now we are running on the XTAL, optimise the flash memory wait states. */
    vAHI_OptimiseWaitStates();

    /* Initialise the debug diagnostics module to use UART0 at 115K Baud;
     * Do not use UART 1 if LEDs are used, as it shares DIO with the LEDS
     */
    DBG_vUartInit(DBG_E_UART_0, DBG_E_UART_BAUD_RATE_115200);

    /* Restore Mac settings (turns radio on) */
    vMAC_RestoreSettings();

    APP_vSetUpHardware();

	DBG_vPrintf(TRACE_APP, "\n\r\n\r*** WAKE UP ROUTINE ***\n\r");

    ZTIMER_vWake();

	nd005_init();
	ZTIMER_eStart(u8TimerWatchdog, STATE_MACHINE_WDG_TIME);
}

PRIVATE void vPollCallBack(void)
{
	if(lockFlag)
	{
		/* Set wakeup time */
		PWRM_eScheduleActivity(&sPoll, SECS_TO_TICKS(1), vPollCallBack);
	}
	else
	{
		lockFlag = TRUE;

		if(!nwk_isConnected())
		{
			/* Find a new network */
			nwk_discovery();
			DBG_vPrintf(TRACE_APP, "\n\rAPP: CONNECTING_NWK_STATE\n\r");
			app_currentState = CONNECTING_NWK_STATE;
		}
		else
		{
			/* Poll data from Stack */
			ZPS_eAplZdoPoll();
			DBG_vPrintf(TRACE_APP, "APP: POLL_DATA_STATE\n\r");
			app_currentState = POLL_DATA_STATE;
		}
	}
}

PRIVATE void vDataCallBack(void)
{
	if(lockFlag)
	{
		/* Set wakeup time */
		PWRM_eScheduleActivity(&sData, SECS_TO_TICKS(1), vDataCallBack);
	}
	else
	{
		lockFlag = TRUE;

		if(!nwk_isConnected())
		{
			/* Find a new network */
			nwk_discovery();
			DBG_vPrintf(TRACE_APP, "\n\rAPP: CONNECTING_NWK_STATE\n\r");
			app_currentState = CONNECTING_NWK_STATE;
		}
		else
		{
			DBG_vPrintf(TRACE_APP, "\n\r\n\r*** WAKE UP ROUTINE ***\n\r");
			DBG_vPrintf(TRACE_APP, "APP: SEND_DATA_STATE\n\r");
			app_currentState = SEND_DATA_STATE;
		}
	}
}

PUBLIC void APP_cbTimerWatchdog(void *pvParam)
{
	DBG_vPrintf(TRACE_APP, "APP: State machine watchdog timeout\n\r");
	DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");
	app_currentState = PREP_TO_SLEEP_STATE;
}

PRIVATE void vfExtendedStatusCallBack (ZPS_teExtendedStatus eExtendedStatus)
{
	DBG_vPrintf(TRACE_APP, "ERROR: Extended status 0x%x\n", eExtendedStatus);
    DBG_vPrintf(TRACE_APP, "ERROR: EPID: 0x%016llx\n", ZPS_u64AplZdoGetNetworkExtendedPanId());
}

PRIVATE void APP_stateMachine(void)
{
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

	/* Main State Machine */
	switch (app_currentState)
	{
		case CONNECTING_NWK_STATE:
		{
			if(nwk_getDiscStatus() != NWK_DISC_NO_EVENT)
			{
				DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");
				app_currentState = PREP_TO_SLEEP_STATE;
			}
			if(nwk_isConnected())
			{
				DBG_vPrintf(TRACE_APP, "\n\rAPP: POLL_DATA_STATE\n\r");
				app_currentState = POLL_DATA_STATE;
			}
		}
		break;

		case POLL_DATA_STATE:
		{
			uint8 pollStatus = nwk_getPollStatus();

			if(pollStatus == NWK_POLL_NEW_MESSAGE)
			{
				DBG_vPrintf(TRACE_APP, "\n\rAPP: HANDLE_DATA_STATE\n\r");
				app_currentState = HANDLE_DATA_STATE;
			}
			else if (pollStatus == NWK_POLL_NO_MESSAGE ||
					 pollStatus == NWK_POLL_NO_ACK)
			{
				DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");
				app_currentState = PREP_TO_SLEEP_STATE;
			}
		}
		break;

		case HANDLE_DATA_STATE:
		{
			nwk_getData(APP_rxBuffer);
			APP_handleData(APP_rxBuffer);

			DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");
			app_currentState = PREP_TO_SLEEP_STATE;
		}
		break;

		case SEND_DATA_STATE:
		{
			APP_txBuffer[0] = '*';
			APP_txBuffer[1] = 255;
			APP_txBuffer[2] = 0x0f01;
			APP_txBuffer[3] = 1;
			nwk_sendData(APP_txBuffer, 4);

			DBG_vPrintf(TRACE_APP, "\n\rAPP: PREP_TO_SLEEP_STATE\n\r");
			app_currentState = PREP_TO_SLEEP_STATE;
		}
		break;

		case PREP_TO_SLEEP_STATE:
		{
			nd005_lowPower(TRUE);
			ZTIMER_eStop(u8TimerWatchdog);

			lockFlag = FALSE;

			DBG_vPrintf
			(
				TRACE_APP,
				"APP: Sleep for %d seconds\n\r",
				10
			);

			/* Set wakeup time */
			PWRM_eScheduleActivity
			(
				&sPoll,
				SECS_TO_TICKS(10),
				vPollCallBack
			);

			/* Set wakeup time */
			PWRM_eScheduleActivity
			(
				&sData,
				SECS_TO_TICKS(30),
				vDataCallBack
			);

			app_currentState = SLEEP_STATE;
		}
		break;

		case SLEEP_STATE:
		{
			/* Waits until OS sends the device to sleep */
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

PRIVATE void APP_handleData(uint16 * data_ptr)
{
	switch(data_ptr[0])
	{
		case '&':
		{
			DBG_vPrintf(TRACE_APP, "APP: Broadcast command received\n\r");
			APP_txBuffer[0] = '&';
			APP_txBuffer[1] = DEVICE_TYPE;
			APP_txBuffer[2] = VERSION_MAJOR;
			APP_txBuffer[3] = VERSION_MINOR;
			nwk_sendData(APP_txBuffer, 4);
		}
		break;

		case '~':
		{
			DBG_vPrintf(TRACE_APP, "APP: Configuration command received\n\r");
		}
		break;

		case '$':
		{
			DBG_vPrintf(TRACE_APP, "APP: GO command received\n\r");
		}
		break;

		default:
		{
			DBG_vPrintf(TRACE_APP, "APP: Unknown command received\n\r");
		}
		break;
	}
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
