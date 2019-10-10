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

#include "dbg.h"
#include "dbg_uart.h"
#include "AppHardwareApi.h"

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

PRIVATE uint16 getMedianAvg(uint8 adcChannel, uint8 samples);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

PUBLIC seDeviceDesc_t s_device;

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

	nd005_lowPower(TRUE);
}

PUBLIC void nd005_lowPower(uint8 enable)
{
	if (enable)
	{
		/* Disable ad8231 */
		ad8231_disable();

		/* Put ltc1661 to sleep */
		ltc1661_sleep();

		/* Disable Wheatstone Bridge */
		vAHI_DioSetOutput((1 << WB_ENABLE_PIN), 0x0);

		/* Enable Powersave Mode */
		vAHI_DioSetOutput(0x0, (1 << POWERSAVE_PIN));
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

PUBLIC uint16 nd005_getTemperature(void)
{
	/* Start ADC Conversion */
	MCP3204_init(0);

	return getMedianAvg(1, 10);
}

PUBLIC uint16 nd005_getBattery(void)
{
	/* Start ADC Conversion */
	MCP3204_init(0);

	return getMedianAvg(0, 10);
}

PUBLIC uint16 nd005_getSensorValue(void)
{
	ad8231_setGain(s_device.gainValue);
	ltc1661_setDAC_A(s_device.channelAValue);
	ltc1661_setDAC_B(s_device.channelBValue);

	/* Start ADC Conversion */
	MCP3204_init(0);

	return getMedianAvg(2, 10);
}

PUBLIC bool nd005_getConfigButton(void)
{
	if (!((1 << CONFIG_BUTTON_PIN) & u32AHI_DioReadInput())) return FALSE;
	else return TRUE;
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

PRIVATE uint16 getMedianAvg(uint8 adcChannel, uint8 samples)
{
	uint16 i, j, Imin, temp;
	uint16 results[samples+1];

	if (samples < 3) return 0;

	for (i = 0; i < samples; i++) results[i] = MCP3204_convert(0, adcChannel);

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

	temp = results[3];
	temp += results[4];
	temp += results[5];
	temp /= 3;

	return temp;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
