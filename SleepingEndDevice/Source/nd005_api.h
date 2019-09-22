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
	sleepingEndDeviceStates_t   currentState;
	sleepingEndDeviceStates_t   previousState;
} seDeviceDesc_t;

enum {
    PDM_APP_ID_SAMPLE_PERIOD = 0x100,   // configured sample period value
    PDM_APP_ID_CONFIGURED,              // flag indicating configuration is finished and device is active
    PDM_APP_ID_EPID,                    // EPID of current/last authorized network
    PDM_APP_ID_BLACKLIST,               // list of incompatible network EPIDs
    PDM_APP_ID_CHANNEL_A,				// configured value for DAC's channel A
    PDM_APP_ID_CHANNEL_B,				// configured value for DAC's channel B
    PDM_APP_ID_GAIN,					// configured value for INAMP's Gain
};
/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/** @brief Initializes the Zigbee network API library
 *
 *  @param Void.
 *  @return Void.
 */
PUBLIC void nd005_init(void);

/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

#endif /*ND005_API_H*/
