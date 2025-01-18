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

//#define USE_MALLOC
	
#define EVENT_TABLE_SIZE ( DEGREE_PER_ENGINE_CYCLE / TRIGGER_WHEEL_RESOLUTION)
#define MAX_EVENT 12
#ifdef USE_MALLOC
static struct event **event_table;
#else
static struct event malloc_event[MAX_EVENT];
static struct event *event_table[EVENT_TABLE_SIZE];
#endif
static int event_table_entry_nr;

static volatile unsigned char event_index, pending_event;

void event_register(int degree, fcn_t fcn, unsigned char cookie)
{
	struct event *e;
	static int index = 0;

	/* Sanity check */
	if(degree < 0)
		degree = DEGREE_PER_ENGINE_CYCLE + degree;
	if(degree < 0 || degree > DEGREE_PER_ENGINE_CYCLE)
		DIE(EVENT);
	if(event_table[degree/10])
		DIE(EVENT);
#ifdef USE_MALLOC
	e = malloc(sizeof(struct event));
	if(!e)
		DIE(EVENT);
#else
	if(index >= MAX_EVENT)
		DIE(EVENT);
	e = &malloc_event[index];
#endif
	e->cookie = cookie;
	e->fcn = fcn;
	index++;

	event_table[degree/10] = e; /* Signal even_tick */
	DEBUG("EVENT Register %d %p\n",degree/10,fcn);
}

void event_callback(void)
{
	struct event *e;

	if(pending_event == 0xff)
		return;
	e = event_table[pending_event]; 
	e->fcn(e);
	pending_event = 0xff; /* ACK we are done processing the event */
}

void event_tick(int flag)
{
	if(event_table[event_index]){
		if(flag < 0)
			DIE(EVENT);
		if(pending_event != 0xff)
			DIE(EVENT);
		pending_event = event_index; /* Publish the current event */
	}
	if(event_index == (event_table_entry_nr - 1) )
		event_index = 0;
	else
		event_index++;
}

void event_set_position(int pos)
{
	if(pos >= event_table_entry_nr)
		DIE(EVENT);
	event_index = pos;
}

void event_init(int size)
{
	if(size != EVENT_TABLE_SIZE)
		DIE(EVENT);
	event_index = 0;
	pending_event = 0xff;
	event_table_entry_nr = size;
#ifdef USE_MALLOC
	event_table = malloc( sizeof(struct event *) * size);
#endif
	memset(event_table, 0, sizeof(struct event *) * size);
}

