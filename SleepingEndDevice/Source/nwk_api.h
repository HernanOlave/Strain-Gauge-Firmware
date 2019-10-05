/**
 * @file nwk_api.h
 * @brief
 *
 * @author Wisely SpA
 * @date 21-Sep-19
 *
 */

#ifndef NWK_API_H_
#define NWK_API_H_

/****************************************************************************/
/***        Libraries                                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

typedef enum
{
    NWK_POLL_NO_EVENT,
    NWK_POLL_NEW_MESSAGE,
    NWK_POLL_NO_MESSAGE,
    NWK_POLL_NO_ACK,
    NWK_POLL_UNK_ERROR
} pollReturnValues_t;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/** @brief Initializes the Zigbee network API library
 *
 *  @param Void.
 *  @return Void.
 */
PUBLIC void nwk_init(void);

/** @brief Handles the events and state machine of the the Zigbee network
 *         API library. It must be called in the main loop.
 *
 *  @param Void.
 *  @return Void.
 */
PUBLIC void nwk_taskHandler(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC void nwk_discovery(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC void nwk_setEpid(uint64 epid);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC uint64 nwk_getEpid(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC pollReturnValues_t nwk_getPollStatus(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC bool nwk_isConnected(void);

/** @brief
 *
 *  @param
 *  @return
 */
PUBLIC void nwk_sendData(uint16 * data_ptr, uint16 size);


/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/

#endif /*NWK_API_H_*/
