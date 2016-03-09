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
* Testability and easy of debugging.
* Easy of customization and support of different MCUs;
* Re-usability of logic and simplicity of scaling up and down;
* Memory used by internal code should depend on necessity and driven by configuration. Thus, reducing memory consumption and prevention of "dead" code;

DESIGN/USAGE NOTES
==================
Important design decisions and usage notes:
* Multiple state machines should peacefully co-exist and play together;
* Structure of application should resemble Arduino sketch, namely it should have void setup() {...} and void loop() {...} functions. main(...) function should not be defined or
  it needs to be surrounded by #ifndef OFSM_CONFIG_SIMULATION ... main(...) #endif for simulation to work, as it declares its own main() function.
* OFSM is relaying on external "heartbeat" provider, so it can be easily replaced and configured independently.
  Default provider uses Timer0. Which breaks all Arduino's time API. 
  This is done on purpose, as state machine is based on completely different paradigm where only relative time is important for proper functioning;
  Any use of delay(...), micros() and millis(...) may indicate bad state machine design.
* Internal time within OFSM is measured in ticks. Tick may represent microseconds, milliseconds or any other time dimensions depending on "heartbeat" provider. (see above).
  Thus, if rules (stated in this section) are followed. The same implementation can run with different speeds and environments.
  Also by breaking standard Arduino timer functionality, it reduces the potential amount of generated interrupts that may cause non-intended loop wakeup. Which again, helps to reduce power consumption.
* As outcome of previous statement, it is recommended that event handlers rely on relative time (in ticks) supplied by OFSM (or in worse case on ofsm_get_time());
* It is recommended that handlers never do a time math on their own, and rely completely on relative delays. As time math should account for timer overflow to work in all cases.
* Event handlers are not expected to know transition states, and should rely on events to move FSM. (see Handlers API for details);
* FSMs can be organized into groups of dependent state machines. NOTE: One group shares the same set of events. Each queued event processed by all FSMs within the group;
* Events are never directly processed but queued, allowing better parallelism between multiple FSM.
* Event queues are protected from overflow. Thus if events get generated faster than FSMs can process them, the events will be compressed (see ofsm_queue...()) or dropped;
* The same event handler can be shared between the states of the same FSM, within different FSMs or even FSMs of different groups;
* FSMs can be shared between different groups.
* OFSM is driven by time (transition delays) and events (from interrupt handlers) and TIMEOUT event become a very important concept.
  Therefore, event id 0 is reserved for TIMEOUT/STATE ENTERING event and must be declared in all FSMs;

TIMEOUT EVENT NOTES
====================
Even though timeout event can be queued to all groups, it doesn't mean that all FSM instances will need to handle it.
OFSM makes sure that if FSM instance requested timeout that expires in the future (relatively to current timeout event) or it is in infinite sleep,
 that FSM handler of such FSM instance will NOT be called.

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
* ...DECLARE... macros should be called in the global scope. Outside of setup() and loop() functions.
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
Interrupt handlers are not expected to perform real work (except cases when they must, like PWM etc) but instead queue an event into corresponding group(s).
To queue an event the following API can be used by interrupt handler:
* ofsm_queue_group_event(groupIndex, eventCode, eventData)
* ofsm_queue_global_event(eventCode, eventData) //queue the same event to all groups

FSM EVENT HANDLERS API
======================
The following set of functions can be called from any event handler:
* fsm_prevent_transition(OFSMState *fsm)

* fsm_set_transition_delay(OFSMState *fsm, unsigned long delayTicks)
* fsm_set_inifinite_delay(OFSMState *fsm)
* fsm_set_next_state(OFSMState *fsm, uint8_t nextStateId)            //TRY TO AVOID IT! overrides default transition state from the handler

* fsm_get_private_data(OFSMState *fsm)
* fsm_get_private_data_cast(OFSMState *fsm, castType)                // example: MyPrivateStruct_t *data = fsm_get_private_data_cast(fsm, MyPrivateStruct_t*)
* fsm_get_state(OFSMState *fsm)
* fsm_get_time_left_before_timeout(OFSMState *fsm)                   
* fsm_get_fsm_ndex(OFSMState *fsm)                                   //index of fsm in the group
* fsm_get_group_index(OFSMState *fsm)                                //index of group in OFSM
* fsm_get_event_code(OFSMState *fsm)
* fsm_get_event_data(OFSMState *fsm)

* fsm_queue_group_event(OFSMState *fsm, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) //queue event into current group

* ofsm_queue_global_event(uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData)
* ofsm_debug_printf(level,format, ....)	                       //Simulation mode debug print

CONFIGURATION
=============
OFSM can be "shaped" in many different ways using configuration switches. NOTE: all needed configuration switches must be defined before #include <ofsm.h>:
#define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY              //Default 0. Specifies default transition delay, used if event handler didn't set one.
#define OFSM_CONFIG_EVENT_DATA_TYPE uint8_t                     //Default uint8_t (8 bits). Event data type.
#define OFSM_CONFIG_ATOMIC_BLOCK ATOMIC_BLOCK                   //Default: ATOMIC_BLOCK; Retain compatibility with original: see implementation details in <util/atomic.h> from AVR SDK.
#define OFSM_CONFIG_ATOMIC_RESTORESTATE ATOMIC_RESTORESTATE     //Default: ATOMIC_RESTORESTATE see <util/atomic.h>
#define OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER              //Default: undefined. When defined OFMS start support for initialization handlers. This will consume a little bit of memory, as handler place holder and initialization logic will be implemented.

//By default OFSM uses timer0 to implement heartbeat provider,
//If this macro is defined, standard heartbeat provided will not be implemented 
//and (IMPORTANT) Timer0 will not be explicitly mangled with, thus you may have unexpected source of interrupts coming from Arduino time facilities. 
//Custom heartbeat provider is expected to call ofsm_hearbeat(unsigned long currentTicktime);
#define OFSM_CONFIG_CUSTOM_HEARTBEAT_PROVIDER                   //If defined, custom timing function: expected to call: ofsm_hearbeat(unsigned long currentTicktime)

// --------------Simulation Specific Macros -------------------
#define OFSM_CONFIG_SIMULATION									//Default undefined. See PC SIMULATION section for additional info
#define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 3                    //Default undefined. Enables debug print functionality. Also is used as debug message level filter.
#define OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP        //Default undefined. When defined ofsm_debug_print() it will prefix debug messages with [<current time in ticks>]<debug message>.

