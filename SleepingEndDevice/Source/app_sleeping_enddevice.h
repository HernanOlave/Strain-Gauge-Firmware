/*****************************************************************************
 *
 * MODULE:				JN-AN-1184 ZigBeePro Application Template
 *
 * COMPONENT:			app_sleeping_enddevice.h
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

#ifndef APP_SLEEPING_ENDDEVICE_H_
#define APP_SLEEPING_ENDDEVICE_H_

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#define BLACKLIST_MAX   			32		// max number of blacklist EPIDs
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
	NWK_STARTUP_STATE,
    NWK_DISC_STATE,
    NWK_JOIN_STATE,
    NWK_AUTH_STATE,
    NWK_LEAVE_STATE,
    NWK_REJOIN_STATE
} networkStates_t;

typedef enum
{
    NETWORK_STATE,
    POLL_DATA_STATE,
    HANDLE_DATA_STATE,
    READ_SENSOR_STATE,
    SEND_DATA_STATE,
    PREP_TO_SLEEP_STATE,
    SLEEP_STATE,
    WAKE_UP_STATE
} sleepingEndDeviceStates_t;

typedef enum
{
    FRAME_SUCCESS,
    FRAME_BAD_SIZE,
    FRAME_BAD_FORMAT,
    FRAME_UNK_ERROR,
} frameReturnValues_t;

typedef struct
{
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

PUBLIC void APP_vInitialiseSleepingEndDevice(void);
PUBLIC void APP_vtaskSleepingEndDevice(void);

/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

#endif /*APP_SLEEPING_ENDDEVICE_H_*/
