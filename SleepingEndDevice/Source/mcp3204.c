/*
 * MCP3204 library
 * mcp3204.c
 *
 * Copyright (c) 2014  Goce Boshkovski
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

/** @file mcp3204.c
 *  @brief Implements the functions defined in the header file.
 *
 * @author Goce Boshkovski
 * @date 17-Aug-14
 * @copyright GNU General Public License v2.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>

#include "dbg.h"
#include "AppHardwareApi.h"
#include "mcp3204.h"

#ifndef DEBUG_APP
	#define TRACE_APP 	FALSE
#else
	#define TRACE_APP 	TRUE
#endif

#define NXP_JN516X_PERIPHERAL_CLOCK 16000000 /*That's the peripheral clock supported by the JN516x*/
#define SPI_SPEED 1000000 /*Desired SPI speed = 1MHz*/
#define SPI_CLOCK_DIVIDER (NXP_JN516X_PERIPHERAL_CLOCK/SPI_SPEED)

#define LOWER_CS() do { vAHI_SpiWaitBusy(); vAHI_SpiSelect(1<<0); vAHI_SpiWaitBusy(); } while(0)
#define RAISE_CS() do { vAHI_SpiWaitBusy(); vAHI_SpiSelect(0); vAHI_SpiWaitBusy(); } while(0)

/*
 * The function configures the SPI interface of JN516x
 * according to MCP3204 SPI properties.
 */
int MCP3204_init(SPIMode spi_mode)
{
	uint8_t bPolarity, bPhase;

	if (spi_mode)
	{
		bPolarity = 1;
		bPhase = 1;
	}
	else
	{
		bPolarity = 0;
		bPhase = 0;
	}

	vAHI_SpiConfigure(0,
			  	  	  E_AHI_SPIM_MSB_FIRST,
			  	  	  bPolarity,
			  	  	  bPhase,
			  	  	  SPI_CLOCK_DIVIDER,
			  	  	  E_AHI_SPIM_INT_DISABLE,
			  	  	  E_AHI_SPIM_AUTOSLAVE_DSABL);

	return 0;
}

/*
 * Start the AD conversion process and read the digital value
 * of the analog signal from MCP3204.
 */
unsigned int MCP3204_convert(inputChannelMode channelMode, inputChannel channel)
{
	uint8_t tx;
	uint32_t rx;

	/* set the start bit */
	tx = 0b10000;

	/* define the channel input mode */
	if (channelMode==singleEnded)
		tx |= 0b01000;
	if (channelMode==differential)
		tx &= 0b10111;

	/* set the input channel */
	tx |= (channel & 0b0111);

	LOWER_CS();

	vAHI_SpiStartTransfer(7, tx);
	vAHI_SpiWaitBusy();
	vAHI_SpiStartTransfer(13, 0);
	vAHI_SpiWaitBusy();

	rx = u16AHI_SpiReadTransfer16();

	RAISE_CS();

	return rx & 0x0fff;
}