//If OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM is undefined, it get the same value as OFSM_CONFIG_SIMULATION_DEBUG_LEVEL.
//OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM value determines level of debug/trace output from OFSM. The following are debug output categories levels used by OFSM:
// 0 - no trace/debug messages from OFSM
// 1 - sever errors; if you getting one of those there is a good chance that definition of the State Machine is incorrect.
// 2 - state transition messages.
// 3 - information messages.
// 4 - debug messages.
#define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM OFSM_CONFIG_SIMULATION_DEBUG_LEVEL    //Default OFSM_CONFIG_SIMULATION_DEBUG_LEVEL. when level 0, all debug prints from OFSM will be disabled.
#define OFSM_CONFIG_SIMULATION_TICK_MS 1000                  //Default 1000 milliseconds in one tick.
#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS 0     //Default 0. Sleep period (in milliseconds) before reading new simulation event. May be helpful in batch processing mode.
#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE					//Default undefined, When defined heartbeat is manually invoked. see PC SIMULATION SCRIPT MODE for details.
#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 0     //Default: 0 - (wakeup when queued, including timeout); Other values: 1 - (wakeup explicitly by 'w[akeup]' command and timeout via 'h[eartbeat] command); 2 - (wakeup only by 'w[akeup]')

CUSTOMIZATION
=============
#define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_enter_sleep                   //typedef: void _ofsm_enter_sleep().

//Is almost never needs to be implemented, as wakeup naturally occurs when interrupt is called. 
//It is Mostly used for simulation mode where more work needs to be done to wakeup the main loop. 
#define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_wakeup                             //typedef: void _ofsm_wakeup().

// --------------Simulation Specific Macros -------------------
#define OFSM_CONFIG_CUSTOM_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER_FUNC _ofsm_simulation_status_report_printer // typedef: void custom_func(OFSMSimulationStatusReport *r)
#define OFSM_CONFIG_CUSTOM_SIMULATION_COMMAND_HOOK_FUNC						    //Default: undefined; typedef: bool func(std::deque<std::string> &tokens); see PC SIMULATION EVENT GENERATOR for details.
#define OFSM_CONFIG_CUSTOM_SIMULATION_EVENT_GENERATOR_FUNC _ofsm_simulation_event_generator //typedef: int _ofsm_simulation_event_generator(const char* fileName). Function: expected to call: ofsm_hearbeat(unsigned long currentTicktime)

PC SIMULATION
=============
Ultimate goal is to be able to run properly formatted sketch in simulation mode on any PC using GCC or other C+11 compatible compiler (including VS012) without any change.
To do that skatch code should follow the following rules:
* OFSM_CONFIG_SIMULATION definition should appear at the very top of the sketch file before any other includes
* All MCU specific code should be surrounded by
    #ifndef OFSM_CONFIG_SIMULATION
        <MCU specific code>
    #endif
* All simulation specific code should be similarly surrounded with #ifdef OFSM_CONFIG_SIMULATION.... (fsm_debug_printf() can be left as is, it works in both simulation and non-simulated environments)
    Default simulation implementation is based on C++ threads. First thread is running state machine and Second is running heartbeat provider with resolution of  OFSM_CONFIG_SIMULATION_TICK_MS.
    Main program's thread is used as event generator. Default implementation reads input in infinite loop and queues corresponding event to OFSM. see PC SIMULATION EVENT GENERATOR section for details.
    IMPORTANT NOTE: For simulation to work, it re-defines almost all OFSM_CONFIG_CUSTOM.... macros.
    Therefore, if user does customization for MCU purposes, all declarations should be surrounded by #ifndef OFSM_CONFIG_SIMULATION .... #endif construct.
    At the same time one can completely override all parts of simulation as well.

PC SIMULATION EVENT GENERATOR
=============================
Event generator is implemented as infinite loop which reads event information for the input and queues it in OFSM.
Simulation command format: <command>,<parameters>; Value surrounded by '[]' denotes optional part(s).
Any command can optionally be followed by '=' <assert compare string>. 
    When specified, event generator will compare <assert compare string> and last output (usually from 'report' or custom command) and issue an assert if there is a mismatch.
    This way Unit Tests can be created to ensure proper/expected functionality after the changes.
'//' designates comment. Event generator will ignore any text appearing after '//'.
The following are commands supported by event generator out of box;
* e[exit] - exit simulation;
* d[elay][,<sleep_milliseconds>] - forces event generator to sleep for <sleep_milliseconds> before reading next command; default sleep is 1000 milliseconds.
    -Example:
        1) sleep,2000		//sleep for 2 seconds
        2) s,2000			// the same as above
* q[ueue][,<modifiers>][,<event code>[,<event data>[,<group index>]]] - queue <event code> into OFSM.
    -<modifiers> - (optional) either 'g' or 'f' or both; where: 'g' - if specified causes event to be queued for all groups (global event), 'f' - forces new event vs. possible replacement of previously queued
    -Examples:
        1) queue,g,0,0,1	//queue global event code 0 event data 0 into all groups;
        2) q,1				//queue event code 1 event data 0 into group 0;
        3) q,f,2,1,1		//queue event code 2 event data 1 into group 1, force new event flag is set.
* h[eartbeat][,<current time (in ticks)>] // calls OFSM heartbeat with specified time; see also PC SIMULATION SCRIPT MODE;
    -Examples:
        1) heartbeat,1000	//ping OFSM and set current OFSM time to 1000 ticks
        2) h,1000			//same as above
* s[tatus][,<group index>[,<fsm index>] // prints out status report about current state of specified FSM, if not specified <group index> and <fsm index> assumed to be 0
    - see PC SIMULATION REPORT FORMAT for details about produced output.

* p[rint][,<string>]		// prints out <string>
* w[akup]					// explicitly wakeup OFSM; ignored unless OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE > 0
* r[eset]                   // reset and restart OFSM;


You can define OFSM_CONFIG_CUSTOM_SIMULATION_COMMAND_HOOK_FUNC that will be called before command gets processed by the event generator.
This way you can extend standard set of commands or change their default behavior.
By returning true hook signals event generator that command was processed. Otherwise, event generator will continue processing the command as usual.
Custom hook may call: ofsm_simulation_set_assert_compare_string(const char* assertCompareString) to allow support for test asserts.

PC SIMULATION SCRIPT MODE
=========================
* By default (OFSM_CONFIG_SIMULATION_SCRIPT_MODE is undefined). simulation process runs three threads:
        1) FSM thread: where FSM is running it's loop
        2) Timer thread: this thread is running timer which periodically call heartbeat to supply time into OFSM and wakes up OFSM in case of Timeout.
        3) Main thread: used by event generator.
    Such a mode allows dynamic/interactive testing of the OFSM.
* Script Mode can be used for thorough state machine logic testing. In this mode everything runs in single thread;
    Heartbeat is expected to be explicitly called from the script (see heartbeat command), that allows to setup precise test scenarios with predetermined outcome
    and creation of repeatable test cases.
*There are couple of ways to supply data for Script mode:
    1) piping input data into simulated sketch executable (example: mysketch < TestScript.txt)
    2) or by specifying <script file>  on the command line. (example: mysketch TestScript.txt)

