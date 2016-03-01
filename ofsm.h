/*
ofsm.h - "Orchestrated" Finite State Machines library (OFSM) for Micro-controllers (MCU)
Copyright (c) 2016 Maksym Moyseyev.  All right reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
.............................................................................

WHAT IT IS
==========
OFSM is a lightweight Finite State Machine macro library to:
* Help writing MCU power efficient sketches;
* Encourage more efficient code organization by use of interrupts (as much as possible), while keeping interrupt handlers small and fast;

GOALS
=====
Main goals of this implementation are:
* Testability!
* Easy of debugging, including debugging on PC before flushing to the MCU;
* Easy of customization and support of different MCUs;
* Re-usability of logic and simplicity of scaling up and down;
* Memory used by internal code should depend on necessity and driven by configuration. Thus, reducing memory consumption and prevention of "dead" code;

DESIGN/USAGE NOTES
==================
Important design decisions and usage notes:
* Multiple state machines should peacefully co-exist and play together;
* Structure of application should resemble Arduino sketch, namely it should have void setup() {...} and void loop() {...} functions. main(...) function should not be defined or
  it needs to be surrounded by #ifndef OFSM_CONFIG_PC_SIMULATION ... main(...) #endif for simulation to work, as it declares its own main() function.
* OFSM is relaying on external "heartbeat" provider, so it can be easily replaced and configured independently.
  Default provider piggybacks on Arduino Timer_0 clock;
* Internal time within OFSM is measured in ticks. Tick may represent microseconds, milliseconds or any other time dimensions depending on "heartbeat" provider. (see above).
  Thus, if rules (stated in this section) are followed. The same implementation can run with different speeds and environments.
* As outcome of previous statement, it is recommended that event handlers rely on time (in ticks) supplied by OFSM (ofsm_get_time()) instead of external sources (eg: millis()/micros ...);
* It is recommended that handlers never do a time math on their own, and rely completely on relative delays. As time math should account for timer overflow to work in all cases.
* Event handlers are not expected to know transition states, and should rely on events to move FSM. (see Handlers API for details);
* FSMs can be organized into groups of dependent state machines. NOTE: One group shares the same set of events. Each queued event processed by all FSMs within the group;
* Events are never directly processed but queued, allowing better parallelism between multiple FSM.
* Event queues are protected from overflow. Thus if events get generated faster than FSMs can process them, the events will be compressed (see ofsm_queue...()) or dropped;
* The same event handler can be shared between the states of the same FSM, within different FSMs or even FSMs of different groups;
* FSMs can be shared between different groups.
* Default hardware specific implementation relies on AVR API (vs. Arduino API) except couple of cases related to Timer and Serial access.
* OFSM is driven by time (transition delays) and events (from interrupt handlers) and TIMEOUT event become a very important concept.
  Therefore, event id 0 is reserved for TIMEOUT/STATE ENTERING event and must be declared in all FSMs;

API NOTE
========
* API related documentation sections are created to provide with quick reference of functionality and application structure.
 More detailed documentation (when needed) can be found above implementation/declaration of concrete function, define etc;

FSM DESCRIPTION/DECLARATION API
===============================
* All event handlers are: typedef void(*FSMHandler)(OFSM *fsm); Example: void MyHandler(OFSMState *fsm){...};
* Initialization handler (when provided) acts as regular event handler in terms of access to fsm_... function and makes the same impact to FSM state;
* Example of Transition Table declaration (Ex - event X, Sx - state X, Hxy - state X Event Y handler) (in reality all names are much more meaningful):
OFSMTransition transitionTable1[][1 + E3] = {
//timeout,      E1,         E2          E3
{ { H00, S0 }, { H01, S1 }, { H02, S0 },{ H03, S1 } },  //S0
{ { null,S0 }, { H11, S0 }, { null,S0 },{ H11, S1 } },  //S1
};

* Set of preprocessor macros to help user to declare state machine with list amount of effort. These macros include:
    OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr)
    OFSM_DECLARE_GROUP_SIZE_1(eventQueueSize, fsmId0) //setup group of 1 FSM
    ...
    OFSM_DECLARE_GROUP_SIZE_5(eventQueueSize, fsmId0, ....,fsmId4) //setup group of 5 FSMs
    OFSM_DECLARE_1(grpId0) //OFSM with 1 group
    ...
    OFSM_DECLARE_5(grpId0,....grpId4) //OFSM with 10 groups
    OFSM_DECLARE_BASIC(transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr) //single FSM single Group declaration

IMPORTANT: 
* ...DECLARE... macros should be called in the global scope.
* OFSM_SETUP() macro must be called at the very beginning of setup() function.
Example:
 ...
 ...OFSM_DECLARE...
 void setup() {
	 OFSM_SETUP();
	 ...
 }
 void loop() {
    OFSM_LOOP(); //this call never returns 
 }
 ...

INITIALIZATION AND START API
===========================
* All configuration switches (#define OFSM_CONFIG...)  should appear before "ofsm.h" gets included;
* call OFSM_SETUP() to initialize hardware, setup interrupts. During initialization phase all registered FSM initialization handlers will be called;
* call OFSM_LOOP() to start OFSM, this function never returns;
* When FSM is declared, it gets configured with INFINITE_SLEEP flag set. If wakeup at start time is required the following methods can be used:
  1) When FSM initialization handler is supplied it acts like regular FSM event handler  and can adjust FSM state by means of fsm_... API. For example: it can set transition delay as need.
  2) ofsm_queue... API calls can be used right before OFSM_LOOP() to queue desired events. Including TIMEOUT event.
  NOTE: TIMEOUT event doesn't wake FSM that is in INFINITE_TIMEOUT by default. But it will do it once if event was queued before OFSM_LOOP().


INTERRUPT HANDLERS API
======================
Interrupt handlers are expected to "know about" the following functions:
* ofsm_queue_group_event(groupIndex, eventCode, eventData)
* ofsm_queue_global_event(eventCode, eventData) //queue the same event to all groups

FSM EVENT HANDLERS API
======================
* fsm_prevent_transition(OFSM *fsm)
* fsm_set_transition_delay(OFSM *fsm, unsigned long delayTicks)
* fsm_set_inifinite_delay(OFSM *fsm)
* fsm_get_private_data(OFSM *fsm)
* fsm_get_private_data_cast(OFSM *fsm, castType)                // example: MyPrivateStruct_t *data = fsm_get_private_data_cast(fsm, MyPrivateStruct_t*)
* fsm_get_fsm_ndex(OFSM *fsm)                                   //index of fsm in the group
* fsm_get_group_index(OFSM *fsm)                                //index of group in OFSM
* fsm_get_state(OFSM *fsm)
* fsm_get_event_code(OFSM *fsm)
* fsm_get_event_data(OFSM *fsm)
* fsm_set_next_state(OFSM *fsm, uint8_t nextStateId)            //TRY TO AVOID IT! overrides default transition state from the handler 
* fsm_queue_group_event(OFSM *fsm, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData)
* ofsm_queue_global_event(uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData)
* ofsm_debug_printf(format, ....)	                            //debug print

CONFIGURATION
=============
OFSM can be "shaped" in many different ways using configuration switches. NOTE: all configuration switches must be defined before #include <ofsm.h>:
#define OFSM_CONFIG_DEBUG_LEVEL 3                               //Default undefined. Enables debug print functionality
#define OFSM_CONFIG_DEBUG_LEVEL_OFSM OFSM_CONFIG_DEBUG_LEVEL    //Default OFSM_CONFIG_DEBUG_LEVEL. when level 0, all debug prints from OFSM will be disabled.
#define OFSM_CONFIG_EXTERNAL_SERIAL_INITIALIZATION              //Default undefined. if defined no serial initialization will be made, assuming it is initialized elsewhere
#define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY              //Default 0. Specifies default transition delay, if event handler didn't set one.
#define OFSM_CONFIG_WAKEUP_TIME_RELATIVE_TO_MOST_CURRENT_TIME   //Default undefined. If precision of delay is important define this constant. Otherwise, keep it undefined to achieve better synchronism with other State Machines.
#define OFSM_CONFIG_EVENT_DATA_TYPE uint8_t                     //Default uint8_t (8 bits). Event data type.
#define OFSM_CONFIG_SLEEP_MODE SLEEP_MODE_PWR_DOWN              //Default: SLEEP_MODE_PWR_DOWN, see ( <avr/sleep.h> for more options)
#define OFSM_CONFIG_DEBUG_PRINT_ADD_TIMESTAMP                   //Default undefined. When defined ofsm_debug_print() it will prefix debug messages with [<current time in ticks>]<debug message>.
#define OFSM_CONFIG_PC_SIMULATION                               //Default undefined. See PC SIMULATION section for additional info
#define OFSM_CONFIG_PC_SIMULATION_TICK_MS 1000                  //Default 1000 milliseconds in one tick.
#define OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS 0     //default 0. Sleep period (in milliseconds) before reading new simulation event. May be helpful in batch processing mode.
#define OFSM_CONFIG_ATOMIC_BLOCK ATOMIC_BLOCK                   //Default: ATOMIC_BLOCK; Retail compatibility with original: see implementation details in <util/atomic.h> from AVR SDK.
#define OFSM_CONFIG_ATOMIC_RESTORESTATE ATOMIC_RESTORESTATE     //Default: ATOMIC_RESTORESTATE see <util/atomic.h>

CUSTOMIZATION
=============
#define OFSM_CONFIG_CUSTOM_DEBUG_PUTS _ofsm_debug_puts                          //typedef: void _ofsm_debug_puts(const char* ). Function that prints/sends formatted debug messages.
#define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_enter_sleep                   //typedef: void _ofsm_enter_sleep().
#define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_wakeup                             //typedef: void _ofsm_wakeup().
#define OFSM_CONFIG_CUSTOM_INIT_HEARTBEAT_PROVIDER_FUNC _ofsm_piggyback_timer_0 //typedef: void _ofsm_piggyback_timer_0(). Function: expected to call: ofsm_hearbeat(unsigned long currentTicktime)
#define OFSM_CONFIG_CUSTOM_PC_SIMULATION_EVENT_GENERATOR_FUNC _ofsm_simulation_event_generator //typedef: void _ofsm_simulation_event_generator(). Function: expected to call: ofsm_hearbeat(unsigned long currentTicktime)

PC SIMULATION
=============
Ultimate goal is to be able to run properly formatted sketch in simulation mode on any PC using GCC or other C+11 compatible compiler (including VSC012) without any change.
To do that skatch code should follow the following rules:
* OFSM_CONFIG_PC_SIMULATION definition should appear at the very top of the sketch file before any other includes
* All MCU specific code should be surrounded by
	#ifndef OFSM_CONFIG_PC_SIMULATION
		<MCU specific code>
	#endif
* All simulation specific code should be similarly surrounded with #ifdef OFSM_CONFIG_PC_SIMULATION.... (fsm_debug_printf() can be left as is, it works in both simulation and non-simulated environments)
	Default simulation implementation is based on C++ threads. First thread is running state machine and Second is running heartbeat provider with resolution of  OFSM_CONFIG_PC_SIMULATION_TICK_MS.
	Main program's thread is used as event generator. Default implementation reads input in infinite loop and queues corresponding event to OFSM. see PC SIMULATION EVENT GENERATOR section for details.
	IMPORTANT NOTE: For simulation to work, it re-defines almost all OFSM_CONFIG_CUSTOM.... macros.
	Therefore, if user does customization for MCU purposes, all declarations should be surrounded by #ifndef OFSM_CONFIG_PC_SIMULATION .... #endif construct.
	At the same time one can completely override all parts of simulation as well.

PC SIMULATION EVENT GENERATOR
=============================
Event generator is implemented as infinite loop which reads event even information for stdin and queues it in OFSM.
Input Format is: ['f']<event code or command>[,<event data>[,<designated group index>] ([] - denotes optional parameters).
event command can be prefixed with 'f' to indicate that event needs to be forced (see queue_event api for explanations). By default event will replace the same event on the top of the queue.
<event code or command> - numeric event code or word 'exit' (to exit simulation) or 'sleep' (to put generator to sleep for number of milliseconds).
sleep is not needed when event generator works in interactive mode as it is naturally sleeps while awaiting users input, but may be important in batch mode (see below).
<event data> - (optional) number representing event data (default is 0) or number of milliseconds in case of 'sleep' (default 1000 (one second)).
<designated group index> - (optional) group index for which event should be queued (default is 0), or letter 'g' to queue global event (to all groups)
Examples of input:
* 1  //queue event 1 data 0 into group 0
* 1,200,1 //queue event 1 data 200 into group 1
* sleep,10000 //sleep for 10 seconds before accepting new input)
* exit //terminate simulation

Simulation can also be performed in batch mode. In which case user simply writes sequence of events into text file and pipes it into the sketch executable, example:
* myscketch < events.txt

LIMITATIONS
============
* Number of events in single FSM, number of FSMs in single group, number of groups within OFSM must not exceed 255!

TODO
====
* Remove group maintenance overhead for single group setups.
* simulation: use -f <file> command line parameter to supply event file
* simulation: allow manual heartbeat from event generator
*/
#ifndef __OFSM_H_
#define __OFSM_H_

