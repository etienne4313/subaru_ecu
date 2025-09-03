/*
 * Hyundai 60-2 Crank Trigger Wheel Driver
 * For Hyundai Elantra 1.8L (common 60-2 pattern)
 *
 * 60 possible teeth, 58 actual teeth, 2 missing (single gap)
 * Missing tooth marks TDC for cylinder 1
 */

#include <ecu.h>

#define TOOTH_COUNT 60
#define MISSING_TOOTH_GAP 2
#define SYNC_TOOTH_POSITION 60 // After the missing gap, tooth 0

#define MIN_TICK_PERIOD_USEC_6000RPM (166) // 1/(6000/60)/60 = 166us
#define MAX_TICK_PERIOD_USEC_80RPM (125000UL) // 1/(80/60)/60 = 125000us
#define AVERAGE_RUN_PERIOD (2000UL)

#define MIN_SAMPLE 10
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
	unsigned short old = vector[idx];
	vector[idx] = t;
	running_sum += t;
	running_sum -= old;
	idx++;
	if(idx == AVG_SIZE)
		idx = 0;
}

static unsigned long trigger_wheel_get_average(void)
{
	OS_CPU_SR cpu_sr;
	unsigned long t;
	OS_ENTER_CRITICAL();
	t = running_sum;
	OS_EXIT_CRITICAL();
	return t >> AVG_BIT_SHIFT;
}

unsigned char run_trigger_wheel(unsigned short t)
{
	static unsigned char state = 0, ctr, tooth_ctr;
	int err = ENGINE_INIT;
	unsigned long a;

	if(record_mode)
		FORCE_PRINT("%d:%ld\n", t, trigger_wheel_get_average());

	if (t > MAX_TICK_PERIOD_USEC_80RPM || t < MIN_TICK_PERIOD_USEC_6000RPM){
		if(state == 3){
			FORCE_PRINT("Glitch %d:%d\n", t, state);
			DIE(TRIGGER);
		}
		state = 0;
	}

	switch (state){
	case 0: // Initialization
		ctr = 0;
		state = 1;
		tooth_ctr = 1;
		capture_t = 0;
		init_vector();
		break;
	case 1:
		// Gather stable pulses
		if(t < 20000){
			add_vector(t);
			if(ctr >= MIN_SAMPLE){
				ctr = 0;
				state = 2;
				break;
			}
			break;
		}
		state = 0;
		break;
	case 2: // Look for missing tooth
		err = ENGINE_CRANK;
		a = trigger_wheel_get_average();
		if(t > (a<<1)){
			PRINT("Missing tooth detected %d:%ld\n", t, a);
			ctr = 0;
			tooth_ctr = 1;
			event_set_position(0); // TDC for cyl 1
			state = 3;
			break;
		}
		add_vector(t);
		break;
	case 3: // Main ticker
		if(trigger_wheel_get_average() > AVERAGE_RUN_PERIOD)
			err = ENGINE_CRANK;
		else
			err = ENGINE_RUN;

		if(tooth_ctr == TOOTH_COUNT)
			tooth_ctr = 1;
		else
			tooth_ctr++;

		// Check for missing tooth
		if(tooth_ctr == TOOTH_COUNT){
			a = trigger_wheel_get_average();
			if(!(t > (a<<1))){
				FORCE_PRINT("SYNC lost %d:%ld\n", t, a);
				DIE(TRIGGER);
			}
		}else{
			add_vector(t);
		}
		event_tick(0);
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
	one_turn = trigger_wheel_get_average() * (TOOTH_COUNT - MISSING_TOOTH_GAP);
	t = (USEC_PER_SEC * 60 ) / one_turn;
	return t;
}

unsigned long deg_to_usec(int degree)
{
	if(degree <= 0)
		return 0;
	return ( (trigger_wheel_get_average() * (unsigned long)degree) / (360UL/TOOTH_COUNT) );
}

int trigger_wheel_init(void)
{
	init_vector();
	trigger_wheel_init_platform();
	return 0;
}
