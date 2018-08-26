/*
 * pdm_app_ids.h
 *
 *  Created on: Jul 1, 2018
 *      Author: Peter
 */

#ifndef PDM_APP_IDS_H_
#define PDM_APP_IDS_H_

#include "app_sleeping_enddevice.h"

enum {
    PDM_APP_ID_SAMPLE_PERIOD = 0x100,   // configured sample period value
    PDM_APP_ID_CONFIGURED,              // flag indicating configuration is finished and device is active
    PDM_APP_ID_EPID,                    // EPID of current/last authorized network
    PDM_APP_ID_BLACKLIST,               // list of incompatible network EPIDs
};

#endif /* PDM_APP_IDS_H_ */