/*Visual C/C++ support*/
#ifdef _MSC_VER
#   include<stdint.h> /*for uint8_t support*/
#   ifndef __attribute__
#       define __attribute__(xxx) 
#   endif
#endif

#ifdef OFSM_CONFIG_PC_SIMULATION
#   include <iostream>
#   include <thread>
#   include <string>
#   include <vector>
#   include <sstream>
#   include <algorithm>
#   include <mutex>
#   include <condition_variable>
#endif

//default event data type
#ifndef OFSM_CONFIG_EVENT_DATA_TYPE
#	define OFSM_CONFIG_EVENT_DATA_TYPE uint8_t
#endif

/*--------------------------------
Type definitions
----------------------------------*/
struct OFSMTransition;
struct OFSMEventData;
struct OFSM;
struct OFSMState;
struct OFSMGroup;
typedef void(*OFSMHandler)(OFSMState *fsmState);

/*#define ofsm_get_time(time,timeFlags) //see implementation below */

void ofsm_heartbeat(unsigned long currentTime);
void ofsm_queue_global_event(bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
void ofsm_queue_group_event(uint8_t groupIndex, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
void ofsm_heartbeat(unsigned long currentTime);

void _ofsm_queue_group_event(uint8_t groupIndex, OFSMGroup *group, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
void _ofsm_group_process_pending_event(OFSMGroup *group, uint8_t groupIndex, unsigned long *groupEarliestWakeupTime, uint8_t *groupAndedFsmFlags);
void _ofsm_fsm_process_event(OFSM *fsm, uint8_t groupIndex, uint8_t fsmIndex, OFSMEventData *e);
void _ofsm_check_timeout();
void _ofsm_setup();
void _ofsm_start();

/*see declaration of fsm_... macros below*/

#ifdef OFSM_CONFIG_DEBUG_LEVEL
#define _OFSM_IMPL_DEBUG_PRINTF
#include <string>
void ofsm_debug_printf(const char* format, ...);
#endif

/*---------------------
Simulation defines
-----------------------*/
#ifdef OFSM_CONFIG_PC_SIMULATION
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <condition_variable>

/*DEBUG should be turned on during simulation*/
#ifndef OFSM_CONFIG_DEBUG_LEVEL
#define OFSM_CONFIG_DEBUG_LEVEL 0
#endif

/*other overrides*/
#ifndef OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY
#	define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0
#endif 

#ifndef OFSM_CONFIG_PC_SIMULATION_TICK_MS
#	define OFSM_CONFIG_PC_SIMULATION_TICK_MS 1000
#endif

#ifndef OFSM_CONFIG_CUSTOM_PC_SIMULATION_EVENT_GENERATOR_FUNC
	void _ofsm_simulation_event_generator();
#	define OFSM_CONFIG_CUSTOM_PC_SIMULATION_EVENT_GENERATOR_FUNC _ofsm_simulation_event_generator
#	define _OFSM_IMPL_EVENT_GENERATOR
#endif

#ifndef OFSM_CONFIG_CUSTOM_INIT_HEARTBEAT_PROVIDER_FUNC
	static void inline _ofsm_dummy()__attribute__((__always_inline__)){}
#	define OFSM_CONFIG_CUSTOM_INIT_HEARTBEAT_PROVIDER_FUNC _ofsm_dummy
#endif

#ifndef OFSM_CONFIG_CUSTOM_WAKEUP_FUNC
#	define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_simulation_wakeup
	void _ofsm_simulation_wakeup();
#	define _OFSM_IMPL_WAKEUP
#endif

#ifndef OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC
#	define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_simulation_enter_sleep
	void _ofsm_simulation_enter_sleep();
#	define _OFSM_IMPL_ENTER_SLEEP
#endif

#ifndef OFSM_CONFIG_CUSTOM_DEBUG_PUTS
#	ifdef OFSM_CONFIG_DEBUG_LEVEL
		void _ofsm_simulation_debug_puts(const char*);
#		define OFSM_CONFIG_CUSTOM_DEBUG_PUTS _ofsm_simulation_debug_puts
#		define _OFSM_IMPL_DEBUG_PUTS
#	endif
#endif

#ifndef OFSM_CONFIG_ATOMIC_BLOCK
    static std::recursive_mutex _ofsm_simulation_mutex; 
	static uint8_t _ofsm_simulation_atomic_counter;
#	ifdef OFSM_CONFIG_ATOMIC_RESTORESTATE
#		undef OFSM_CONFIG_ATOMIC_RESTORESTATE
#	endif
#	define OFSM_CONFIG_ATOMIC_RESTORESTATE _ofsm_simulation_mutex
#	define OFSM_CONFIG_ATOMIC_BLOCK(type) for(type.lock(), _ofsm_simulation_atomic_counter = 0; _ofsm_simulation_atomic_counter++ < 1; type.unlock())
#endif /*OFSM_CONFIG_ATOMIC_BLOCK*/

#ifndef OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS
#	define OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS 0
#endif

#else /*is not OFSM_CONFIG_PC_SIMULATION*/

#ifndef OFSM_CONFIG_CUSTOM_INIT_HEARTBEAT_PROVIDER_FUNC 
#	define OFSM_CONFIG_CUSTOM_INIT_HEARTBEAT_PROVIDER_FUNC _ofsm_piggyback_timer_0
#endif

#ifndef OFSM_CONFIG_CUSTOM_WAKEUP_FUNC
#	define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_wakeup
#endif

#ifndef OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC
#	define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_enter_sleep
#endif

#ifndef OFSM_CONFIG_CUSTOM_DEBUG_PUTS
#	ifdef OFSM_CONFIG_DEBUG_LEVEL
#		include <SoftwareSerial.h>
		void _ofsm_debug_puts(const char*);
#		define OFSM_CONFIG_CUSTOM_DEBUG_PUTS _ofsm_debug_puts
#		define _OFSM_IMPL_DEBUG_PUTS
#	endif
#endif

#ifndef OFSM_CONFIG_ATOMIC_BLOCK
#	include <util/atomic.h>
#	define OFSM_CONFIG_ATOMIC_BLOCK ATOMIC_BLOCK
#endif

#ifndef OFSM_CONFIG_ATOMIC_RESTORESTATE
#	define OFSM_CONFIG_ATOMIC_RESTORESTATE ATOMIC_RESTORESTATE
#endif

#endif /*OFSM_CONFIG_PC_SIMULATION*/

/*--------------------------------
General defines
----------------------------------*/
/*default event data type*/
#ifndef OFSM_CONFIG_EVENT_DATA_TYPE
#	define OFSM_CONFIG_EVENT_DATA_TYPE uint8_t
#endif

/*define OFSM_CONFIG_DEBUG_LEVEL_OFSM*/
#ifndef OFSM_CONFIG_DEBUG_LEVEL_OFSM
#	ifndef OFSM_CONFIG_DEBUG_LEVEL
#		define OFSM_CONFIG_DEBUG_LEVEL_OFSM 0
#	else /*is debug*/
#		ifndef _ofsm_debug_printf
#			include <stdarg.h>
#		endif
#		define OFSM_CONFIG_DEBUG_LEVEL_OFSM OFSM_CONFIG_DEBUG_LEVEL
#	endif
#endif

/*--------------------------------
Type definitions
----------------------------------*/
struct OFSMTransition {
	OFSMHandler eventHandler;
	uint8_t newState;
};

struct OFSMEventData {
	uint8_t                     eventCode;
	OFSM_CONFIG_EVENT_DATA_TYPE eventData;
};

struct OFSM {
	OFSMTransition**    transitionTable;
	uint8_t             transitionTableEventCount;  /*number of elements in each row (number of events defined)*/
	OFSMHandler         initHandler;                /*optional, can be null*/
	void*               fsmPrivateInfo;
	uint8_t             flags; 
	unsigned long       wakeupTime; 
	uint8_t             currentState;
};

struct OFSMState {
	OFSM				*fsm;
	OFSMEventData*      e;
	unsigned long		timeLeftBeforeTimeout;      /*time left before timeout set by previous transition*/
	uint8_t             groupIndex;                 /*group index where current fsm is registered*/
	uint8_t             fsmIndex;	                /*fsm index within group*/
};

struct OFSMGroup {
	OFSM**              fsms;
	uint8_t             groupSize;
	OFSMEventData*      eventQueue;
	uint8_t             eventQueueSize;

	volatile uint8_t    flags; 
	volatile uint8_t    nextEventIndex; //queue cell index that is available for new event
	volatile uint8_t    currentEventIndex; //queue cell that is being processed by ofsm
};

/*defined typedef void(*OFSMHandler)(OFSMState *fsmState);*/

/*------------------------------------------------
Flags
-------------------------------------------------*/
//Common flags
#define _OFSM_FLAG_INFINITE_SLEEP			0x1
#define _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW	0x2 /*indicate if wakeup time is scheduled after post overflow of time register*/
#define _OFSM_FLAG_ALL (_OFSM_FLAG_INFINITE_SLEEP | _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW)

//FSM Flags
#define _OFSM_FLAG_FSM_PREVENT_TRANSITION	0x10
#define _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE	0x20
#define _OFSM_FLAG_FSM_FLAG_ALL (_OFSM_FLAG_ALL | _OFSM_FLAG_FSM_PREVENT_TRANSITION | _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE )

//GROUP Flags
#define _OFSM_FLAG_GROUP_BUFFER_OVERFLOW	0x10

//Orchestra Flags
#define _OFSM_FLAG_OFSM_EVENT_QUEUED	0x10
#define _OFSM_FLAG_OFSM_TIMER_OVERFLOW	0x20 
#define _OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT	0x40 /*used at startup to allow queued timeout event to wakeup FSM*/

/*------------------------------------------------
Macros
-------------------------------------------------*/
#define fsm_prevent_transition(fsms)					((fsms->fsm)[0].flags |= _OFSM_FLAG_FSM_PREVENT_TRANSITION)

#define fsm_set_transition_delay(fsms, delayTicks)		((fsms->fsm)[0].wakeupTime = delayTicks)
#define fsm_set_inifinite_delay(fsms)					((fsms->fsm)[0].flags |= _OFSM_FLAG_INFINITE_SLEEP)
#define fsm_set_next_state(fsms, nextStateId)			((fsms->fsm)[0].flags |= _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE, (fsms->fsm)[0].currentState = nextStateId)

#define fsm_get_private_data(fsms)						((fsms->fsm)[0].fsmPrivateInfo)
#define fsm_get_private_data_cast(fsms, castType)		((castType)((fsm->fsm)[0].PrivateInfo)
#define fsm_get_state(fsms)								((fsms->fsm)[0].currentState)
#define fsm_get_time_left_before_timeout(fsms)          (fsms->timeLeftBeforeTimeout)
#define fsm_get_fsm_index(fsms)							(fsms->index)	
#define fsm_get_group_index(fsms)						(fsms->groupIndex)
#define fsm_get_event_code(fsms)						((fsms->e)[0].eventCode)
#define fsm_get_event_data(fsms)						((fsms->e)[0].eventData)


#define ofsm_get_time(currentTime, timeFlags) \
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
		currentTime = _ofsmTime; \
        timeFlags = _ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW; \
	}

#define _OFSM_MIN(a,b) a > b ? b : a;
#define _OFSM_GET_TRANSTION(fsm, eventCode) ((OFSMTransition*)( (fsm->transitionTableEventCount * fsm->currentState +  eventCode) * sizeof(OFSMTransition) + (char*)fsm->transitionTable) )

/*time comparison*/
/*ao, bo - 'o' means overflow*/
#define _OFSM_TIME_A_GT_B(a, ao, b, bo)  ( (a  >  b) && (ao || !bo) )
#define _OFSM_TIME_A_GTE_B(a, ao, b, bo) ( (a  >=  b) && (ao || !bo) )

/*--------------------------------------
Implementation
----------------------------------------*/
OFSMGroup**             _ofsmGroups;
uint8_t                 _ofsmGroupCount;
volatile uint8_t        _ofsmFlags;
volatile unsigned long  _ofsmWakeupTime;
volatile unsigned long  _ofsmTime;

static void inline _ofsm_fsm_process_event(OFSM *fsm, uint8_t groupIndex, uint8_t fsmIndex, OFSMEventData *e) __attribute__((__always_inline__)) 
{
	//TBI:
	OFSMTransition *t;
	uint8_t prevState;
	uint8_t oldFlags;
	unsigned long oldWakeupTime;
	uint8_t wakeupTimeGTcurrentTime;
	OFSMState fsmState;
	unsigned long currentTime;
	uint8_t timeFlags;

#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	long delay = -1;
	char overrideStateFlag[] = { '\0', '\0' };
#endif
	
	if (e->eventCode >= fsm->transitionTableEventCount) {
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM
		ofsm_debug_printf("G(%i)F(%i): Unexpected Event!!! Ignored eventCode %i.\n", groupIndex, fsmIndex, e->eventCode);
#endif
		return;
	}

    ofsm_get_time(currentTime, timeFlags);
	
	//check if wake time has been reached, wake up immediately if not timeout event, ignore non-handled   events.
	t = _OFSM_GET_TRANSTION(fsm, e->eventCode);
	wakeupTimeGTcurrentTime = _OFSM_TIME_A_GT_B(fsm->wakeupTime, (fsm->flags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW), currentTime, (timeFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW));
    if (!t->eventHandler || (0 == e->eventCode && (((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) && !(_ofsmFlags & _OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT)) || wakeupTimeGTcurrentTime))) {
		if (!t->eventHandler) {
			fsm->flags |= _OFSM_FLAG_INFINITE_SLEEP;
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 2
			ofsm_debug_printf("G(%i)F(%i): Handler is not specified. Assuming infinite sleep.\n", groupIndex, fsmIndex);
		}
		else if ((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) && !(_ofsmFlags & _OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT)) {
			ofsm_debug_printf("G(%i)F(%i): State Machine is in infinite sleep.\n", groupIndex, fsmIndex);
		}
		else if (wakeupTimeGTcurrentTime) {
			ofsm_debug_printf("G(%i)F(%i): State Machine is asleep. Wakeup is scheduled in %i ticks.\n", groupIndex, fsmIndex, fsm->wakeupTime - currentTime);
#endif
		}
		return;
	}

#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
	ofsm_debug_printf("G(%i)F(%i): State: %i, processing eventCode %i eventData %i(0x%08X)...\n", groupIndex, fsmIndex, fsm->currentState, e->eventCode, e->eventData, e->eventData);
#endif 
	//call handler
	oldFlags = fsm->flags;
	oldWakeupTime = fsm->wakeupTime;
	fsm->wakeupTime = 0;
	fsm->flags &= ~_OFSM_FLAG_FSM_FLAG_ALL; //clear flags

	fsmState.fsm = fsm;
	fsmState.e = e;
	fsmState.groupIndex = groupIndex;
	fsmState.fsmIndex = fsmIndex;
    fsmState.timeLeftBeforeTimeout = 0;

    if(oldFlags & _OFSM_FLAG_INFINITE_SLEEP) {
        fsmState.timeLeftBeforeTimeout = 0xFFFFFFFF;
    } else {
        if(wakeupTimeGTcurrentTime) {
            fsmState.timeLeftBeforeTimeout = oldWakeupTime - currentTime; /* time overflow will be accounted for*/
        }
    }

	(t->eventHandler)(&fsmState);
	
	//check if error was reported, restore original FSM state
	if (fsm->flags & _OFSM_FLAG_FSM_PREVENT_TRANSITION) {
		fsm->flags = oldFlags;
		fsm->wakeupTime = oldWakeupTime;
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		ofsm_debug_printf("G(%i)F(%i): Handler requested no transition. FSM state was restored.\n", groupIndex, fsmIndex);
#endif
		return;
	}
	
	//make a transition
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	prevState = fsm->currentState;
#endif
	if (!(fsm->flags & _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE)) {
		fsm->currentState = t->newState;
	}
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	else {
		overrideStateFlag[0] = 'O';
	}
#endif
	
	//check transition delay, assume infinite sleep if new state doesn't accept Timeout Event
	t = _OFSM_GET_TRANSTION(fsm, 0);
	if (!t->eventHandler) {
		fsm->flags |= _OFSM_FLAG_INFINITE_SLEEP; //set infinite sleep
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		delay = -1;
#endif
	}
	else if (0 == fsm->wakeupTime) {
		fsm->wakeupTime = currentTime + OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY;
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		delay = OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY;
#endif
	}
	else {
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		delay = fsm->wakeupTime;
#endif
		fsm->wakeupTime += currentTime;
		if (fsm->wakeupTime < currentTime) {
			fsm->flags |= _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW;
		}
	}
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	ofsm_debug_printf("G(%i)F(%i): Transitioning from state %i ==> %s%i. Transition delay: %i\n", groupIndex, fsmIndex, prevState, overrideStateFlag, fsm->currentState, delay);
#endif

}

static void inline _ofsm_group_process_pending_event(OFSMGroup *group, uint8_t groupIndex, unsigned long *groupEarliestWakeupTime, uint8_t *groupAndedFsmFlags) __attribute__((__always_inline__)) 
{
	OFSMEventData e;
	OFSM *fsm;
	uint8_t andedFsmFlags = (uint8_t)0xFFFF;
	unsigned long earliestWakeupTime = 0xFFFFFFFF;
	uint8_t i;
	uint8_t eventPending = 1;

	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
		if (group->currentEventIndex == group->nextEventIndex && !(group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
			eventPending = 0;
		}
		else {
			/*copy event (instead of reference), because event data can be modified during ...queue_event... from interrupt.*/
			e = ((group->eventQueue)[group->currentEventIndex]);
		    
            group->currentEventIndex++;
		    if (group->currentEventIndex == group->eventQueueSize) {
			    group->currentEventIndex = 0;
		    }
    
            group->flags &= ~_OFSM_FLAG_GROUP_BUFFER_OVERFLOW; //clear buffer overflow

            /*set: other events pending if nextEventIdex points further in the queue */
		    if (group->currentEventIndex != group->nextEventIndex) {
			    _ofsmFlags |= _OFSM_FLAG_OFSM_EVENT_QUEUED;
		    }
		}
	}
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 2		
		ofsm_debug_printf("G(%i): currentEventIndex %i, nextEventIndex %i.\n", groupIndex, group->currentEventIndex, group->nextEventIndex);
#endif
	//Queue considered empty when (nextEventIndex == currentEventIndex) and buffer overflow flag is NOT set
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 2
	if (!eventPending) {
		ofsm_debug_printf("G(%i): Event queue is empty.\n", groupIndex);
	}
#endif

    //iterate over fsms
	for (i = 0; i < group->groupSize; i++) {
		fsm = (group->fsms)[i];
		//if queue is empty don't call fsm just collect info
		if (eventPending) {
            _ofsm_fsm_process_event(fsm, groupIndex, i, &e);
		}

		//Take sleep period unless infinite sleep
		if (!(fsm->flags & _OFSM_FLAG_INFINITE_SLEEP)) {
            if(_OFSM_TIME_A_GT_B(earliestWakeupTime, (andedFsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW), fsm->wakeupTime, (fsm->flags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW))) {
                earliestWakeupTime = fsm->wakeupTime;
            }
		}
        andedFsmFlags &= fsm->flags;
	}

	*groupEarliestWakeupTime = earliestWakeupTime;
	*groupAndedFsmFlags  = andedFsmFlags;
}

void _ofsm_setup() {
	//TBI:
}

void _ofsm_start() {
	uint8_t i;
	OFSMGroup *group;
	uint8_t andedFsmFlags;
	unsigned long earliestWakeupTime;
	uint8_t groupAndedFsmFlags;
	unsigned long groupEarliestWakeupTime;
	unsigned long currentTime;
	uint8_t timeFlags;

	//start main loop
	while (1) {
		OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
			_ofsmFlags |= _OFSM_FLAG_INFINITE_SLEEP; //prevents _ofsm_check_timeout() to ever accessing _ofsmWakeupTime and queue timeout while in process
			_ofsmFlags &= ~_OFSM_FLAG_OFSM_EVENT_QUEUED; //reset event queued flag
		}

		andedFsmFlags = (uint8_t)0xFFFF;
		earliestWakeupTime = 0xFFFFFFFF;
		for (i = 0; i < _ofsmGroupCount; i++) {
			group = (_ofsmGroups)[i];
#if OFSM_CONFIG_DEBUG_LEVEL > 2
			ofsm_debug_printf("OFSM: Processing event for group index %i...\n", i);
#endif
			_ofsm_group_process_pending_event(group, i, &groupEarliestWakeupTime, &groupAndedFsmFlags);

            if (!(groupAndedFsmFlags & _OFSM_FLAG_INFINITE_SLEEP)) {
                if(_OFSM_TIME_A_GT_B(earliestWakeupTime, (andedFsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW), groupEarliestWakeupTime, (groupAndedFsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW))) {
                    earliestWakeupTime = groupEarliestWakeupTime;
                }
            }
			andedFsmFlags &= groupAndedFsmFlags;
		}

		//if have pending events in either of group, repeat the step
		if (_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED) {
#if OFSM_CONFIG_DEBUG_LEVEL > 2
			ofsm_debug_printf("OFSM: At least one group has pending event(s). Re-process all groups.\n", i);
#endif
			continue;
		}

		if (!(andedFsmFlags & _OFSM_FLAG_INFINITE_SLEEP)) {
			ofsm_get_time(currentTime, timeFlags);
			if (_OFSM_TIME_A_GTE_B(currentTime, ((uint8_t)timeFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW), earliestWakeupTime, (andedFsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW))) {
#if OFSM_CONFIG_DEBUG_LEVEL > 2
				ofsm_debug_printf("OFSM: Reached timeout. Queue global timeout event.\n", i);
#endif
				ofsm_queue_global_event(false, 0, 0);
				continue;
			}
		}
		//There is no need for ATOMIC, because _ofsm_check_timeout() will never access _ofsmWakeupTime while in process
		//		OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
		_ofsmWakeupTime = earliestWakeupTime;
		_ofsmFlags = (_ofsmFlags & ~_OFSM_FLAG_ALL)|(andedFsmFlags & _OFSM_FLAG_ALL);
		//		}

		//if scheduled time is in overflow and timer is in overflow reset timer overflow flag
		OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
			if ((_ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP) || ((_ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW) && (_ofsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW)))
			{
				_ofsmFlags &= ~(_OFSM_FLAG_OFSM_TIMER_OVERFLOW | _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW);
			}
            _ofsmFlags &= ~_OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT;
		}
#if OFSM_CONFIG_DEBUG_LEVEL > 2
		ofsm_debug_printf("OFSM: Entering sleep... Wakeup Time %i.\n", _ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP ? -1 : _ofsmWakeupTime);
#endif
		OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC();

#if OFSM_CONFIG_DEBUG_LEVEL > 2
		ofsm_debug_printf("OFSM: Waked up.\n");
#endif
	}
}

