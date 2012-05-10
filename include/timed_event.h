#ifndef TIMED_EVENT
#define TIMED_EVENT

#define CAN       1
#define SERVICE    2

#include <event.h>

class DC;

typedef int (*devices_action_func)(DC& container);

#define NORMAL_EVENT 0xE0
#define COUNTER_EVENT 0xEC

/* timed_event structure holds information about one of the events that should be performed regularly.
 * Currently, there are two types of event execution:
 *  
 * NORMAL_EVENT:
 * - start time describes a moment after server start, when event must be performed for the first time.
 * - interval describes repetition period of corresponding event.
 * COUNTER_EVENT events:
 * - start time specifies a moment when corresponding event must be performed relatively to some point of time
 * - interval describes internal state of event (counter):
 *   { tv_sec,tv_usec } in timeval structure used here for compatibility
 *   corresponds to { max_value,current_value }
 * - - max_value holds maximal index value for current_value.
 *     When current_value reaches max_value, event should be fired.
 * - - current_value holds index value.
 *     This value increments every time internal callback reaches point, when event can be fired.
 */
struct timed_event
{
	const timeval start;
	timeval interval;
	const devices_action_func action;
        int type;
	event ev;
	void* arg;
};

//prepare given set of timed events for execution
void event_set_timed_events(timed_event* timed_events,uint32_t len,DC* container);

//add given set of timed events to libevent loop as a given event type
void event_add_timed_events(timed_event* timed_events,uint32_t len,int type);

#endif //TIMED_EVENT
