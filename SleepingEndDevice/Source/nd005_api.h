/**
 * @file nd005_api.h
 * @brief
 *
 * @author Wisely SpA
 * @date 22-Sep-19
 *
 */

#ifndef ND005_API_H
#define ND005_API_H

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#define CHANNEL_A_DEFAULT_VALUE		2048	// DAC's channel A
#define CHANNEL_B_DEFAULT_VALUE		2048	// DAC's channel B
#define GAIN_DEFAULT_VALUE			32		// INAMP Gain
#define DEFAULT_SLEEP_TIME			5		// seconds TODO: change this in production
#define DEFAULT_SAMPLE_PERIOD		10		// seconds

#define CONFIG_BUTTON_PIN			13
#define LN_PIN						17
#define POWERSAVE_PIN				12
#define WB_ENABLE_PIN				11

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef enum
{
    FRAME_SUCCESS,
    FRAME_BAD_SIZE,
    FRAME_BAD_FORMAT,
    FRAME_UNK_ERROR,
} frameReturnValues_t;

typedef struct
{
	uint8						systemStrikes;
	bool						isConfigured;
	uint16						samplePeriod;
	uint16						channelAValue;
	uint16						channelBValue;
	uint16						gainValue;
	uint16						sensorValue;
	uint16						temperatureValue;
	uint16						batteryLevel;
	uint16						sleepTime;
} seDeviceDesc_t;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC void nd005_init(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC void nd005_lowPower(uint8 enable);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC uint16 nd005_getTemperature(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC uint16 nd005_getBattery(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC uint16 nd005_getSensorValue(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC bool nd005_getConfigButton(void);


/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

#endif /*ND005_API_H*/