static void inline _ofsm_check_timeout() __attribute__((__always_inline__))
{
	/*not need as it is called from within atomic block	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { */
	//do nothing if infinite sleep
	if (_ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP) {
		//catch situation when new event was queued just before main loop went to sleep
		if (_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED) {
			_ofsmFlags &= ~_OFSM_FLAG_OFSM_EVENT_QUEUED;
			OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
		}
		return;
	}

	if (_OFSM_TIME_A_GTE_B(_ofsmTime, (_ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW), _ofsmWakeupTime, (_ofsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW))) {
		ofsm_queue_global_event(false, 0, 0); //this call will wakeup main loop
	}
	/*	}*/
}

static void inline ofsm_heartbeat(unsigned long currentTime) __attribute__((__always_inline__))
{
	unsigned long prevTime;
	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
		prevTime = _ofsmTime;
		_ofsmTime = currentTime;
		if (_ofsmTime < prevTime) {
			_ofsmFlags |= _OFSM_FLAG_OFSM_TIMER_OVERFLOW;
		}
		_ofsm_check_timeout();
	}
}

void _ofsm_queue_group_event(uint8_t groupIndex, OFSMGroup *group, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) {
	uint8_t copyNextEventIndex;
	OFSMEventData *event;
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
	uint8_t debugFlags = 0x1; //set buffer overflow
#endif
	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
		copyNextEventIndex = group->nextEventIndex;

		if (!(group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
			debugFlags = 0; //remove buffer overflow
#endif
			if (group->nextEventIndex == group->currentEventIndex) {
				forceNewEvent = true; //all event are processed by FSM and event should never reuse previous event slot.
			}
			else if (0 == eventCode) {
				forceNewEvent = false; //always replace timeout event
			}
		}

		//update previous event if previous event codes matches 
		if (!forceNewEvent) {
			if (copyNextEventIndex == 0) {
				copyNextEventIndex = group->eventQueueSize;
			}
			event = &(group->eventQueue[copyNextEventIndex - 1]);
			if (event->eventCode != eventCode) {
				forceNewEvent = 1;
			}
			else {
				event->eventData = eventData;
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
				debugFlags |= 0x2; //set event replaced flag
#endif
			}
		}

		if (!(group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
			if (forceNewEvent) {
				group->nextEventIndex++;
				if (group->nextEventIndex >= group->eventQueueSize) {
					group->nextEventIndex = 0;
				}

				//queue event
				event = &(group->eventQueue[copyNextEventIndex]);
				event->eventCode = eventCode;
				event->eventData = eventData;

				//set event queued flag, so that _ofsm_start() knows if it need to continue processing
				_ofsmFlags |= (_OFSM_FLAG_OFSM_EVENT_QUEUED);

				//event buffer overflow disable further events
				if (group->nextEventIndex == group->currentEventIndex) {
					group->flags |= _OFSM_FLAG_GROUP_BUFFER_OVERFLOW; //set buffer overflow flag, so that no new events get queued
				}
			}
		}
	}
	OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();

#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
	if (!(debugFlags & 0x2) && debugFlags & 0x1) {
		ofsm_debug_printf("G(%i): Buffer overflow. eventCode %i eventData %i(0x%08X) dropped.\n", groupIndex, eventCode, eventData, eventData);
	}
#endif
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	else {
		ofsm_debug_printf("G(%i): Queued eventEvent %i eventData %i (0x%08X) (Updated %i, Buffer overflow %i).\n", groupIndex, eventCode, eventData, eventData, (debugFlags & 0x2) > 0, debugFlags & 0x1);
#   if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 2
		ofsm_debug_printf("G(%i): currentEventIndex %i, nextEventIndex %i.\n", groupIndex, group->currentEventIndex, group->nextEventIndex);
#   endif
	}
#endif
}

#ifdef _OFSM_IMPL_DEBUG_PRINTF
void ofsm_debug_printf(const char* format, ...) {
	char buf[256];
	char *b = buf;
	va_list argp;
	va_start(argp, format);
	vsprintf_s(b, ((sizeof(buf) / sizeof(*buf)) - (b - buf)), format, argp);
	va_end(argp);
	OFSM_CONFIG_CUSTOM_DEBUG_PUTS(buf);
}
#else 
#	define ofsm_debug_printf(...) do{}while(0)
#endif /* _OFSM_IMPL_DEBUG_PRINTF */

static void inline ofsm_queue_group_event(uint8_t groupIndex, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) __attribute__((__always_inline__))
{
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM
	if (groupIndex >= _ofsmGroupCount) {
		ofsm_debug_printf("OFSM: Invalid Group Index!!! Dropped eventCode %i eventData %i. \n", groupIndex, eventCode, eventData);
		return; 
	}
#endif
	_ofsm_queue_group_event(groupIndex, _ofsmGroups[groupIndex], forceNewEvent, eventCode, eventData);
}

void ofsm_queue_global_event(bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) {
	uint8_t i;
	OFSMGroup *group;

	for (i = 0; i < _ofsmGroupCount; i++) {
		group = (_ofsmGroups)[i];
#if(OFSM_CONFIG_DEBUG_LEVEL_OFSM > 2)
		ofsm_debug_printf("OFSM: Event queuing group %i...\n", i);
#endif
		_ofsm_queue_group_event(i, group, forceNewEvent, eventCode, eventData);
	}
}

/*------------------------------------------------
Simulation and non-simulation specific code 
--------------------------------------------------*/
#ifdef OFSM_CONFIG_PC_SIMULATION

std::mutex cvm;
std::condition_variable cv;

#ifdef _OFSM_IMPL_ENTER_SLEEP
void _ofsm_simulation_enter_sleep() {
        std::unique_lock<std::mutex> lk(cvm);

        cv.wait(lk);
		_ofsmFlags &= ~_OFSM_FLAG_OFSM_EVENT_QUEUED;
		lk.unlock();
}
#endif /* _OFSM_IMPL_ENTER_SLEEP */

#ifdef _OFSM_IMPL_WAKEUP
void _ofsm_simulation_wakeup() {
    std::unique_lock<std::mutex> lk(cvm);
    lk.unlock();
    cv.notify_one();
}
#endif /* _OFSM_IMPL_WAKEUP */

#ifdef _OFSM_IMPL_DEBUG_PUTS
void _ofsm_simulation_debug_puts(const char* str) {
	unsigned long time;
    uint8_t timeFlags;
    ofsm_get_time(time, timeFlags);
	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
#ifdef OFSM_CONFIG_DEBUG_PRINT_ADD_TIMESTAMP
		std::cout << "[" << time << "]";
#endif
		std::cout << str;
		std::cout.flush();
	}
}
#endif /* _OFSM_IMPL_DEBUG_PUTS */

