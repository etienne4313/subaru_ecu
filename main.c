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

int debug = 0;

/******************************************************************************/
/* Globals */
/******************************************************************************/
int trim_flag = 0;
int timing_advance = 0, timing_advance_enabled = 0;
int fuel_msec = 6;
int record_mode = 0;
volatile unsigned short capture_t;
volatile unsigned long curr_time;
#ifdef __DWELL_TEST__
volatile unsigned long T1, DWELL_DEBUG;
#endif
#ifdef __INJ_TEST__
volatile unsigned long T2, INJ_DEBUG;
#endif

/******************************************************************************/
/* RTOS */
/******************************************************************************/
OS_EVENT *engine_event;
static OS_STK management_thread_stack[STACK_SIZE];
static OS_STK engine_thread_stack[STACK_SIZE];

static int ON = 0;

static void user_cmd(int *timing_advance, int *fuel_msec)
{
	int r, x, y;
	unsigned char d;
	unsigned long u;

	if(!USART_data_available())
		return;
	d = getchar();
	switch (d) {
	case 't':
		trim_flag = 1;
		FORCE_PRINT("Trim\n");
		break;
	case 's':
		if(timing_advance_enabled){
			FORCE_PRINT("Timing OFF\n");
			timing_advance_enabled = 0;
		}
		else{
			FORCE_PRINT("Timing ON\n");
			timing_advance_enabled = 1;
		}
		break;
	case '=':
		if(*timing_advance < 40)
			(*timing_advance)++;
		FORCE_PRINT("T %d\n",*timing_advance);
		break;
	case '-':
		if(*timing_advance > 0)
			(*timing_advance)--;
		FORCE_PRINT("T %d\n",*timing_advance);
		break;
	case ']':
		if(*fuel_msec < 20)
			(*fuel_msec)++;
		FORCE_PRINT("F %d\n",*fuel_msec);
		break;
	case '[':
		if(*fuel_msec > 0)
			(*fuel_msec)--;
		FORCE_PRINT("F %d\n",*fuel_msec);
		break;
	case 'x':
		PRINT("KILL\n");
		DIE(MANAGEMENT);
		break;
	case 'r':
		r = get_rpm();
		u = deg_to_usec(10);
		FORCE_PRINT("RPM %d:%ld\n",r,u);
		break;
#define PRIME_FUEL 17
	case 'p':
		PRINT("Prime injector\n");
		for(y=0; y<4; y++){
			io_open_injector(y+1);
			for(x=0; x<PRIME_FUEL; x++)
				DELAY_MSEC(1);
			io_close_injector(y+1, 0);
		}
		PRINT("Prime injector done\n");
		break;
	case 'o':
		FORCE_PRINT("ON\n");
		io_relay_on();
		ON = 1;
		break;
	case 'k':
		starter_on();
		break;
	case 'y':
		if(record_mode)
			record_mode = 0;
		else
			record_mode = 1;
		break;

	/* Stable and Trimmed */
	case 'd':
#ifdef __DWELL_TEST__
		FORCE_PRINT("DWELL %ld\n", DWELL_DEBUG);
#endif
#ifdef __INJ_TEST__
		FORCE_PRINT("INJ %ld\n", INJ_DEBUG);
#endif
		break;
	default:
		break;
	}
}

static void management_thread(void *p)
{
	OS_CPU_SR cpu_sr;
	int loop = 0;

	watchdog_enable(WATCHDOG_250MS);
	wdt_reset();

	while(1){
		/* Feed the WD */
		wdt_reset();

		/* Run the User CLI */
		user_cmd(&timing_advance, &fuel_msec);

		if(!(loop%20)){
			OS_ENTER_CRITICAL();
			/* Duty cycle Gaz pump. Watch for read/modify/write against IRQ and high prioroty task */
			if(ON)
				gaz_toggle();
			OS_EXIT_CRITICAL();
		}

		loop++;

		OSTimeDlyHMSM(0,0,0,100);
	}
}

void osdie(int err, int line)
{
	OS_CPU_SR cpu_sr;

	OS_ENTER_CRITICAL();
#ifdef _BUG_328_NANO_
	/* 
	 * Clone 328 Nano bootloader doesn't clear the WD when coming up and keeps reloading evey 16msec
	 * Here we are branching back to RESET vector directly.
	 * 	- Punch the WD ASAP to avoid going in a bootloop at the bootloaded level
	 * 	- Add timeout in the WD to make sure we can survived thru the bootload all the way back to main()
	 * 	- Stack and memory will get cleaned up OK
	 * 	- I/O won't comeback to default reset value but we are doing clone_all_io() in all cases.
	 * 	- We must stop all the timers manually here.
	 */
	wdt_reset();
	watchdog_enable(WATCHDOG_2S);
	fprintf(stderr, "DIE %d : %d\n", err, line);
	{
		/* timer1_disable */
		unsigned char t;
		t = TCCR1B;
		t &= ~(1<<CS02   | 1<<CS01  | 1<<CS00);
		TCCR1B = t;
	}
	__asm__ __volatile__ (  "jmp __ctors_end \n\t" );
#endif
	FORCE_PRINT("DIE %d : %d\n", err, line); 
	close_all_io();
	/* Wait for WD to reset */
	while(1){};
	OS_EXIT_CRITICAL();
}

int main(void)
{
	OS_CPU_SR cpu_sr;

	watchdog_enable(WATCHDOG_2S);
	wdt_reset();

	OS_ENTER_CRITICAL();

	lib_init();

	io_init();

#ifdef __UNIT_TEST__
	unit_test();
#endif

	FORCE_PRINT("ENTER\n");
	
	OSInit();

	/* Low priority Management thread: GUI, gaz pump, watchdog, engine state */
	OSTaskCreate(management_thread, NULL, &management_thread_stack[STK_HEAD(STACK_SIZE)], 3);
	/* Highest priority Engine thread: Engine state machine */
	OSTaskCreate(engine_thread, NULL, &engine_thread_stack[STK_HEAD(STACK_SIZE)], 1);

	engine_event = OSSemCreate(0);
	
	timer_init();

	/* IRQ are disabled up to this point where we run start the first thread */
	OSStart();

	DIE(ERROR_INIT);

	OS_EXIT_CRITICAL();
	return 0;
}

