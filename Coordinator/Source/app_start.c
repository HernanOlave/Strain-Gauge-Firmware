/*****************************************************************************
 *
 * MODULE:				JN-AN-1184 ZigBeePro Application Template
 *
 * COMPONENT:			app_start.c
 *
 * DESCRIPTION:			Coordinator Initialization and Startup
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
#include <AppApi.h>
#include <pwrm.h>
#include <dbg.h>
#include <dbg_uart.h>
#include <app_pdm.h>
#include "app_common.h"
#include <app_coordinator.h>
#include <zps_nwk_pub.h>
#include "pdum_gen.h"
#include "ZQueue.h"
#include "portmacro.h"
#include "zps_apl_af.h"
#include "mac_vs_sap.h"
#include "AppHardwareApi.h"
#include "dbg.h"
#include "ZTimer.h"

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
/*#defines*/
#define TIMER_QUEUE_SIZE             8
#define MLME_QUEQUE_SIZE             4
#define MCPS_QUEUE_SIZE             24
#define ZPS_QUEUE_SIZE               2
#define APP_QUEUE_SIZE               2
#define MCPS_DCFM_QUEUE_SIZE         8

#define UART1_TX_BUFFER_SIZE	64
#define UART1_RX_BUFFER_SIZE	64

#if JENNIC_CHIP_FAMILY == JN517x
#define NVIC_INT_PRIO_LEVEL_BBC     (7)
#endif
/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PUBLIC void APP_vInitResources(void);
PUBLIC void APP_vSetUpHardware(void);
PUBLIC void app_vMainloop(void);
PRIVATE void vInitialiseApp(void);
PRIVATE void vfExtendedStatusCallBack (ZPS_teExtendedStatus eExtendedStatus);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

PUBLIC uint8 u8App_tmr1sec;
/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
/*local variables*/
PRIVATE MAC_tsMcpsVsCfmData asMacMcpsDcfm[MCPS_DCFM_QUEUE_SIZE];
PRIVATE zps_tsTimeEvent asTimeEvent[TIMER_QUEUE_SIZE];
PRIVATE MAC_tsMcpsVsDcfmInd asMacMcpsDcfmInd[MCPS_QUEUE_SIZE];
PRIVATE MAC_tsMlmeVsDcfmInd  asMacMlmeVsDcfmInd[MLME_QUEQUE_SIZE];
PRIVATE ZPS_tsAfEvent asAppEvents[APP_QUEUE_SIZE];
PRIVATE ZPS_tsAfEvent asStackEvents[ZPS_QUEUE_SIZE];

#if SBC_UART_DISABLE == 0
PRIVATE uint8 uart1TxBuffer[UART1_TX_BUFFER_SIZE];
PRIVATE uint8 uart1RxBuffer[UART1_RX_BUFFER_SIZE];
#endif

PRIVATE ZTIMER_tsTimer asTimers[4];

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
extern void vAHI_WatchdogRestart(void);
extern void PWRM_vManagePower(void);
extern void zps_taskZPS(void);

/*****************************************************************************
 *
 * NAME: vAppMain
 *
 * DESCRIPTION:
 * Entry point for application from a cold start.
 *
 * RETURNS:
 * Never returns.
 *
 ****************************************************************************/
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

    DBG_vPrintf(TRACE_APP, "Version: %d.%d\n", VERSION_MAJOR, VERSION_MINOR);
    DBG_vPrintf(TRACE_APP, "Built: %s %s\n\n", __DATE__, __TIME__);

#if JENNIC_CHIP_FAMILY==JN516x
    /*
     * Initialise the stack overflow exception to trigger if the end of the
     * stack is reached. See the linker command file to adjust the allocated
     * stack size.
     */
    vAHI_SetStackOverflow(TRUE, (uint32)&_stack_low_water_mark);
#endif
    /*
     * Catch resets due to watchdog timer expiry.
     */
    if (bAHI_WatchdogResetEvent())
    {
        DBG_vPrintf(TRACE_APP, "APP: Watchdog timer has reset device!\n");
        //vAHI_WatchdogStop();
        //while (1);
    }

    /* Define HIGH_POWER_ENABLE to enable high power module */
#ifdef HIGH_POWER_ENABLE
    vAppApiSetHighPowerMode( APP_API_MODULE_HPM06, TRUE );
#endif


    /* Initialise various components */

