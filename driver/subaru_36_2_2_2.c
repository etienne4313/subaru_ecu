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

/*
 * Subaru 36_2_2_2
 * Crank Trigger wheel: 36 teeth (10 degree each ) and 3 group of 2 missing => 30 teeth
 *
 * Per-Spec: *A* (Tooth 32) is 40 Degree BTDC for CYL 1,2
 * Per-Spec: *B* (Tooth 14) is 40 Degree BTDC for CYL 3,4
 *
 *                                  *A*                                                   *B*
 *  |  |  |  |  |  |  |  |  |  X  X  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  X  X  |  X  X  |  |  |  | 
 *  21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20
 *                                   ^                                                     ^        ^
 *                                  SYNC #1                                              SYNC #3   SYNC #2
 *                                   |40BTDC                                               |40BTDC
 *                                      |30BTDC                                               |30BTDC
 *                                         |20BTDC                                               |20BTDC 
 *                                            |10BTDC                                               |10BTDC
 *                                               |0 =>TDC(1,2) => 0,360 deg                            |0 =>TDC(3,4) => 180,540 deg
 *
 * Few observations from Crank DATA:
 *  - Same period say '22-23-24---29' has two different phases WRT to CAM
 * 	- When going towards a TDC cycle, the cycle time 't' increase
 * 	- The cycle 22-23-24---29-*A* is characterize by a sudden spike @*A* then time increase toward TDC and goes down in 1-2-3-4
 * 	- The cycle  8- 9-10---11-*B* is characterize by a sudden spike @*B* then there is an even bigger spike @17 and goes down in 18-19-20
 */

#include <ecu.h>

#define TOOTH_COUNT 36
#define SYNC_1_TOOTH_CTR_POSITION 32 
#define SYNC_1_DEGREE_POSITION 680
#define SYNC_2_TOOTH_CTR_POSITION 17 
#define SYNC_2_DEGREE_POSITION 170
#define SYNC_3_TOOTH_CTR_POSITION 14
#define SYNC_3_DEGREE_POSITION 140

/* 
 * The smallest period is when the RPM is high @6000RPM
 * 	1/(@6000 RPM / 60) / 36 == 277uSec
 *
 * The largest period is during cranking @30RPM but we need to 
 * remember that there are missing tooth hence X3
 * 	1/(@30 RPM / 60) / 36 => 13888uSec
 *
 * The average period to declare the engine running is @500RPM
 * 	1/(@500 RPM / 60) / 36 => 3333uSec
 *
 * NOTE that everything is contained in an unsigned short
 */
#define MIN_TICK_PERIOD_USEC_6000RPM (277)
#define MAX_TICK_PERIOD_USEC_30RPM (3*13888UL)
#define AVERAGE_RUN_PERIOD (3333UL)

#define MIN_SAMPLE 10 /* Debouncing Number of pulse */

#define AVG_SIZE 8
#define AVG_BIT_SHIFT 3


static int idx;
static unsigned short vector[AVG_SIZE];
static unsigned long running_sum;

static void init_vector(void)
{
	memset(vector, 0, sizeof(vector));
	running_sum = 0;
	idx = 0;
}

static void add_vector(unsigned short t)
{
	unsigned short old;
	
	/* Moving avegage; Initially old is = 0 so the sum is building up */
	old = vector[idx];
	vector[idx] = t;

	/* NOTE that we avoid 'running_sum += t - old' bcos t-old can be negative and we are with unsigned arithmetic */
	running_sum += t;
	running_sum -= old;
	
	idx++;
	if(idx == AVG_SIZE)
		idx = 0;
}

/* Return average tick# for 1 period */
static unsigned long trigger_wheel_get_average(void) 
{
	OS_CPU_SR cpu_sr;
	unsigned long t;
	OS_ENTER_CRITICAL();	
	t = running_sum;
	OS_EXIT_CRITICAL();
	return t >> AVG_BIT_SHIFT;
}

/*
 * t is the pulse period in usec measured on the rising edge
 */
