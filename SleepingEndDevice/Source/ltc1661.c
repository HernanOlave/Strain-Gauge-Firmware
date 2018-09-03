/*
 * ltc1661 library
 * ltc1661.c
 *
 * Copyright (c) 2018  Wisely SpA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

/** @file ltc1661.c
 *  @brief Implements the functions defined in the header file.
 *
 * @author Wisely SpA
 * @date 27-Aug-18
 * @copyright GNU General Public License v2.
 *
 */

#include "dbg.h"
#include "AppHardwareApi.h"
#include "ltc1661.h"

#ifndef DEBUG_APP
	#define TRACE_APP 	FALSE
#else
	#define TRACE_APP 	TRUE
#endif

#define NXP_JN516X_PERIPHERAL_CLOCK 16000000 /*That's the peripheral clock supported by the JN516x*/
#define SPI_SPEED 1000000 /*Desired SPI speed = 1MHz*/
#define SPI_CLOCK_DIVIDER (NXP_JN516X_PERIPHERAL_CLOCK/SPI_SPEED)

#define LOWER_CS() vAHI_DioSetOutput(0x0, (1 << LTC1661_PIN_CS));
#define RAISE_CS() vAHI_DioSetOutput((1 << LTC1661_PIN_CS), 0x0);

void ltc1661_init(void)
{
	uint8_t bPolarity, bPhase;

	bPolarity = 0;
	bPhase = 0;

	vAHI_SpiConfigure(0,
				  	  E_AHI_SPIM_MSB_FIRST,
				  	  bPolarity,
				  	  bPhase,
				  	  SPI_CLOCK_DIVIDER,
				  	  E_AHI_SPIM_INT_DISABLE,
				  	  E_AHI_SPIM_AUTOSLAVE_DSABL);

	vAHI_DioSetDirection(0x0,(1 << LTC1661_PIN_CS));
}

void ltc1661_sleep(void)
{
	uint16_t tx = 0;

	tx |= (LTC1661_CTRLCODE_SLEEP << 12);
	DBG_vPrintf(TRACE_APP, "SPI: %x\n", tx);

	LOWER_CS();

	vAHI_SpiStartTransfer16(tx);
	vAHI_SpiWaitBusy();

	RAISE_CS();
}

void ltc1661_setDAC_A(unsigned int value)
{
	uint16_t tx = 0;

	tx |= (LTC1661_CTRLCODE_UPDA_A << 12);
	tx |= (value & 0x0fff);

	LOWER_CS();

	vAHI_SpiStartTransfer16(tx);
	vAHI_SpiWaitBusy();

	RAISE_CS();
}

void ltc1661_setDAC_B(unsigned int value)
{
	uint16_t tx = 0;

	tx |= (LTC1661_CTRLCODE_UPDA_B << 12);
	tx |= (value & 0x0fff);

	LOWER_CS();

	vAHI_SpiStartTransfer16(tx);
	vAHI_SpiWaitBusy();

	RAISE_CS();
}

