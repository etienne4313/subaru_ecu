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

/*
 * 4 Stroke Engine ( $$$$ Dwell, ^^^^ Ignition, #### Fuel )
 *
 * 	5msec Dwell: @1000RPM=30deg; @2000RPM=60deg; @4000RPM=120deg
 *
 *           40 BTDC      0 TDC            180 BDC           360 TDC           540 BDC
 *            ||-----------||-----Power-----||-----Exhaust----||---Admission----||---Compression---
 *  $$$$$$$$$$$$
 *              ^^^^^^^^^^^
 *                           #################################
 *
 * The firing order determine the TDC sequence
 *  EX 1-3-2-4 => TDC1    TDC3    TDC2    TDC4
 *                0deg    180deg  360deg  540deg
 *
 * Over 360deg, TDC can be either Power OR Intake stroke and without the CAM
 * signal we don't know which phase we are in since TDC1 Power stroke can be
 * either 0deg (***) OR 360deg (!!!) as per picture below.
 *
 *        0 deg            180 deg           360 deg           540 deg           720 deg
 * ---------||---------------||----------------||----------------||--------------||
 *          TDC1             TDC2              TDC3             TDC4
 *
 * CYL1:    TDC***                             TDC!!!
 *          Power-->                           Intake-->
 *
 * CYL2:    TDC!!!                             TDC***
 *          Intake-->                          Power-->
 *
 * CYL3:                     TDC***                              TDC!!!
 *                           Power-->                            Intake-->
 *
 * CYL4:                     TDC!!!                              TDC***
 *                           Intake-->                           Power-->
 *
 * For that reason the ignition mode is initially set to wasted spark i.e. fire
 * during the Power stroke and the Exhaust stroke of a given pair of cylinders.
 *
 * Below the function trim_to_sequential() dynamically discover the phase of the
 * engine by guessing the ignition phase and noting down if its the right guess
 * OR not by monitoring any drop in the RPM.
 *
 * For Intake we don't care much bcos the fuel can sit on the valve for some time
 * either the Intake valve opens right away or in another 360 deg. So for that
 * reason we pick a default phase for the fuel and adjust it later in trim_to_sequential()
 */
static struct engine_schedule four_stroke[4] = {
	{ .degree = 0,   .coil_cyl = CYL12, .fuel_cyl = CYL1 },
	{ .degree = 180, .coil_cyl = CYL34, .fuel_cyl = CYL3 },
	{ .degree = 360, .coil_cyl = CYL21, .fuel_cyl = CYL2 },
	{ .degree = 540, .coil_cyl = CYL43, .fuel_cyl = CYL4 },
};

/******************************************************************************/
/* ENGINE TRIM */
/******************************************************************************/
static void tdc1_0deg(void)
{
	struct engine_schedule *sched;

	FORCE_PRINT("TDC1 @0deg \n");
	sched = &four_stroke[0];
	sched->coil_cyl = CYL1;
	sched->fuel_cyl = CYL1;
	sched = &four_stroke[1];
	sched->coil_cyl = CYL3;
	sched->fuel_cyl = CYL3;
	sched = &four_stroke[2];
	sched->coil_cyl = CYL2;
	sched->fuel_cyl = CYL2;
	sched = &four_stroke[3];
	sched->coil_cyl = CYL4;
	sched->fuel_cyl = CYL4;
}

static void tdc1_360deg(void)
{
	struct engine_schedule *sched;

	FORCE_PRINT("TDC1 @360deg \n");
	sched = &four_stroke[0];
	sched->coil_cyl = CYL2;
	sched->fuel_cyl = CYL2;
	sched = &four_stroke[1];
	sched->coil_cyl = CYL4;
	sched->fuel_cyl = CYL4;
	sched = &four_stroke[2];
	sched->coil_cyl = CYL1;
	sched->fuel_cyl = CYL1;
	sched = &four_stroke[3];
	sched->coil_cyl = CYL3;
	sched->fuel_cyl = CYL3;
}

static void trim_to_sequential(void)
{
	int	r;
	struct engine_schedule *sched;
	static int state = 0, ctr = 0, avg_rpm = 0, min_rpm = 0;

	if(state == -1)
		return;

	r = get_rpm();
//	PRINT("RPM %d:%d\n",r, state);
	
	switch (state){
	case 0:
		avg_rpm = avg_rpm + r;
		ctr++;
		if(ctr >= 16){
			avg_rpm = avg_rpm >> 4;
			min_rpm = divu10(avg_rpm);
			min_rpm = avg_rpm - min_rpm;
			PRINT("Target RPM >= %d\n", min_rpm);
			ctr = 0;
			state = 1;
		}
		break;
	case 1:
		/*
		 * Take down half of the wasted spark configuration
		 * 	e.g. Leave TDC3 = CYL21 and TDC4 = CYL43 
		 * 	and note down a 10% deviation from current value.
		 */
		sched = &four_stroke[0];
		sched->coil_cyl = CYL1;
		sched = &four_stroke[1];
		sched->coil_cyl = CYL3;
		state = 2;
		break;
	case 2:
		/*
		 * If RPM is holding within 10% for few turns that means we are TDC1 @ 0deg
		 * so we can trim down the remaining TDC3 and TDC4
		 */
		ctr++;
		if(ctr > 10 && r >= min_rpm){
			tdc1_0deg();
			state = -1;
			break;
		}
		if(r < min_rpm ){
			/* 
			 * If RPM drops then put everything back in full Wasted Spark
			 */
			PRINT("RECOVER TDC1 @0deg \n");
			sched = &four_stroke[0];
			sched->coil_cyl = CYL12;
			sched = &four_stroke[1];
			sched->coil_cyl = CYL34;
			state = 3;
			break;
		}
		break;
	case 3:
		/* 
		 * We know we are not in TDC1 @ 0deg so we must be in TDC1 @ 360deg
		 * Let's wait for the RPM to recover
		 */
		if(r >= min_rpm)
			state = 4;
		break;
	case 4:
		tdc1_360deg();
		state = -1;
		break;
	default:
		break;
	}
}