PC SIMULATION REPORT FORMAT
===========================
see implementation of _ofsm_simulation_create_status_report() and _ofsm_simulation_status_report_printer() below for details.

LIMITATIONS
============
* Number of events in single FSM, number of FSMs in single group, number of groups within OFSM must not exceed 255!

*/
#ifndef __OFSM_H_
#define __OFSM_H_

/*Visual C/C++ support*/
#ifdef _MSC_VER
#   ifndef __attribute__
#       define __attribute__(xxx)
#   endif
#else
#endif /*_MSC_VER*/



#ifdef OFSM_CONFIG_SIMULATION
#   include <iostream>
#	include <fstream>
#   include <thread>
#   include <string>
#   include <deque>
#   include <sstream>
#   include <algorithm>
#   include <mutex>
#   include <condition_variable>
#	include <functional>
#	include <cctype>
#	include <locale>
#   include <string.h>
#	include <stdio.h>
#   include <stdint.h> /*for uint8_t support*/
#   define _OFSM_TIME_DATA_TYPE uint32_t //make it the same size as AVR unsigned long
#else
#   define _OFSM_TIME_DATA_TYPE unsigned long
#endif

/*default event data type*/
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

/*------------------------------------------------
The ofsm_debug_printf() available in both simulation and non-simulated modes.
In non-simulated mode it does nothing and consumes no memory. Compiler will optimize if out.
Sketch code that uses this function may be left untouched when compiled for MCU and it will NOT cause any memory consumption.
-------------------------------------------------*/
/*#define ofsm_debug_printf(...) //see implementation below*/

void ofsm_queue_global_event(bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
void ofsm_queue_group_event(uint8_t groupIndex, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
static inline void ofsm_heartbeat(_OFSM_TIME_DATA_TYPE currentTime)  __attribute__((__always_inline__));

void _ofsm_queue_group_event(uint8_t groupIndex, OFSMGroup *group, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
static inline void _ofsm_group_process_pending_event(OFSMGroup *group, uint8_t groupIndex, _OFSM_TIME_DATA_TYPE *groupEarliestWakeupTime, uint8_t *groupAndedFsmFlags) __attribute__((__always_inline__));
static inline void _ofsm_fsm_process_event(OFSM *fsm, uint8_t groupIndex, uint8_t fsmIndex, OFSMEventData *e) __attribute__((__always_inline__));
static inline void _ofsm_check_timeout() __attribute__((__always_inline__));
static inline void _ofsm_setup_hardware()  __attribute__((__always_inline__));
void _ofsm_setup();
void _ofsm_start();

/*see declaration of fsm_... macros below*/


/*---------------------
Simulation defines
-----------------------*/
#ifdef OFSM_CONFIG_SIMULATION

struct OFSMSimulationStatusReport;

volatile char _ofsm_simulation_assert_compare_string[256];

#define ofsm_simulation_set_assert_compare_string(str) \
OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
    std::string s = str; \
    s = trim(s); \
    _ofsm_strncpy((char*)_ofsm_simulation_assert_compare_string, s.c_str(), 255); \
}

/*DEBUG should be turned on during simulation*/
#ifndef OFSM_CONFIG_SIMULATION_DEBUG_LEVEL
#	define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 0
#	ifndef OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM
#		define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM OFSM_CONFIG_SIMULATION_DEBUG_LEVEL
#	endif
#endif

#ifdef _MSC_VER
#	define _ofsm_strncpy(dest, src, size) strcpy_s(dest, size, src)
#   define _ofsm_snprintf(dest, size, format, ...) sprintf_s(dest, size, format, __VA_ARGS__)
#else
#   define _ofsm_snprintf(dest, size, format, ...) snprintf_s(dest, size, format, __VA_ARGS__)
#	define _ofsm_strncpy(dest, src, size) strncpy(dest, src, size)
#endif /*_MSC_VER*/

#ifdef OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP
#   define ofsm_debug_printf(level,  ...) \
        if( level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL ) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                _OFSM_TIME_DATA_TYPE __time; \
                ofsm_get_time(__time, __time); \
                printf("[%lu] ", __time); \
                printf(__VA_ARGS__); \
            } \
        }
#   define _ofsm_debug_printf(level,  ...) \
        if( level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM ) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                _OFSM_TIME_DATA_TYPE __time; \
                ofsm_get_time(__time, __time); \
                printf("[%lu] ", __time); \
                printf(__VA_ARGS__); \
            } \
        }
#else
#   define _ofsm_debug_printf(level,  ...) \
        if (level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                printf(__VA_ARGS__) \
            } \
        }
#   define _ofsm_debug_printf(level,  ...) \
        if (level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                printf(__VA_ARGS__) \
            } \
        }
#endif // OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP



/*other overrides*/
#ifndef OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY
#	define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0
#endif

#ifndef OFSM_CONFIG_SIMULATION_TICK_MS
#	define OFSM_CONFIG_SIMULATION_TICK_MS 1000
#endif

#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 0
#endif

#ifndef OFSM_CONFIG_CUSTOM_SIMULATION_EVENT_GENERATOR_FUNC
    int _ofsm_simulation_event_generator(const char *fileName);
#	define OFSM_CONFIG_CUSTOM_SIMULATION_EVENT_GENERATOR_FUNC _ofsm_simulation_event_generator
#	define _OFSM_IMPL_EVENT_GENERATOR
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

#ifndef OFSM_CONFIG_ATOMIC_BLOCK
    static std::recursive_mutex _ofsm_simulation_mutex;
    static uint8_t _ofsm_simulation_atomic_counter;
#	ifdef OFSM_CONFIG_ATOMIC_RESTORESTATE
#		undef OFSM_CONFIG_ATOMIC_RESTORESTATE
#	endif
#	define OFSM_CONFIG_ATOMIC_RESTORESTATE _ofsm_simulation_mutex
#	define OFSM_CONFIG_ATOMIC_BLOCK(type) for(type.lock(), _ofsm_simulation_atomic_counter = 0; _ofsm_simulation_atomic_counter++ < 1; type.unlock())
#endif /*OFSM_CONFIG_ATOMIC_BLOCK*/

#ifndef OFSM_CONFIG_SIMULATION_SLEEP_BETWEEN_EVENTS_MS
#	define OFSM_CONFIG_SIMULATION_SLEEP_BETWEEN_EVENTS_MS 0
#endif

#ifndef OFSM_CONFIG_CUSTOM_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER_FUNC
    void _ofsm_simulation_status_report_printer(OFSMSimulationStatusReport* r);
#   define OFSM_CONFIG_CUSTOM_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER_FUNC _ofsm_simulation_status_report_printer
#   define _OFSM_IMPL_SIMULATION_STATUS_REPORT_PRINTER
#endif

