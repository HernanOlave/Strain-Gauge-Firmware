/*
 * LTC1661 library
 * ltc1661.h
 *
 * Copyright (c) 2018  Wisely SpA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

/**
 * @file ltc1661.h
 * @brief
 * Header file of ad8231 library. It contains the prototypes of all
 * functions available in the library, definitions of all macros
 * and constans.
 *
 * @author Wisely SpA
 * @date 27-Aug-18
 * @copyright GNU General Public License v2.
 *
 */

#ifndef LTC1661_H_
#define LTC1661_H_

/** \defgroup ltc1661 definitions */
/* @{ */

#define LTC1661_CTRLCODE_NOOP			0x0
#define LTC1661_CTRLCODE_LOAD_A			0x1
#define LTC1661_CTRLCODE_LOAD_B			0x2
#define LTC1661_CTRLCODE_LOAD_BOTH 		0x8
#define LTC1661_CTRLCODE_UPDA_A			0x9
#define LTC1661_CTRLCODE_UPDA_B			0xA
#define LTC1661_CTRLCODE_WAKEUP			0xD
#define LTC1661_CTRLCODE_SLEEP			0xE
#define LTC1661_CTRLCODE_UPDA_BOTH		0xF

#define DIO1							1
#define LTC1661_PIN_CS					DIO1

/* @} */

/** \defgroup ltc1661 library functions */
/* @{ */

/**
 * @brief The function configures the SPI for ltc1661 use.
 *
 * @param none
 *
 * @return none.
 */
void ltc1661_init(void);

/**
 * @brief The function puts the device into sleep mode.
 *
 * @param none
 *
 * @return none.
 */
void ltc1661_sleep(void);

/**
 * @brief The function wake ups and sets the DAC A port.
 *
 * @param[in] value determines the output value of DAC port.
 *
 * @return none.
 */
void ltc1661_setDAC_A(unsigned int value);

/**
 * @brief The function wake ups and sets the DAC B port.
 *
 * @param[in] value determines the output value of DAC port.
 *
 * @return none.
 */
void ltc1661_setDAC_B(unsigned int value);

/* @} */
#endif /* LTC1661_H_ */
