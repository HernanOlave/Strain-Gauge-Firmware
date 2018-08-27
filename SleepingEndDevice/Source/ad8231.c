/*
 * ad8231 library
 * ad8231.c
 *
 * Copyright (c) 2018  Wisely SpA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

/** @file ad8231.c
 *  @brief Implements the functions defined in the header file.
 *
 * @author Wisely SpA
 * @date 27-Aug-18
 * @copyright GNU General Public License v2.
 *
 */

#include "dbg.h"
#include "AppHardwareApi.h"
#include "ad8231.h"

#ifndef DEBUG_APP
	#define TRACE_APP 	FALSE
#else
	#define TRACE_APP 	TRUE
#endif


void ad8231_init(void)
{
	// initialize ad8231 pins as outputs
	vAHI_DioSetDirection(0x0,
						(1 << AD8231_PIN_CS) |
						(1 << AD8231_PIN_A0) |
						(1 << AD8231_PIN_A1) |
						(1 << AD8231_PIN_A2) );
}

void ad8231_enable(void)
{
	vAHI_DioSetOutput(0x0, (1 << AD8231_PIN_CS));
}

void ad8231_disable(void)
{
	vAHI_DioSetOutput((1 << AD8231_PIN_CS), 0x0);
}

void ad8231_setGain(unsigned char gain)
{

	switch(gain)
	{
		case AD8231_GAIN_1:
		vAHI_DioSetOutput(0x0 , (1 << AD8231_PIN_A0)|(1 << AD8231_PIN_A1)|(1 << AD8231_PIN_A2));
		break;

		case AD8231_GAIN_2:
		vAHI_DioSetOutput((1 << AD8231_PIN_A0), (1 << AD8231_PIN_A1)|(1 << AD8231_PIN_A2));
		break;

		case AD8231_GAIN_4:
		vAHI_DioSetOutput((1 << AD8231_PIN_A1), (1 << AD8231_PIN_A0)|(1 << AD8231_PIN_A2));
		break;
		case AD8231_GAIN_8:
		vAHI_DioSetOutput((1 << AD8231_PIN_A0)|(1 << AD8231_PIN_A1), (1 << AD8231_PIN_A2));
		break;

		case AD8231_GAIN_16:
		vAHI_DioSetOutput((1 << AD8231_PIN_A2), (1 << AD8231_PIN_A0)|(1 << AD8231_PIN_A1));
		break;

		case AD8231_GAIN_32:
		vAHI_DioSetOutput((1 << AD8231_PIN_A0)|(1 << AD8231_PIN_A2), (1 << AD8231_PIN_A1));
		break;

		case AD8231_GAIN_64:
		vAHI_DioSetOutput((1 << AD8231_PIN_A1)|(1 << AD8231_PIN_A2), (1 << AD8231_PIN_A0));
		break;

		case AD8231_GAIN_128:
		vAHI_DioSetOutput((1 << AD8231_PIN_A1)|(1 << AD8231_PIN_A2)|(1 << AD8231_PIN_A0), 0x0);
		break;

		default:
		vAHI_DioSetOutput(0x0 , (1 << AD8231_PIN_A0)|(1 << AD8231_PIN_A1)|(1 << AD8231_PIN_A2));
		break;
	}
}