#else /*is NOT OFSM_CONFIG_SIMULATION*/

#undef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
#undef OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE

#define ofsm_debug_printf(level,  ...)
#define _ofsm_debug_printf(level,  ...)


#ifndef OFSM_CONFIG_CUSTOM_WAKEUP_FUNC
#	define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_wakeup
#endif

#ifndef OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC
#	define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_enter_sleep
#endif

#ifndef OFSM_CONFIG_ATOMIC_BLOCK
#	include <util/atomic.h>
#	define OFSM_CONFIG_ATOMIC_BLOCK ATOMIC_BLOCK
#endif

#ifndef OFSM_CONFIG_ATOMIC_RESTORESTATE
#	define OFSM_CONFIG_ATOMIC_RESTORESTATE ATOMIC_RESTORESTATE
#endif

#endif /*OFSM_CONFIG_SIMULATION*/

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
#ifdef OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER
    OFSMHandler         initHandler;                /*optional, can be null*/
#endif
    void*               fsmPrivateInfo;
    uint8_t             flags;
    _OFSM_TIME_DATA_TYPE wakeupTime;
    uint8_t             currentState;
};

struct OFSMState {
    OFSM					*fsm;
    OFSMEventData*			e;
    _OFSM_TIME_DATA_TYPE	timeLeftBeforeTimeout;      /*time left before timeout set by previous transition*/
    uint8_t					groupIndex;                 /*group index where current fsm is registered*/
    uint8_t					fsmIndex;	                /*fsm index within group*/
};

struct OFSMGroup {
    OFSM**					fsms;
    uint8_t					groupSize;
    OFSMEventData*			eventQueue;
    uint8_t					eventQueueSize;

    volatile uint8_t		flags;
    volatile uint8_t		nextEventIndex; //queue cell index that is available for new event
    volatile uint8_t		currentEventIndex; //queue cell that is being processed by ofsm
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
#define _OFSM_FLAG_OFSM_SIMULATION_EXIT		0x80

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
#define fsm_queue_group_event(fsms, forceNewEvent, eventCode, eventData) \
    ofsm_queue_group_event(fsm_get_group_index(fsms), bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData)


#define ofsm_get_time(currentTime, timeFlags) \
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
        timeFlags = _ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW; \
        currentTime = _ofsmTime; \
    }

#define _OFSM_GET_TRANSTION(fsm, eventCode) ((OFSMTransition*)( (fsm->transitionTableEventCount * fsm->currentState +  eventCode) * sizeof(OFSMTransition) + (char*)fsm->transitionTable) )

/*time comparison*/
/*ao, bo - 'o' means overflow*/
#define _OFSM_TIME_A_GT_B(a, ao, b, bo)  ( (a  >  b) && (ao || !bo) )
#define _OFSM_TIME_A_GTE_B(a, ao, b, bo) ( (a  >=  b) && (ao || !bo) )

/*--------------------------------------
Implementation
----------------------------------------*/
OFSMGroup**				_ofsmGroups;
uint8_t                 _ofsmGroupCount;
volatile uint8_t        _ofsmFlags;
volatile _OFSM_TIME_DATA_TYPE  _ofsmWakeupTime;
volatile _OFSM_TIME_DATA_TYPE  _ofsmTime;

static inline void _ofsm_fsm_process_event(OFSM *fsm, uint8_t groupIndex, uint8_t fsmIndex, OFSMEventData *e)
{
    OFSMTransition *t;
    uint8_t oldFlags;
    _OFSM_TIME_DATA_TYPE oldWakeupTime;
    uint8_t wakeupTimeGTcurrentTime;
    OFSMState fsmState;
    _OFSM_TIME_DATA_TYPE currentTime;
    uint8_t timeFlags;

#ifdef OFSM_CONFIG_SIMULATION
    long delay = -1;
    char overridenState = ' ';
#endif

    if (e->eventCode >= fsm->transitionTableEventCount) {
        _ofsm_debug_printf(1,  "F(%i)G(%i): Unexpected Event!!! Ignored eventCode %i.\n", fsmIndex, groupIndex, e->eventCode);
        return;
    }

    ofsm_get_time(currentTime, timeFlags);

    //check if wake time has been reached, wake up immediately if not timeout event, ignore non-handled   events.
    t = _OFSM_GET_TRANSTION(fsm, e->eventCode);
    wakeupTimeGTcurrentTime = _OFSM_TIME_A_GT_B(fsm->wakeupTime, (fsm->flags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW), currentTime, (timeFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW));
    if (!t->eventHandler || (0 == e->eventCode && (((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) && !(_ofsmFlags & _OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT)) || wakeupTimeGTcurrentTime))) {
        if (!t->eventHandler) {
            fsm->flags |= _OFSM_FLAG_INFINITE_SLEEP;
#ifdef OFSM_CONFIG_SIMULATION
            _ofsm_debug_printf(4,  "F(%i)G(%i): Handler is not specified, state %i event code %i. Assuming infinite sleep.\n", fsmIndex, groupIndex, fsm->currentState, e->eventCode);
        }
        else if ((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) && !(_ofsmFlags & _OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT)) {
            _ofsm_debug_printf(4,  "F(%i)G(%i): State Machine is in infinite sleep.\n", fsmIndex, groupIndex);
        }
        else if (wakeupTimeGTcurrentTime) {
            _ofsm_debug_printf(4,  "F(%i)G(%i): State Machine is asleep. Wakeup is scheduled in %lu ticks.\n", fsmIndex, groupIndex, fsm->wakeupTime - currentTime);
#endif
        }
        return;
    }

    _ofsm_debug_printf(2,  "F(%i)G(%i): State: %i. Processing eventCode %i eventData %i(0x%08X)...\n", fsmIndex, groupIndex, fsm->currentState, e->eventCode, e->eventData, (int)e->eventData);

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
        fsm->flags = oldFlags | _OFSM_FLAG_FSM_PREVENT_TRANSITION;
        fsm->wakeupTime = oldWakeupTime;
        _ofsm_debug_printf(3,  "F(%i)G(%i): Handler requested no transition. FSM state was restored.\n", fsmIndex, groupIndex);
        return;
    }

    //make a transition
#ifdef OFSM_CONFIG_SIMULATION
    uint8_t prevState = fsm->currentState;
#endif
    if (!(fsm->flags & _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE)) {
        fsm->currentState = t->newState;
    }
#ifdef OFSM_CONFIG_SIMULATION
    else {
        overridenState = '!';
    }