unsigned char run_trigger_wheel(unsigned short t)
{
	static unsigned char state = 0, ctr, tooth_ctr;
	int err = ENGINE_INIT;
	unsigned long a;

	if(record_mode)
		FORCE_PRINT("%d:%ld\n", t, trigger_wheel_get_average());

	/* Account for the missing tooth */
	if (t > MAX_TICK_PERIOD_USEC_30RPM || t < MIN_TICK_PERIOD_USEC_6000RPM){
		/* Losing the SYNC at run-time is no good */
		if(state == 4){ /* Losing SYNC at run-time is no good */
			FORCE_PRINT("Glitch %d:%d\n", t, state);
			DIE(TRIGGER);
		}
		state = 0;
	}

	switch (state){

	case 0: /* Initialization */
		ctr = 0;
		state = 1;
		tooth_ctr = 1;
		capture_t = 0;
		init_vector();
		break;

	case 1:
		/* Gather some stable pulse during crank */
		if(t < 20000){ // TODO need proper check
			add_vector(t);
			if(ctr >= MIN_SAMPLE){
				FORCE_PRINT("Signal OK\n");
				ctr = 0;
				state = 2;
				break;
			}
			break;
		}
		state = 0;
		break;

	case 2: /* Scan for first missing tooth */
		err = ENGINE_CRANK;
		if(ctr > 20){ /* don't get stuck here TODO */
			FORCE_PRINT("No Sync\n");
			state = 0;
			break;
		}
		a = trigger_wheel_get_average();
		if(t > (a<<1)){ /* Twice the amplitude of average is a missing tooth */
			PRINT("First Missing tooth SKIP %d:%d:%ld\n", ctr, t, a);
			ctr = 0;
			state = 3;
			break;
		}
		add_vector(t); /* Don't add missing tooth to average */
		break;
	
	case 3: /* Scan for second missing tooth */
		err = ENGINE_CRANK;
		a = trigger_wheel_get_average();
		if( (t > (a<<1)) && (ctr <2) ){ /* Twice the amplitude of average & right after the First missing tooth */
			PRINT("Second Missing tooth %d\n", t);
			tooth_ctr = SYNC_2_TOOTH_CTR_POSITION;
			event_set_position(SYNC_2_DEGREE_POSITION / TRIGGER_WHEEL_RESOLUTION);
		}
		else{
			/* Adjust the first missing tooth position */
			PRINT("First Missing tooth ADJUST %d\n", t);
			tooth_ctr = SYNC_1_TOOTH_CTR_POSITION+1;
			event_set_position( (SYNC_1_DEGREE_POSITION+10) / TRIGGER_WHEEL_RESOLUTION);
		}
		add_vector(t); /* Don't add missing tooth to average */
		event_tick(0);
		state = 4;
		break;

	case 4: /* Main ticker */
		if(trigger_wheel_get_average() > AVERAGE_RUN_PERIOD)
			err = ENGINE_CRANK;
		else
			err = ENGINE_RUN;

		if(tooth_ctr == TOOTH_COUNT) 
			tooth_ctr = 1;
		else
			tooth_ctr++;

		/* Sanity check, looking for a missing tooth ==> Twice the amplitude of average */
		if(tooth_ctr == SYNC_1_TOOTH_CTR_POSITION || tooth_ctr == SYNC_2_TOOTH_CTR_POSITION || tooth_ctr == SYNC_3_TOOTH_CTR_POSITION){
			a = trigger_wheel_get_average();
			if( !(t > (a<<1)) ){
				FORCE_PRINT("SYNC %d:%ld\n", t, a);
				DIE(TRIGGER);
			}
		}
		else
			add_vector(t);

		event_tick(0);

		/* Check if _NEXT_ tooth is missing and if yes fake it */
		if( (tooth_ctr == 11) || (tooth_ctr == 14) || (tooth_ctr == 29) ){
			/* Fake missing tooth */
			tooth_ctr++;	// Don't bother with the wrap around
			event_tick(-1);
			/* Fake missing tooth */
			tooth_ctr++;	// Don't bother with the wrap around
			event_tick(-1);
		}
		break;
		
	default:
		DIE(TRIGGER);
	}
	ctr++;
	return err;
}

int get_rpm(void)
{
	unsigned long one_turn, t;
	one_turn = trigger_wheel_get_average() * (360UL / TRIGGER_WHEEL_RESOLUTION);
	t = (USEC_PER_SEC * 60 ) / one_turn;
	return t;
}

/*
 * At the current rate, how long does it take to go over n degree
 */
unsigned long deg_to_usec(int degree)
{
	if(degree <= 0)
		return 0;
	return ( (trigger_wheel_get_average() * (unsigned long)degree) /10UL);
}

int trigger_wheel_init(void)
{
	init_vector();
	trigger_wheel_init_platform();
	return 0;
}

