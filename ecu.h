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
#ifndef __ECU_H_
#define __ECU_H_

//#define __DWELL_TEST__
//#define __INJ_TEST__
//#define __LOOP_TIMING_TEST__
//#define __UNIT_TEST__ /* Basic IO test */

#include <ucos_ii.h>

/******************************************************************************/
/* RTOS */
/******************************************************************************/
extern OS_EVENT *engine_event;
void engine_thread(void *p);

/******************************************************************************/
/* Globals */
/******************************************************************************/
extern int trim_flag;
extern int timing_advance, timing_advance_enabled;
extern int fuel_msec;
extern int record_mode;
extern volatile unsigned short capture_t;
extern volatile unsigned long curr_time;
#ifdef __DWELL_TEST__
extern volatile unsigned long T1, DWELL_DEBUG;
#endif
#ifdef __INJ_TEST__
extern volatile unsigned long T2, INJ_DEBUG;
#endif

/******************************************************************************/
/* Engine 4 cyl / 4 stroke definition */
/******************************************************************************/
#define CYL1 1
#define CYL2 2
#define CYL12 12
#define CYL21 12
#define CYL3 3
#define CYL4 4
#define CYL34 34
#define CYL43 34
#define DEGREE_PER_ENGINE_CYCLE 720UL

struct engine_schedule{
	int degree;
	unsigned char coil_cyl;
	unsigned char coil_ctr;
	unsigned char fuel_cyl;
	unsigned char fuel_ctr;
};

enum engine_state{
	ENGINE_STOP = 0,
	ENGINE_INIT,
	ENGINE_CRANK,
	ENGINE_RUN,
	ENGINE_DEAD,
};

/******************************************************************************/
/* Trigger wheel */
/******************************************************************************/
#define TRIGGER_WHEEL_RESOLUTION 10UL /* Subaru 36-2-2-2 is 10 deg per tooth */

int trigger_wheel_init(void);
void trigger_wheel_init_platform(void);
unsigned char run_trigger_wheel(unsigned short period);
int get_rpm(void);
unsigned long deg_to_usec(int degree);

/******************************************************************************/
/* Event */
/******************************************************************************/
struct event;
typedef void(*fcn_t)(struct event *e);
struct event{
	unsigned char cookie;
	fcn_t fcn;
};

/* Bring everything in [0-DEGREE_PER_ENGINE_CYCLE] */
static inline int normalize_deg(int deg)
{
	if(deg < 0)
		return DEGREE_PER_ENGINE_CYCLE + deg;
	if(deg >= DEGREE_PER_ENGINE_CYCLE)
		return deg - DEGREE_PER_ENGINE_CYCLE;
	return deg;
}

void event_register(int degree, fcn_t fcn, unsigned char cookie);
void event_callback(void);
void event_tick(int flag);
void event_set_position(int pos);
void event_init(int size);

/******************************************************************************/
/* IO */
/******************************************************************************/
void io_init(void);
void close_all_io(void);
void io_open_injector(int inj);
void io_close_injector(int inj, unsigned long t);
void io_open_coil(int coil, unsigned long t);
void io_close_coil(int coil, unsigned long t);
void io_relay_off(void);
void io_relay_on(void);
void gaz_relay_off(void);
void gaz_relay_on(void);
void gaz_toggle(void);
void starter_off(void);
void starter_on(void);

/******************************************************************************/
/* Error handling */
/******************************************************************************/
enum error_condition{
	ERROR_INIT = 1,
	MANAGEMENT,
	ENGINE,
	EVENT,
	TRIGGER,
};

#undef DEBUG
#define DEBUG(fmt, args...) do {\
} while(0)

#undef PRINT
#define PRINT(fmt, args...) do {\
} while(0)

/******************************************************************************/
/* Unit Test */
/******************************************************************************/
void unit_test(void);

#endif