#endif

    //check transition delay, assume infinite sleep if new state doesn't accept Timeout Event
    t = _OFSM_GET_TRANSTION(fsm, 0);
    if (!t->eventHandler) {
        fsm->flags |= _OFSM_FLAG_INFINITE_SLEEP; //set infinite sleep
#ifdef OFSM_CONFIG_SIMULATION
        delay = -1;
#endif
    }
    else if (0 == fsm->wakeupTime) {
        fsm->wakeupTime = currentTime + OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY;
#ifdef OFSM_CONFIG_SIMULATION
        delay = OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY;
#endif
    }
    else {
#ifdef OFSM_CONFIG_SIMULATION
        delay = fsm->wakeupTime;
#endif
        fsm->wakeupTime += currentTime;
        if (fsm->wakeupTime < currentTime) {
            fsm->flags |= _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW;
        }
    }
    _ofsm_debug_printf(2,  "F(%i)G(%i): Transitioning from state %i ==>%c%i. Transition delay: %ld\n", fsmIndex, groupIndex,  prevState, overridenState, fsm->currentState, delay);
}/*_ofsm_fsm_process_event*/

static inline void _ofsm_group_process_pending_event(OFSMGroup *group, uint8_t groupIndex, _OFSM_TIME_DATA_TYPE *groupEarliestWakeupTime, uint8_t *groupAndedFsmFlags)
{
    OFSMEventData e;
    OFSM *fsm;
    uint8_t andedFsmFlags = (uint8_t)0xFFFF;
    _OFSM_TIME_DATA_TYPE earliestWakeupTime = 0xFFFFFFFF;
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
    
    _ofsm_debug_printf(4,  "G(%i): currentEventIndex %i, nextEventIndex %i.\n", groupIndex, group->currentEventIndex, group->nextEventIndex);
    
    //Queue considered empty when (nextEventIndex == currentEventIndex) and buffer overflow flag is NOT set
    if (!eventPending) {
        _ofsm_debug_printf(4,  "G(%i): Event queue is empty.\n", groupIndex);
    }

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
}/*_ofsm_group_process_pending_event*/

static inline void _ofsm_setup_hardware() {
#ifdef OFSM_CONFIG_SIMULATION
    //TBI:
#endif /*OFSM_CONFIG_SIMULATION*/
}/*_ofsm_setup_hardware*/

void _ofsm_setup() {

    /*In simulation mode, we need to allow to reset OFSM. Reset internal states*/
#ifdef OFSM_CONFIG_SIMULATION
    uint8_t i, k;
    OFSMGroup *group;
    OFSM *fsm;
    /*reset GLOBALS*/
    _ofsmFlags = (_OFSM_FLAG_INFINITE_SLEEP |_OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT);
    _ofsmTime = 0; 
    /*reset groups and FSMs*/
	for (i = 0; i < _ofsmGroupCount; i++) {
		group = (_ofsmGroups)[i];
        group->flags = 0;
        group->currentEventIndex = group->nextEventIndex = 0;
		for (k = 0; k < group->groupSize; k++) {
			fsm = (group->fsms)[k];
            fsm->flags = _OFSM_FLAG_INFINITE_SLEEP;
        }
    }
#else
    _ofsm_setup_hardware();
#endif /*OFSM_CONFIG_SIMULATION*/

#ifdef OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER
    //configure FSMs, call all initialization handlers
#   ifndef OFSM_CONFIG_SIMULATION
    uint8_t i, k;
    OFSMGroup *group;
    OFSM *fsm;
#   endif
    OFSMState fsmState;
    OFSMEventData e = {0,0};
    fsmState.e = &e;
	for (i = 0; i < _ofsmGroupCount; i++) {
		group = (_ofsmGroups)[i];
		for (k = 0; k < group->groupSize; k++) {
			fsm = (group->fsms)[k];
	        _ofsm_debug_printf(4, "F(%i)G(%i): Initializing...\n", k, i );
            fsmState.fsm = fsm;
            fsmState.timeLeftBeforeTimeout = 0;
            fsmState.groupIndex = i;
            fsmState.fsmIndex = k;
	        if (fsm->initHandler) {
		        (fsm->initHandler)(&fsmState);
	        }
		}
	}
#endif
} /*_ofsm_setup*/

void _ofsm_start() {
    uint8_t i;
    OFSMGroup *group;
    uint8_t andedFsmFlags;
    _OFSM_TIME_DATA_TYPE earliestWakeupTime;
    uint8_t groupAndedFsmFlags;
    _OFSM_TIME_DATA_TYPE groupEarliestWakeupTime;
    _OFSM_TIME_DATA_TYPE currentTime;
    uint8_t timeFlags;
#ifdef OFSM_CONFIG_SIMULATION
	bool doReturn = false;
#endif 

    //start main loop
    do
    {
        OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
            _ofsmFlags |= _OFSM_FLAG_INFINITE_SLEEP; //prevents _ofsm_check_timeout() to ever accessing _ofsmWakeupTime and queue timeout while in process
            _ofsmFlags &= ~_OFSM_FLAG_OFSM_EVENT_QUEUED; //reset event queued flag
#ifdef OFSM_CONFIG_SIMULATION
			if (_ofsmFlags &_OFSM_FLAG_OFSM_SIMULATION_EXIT) {
				doReturn = true; /*don't return or break here!!, or ATOMIC_BLOCK mutex will remain blocked*/
            }
#endif
        }
#ifdef OFSM_CONFIG_SIMULATION
		if (doReturn) {
			return;
		}
#endif


        andedFsmFlags = (uint8_t)0xFFFF;
        earliestWakeupTime = 0xFFFFFFFF;
        for (i = 0; i < _ofsmGroupCount; i++) {
            group = (_ofsmGroups)[i];
            _ofsm_debug_printf(4,  "O: Processing event for group index %i...\n", i);
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
            _ofsm_debug_printf(4,  "O: At least one group has pending event(s). Re-process all groups.\n");
            continue;
        }

        if (!(andedFsmFlags & _OFSM_FLAG_INFINITE_SLEEP)) {
            ofsm_get_time(currentTime, timeFlags);
            if (_OFSM_TIME_A_GTE_B(currentTime, ((uint8_t)timeFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW), earliestWakeupTime, (andedFsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW))) {
                _ofsm_debug_printf(3,  "O: Reached timeout. Queue global timeout event.\n");
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
#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
        _ofsm_debug_printf(4,  "O: Entering sleep... Wakeup Time %ld.\n", _ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP ? -1 : _ofsmWakeupTime);
        OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC();

		_ofsm_debug_printf(4,  "O: Waked up.\n");
#else
        _ofsm_debug_printf(4,  "O: Step through OFSM is complete.\n");
#endif

#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
    } while (1);
#else
    } while(_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED);
#endif
}/*_ofsm_start*/