#ifdef _OFSM_IMPL_EVENT_GENERATOR
void _ofsm_simulation_sleep_thread(int sleepMilliseconds) {
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMilliseconds));
}

void _ofsm_simulation_sleep(int sleepMilliseconds) {
	std::thread sleepThread(_ofsm_simulation_sleep_thread, sleepMilliseconds);
	sleepThread.join();
}


void _ofsm_simulation_event_generator() {
	std::string line;
	while (!std::cin.eof())
	{

		//read line from stdin
		std::getline(std::cin, line);
		
		//prepare for parsing
		std::stringstream strStream(line);
		std::string token;
		std::vector<std::string> tokens;

		//parse by tokens 
		while (std::getline(strStream, token, ',')) {
			if (token.find_first_not_of(' ') >= 0) {
				tokens.push_back(token);
			}
		}

		std::string eventCodeStr;
		int eventCode = 0;
		int eventData = 0; 
		std::string targetStr = "0";
		uint8_t eventTargetGroup = 0;
		bool forceNewEvent = false;
		
		//get components
		std::string t;
		switch (tokens.size()) {
		case 3:
			targetStr = tokens[2];
			std::transform(targetStr.begin(), targetStr.end(), targetStr.begin(), ::tolower);
			eventTargetGroup = atoi(targetStr.c_str());
		case 2:
			eventData = atoi(tokens[1].c_str());
		case 1:
			t = tokens[0];
			std::transform(t.begin(), t.end(), t.begin(), ::tolower);
			if (std::string::npos != t.find("exit")) {
#if(OFSM_CONFIG_DEBUG_LEVEL > 1)
				ofsm_debug_printf("EG: Exiting...\n");
#endif
				return;
			}
			if (std::string::npos != t.find("sleep")) {
				if (tokens.size() == 1) {
					eventData = 1000;
				}
#if(OFSM_CONFIG_DEBUG_LEVEL > 1)
				ofsm_debug_printf("EG: Entering sleep for %i milliseconds...\n", eventData);
#endif
				_ofsm_simulation_sleep(eventData);
				continue;
			}
			if (t[0] == 'f') {
				forceNewEvent = true;
				t = t.substr(1);
			}
			eventCode = atoi(t.c_str());
			break;
		default:
			continue; //skip empty lines
		}
		
		if (std::string::npos != targetStr.find("g")) {
#if(OFSM_CONFIG_DEBUG_LEVEL > 1)
			ofsm_debug_printf("EG: Queuing eventCode: %i eventData: %i GLOBAL (force new event: %i)...\n", eventCode, eventData, forceNewEvent);
#endif
			ofsm_queue_global_event(forceNewEvent, eventCode, eventData);
		}
		else {
#if(OFSM_CONFIG_DEBUG_LEVEL > 1)
			ofsm_debug_printf("EG: Queuing eventCode: %i eventData: %i; group %i (force new event: %i)...\n", eventCode, eventData, eventTargetGroup, forceNewEvent);
#endif
			ofsm_queue_group_event(eventTargetGroup, forceNewEvent, eventCode, eventData);
		}
#if (OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS > 0) 
		_ofsm_simulation_sleep(OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS);
#endif
	}
}
#endif /* _OFSM_IMPL_EVENT_GENERATOR */
void setup();
void loop();

