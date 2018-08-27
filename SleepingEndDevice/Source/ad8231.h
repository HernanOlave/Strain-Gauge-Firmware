/*
 * AD8231 library
 * ad8231.h
 *
 * Copyright (c) 2018  Wisely SpA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

/**
 * @file ad8231.h
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

#ifndef AD8231_H_
#define AD8231_H_

/** \defgroup ad8231 definitions */
/* @{ */

#define AD8231_GAIN_1		1
#define AD8231_GAIN_2		2
#define AD8231_GAIN_4		4
#define AD8231_GAIN_8		8
#define AD8231_GAIN_16		16
#define AD8231_GAIN_32		32
#define AD8231_GAIN_64		64
#define AD8231_GAIN_128		128

#define DIO0				0
#define DIO14				14
#define DIO15				15
#define DIO16				16

#define AD8231_PIN_CS		DIO0
#define AD8231_PIN_A0		DIO14
#define AD8231_PIN_A1		DIO15
#define AD8231_PIN_A2		DIO16

/* @} */

/** \defgroup ad8231 library functions */
/* @{ */

/**
 * @brief The function configures the ad8231 PINs.
 *
 * @param none
 *
 * @return none.
 */
void ad8231_init(void);

/**
 * @brief The function enables the ad8231 through CS_PINSPI.
 *
 * @param none
 *
 * @return none.
 */
void ad8231_enable(void);

/**
 * @brief The function disables the ad8231 through CS_PINSPI.
 *
 * @param none
 *
 * @return none.
 */
void ad8231_disable(void);

/**
 * @brief The function set the gain of the ad8231 through A2, A1, A0 PINs.
 *
 * @param[in] gain sets the gain through the A2, A1 and A0 Pins.
 *
 * @return none.
 */
void ad8231_setGain(unsigned char gain);

/* @} */
#endif /* AD8231_H_ */
