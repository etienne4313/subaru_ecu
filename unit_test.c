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
			
#ifdef __UNIT_TEST__
static void injector_test(int inj)
{
	int x=0;
	FORCE_PRINT( "Injector #%d toggle\n", inj);
	io_relay_on();
	while(x<100){
		io_open_injector(inj);
		DELAY_MSEC(10);
		io_close_injector(inj, 0);
		DELAY_MSEC(10);
		x++;
	}
	io_relay_off();
}

static void coil_test(int cyl)
{
	int x=0;

	FORCE_PRINT( "Coil Cyl #%d toggle\n", cyl);
	io_relay_on();

	while(x<100){
		io_open_coil(cyl, 0);
		DELAY_MSEC(4); // DWELL TIME
		io_close_coil(cyl, 0);
		DELAY_MSEC(10);
		x++;
	}
	io_relay_off();
}

static void relay_test(void)
{
	int x=0;

	FORCE_PRINT( "Relay toggle\n");
	while(x<3){
		io_relay_on();
		DELAY_MSEC(1000);
		gaz_relay_on();
		DELAY_MSEC(1000);
		io_relay_off();
		DELAY_MSEC(1000);
		gaz_relay_off();
		DELAY_MSEC(1000);
		x++;
	}
	io_relay_off();
}

static void full_sequence(void)
{
	FORCE_PRINT( "Full UT\n");

	relay_test();
	injector_test(1);
	DELAY_MSEC(1000);
	injector_test(2);
	DELAY_MSEC(1000);
	injector_test(3);
	DELAY_MSEC(1000);
	injector_test(4);
	DELAY_MSEC(1000);
	coil_test(1);
	DELAY_MSEC(1000);
	coil_test(2);
	DELAY_MSEC(1000);
	coil_test(3);
	DELAY_MSEC(1000);
	coil_test(4);
	DELAY_MSEC(1000);
}

void unit_test(void)
{
	unsigned char i;

	/* Disable WD for UT */
	wdt_reset();
	watchdog_enable(WATCHDOG_OFF);
again:
	USART_Flush();
	io_relay_off();
	FORCE_PRINT( "Going into debug mode, Enter TC, x to continue to main loop\n");
	i = getchar();
	switch(i){
		case 'v':
			full_sequence();
			break;
		case 'r':
			relay_test();
			break;
		case '1':
			injector_test(1);
			break;
		case '2':
			injector_test(2);
			break;
		case '3':
			injector_test(3);
			break;
		case '4':
			injector_test(4);
			break;
		case 'a':
			coil_test(1);
			break;
		case 'b':
			coil_test(2);
			break;
		case 'c':
			coil_test(3);
			break;
		case 'd':
			coil_test(4);
			break;
		case 'x':
			watchdog_enable(WATCHDOG_2S); /* Set the WD back to original setting before leaving UT */
			wdt_reset();
			return;
		default:
			FORCE_PRINT( "Error\n");
		break;
	}
	goto again;
}

#endif /* __UNIT_TEST__ */