void _ofsm_simulation_fsm_thread(int ignore) {
	setup();
	loop();
}

void _ofsm_simulation_heartbeat_provider_thread(int tickSize) {
	unsigned long currentTime = 0;
	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(tickSize));
		currentTime++;
		ofsm_heartbeat(currentTime);
	}
}

int main(int argc, char* argv[])
{
	//start fsm thread
	std::thread fsmThread(_ofsm_simulation_fsm_thread, 0);
	fsmThread.detach();

	//start timer thread
	std::thread heartbeatProviderThread(_ofsm_simulation_heartbeat_provider_thread, OFSM_CONFIG_PC_SIMULATION_TICK_MS);
	heartbeatProviderThread.detach();

	//call event generator
	OFSM_CONFIG_CUSTOM_PC_SIMULATION_EVENT_GENERATOR_FUNC();
	return 0;
}

#else /* if not OFSM_CONFIG_PC_SIMULATION*/

#ifdef _OFSM_IMPL_DEBUG_PUTS
void _ofsm_debug_puts(const char* str) {
#ifdef OFSM_CONFIG_DEBUG_PRINT_ADD_TIMESTAMP
	unsigned long time;
    uint8_t timeFlags;
    ofsm_get_time(time, timeFlags);
#endif
	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
#ifdef OFSM_CONFIG_DEBUG_PRINT_ADD_TIMESTAMP
		char timeStamp[12];
		itoa(time, timeStamp, 10);
		Serial.write("[");
		Serial.write(timeStamp);
		Serial.write("]");
#endif
		for (char *p = &str[0]; *p; p++) { /* emulate cooked mode for newlines */
			if (*p == '\n') {
				Serial.write('\r');
			}
			Serial.write(*p);
		}
	}
}
#endif /* _OFSM_IMPL_DEBUG_PUTS */

