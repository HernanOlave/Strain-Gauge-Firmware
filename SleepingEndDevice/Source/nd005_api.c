/**
 * @file nd005_api.c
 * @brief
 *
 * @author Wisely SpA
 * @date 22-Sep-19
 *
 */

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <jendefs.h>

#include "nd005_api.h"
#include "mcp3204.h"
#include "ad8231.h"
#include "ltc1661.h"

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

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void nd005_init(void)
{
	/* Setup CONFIG_BUTTON as input with pull-up */
	vAHI_DioSetDirection((1 << CONFIG_BUTTON_PIN), 0x0);
	vAHI_DioSetPullup((1 << CONFIG_BUTTON_PIN), 0x0);

	/* Setup 3V Low Noise, Powersave and WB Enable Pins as outputs */
	vAHI_DioSetDirection(0x0,(1 << LN_PIN));
	vAHI_DioSetDirection(0x0,(1 << POWERSAVE_PIN));
	vAHI_DioSetDirection(0x0,(1 << WB_ENABLE_PIN));

	/* Enable 3V Low Noise */
	vAHI_DioSetOutput((1 << LN_PIN), 0x0);

	/* Initialize ad8231 */
	ad8231_init();

	/* Initialize ltc1661 */
	ltc1661_init();

	nd005_lowPower(FALSE);
}

PUBLIC void nd005_lowPower(uint8 enable)
{
	if (enable)
	{
		/* Disable ad8231 */
		ad8231_disable();

		/* Put ltc1661 to sleep */
		ltc1661_sleep();

		/* Enable Powersave Mode */
		vAHI_DioSetOutput(0x0, (1 << POWERSAVE_PIN));

		/* Disable Wheatstone Bridge */
		vAHI_DioSetOutput((1 << WB_ENABLE_PIN), 0x0);
	}
	else
	{
		/* Enable ad8231 */
		ad8231_enable();

		/* Disable Powersave Mode */
		vAHI_DioSetOutput((1 << POWERSAVE_PIN), 0x0);

		/* Enable Wheatstone Bridge */
		vAHI_DioSetOutput(0x0, (1 << WB_ENABLE_PIN));
	}
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