#if SBC_UART_DISABLE == 0
	/* Set up UART1 for external device communications (SBC) */
	bool_t success = bAHI_UartEnable( E_AHI_UART_1, uart1TxBuffer, UART1_TX_BUFFER_SIZE, uart1RxBuffer, UART1_RX_BUFFER_SIZE );
	if( !success )
	{
		DBG_vPrintf(TRACE_APP, "APP: UART1 Init Failure\n");
		//TODO: Handle error
	}

	vAHI_UartSetBaudRate( E_AHI_UART_1, E_AHI_UART_RATE_115200 );

	vAHI_UartSetControl(
			E_AHI_UART_1,
			0,
			E_AHI_UART_PARITY_DISABLE,
			E_AHI_UART_WORD_LEN_8,
			E_AHI_UART_1_STOP_BIT,
			0
	);
#endif

    vInitialiseApp();

}

/*****************************************************************************
 *
 * NAME: vAppRegisterPWRMCallbacks
 *
 * DESCRIPTION:
 *
 * RETURNS:
 *
 ****************************************************************************/
void vAppRegisterPWRMCallbacks(void)
{
    /* nothing to register as device does not sleep */
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/*****************************************************************************
 *
 * NAME: vInitialiseApp
 *
 * DESCRIPTION:
 * Initialises JenOS modules and application.
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void vInitialiseApp(void)
{
    /* Initialise JenOS modules. Initialise Power Manager even on non-sleeping nodes
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

    /* Initialise Protocol Data Unit Manager */
    PDUM_vInit();

    /* Register callback that provides information about stack errors */
    ZPS_vExtendedStatusSetCallback(vfExtendedStatusCallBack);

    APP_vInitResources();
    APP_vSetUpHardware();
    /* initialise application */
    APP_vInitialiseCoordinator();

    app_vMainloop();

}

/****************************************************************************
 *
 * NAME: vfExtendedStatusCallBack
 *
 * DESCRIPTION:
 * ZPS extended error callback .
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PRIVATE void vfExtendedStatusCallBack (ZPS_teExtendedStatus eExtendedStatus)
{
	DBG_vPrintf(TRACE_APP, "ERROR: Extended status %x\n", eExtendedStatus);
}


/****************************************************************************
 *
 * NAME: APP_vSetUpHardware
 *
 * DESCRIPTION:
 * Set up interrupts
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void APP_vSetUpHardware(void)
{
#if (JENNIC_CHIP_FAMILY == JN517x)
    u32AHI_Init();
    vAHI_InterruptSetPriority(MICRO_ISR_MASK_BBC,     NVIC_INT_PRIO_LEVEL_BBC );
#else
    TARGET_INITIALISE();
    /* clear interrupt priority level  */
    SET_IPL(0);
    portENABLE_INTERRUPTS();
#endif
}


/****************************************************************************
 *
 * NAME: APP_vInitResources
 *
 * DESCRIPTION:
 * Initialise resources (timers, queue's etc)
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void APP_vInitResources(void)
{

    /* Initialise the Z timer module */
    ZTIMER_eInit(asTimers, sizeof(asTimers) / sizeof(ZTIMER_tsTimer));

    ZQ_vQueueCreate(&zps_msgMlmeDcfmInd,         MLME_QUEQUE_SIZE,      sizeof(MAC_tsMlmeVsDcfmInd), (uint8*)asMacMlmeVsDcfmInd);
    ZQ_vQueueCreate(&zps_msgMcpsDcfmInd,         MCPS_QUEUE_SIZE,       sizeof(MAC_tsMcpsVsDcfmInd), (uint8*)asMacMcpsDcfmInd);
    ZQ_vQueueCreate(&zps_TimeEvents,             TIMER_QUEUE_SIZE,      sizeof(zps_tsTimeEvent),     (uint8*)asTimeEvent);
    ZQ_vQueueCreate(&APP_msgZpsEvents,           ZPS_QUEUE_SIZE,        sizeof(ZPS_tsAfEvent),       (uint8*)asStackEvents);
    ZQ_vQueueCreate(&APP_msgStrainGaugeEvents,   APP_QUEUE_SIZE,        sizeof(ZPS_tsAfEvent),       (uint8*)asAppEvents);
	ZQ_vQueueCreate(&zps_msgMcpsDcfm,            MCPS_DCFM_QUEUE_SIZE,  sizeof(MAC_tsMcpsVsCfmData),(uint8*)asMacMcpsDcfm);

}

/****************************************************************************
 *
 * NAME: APP_vMainLoop
 *
 * DESCRIPTION:
 * Main application loop
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
PUBLIC void app_vMainloop(void)
{
    /* idle task commences on exit from OS start call */
    while (TRUE) {
        zps_taskZPS();
        APP_vtaskCoordinator();
        APP_vtaskMyEndPoint();
        ZTIMER_vTask();
        /* kick the watchdog timer */
        vAHI_WatchdogRestart();
        PWRM_vManagePower();
    }
}



/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