#endif /*OFSM_CONFIG_PC_SIMULATION*/

/*----------------------------------------------
Setup helper macros
-----------------------------------------------*/

#define _OFSM_DECLARE_GET(name, id) (name##id)
#define _OFSM_DECLARE_GROUP_EVENT_QUEUE(grpId, eventQueueSize) OFSMEventData _ofsm_decl_grp_eq_##grpId[eventQueueSize];  

#define _OFSM_DECLARE_GROUP_FSM_ARRAY_1(grpId, fsmId0) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_2(grpId, fsmId0, fsmId1) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_3(grpId, fsmId0, fsmId1, fsmId2) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1, &_ofsm_decl_fsm_##fsmId2 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_4(grpId, fsmId0, fsmId1, fsmId2, fsmId3) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1, &_ofsm_decl_fsm_##fsmId2, &_ofsm_decl_fsm_##fsmId3 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_5(grpId, fsmId0, fsmId1, fsmId2, fsmId3, fsmId4) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1, &_ofsm_decl_fsm_##fsmId2, &_ofsm_decl_fsm_##fsmId3, &_ofsm_decl_fsm_##fsmId4 };

#define _OFSM_DECLARE_GROUP(grpId) \
	OFSMGroup _ofsm_decl_grp_##grpId = {\
		_OFSM_DECLARE_GET(_ofsm_decl_grp_fsms_, grpId),\
		sizeof(_OFSM_DECLARE_GET(_ofsm_decl_grp_fsms_, grpId))/sizeof(*_OFSM_DECLARE_GET(_ofsm_decl_grp_fsms_, grpId)),\
		_OFSM_DECLARE_GET(_ofsm_decl_grp_eq_, grpId),\
		sizeof(_OFSM_DECLARE_GET(_ofsm_decl_grp_eq_, grpId))/sizeof(*_OFSM_DECLARE_GET(_ofsm_decl_grp_eq_, grpId))\
	}