void _ofsm_queue_group_event(uint8_t groupIndex, OFSMGroup *group, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) {
    uint8_t copyNextEventIndex;
    OFSMEventData *event;
#if OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM > 0
    uint8_t debugFlags = 0x1; //set buffer overflow
#endif
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
        copyNextEventIndex = group->nextEventIndex;

        if (!(group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
#if OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM > 0
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
#if OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM > 0
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
#ifdef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
#   if OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE == 0
		OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
#   endif
#else
    OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
#endif

#ifdef OFSM_CONFIG_SIMULATION
    if (!(debugFlags & 0x2) && debugFlags & 0x1) {
        _ofsm_debug_printf(1,  "G(%i): Buffer overflow. eventCode %i eventData %i(0x%08X) dropped.\n", groupIndex, eventCode, eventData, eventData);
    }
    else {
        _ofsm_debug_printf(3,  "G(%i): Queued eventEvent %i eventData %i (0x%08X) (Updated %i, Set buffer overflow %i).\n", groupIndex, eventCode, eventData, eventData, (debugFlags & 0x2) > 0, (group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW) > 0);
        _ofsm_debug_printf(4,  "G(%i): currentEventIndex %i, nextEventIndex %i.\n", groupIndex, group->currentEventIndex, group->nextEventIndex);
    }
#endif
}/*_ofsm_queue_group_event*/

void ofsm_queue_group_event(uint8_t groupIndex, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData)
{
#ifdef OFSM_CONFIG_SIMULATION
    if (groupIndex >= _ofsmGroupCount) {
        _ofsm_debug_printf(1,  "O: Invalid Group Index %i!!! Dropped eventCode %i eventData %i(0x%08X). \n", groupIndex, eventCode, eventData, eventData);
        return;
    }
#endif
    _ofsm_queue_group_event(groupIndex, _ofsmGroups[groupIndex], forceNewEvent, eventCode, eventData);
}/*ofsm_queue_group_event*/

void ofsm_queue_global_event(bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) {
    uint8_t i;
    OFSMGroup *group;

    for (i = 0; i < _ofsmGroupCount; i++) {
        group = (_ofsmGroups)[i];
        _ofsm_debug_printf(4,  "O: Event queuing group %i...\n", i);
        _ofsm_queue_group_event(i, group, forceNewEvent, eventCode, eventData);
    }
}/*ofsm_queue_global_event*/

static inline void _ofsm_check_timeout()
{
	/*not need as it is called from within atomic block	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { */
	//do nothing if infinite sleep
	if (_ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP) {
		//catch situation when new event was queued just before main loop went to sleep. Probably can be removed, need some analysis TBI:
		if (_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED) {
			_ofsmFlags &= ~_OFSM_FLAG_OFSM_EVENT_QUEUED;
			OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
		}
		return;
	}

	if (_OFSM_TIME_A_GTE_B(_ofsmTime, (_ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW), _ofsmWakeupTime, (_ofsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW))) {
		ofsm_queue_global_event(false, 0, 0); //this call will wakeup main loop

#if OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE == 1 /*in this mode ofsm_queue_... will not wakeup*/
		OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
#endif
	}
	/*	}*/
}/*_ofsm_check_timeout*/

static inline void ofsm_heartbeat(_OFSM_TIME_DATA_TYPE currentTime)
{
	_OFSM_TIME_DATA_TYPE prevTime;
	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
		prevTime = _ofsmTime;
		_ofsmTime = currentTime;
		if (_ofsmTime < prevTime) {
			_ofsmFlags |= _OFSM_FLAG_OFSM_TIMER_OVERFLOW;
		}
		_ofsm_check_timeout();
	}
}/*ofsm_heartbeat*/

/*------------------------------------------------
Simulation and non-simulation specific code
--------------------------------------------------*/
#ifdef OFSM_CONFIG_SIMULATION

struct OFSMSimulationStatusReport {
    uint8_t grpIndex;
    uint8_t fsmIndex;
    _OFSM_TIME_DATA_TYPE ofsmTime;
    //OFSM status
    bool ofsmInfiniteSleep;
    bool ofsmTimerOverflow;
    bool ofsmScheduledTimeOverflow;
    _OFSM_TIME_DATA_TYPE ofsmScheduledWakeupTime;
    //Group status
    bool grpEventBufferOverflow;
    uint8_t grpPendingEventCount;
    //FSM status
    bool fsmInfiniteSleep;
    bool fsmTransitionPrevented;
    bool fsmTransitionStateOverriden;
    bool fsmScheduledTimeOverflow;
    _OFSM_TIME_DATA_TYPE fsmScheduledWakeupTime;
    uint8_t fsmCurrentState;
};

std::mutex cvm;
std::condition_variable cv;

static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

static inline std::string &toLower(std::string &s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

#ifdef _OFSM_IMPL_SIMULATION_STATUS_REPORT_PRINTER
void _ofsm_simulation_status_report_printer(OFSMSimulationStatusReport *r) {
    char buf[80];
    _ofsm_snprintf(buf, (sizeof(buf) / sizeof(*buf)), "-O[%c]-G(%i)[%c,%03d]-F(%i)[%c%c%c]-S(%i)-TW[%010lu%c,O:%010lu%c,F:%010lu%c]"
        //OFSM (-O)
        , (r->ofsmInfiniteSleep ? 'I' : 'i')
        //Group (-G)
        , r->grpIndex
        , (r->grpEventBufferOverflow ? '!' : '.')
        , (r->grpPendingEventCount)
        //Fsm (-F)
        , r->fsmIndex
        , (r->fsmInfiniteSleep ? 'I' : 'i')
        , (r->fsmTransitionPrevented ? 'P' : 'p')
        , (r->fsmTransitionStateOverriden ? 'O' : 'o')
        //Current State (-S)
        , r->fsmCurrentState
        //CurrentTime, Wakeup time (-TW)
        , r->ofsmTime
        , (r->ofsmTimerOverflow ? '!' : '.')
        , r->ofsmScheduledWakeupTime
        , (r->ofsmScheduledTimeOverflow ? '!' : '.')
        , r->fsmScheduledWakeupTime
        , (r->fsmScheduledTimeOverflow ? '!' : '.')
        );
    ofsm_simulation_set_assert_compare_string(buf);
    std::cout << buf << std::endl;
}
#endif

void _ofsm_simulation_create_status_report(OFSMSimulationStatusReport *r, uint8_t groupIndex, uint8_t fsmIndex) {
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
        r->grpIndex = groupIndex;
        r->fsmIndex = fsmIndex;
        r->ofsmTime = _ofsmTime;
        //OFSM
        r->ofsmInfiniteSleep = (bool)((_ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP) > 0);
        r->ofsmTimerOverflow = (bool)((_ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW) > 0);
        r->ofsmScheduledTimeOverflow = (bool)((_ofsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW) > 0);
        r->ofsmScheduledWakeupTime = _ofsmWakeupTime;
        if (r->ofsmInfiniteSleep) {
            r->ofsmScheduledWakeupTime = 0;
            r->ofsmScheduledTimeOverflow = 0;
        }
        //Group
        OFSMGroup *grp = (_ofsmGroups[groupIndex]);
        r->grpEventBufferOverflow = (bool)((grp->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW) > 0);
        if (r->grpEventBufferOverflow) {
            if (grp->currentEventIndex == grp->nextEventIndex) {
                r->grpPendingEventCount = grp->eventQueueSize;
            }
            else {
                r->grpPendingEventCount = grp->eventQueueSize - (grp->currentEventIndex - grp->nextEventIndex);
            }
        }
        else {
            if (grp->nextEventIndex < grp->currentEventIndex) {
                r->grpPendingEventCount = grp->eventQueueSize - (grp->currentEventIndex - grp->nextEventIndex);
            }
            else {
                r->grpPendingEventCount = grp->nextEventIndex - grp->currentEventIndex;
            }
        }
        //FSM
        OFSM *fsm = (grp->fsms)[fsmIndex];
        r->fsmInfiniteSleep = (bool)((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) > 0);
        r->fsmTransitionPrevented = (bool)((fsm->flags & _OFSM_FLAG_FSM_PREVENT_TRANSITION) > 0);
        r->fsmTransitionStateOverriden = (bool)((fsm->flags & _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE) > 0);
        r->fsmScheduledTimeOverflow = (bool)((fsm->flags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW) > 0);
        r->fsmScheduledWakeupTime = fsm->wakeupTime;
        r->fsmCurrentState = fsm->currentState;
        if (r->fsmInfiniteSleep) {
            r->fsmScheduledWakeupTime = 0;
            r->fsmScheduledTimeOverflow = 0;
        }
    }
}

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
#   ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
    std::unique_lock<std::mutex> lk(cvm);
	cv.notify_one();
	lk.unlock();
#   else
    //in script mode call _ofsm_start() directly; it will return
    _ofsm_start();
#   endif
}
#endif /* _OFSM_IMPL_WAKEUP */

