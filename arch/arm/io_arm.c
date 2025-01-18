/*
 * Copyright 2024, Etienne Martineau etienne4313@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <ecu.h>

/******************************************************************************/
/* Input */
/******************************************************************************/
#define CFG_INPUT()

#define CRANK_VAL() 0
//#define CAM_VAL() 0

/******************************************************************************/
/* Output */
/******************************************************************************/
#define CFG_OUTPUT()

#define INJ1_ON()
#define INJ1_OFF()

#define INJ2_ON()
#define INJ2_OFF()

#define INJ3_ON()
#define INJ3_OFF()

#define INJ4_ON()
#define INJ4_OFF()

#define COIL1_ON()
#define COIL1_OFF()

#define COIL2_ON()
#define COIL2_OFF()

#define COIL3_ON()
#define COIL3_OFF()

#define COIL4_ON()
#define COIL4_OFF()

#define RELAY_ON()
#define RELAY_OFF()

#define GAZ_ON()
#define GAZ_OFF()

#define STARTER_ON()
#define STARTER_OFF()

/******************************************************************************/
/* Injector */
/******************************************************************************/
void io_open_injector(int inj)
{
	switch(inj){
	case CYL1:
		INJ1_ON();
#ifdef __INJ_TEST__
		T2 = get_monotonic_time();
#endif
		break;
	case CYL2:
		INJ2_ON();
		break;
	case CYL3:
		INJ3_ON();
		break;
	case CYL4:
		INJ4_ON();
		break;
	default:
		DIE(FATAL);
	}
//	PRINT("INJ ON %d \n", inj);
}

void io_close_injector(int inj, unsigned long t)
{
	switch(inj){
	case CYL1:
		INJ1_OFF();
#ifdef __INJ_TEST__
		INJ_DEBUG = t - T2;
#endif
		break;
	case CYL2:
		INJ2_OFF();
		break;
	case CYL3:
		INJ3_OFF();
		break;
	case CYL4:
		INJ4_OFF();
		break;
	default:
		DIE(FATAL);
	}
//	PRINT("INJ OFF %d \n", inj);
}

/******************************************************************************/
/* Coil */
/******************************************************************************/
void io_open_coil(int coil, unsigned long t)
{
	switch(coil){
	case CYL1:
		COIL1_ON();
#ifdef __DWELL_TEST__
		T1 = t;
#endif
		break;
	case CYL2:
		COIL2_ON();
		break;
	case CYL12:
		COIL1_ON();
		COIL2_ON();
		break;
	case CYL3:
		COIL3_ON();
		break;
	case CYL4:
		COIL4_ON();
		break;
	case CYL34:
		COIL3_ON();
		COIL4_ON();
		break;
	default:
		DIE(FATAL);
	}
//	PRINT("COIL ON %d \n", coil);
}

void io_close_coil(int coil, unsigned long t)
{
	switch(coil){
	case CYL1:
		COIL1_OFF();
#ifdef __DWELL_TEST__
		DWELL_DEBUG = t - T1;
#endif
		break;
	case CYL2:
		COIL2_OFF();
		break;
	case CYL12:
		COIL1_OFF();
		COIL2_OFF();
		break;
	case CYL3:
		COIL3_OFF();
		break;
	case CYL4:
		COIL4_OFF();
		break;
	case CYL34:
		COIL3_OFF();
		COIL4_OFF();
		break;
	default:
		DIE(FATAL);
	}
//	PRINT("COIL OFF %d \n", coil);
}

/******************************************************************************/
/* Relay */
/******************************************************************************/
void io_relay_off(void)
{
	RELAY_OFF();
}

void io_relay_on(void)
{
	RELAY_ON();
}

/******************************************************************************/
/* Starter */
/******************************************************************************/
void starter_off(void)
{
	FORCE_PRINT("STARTER OFF\n");
	STARTER_OFF();
}

void starter_on(void)
{
	FORCE_PRINT("STARTER ON\n");
	STARTER_ON();
}

/******************************************************************************/
/* GAZ Pump */
/******************************************************************************/
static int gaz_state = 0;
void gaz_relay_off(void)
{
//	PRINT("Gaz OFF\n");
	GAZ_OFF();
	gaz_state = 0;
}

void gaz_relay_on(void)
{
//	PRINT("Gaz ON\n");
	GAZ_ON();
	gaz_state = 1;
}

void gaz_toggle(void)
{
	if(gaz_state)
		gaz_relay_off();
	else
		gaz_relay_on();
}

/******************************************************************************/
/* TRIGGER WHEEL */
/******************************************************************************/
static volatile unsigned long old_time = 0;
void fake_irq(void);
void fake_irq(void)
{
	portSAVE_CONTEXT();

	if(!CRANK_VAL()) /* Not interested in the Falling edge signal */
		goto out;

	OSIntEnter();
	if(capture_t) /* Running behind */
		DIE(IRQ);

	curr_time = get_monotonic_time();
	old_time = curr_time - old_time;
	if( old_time >= 65536)
		capture_t = 0;
	else
		capture_t = (unsigned short)old_time;
	old_time = curr_time;

	OSSemPost(engine_event); /* Signal the engine_thread */

	OSIntExit();

out:
	portRESTORE_CONTEXT();
}

void trigger_wheel_init_platform(void)
{
	capture_t = 0;
}

/******************************************************************************/
/* Initialization */
/******************************************************************************/
void close_all_io(void)
{
	GAZ_OFF();
	RELAY_OFF();
	INJ1_OFF();
	INJ2_OFF();
	INJ3_OFF();
	INJ4_OFF();
	COIL1_OFF();
	COIL2_OFF();
	COIL3_OFF();
	COIL4_OFF();
	starter_off();
//	analogDisable();
}

void io_init(void)
{
	CFG_OUTPUT();
	close_all_io();
//	analogEnable();
}