#define _OFSM_DECLARE_GROUP_ARRAY_1(grpId0) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0) };
#define _OFSM_DECLARE_GROUP_ARRAY_2(grpId0, grpId1) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1) };
#define _OFSM_DECLARE_GROUP_ARRAY_3(grpId0, grpId1, grpId2) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId2) };
#define _OFSM_DECLARE_GROUP_ARRAY_4(grpId0, grpId1, grpId2, grpId3) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId2), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId3) };
#define _OFSM_DECLARE_GROUP_ARRAY_5(grpId0, grpId1, grpId2, grpId3, grpId4) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId2), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId3), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId4) };


#define _OFSM_DECLARE_GROUP_N(n, grpId, eventQueueSize, ...) \
	_OFSM_DECLARE_GROUP_EVENT_QUEUE(grpId, eventQueueSize);\
	_OFSM_DECLARE_GROUP_FSM_ARRAY_##n(grpId, __VA_ARGS__);\
	_OFSM_DECLARE_GROUP(grpId);

#define _OFSM_DECLARE_N(n, ...)\
	_OFSM_DECLARE_GROUP_ARRAY_##n(__VA_ARGS__);

#define OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr) \
	OFSM _ofsm_decl_fsm_##fsmId = {\
			(OFSMTransition**)transitionTable, 	/*transitionTable*/ \
			transitionTableEventCount,			/*transitionTableEventCount*/ \
			initializationHandler,				/*initHandler*/ \
			fsmPrivateDataPtr,					/*fsmPrivateInfo*/ \
            _OFSM_FLAG_INFINITE_SLEEP           /*flags*/ \
	};