int _ofsm_simulation_check_for_assert(std::string &assertCompareString, int lineNumber) {
    std::string lastOut = (char*)_ofsm_simulation_assert_compare_string;
    trim(lastOut);
    if (assertCompareString != lastOut) {
        std::cout << "ASSERT at line: " << lineNumber << std::endl;
        std::cout << "\tExpected: " << assertCompareString << std::endl;
        std::cout << "\tProduced: " << lastOut << std::endl;
        return 1;
    }
    return 0;
}

void setup();
void loop();

void _ofsm_simulation_fsm_thread(int ignore) {
    setup();
    loop();
#ifndef OFSM_CONFIG_SIMULATION_SCRIPT
	_ofsm_debug_printf(1, "Exiting Loop thread...\n");
#endif 
}/*_ofsm_simulation_fsm_thread*/

void _ofsm_simulation_heartbeat_provider_thread(int tickSize) {
    _OFSM_TIME_DATA_TYPE currentTime = 0;
	bool doReturn = false;
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(tickSize));

        OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
            _OFSM_TIME_DATA_TYPE time;
            ofsm_get_time(time, time);
            if (time > currentTime) {
                currentTime = time;
            }
            if (_ofsmFlags & _OFSM_FLAG_OFSM_SIMULATION_EXIT) {
				doReturn = true; /*don't return here, as simulation ATOMIC_BLOCK mutex will remain blocked*/
            }
        }
		if (doReturn) {
#ifndef OFSM_CONFIG_SIMULATION_SCRIPT
			_ofsm_debug_printf(1, "Exiting Heartbeat provider thread...\n");
#endif 
			return;
		}
        currentTime++;
        ofsm_heartbeat(currentTime);
    }
}/*_ofsm_simulation_heartbeat_provider_thread*/

#ifdef _OFSM_IMPL_EVENT_GENERATOR
void _ofsm_simulation_sleep_thread(int sleepMilliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMilliseconds));
}/*_ofsm_simulation_sleep_thread*/

void _ofsm_simulation_sleep(int sleepMilliseconds) {
    std::thread sleepThread(_ofsm_simulation_sleep_thread, sleepMilliseconds);
    sleepThread.join();
}/*_ofsm_simulation_sleep*/