/******************************************************************************/
/* BTDC 140 CYL 1 2 3 4 */
/******************************************************************************/
static void btdc_140(struct event *e)
{
	long time;
	OS_CPU_SR cpu_sr;
	struct engine_schedule *sched = &four_stroke[(int)e->cookie];

	/* Dwell starts now;
	 * Max timing advance is 30deg => 110deg from here
	 * Coils needs 5msec Dwell time so cannot reach 4000RPM
	 * 	@1000RPM=30deg; @2000RPM=60deg; @4000RPM=120deg
	 */
	io_open_coil(sched->coil_cyl, get_monotonic_time());

	if(timing_advance_enabled && timing_advance != 0){
		/* Here we project how much time it takes to reach to timing advance point based on the current speed */
		time = deg_to_usec(140 - timing_advance);
		OS_ENTER_CRITICAL();
		schedule_work_absolute(io_close_coil, sched->coil_cyl, curr_time + time); /* Ignition schedule */
		OS_EXIT_CRITICAL();
	}
}

/******************************************************************************/
/* BTDC 10 CYL 1 2 3 4 */
/******************************************************************************/
static void btdc_10(struct event *e)
{
	struct engine_schedule *sched = &four_stroke[(int)e->cookie];

	if(timing_advance_enabled && timing_advance == 0) /* Default when timing advance is enabled */
		io_close_coil(sched->coil_cyl, get_monotonic_time());
}

/******************************************************************************/
/* BTDC 0 CYL 1 2 3 4 */
/******************************************************************************/
static void btdc_0(struct event *e)
{
	OS_CPU_SR cpu_sr;
	struct engine_schedule *sched = &four_stroke[(int)e->cookie];

	/* Always close the coil here */
	io_close_coil(sched->coil_cyl, get_monotonic_time());

	OS_ENTER_CRITICAL();
	io_open_injector(sched->fuel_cyl); /* Now */
	schedule_work_absolute(io_close_injector, sched->fuel_cyl,  get_monotonic_time() + (USEC_PER_MSEC * fuel_msec)); /* FUEL schedule */
	OS_EXIT_CRITICAL();

	if( (e->cookie == 0) && trim_flag ){ /* Trim only from CYL1 */
		trim_to_sequential();
#ifdef __ADVANCE_TIMING_TEST__
		TIMING_DEBUG = curr_time - T1;
#endif
	}
}

void engine_thread(void *p)
{
	int x, init = 0;
	INT8U err;
	OS_CPU_SR cpu_sr;
	unsigned long t;

	event_init(DEGREE_PER_ENGINE_CYCLE / TRIGGER_WHEEL_RESOLUTION);

	for(x=0; x<4; x++){
		event_register(normalize_deg(four_stroke[x].degree - 140), btdc_140, x);
		event_register(normalize_deg(four_stroke[x].degree - 10), btdc_10, x);
		event_register(normalize_deg(four_stroke[x].degree - 0), btdc_0, x);
	}

	/*
	 * Init the trigger wheel IRQ and start driving the event_tick() callback
	 * which is signaling on SEM engine_event
	 */
	trigger_wheel_init();
	
	engine_state = ENGINE_STOP;

	while(1){
		/* 
		 * Wait for the trigger wheel notification
		 * When the engine has started running wait on the semaphore with a timeout so that we can 
		 * catch the scenario where the engine stops
		 */
		if(engine_state == ENGINE_RUN || init == 1){
			OSSemPend(engine_event, 100, &err);
			if(err == OS_ERR_TIMEOUT){
				DIE(-1);
			}
			init = 1;
		}
		else
			OSSemPend(engine_event, 0, &err);

		/* Capture the crank period */
		OS_ENTER_CRITICAL();	
		t = capture_t;
		capture_t = 0;
		OS_EXIT_CRITICAL();

		/* Run the state machine for this engine type */
		engine_state = run_trigger_wheel(t);

		/* Process the event callback */
		event_callback();
#ifdef __LOOP_TIMING_TEST__
		{
			t = get_monotonic_time();
			t = t - curr_time;
			if(t>100)
				FORCE_PRINT("%ld\n", t);
		}

#endif
	}
}