#define OFSM_DECLARE_GROUP_1(grpId, eventQueueSize, fsmId0) _OFSM_DECLARE_GROUP_N(1, grpId, eventQueueSize, fsmId0); 
#define OFSM_DECLARE_GROUP_2(grpId, eventQueueSize, fsmId0, fsmId1) _OFSM_DECLARE_GROUP_N(2, grpId, eventQueueSize, fsmId0, fsmId1); 
#define OFSM_DECLARE_GROUP_3(grpId, eventQueueSize, fsmId0, fsmId1, fsmId2) _OFSM_DECLARE_GROUP_N(3, grpId, eventQueueSize, fsmId0, fsmId1, fsmId2); 
#define OFSM_DECLARE_GROUP_4(grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3) _OFSM_DECLARE_GROUP_N(4, grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3); 
#define OFSM_DECLARE_GROUP_5(grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3, fsmId4) _OFSM_DECLARE_GROUP_N(5, grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3, fsmId4); 

#define OFSM_DECLARE_1(grpId0) _OFSM_DECLARE_N(1, grpId0); 
#define OFSM_DECLARE_2(grpId0, grpId1) _OFSM_DECLARE_N(2, grpId0, grpId1); 
#define OFSM_DECLARE_3(grpId0, grpId1, grpId2) _OFSM_DECLARE_N(3, grpId0, grpId1, grpId2); 
#define OFSM_DECLARE_4(grpId0, grpId1, grpId2, grpId3) _OFSM_DECLARE_N(4, grpId0, grpId1, grpId2, grpId3); 
#define OFSM_DECLARE_5(grpId0, grpId1, grpId2, grpId3, grpId4) _OFSM_DECLARE_N(5, grpId0, grpId1, grpId2, grpId3, grpId4); 

#define OFSM_DECLARE_BASIC(transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr) \
	OFSM_DECLARE_FSM(0, transitionTable1, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr); \
	OFSM_DECLARE_GROUP_1(0, 10, 0); \
	OFSM_DECLARE_1(0);

#define OFSM_SETUP() \
	_ofsmGroups = (OFSMGroup**)_ofsm_decl_grp_arr; \
	_ofsmGroupCount = sizeof(_ofsm_decl_grp_arr) / sizeof(*_ofsm_decl_grp_arr); \
	_ofsmFlags |= (_OFSM_FLAG_INFINITE_SLEEP |_OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT);\
    _ofsmTime = 0; \
	_ofsm_setup();
#define OFSM_LOOP() _ofsm_start();



#endif /*__OFSM_H_*/