int _ofsm_simulation_event_generator(const char *fileName) {
    std::string line;
    std::ifstream fileStream;
    int exitCode = 0;
    int lineNumber = 0;
    std::string assertCompareString;

    //fileName = "D:/Work/devofsm/test/ofsm.test";

    if (fileName) {
        fileStream.open(fileName);
        std::cin.rdbuf(fileStream.rdbuf());
    }

    while (!std::cin.eof())
    {

        //read line from stdin
        std::getline(std::cin, line);
        lineNumber++;
        trim(line);

        //prepare for parsing
        std::stringstream strStream(line);
        std::string t;
        std::deque<std::string> tokens;

        //strip comments
        std::size_t commentPos = line.find("//");
        if (commentPos != std::string::npos) {
            line.replace(commentPos, line.length(), "");
        }

        //get assert string unless p command
        t = line;
        trim(t);
        toLower(t);
        assertCompareString = "";
        if (0 != t.find("p")) {
            std::size_t assertComparePos = line.find("=");
            if (assertComparePos != std::string::npos) {
                assertCompareString = line.substr(assertComparePos + 1);
                trim(assertCompareString);
                line.replace(assertComparePos, line.length(), "");
            }
        }
        //convert commands to lower case
        toLower(line);

        //parse by tokens
        while (std::getline(strStream, t, ',')) {
            if (t.find_first_not_of(' ') >= 0) {
                tokens.push_back(trim(t));
            }
        }

        //skip empty lines
        if (0 >= tokens.size()) {
            continue;
        }

#ifdef OFSM_CONFIG_CUSTOM_SIMULATION_COMMAND_HOOK_FUNC
        if (OFSM_CONFIG_CUSTOM_SIMULATION_COMMAND_HOOK_FUNC(tokens)) {
            continue;
        }
#endif
        //if line starts with digit, assume shorthand of 'queue' command: eventCode[,eventData[,groupIndex]]
        std::string digits = "0123456789";
        if (std::string::npos != digits.find(tokens[0])) {
            tokens.push_front("queue");
        }

        //parse command
        t = tokens[0];
        uint8_t tCount = (uint8_t)tokens.size();

        switch (t[0]) {
        case 'e':			//e[xit]
        {
            _ofsm_debug_printf(4,  "G: Exiting...\n");
            return exitCode;
        }
        break;
        case 'w':			//w[akeup]
        {
#	        if OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE > 0
                OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
#	        else
				printf("ASSERT at line: %i: wakeup command is ignored unless OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE > 0.\n", lineNumber);
				continue;
#			endif
        }
        break;
        case 'd':			//d[elay][,sleepPeriod]
        {
            _OFSM_TIME_DATA_TYPE sleepPeriod = 0;
            if (tCount > 1) {
                sleepPeriod = atoi(tokens[1].c_str());
            }
            if (0 == sleepPeriod) {
                sleepPeriod = 1000;
            }
            _ofsm_debug_printf(4,  "G: Entering sleep for %lu milliseconds...\n", sleepPeriod);
            _ofsm_simulation_sleep(sleepPeriod);
            continue;
        }
        break;
        case 'p':			//p[rint][,<string to be printed>]
        {
            std::string  str = "";
            if (tCount > 1) {
                str = tokens[1];
            }
            std::cout << str << std::endl;
            continue;
        }
        break;
        case 'q':			//q[ueue][,[mods],eventCode[,eventData[,groupIndex]]]
        {
            uint8_t eventCode = 0;
            uint8_t eventData = 0;
            uint8_t eventCodeIndex = 1;
            uint8_t groupIndex = 0;
            bool isGlobal = false;
            bool forceNew = false;
            if (tCount > 1) {
                t = tokens[1];
                isGlobal = std::string::npos != t.find("g");
                forceNew = std::string::npos != t.find("f");
                if (isGlobal || forceNew) {
                    eventCodeIndex = 2;
                }
            }
            //get eventCode
            if (tCount > eventCodeIndex) {
                t = tokens[eventCodeIndex];
                eventCodeIndex++;
                eventCode = atoi(t.c_str());
            }
            //get eventData
            if (tCount > eventCodeIndex) {
                t = tokens[eventCodeIndex];
                eventCodeIndex++;
                eventData = atoi(t.c_str());
            }
            //get groupIndex
            if (tCount > eventCodeIndex) {
                t = tokens[eventCodeIndex];
                eventCodeIndex++;
                groupIndex = atoi(t.c_str());
                if (groupIndex >= _ofsmGroupCount) {
                    printf("ASSERT at line: %i: Invalid Group Index %i.\n", lineNumber, groupIndex);
                    continue;
                }
            }
            //queue event
            if (isGlobal) {
                ofsm_queue_global_event(forceNew, eventCode, eventData);
            }
            else {
                ofsm_queue_group_event(groupIndex, forceNew, eventCode, eventData);
            }
        }
        break;
        case 'h':			// h[eartbeat][,currentTime] 
        {
            _OFSM_TIME_DATA_TYPE currentTime;
            if (tCount > 1) {
                t = tokens[1];
                currentTime = atoi(t.c_str());
            }
            else {
                OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
                    currentTime = _ofsmTime + 1;
                }
            }
            ofsm_heartbeat(currentTime);
        }
        break;
        case 's':			//s[tatus][,groupIndex[,fsmIndex]]
        {
            OFSMSimulationStatusReport report;
            uint8_t groupIndex = 0;
            uint8_t fsmIndex = 0;
            //get group index
            if (tCount > 1) {
                t = tokens[1];
                groupIndex = atoi(t.c_str());
            }
            //get fsm index
            if (tCount > 2) {
                t = tokens[2];
                fsmIndex = atoi(t.c_str());
            }
            _ofsm_simulation_create_status_report(&report, groupIndex, fsmIndex);

            OFSM_CONFIG_CUSTOM_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER_FUNC(&report);
        }
        break;
        case 'r':			//r[eset]
        {
			return -1; /*repeat main loop*/
		}
        break;
        default:			//Unrecognized command!!!
        {
            printf("ASSERT at line: %i: Invalid Command '%s' ignored.\n", lineNumber, line.c_str());
            continue;
        }
        break;
        }


        //check for assert
        if (assertCompareString.length() > 0) {
            exitCode += _ofsm_simulation_check_for_assert(assertCompareString, lineNumber);
        }

#if OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS > 0
        _ofsm_simulation_sleep(OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS);
#endif

    }

    return exitCode;
}/*_ofsm_simulation_event_generator*/
#endif /* _OFSM_IMPL_EVENT_GENERATOR */

int main(int argc, char* argv[])
{
	int retCode = 0;
	do {
#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
		//start fsm thread
		std::thread fsmThread(_ofsm_simulation_fsm_thread, 0);
		fsmThread.detach();

		//start timer thread
		std::thread heartbeatProviderThread(_ofsm_simulation_heartbeat_provider_thread, OFSM_CONFIG_SIMULATION_TICK_MS);
		heartbeatProviderThread.detach();
#else
		//perform setup and make first iteration through the OFSM
		_ofsm_simulation_fsm_thread(0);
#endif

		char *scriptFileName = NULL;
		if (argc > 1) {
			switch (argc) {
			case 2:
				scriptFileName = argv[1];
				break;
			default:
				std::cerr << "Too many argument. Exiting..." << std::endl;
				return 1;
				break;
			}
		}

		//call event generator
		retCode = OFSM_CONFIG_CUSTOM_SIMULATION_EVENT_GENERATOR_FUNC(scriptFileName);

		//if returned assume exit
		OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
			_ofsmFlags = (_OFSM_FLAG_OFSM_SIMULATION_EXIT | _OFSM_FLAG_OFSM_EVENT_QUEUED);
#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
			OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
#endif
		}
		
		_ofsm_debug_printf(1, "Waiting for %i milliseconds for all threads to exit...\n", OFSM_CONFIG_SIMULATION_TICK_MS);
		_ofsm_simulation_sleep(OFSM_CONFIG_SIMULATION_TICK_MS + 10); /*let heartbeat provider thread to exit before starting new thread*/

		if (retCode < 0) {
			_ofsm_debug_printf(1, "Reseting...\n");
		}

} while (retCode < 0);

    return retCode;
}

#else /* if not OFSM_CONFIG_SIMULATION*/

//TBI:

#endif /*OFSM_CONFIG_SIMULATION*/

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

#ifdef OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER
#   define OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr) \
        OFSM _ofsm_decl_fsm_##fsmId = {\
                (OFSMTransition**)transitionTable, 	/*transitionTable*/ \
                transitionTableEventCount,			/*transitionTableEventCount*/ \
                initializationHandler,				/*initHandler*/ \
                fsmPrivateDataPtr,					/*fsmPrivateInfo*/ \
                _OFSM_FLAG_INFINITE_SLEEP           /*flags*/ \
        };
#else 
#   define OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr) \
        OFSM _ofsm_decl_fsm_##fsmId = {\
                (OFSMTransition**)transitionTable, 	/*transitionTable*/ \
                transitionTableEventCount,			/*transitionTableEventCount*/ \
                fsmPrivateDataPtr,					/*fsmPrivateInfo*/ \
                _OFSM_FLAG_INFINITE_SLEEP           /*flags*/ \
        };
#endif

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
