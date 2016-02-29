//
//// StateMachine.cpp : Defines the entry point for the console application.
////
//
//#include <stdio.h>
//#include <stdlib.h>
//#include <conio.h>
//#include <windows.h>
//
////VS shims
//CRITICAL_SECTION __cs;
//int ___cs_counter;
//
//#define uint8_t unsigned char
//#define null 0
//#define millis() (unsigned long)GetTickCount()
//#define micros 0
//#define NONATOMIC_FORCEOFF 0
//#define ATOMIC_BLOCK_INIT InitializeCriticalSection(&__cs)
//#define ATOMIC_RESTORESTATE __cs
//#define ATOMIC_BLOCK(cs) for( EnterCriticalSection(&cs), ___cs_counter = 0; ___cs_counter < 1; ___cs_counter++, LeaveCriticalSection(&cs) )
//#define NONATOMIC_BLOCK(type) if(1)
//#define delay_us Sleep
//#define delay_ms Sleep
//
//void my_printf(const char* format, ...);
//void my_init_heartbeat_timer();
//void my_enter_sleep();
//void my_wakeup();
//#define OFSM_CONFIG_DEBUG_PRINT_FUNC my_printf
//#define FSM_CONFIG_CUSTOM_INIT_HEARTBEAT_TIMER my_init_heartbeat_timer
//#define FSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC my_enter_sleep
//#define FSM_CONFIG_CUSTOM_WAKEUP_FUNC my_wakeup
//
//
////---------------------------------------
////State Machine implementation
////---------------------------------------
//
////if defined FSM will be printing debug messages; if set to 2 - more detailed messages will be printed.
//#define OFSM_CONFIG_DEBUG 3
//
////TBI:
////State Machine relies on accurate time measurement for its operations. There are many ways it can be done.
////For example on Arduino platform timer0 is being used to provide time by millis() and micros() functions.
////
//
////default number of microseconds for State machine state transition delay. This delay will be automatically applied if event handler didn't specify one.
//#define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY_IN_MICROSECONDS 10000
////when set wakeup time is determined by difference between FSMStateInfo::wakeupTime and current time. 
////Otherwise wakeup will be relative to 'currentTime' which is passed into State Machine.
////If latest, it doesn't account for time spent in other state machines handlers and can be off significantly (depending on long and number of other state machines in the 'band')
////if precision of delay is important define this constant. Otherwise, keep it UNDEF to achieve better synchronism with other State Machines. 
//#define OFSM_CONFIG_WAKEUP_TIME_RELATIVE_TO_MOST_CURRENT_TIME 1
////define event data type, default is unsigned long
//#define OFSM_CONFIG_EVENT_DATA_TYPE uint8_t
////custom debug print function, by default ofsm_printf() will be used to print into Serial.
////#define OFSM_CONFIG_DEBUG_PRINT_FUNC my_printf
////if defined, timestamp  will not be generated for debug print statement. Ignored if OFSM_CONFIG_DEBUG_PRINT_FUNC is defined
////#define OFSM_CONFIG_DEBUG_NO_TIMESTAMP
////defines function that is to be used to acquire timestamp for ofsm_printf
//#define OFSM_CONFIG_DEBUG_TIMESTAMP_GET_TIME ofsm_micros
//
////Defines custom function that implements Sleep functionality
////#define FSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC custom_function
////Defines custom function that implements Wakeup functionality.
////NOTE: this function can be called from within awakened state!!!. 
////#define FSM_CONFIG_CUSTOM_WAKEUP_FUNC custom_function
////Define custom function that implements initialization of heartbeat timer. NOTE: it is referenced only if not defined OFSM_CONFIG_PIGGYBACK_TIMER_0
////its handler must call ofsm_heartbeat_callback and pass time passed since last beat.
////#define FSM_CONFIG_CUSTOM_INIT_HEARTBEAT_TIMER custom_function
//
//
////if defined no debug messages will printed by state machine
////#define OFSM_CONFIG_DEBUG_NO_TRACE
////if defined Timeout event will NOT be queued to all fsms at the beginning of play
////#define OFSM_CONFIG_NO_TIMEOUT_EVENT_AT_START
////if defined fresh current time snapshot will be taken before processing FSMs within the group.
////#define OFSM_CONFIG_TAKE_NEW_TIME_SNAPSHOT_FOR_EACH_GROUP
//
//#ifdef OFSM_CONFIG_DEBUG_PRINT_FUNC
//#define ofsm_printf OFSM_CONFIG_DEBUG_PRINT_FUNC
//#else
//#ifdef OFSM_CONFIG_DEBUG
//#include <string.h>
//#include <stdio.h>
//void ofsm_printf(const char* format, ...) {
//	char buf[256];
//	char *b = buf;
//#	ifndef OFSM_CONFIG_DEBUG_NO_TIMESTAMP
//	sprintf_s(buf, sizeof(buf) / sizeof(*buf), sizeof("[%i]: ", OFSM_CONFIG_DEBUG_TIMESTAMP_GET_TIME());
//	b += strlen(buf);
//#	endif
//	va_list argp;
//	va_start(argp, format);
//	vsprintf_s(buf, ((sizeof(buf) / sizeof(*buf)) - (b - buf)), format, argp);
//	va_end(argp);
//	//TBI: call Serial API 
//}
//#else
//#	define ofsm_printf(format, ...) do{}while(0) //see: http://stackoverflow.com/questions/1644868/c-define-macro-for-debug-printing
//#endif
//#endif
//
////IDEAS FOR FUTURE IMPLEMENTATION
//#define OFSM_CONFIG_SLEEP_TIMER_ID 2 //value from 0-2 - timer number to use for sleeping purposes
////If defined state machine will piggyback on pre-initialized timer0 interrupt. For Arduino platform that timer is running at 1kHz.
////Otherwise, OFSM will setup its own timer on OFSM_CONFIG_SLEEP_TIMER_ID
//// #define OFSM_CONFIG_PIGGYBACK_TIMER_0
//
//#define OFSM_MIN(a,b) a > b ? b : a;
////Compare two time variables, returns true if first time argument is greater or equal then second argument. 
////Overflow is considered, assuming that first time argument can not be set further than 0xFFFF ticks into future from second time argument
//#define OFSM_TIME_A_GTE_B(a,b) (((unsigned long)a >= b && ((unsigned long)a - b) < 0xFFFF) || ((unsigned long)a < b && (b - (unsigned long)a) > 0xFFFF))
//#define OFSM_TIME_A_GT_B(a,b) (((unsigned long)a > b && ((unsigned long)a - b) < 0xFFFF) || ((unsigned long)a < b && (b - (unsigned long)a) > 0xFFFF))
////pseudo code: Transition *t = &((fsm->transitionTable)[fsm->currentState][eventCode]);
//#define OFSM_TRANSTION_GET(fsm, eventCode) (fsm->transitionTable + (fsm->currentState * fsm->transitionTableRowSize) + eventCode)
//
//#ifdef OFSM_CONFIG_DEBUG
//#else
//#define OFSM_CONFIG_DEBUG_NO_TRACE
//#endif
//
////Common flags
//#define OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS 0x2
//#define OFSM_FLAG_INFINITE_SLEEP 0x1
//
////FSM Flags
//#define OFSM_FLAG_FSM_PREVENT_TRANSITION 0x4
//
////GROUP Flags
//#define OFSM_FLAG_GROUP_BUFFER_OVERFLOW 0x4
//
////Orchestra Flags
//#define OFSM_FLAG_ORCHESTRA_EVENT_QUEUED 0x4
//
//
//
//struct FSMStateInfo; //
//struct FSM; //finite state machine
//void _ofsm_fsm_process_event(FSM *fsm, uint8_t groupId, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData, unsigned long currentTime);
//typedef void(*FSMHandler)(FSM *fsm);
//
//struct Transition {
//	FSMHandler eventHandler;
//	uint8_t newState;
//};
//
//struct FSM {
//	
//	uint8_t  fsmId;    //READ ONLY! proprietary state machine id. 
//	Transition *transitionTable;
//	uint8_t transitionTableRowSize; //number of elements in each row (number of events defined)
//	FSMHandler initHandler; //optional, can be null
//	void* fsmPrivateInfo;
//
//	//must be less then 0xFFFF0000 or INFINITE delay will be assumed, can be set represent milliseconds or microseconds. see fsm_set_transition_ms_delay()
//	unsigned long  wakeupTime; 
//	uint8_t  groupId;  //group id where current fsm is playing
//	uint8_t  eventCode;  //current event code
//	OFSM_CONFIG_EVENT_DATA_TYPE eventData;
//	uint8_t currentState;
//	uint8_t flags; //0x1 - FSM is in infinite sleep
//};
//
//#define fsm_prevent_transition(fsm) (fsm->flags |= OFSM_FLAG_FSM_PREVENT_TRANSITION)
//#define fsm_set_transition_ms_delay(fsm, milliseconds) (fsm->wakeupTime = milliseconds, fsm->flags |= OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS)
//#define fsm_set_transition_us_delay(fsm, microseconds) (fsm->wakeupTime = microseconds/*, fsm->flag &= ~OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS*/)
//#define fsm_set_inifinite_delay(fsm) (fsm->wakeupTime &= 0xFFFF0000)
//#define fsm_get_private_info(fsm) (fsm->fsmPrivateInfo)
//#define fsm_get_private_info_cast(fsm, type) ((type)fsm->fsmPrivateInfo)
//#define fsm_get_id(fsm) (fsm->fsmId)
//#define fsm_get_group_id(fsm) (fsm->groupId)
//#define fsm_get_state(fsm) (fsm->currentState)
//#define fsm_get_event_code(fsm) (fsm->eventCode)
//#define fsm_get_event_data(fsm) (fsm->eventData)
//
////event data 
//struct FSMEventData {
//	OFSM_CONFIG_EVENT_DATA_TYPE eventData;
//	uint8_t eventCode;
//};
//
////FSM Orchestra group: collection of FSMs sharing the same EVENTS set and EVENT queue.
//struct FSMGroup {
//	FSM **fsms;
//	uint8_t groupSize;
//	FSMEventData *eventQueue;
//	uint8_t eventQueueSize;
//	volatile uint8_t freeIdx; //queue cell index that is available for new event
//	volatile uint8_t inprocessIdx; //queue cell that is being processed by ofsm
//	volatile uint8_t flags; //0x1 - buffer overflow, ignore events until flag is cleared
//};
//
////OFSM Orchestra globals
//FSMGroup **_ofsmGroups;
//uint8_t _ofsmGroupCount;
//volatile unsigned long _ofsmWakeupTime;
//volatile uint8_t _ofsmFlags;
//
//
//#ifdef OFSM_CONFIG_PIGGYBACK_TIMER_0
//#define ofsm_micros micros
//#define ofsm_millis millis
//#else 
//volatile unsigned long _ofsm_time_millis;
//volatile unsigned long _ofsm_time_micros;
//
//static unsigned long inline ofsm_micros() {
//	unsigned long t;
//	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//		t = _ofsm_time_micros;
//	}
//	return t;
//}
//static unsigned long inline ofsm_millis() {
//	unsigned long t;
//	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//		t = _ofsm_time_millis;
//	}
//	return t;
//}
//#endif
//
//#ifdef FSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC
//#define _ofsm_enter_sleep FSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC
//#else
//static void inline _ofsm_enter_sleep() {
//	//TBI: MCU specific code
//}
//#endif
//
//#ifdef FSM_CONFIG_CUSTOM_WAKEUP_FUNC
//#define _ofsm_wakeup FSM_CONFIG_CUSTOM_WAKEUP_FUNC
//#else
//static void inline _ofsm_wakeup() {
//	//TBI: MCU specific code
//}
//#endif
//
//void ofsm_queue_global_event(bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
//
//#ifndef OFSM_CONFIG_DEBUG_PRINT_FUNC 
//static void inline _ofsm_initialize_serial_for_debug_print() {
//	//TBI: MCU specific code
//}
//#endif
//
//static void inline _ofsm_check_timeout() {
//	//do nothing if infinite sleep
//	if (_ofsmFlags & OFSM_FLAG_INFINITE_SLEEP) {
//		//catch situation when new event was queued just before main loop went to sleep
//		if (_ofsmFlags & OFSM_FLAG_ORCHESTRA_EVENT_QUEUED) {
//			_ofsm_wakeup();
//		}
//		return;
//	}
//	if ((_ofsmFlags & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS && OFSM_TIME_A_GTE_B(ofsm_millis(), _ofsmWakeupTime))
//		|| !(_ofsmFlags & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS) && OFSM_TIME_A_GTE_B(ofsm_micros(), _ofsmWakeupTime)) {
//		//queue an timeout event 
//		ofsm_queue_global_event(false, 0, 0); //this call will wakeup main loop
//	}
//}
//
//#ifndef OFSM_CONFIG_PIGGYBACK_TIMER_0
//static void inline ofsm_heartbeat_callback(unsigned long ellapsedTimeMs, unsigned long ellapsedTimeUs) {
//	unsigned long prevMicros;
//	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//		prevMicros = _ofsm_time_micros;
//		_ofsm_time_micros += ellapsedTimeUs;
//		//check for overflow, increase MS portion if happened
//		if (_ofsm_time_micros < prevMicros) {
//			_ofsm_time_millis++;
//		}
//		_ofsm_time_millis += ellapsedTimeMs;
//	}
//	_ofsm_check_timeout();
//}
//#else
////when we piggyback timer_0, we assume that time is being kept by timer subroutine and  time functions micro() and millis() can be used.
//#define ofsm_heartbeat_callback _ofsm_check_timeout
//#endif
//
//
//#ifdef FSM_CONFIG_CUSTOM_INIT_HEARTBEAT_TIMER
//#define _ofsm_initialize_heartbeat_timer FSM_CONFIG_CUSTOM_INIT_HEARTBEAT_TIMER
//#else
//#	ifdef OFSM_CONFIG_PIGGYBACK_TIMER_0
//TBI : ISR method, call ofsm_heartbeat_callback
//#	else 
//static void inline _ofsm_initialize_heartbeat_timer() {
//	//TBI: MCU specific code
//}
//#	endif
//#endif
//
//
//static void inline _ofsm_fsm_initialize(FSM *fsm, uint8_t groupId) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	ofsm_printf("G(%i)FSM(%i): Initializing...\n", groupId, fsm->fsmId);
//#endif
//	if (fsm->initHandler) {
//		(fsm->initHandler)(fsm);
//	}
//}
//
//void ofsm_initialize(FSMGroup **groups, uint8_t groupCount) {
//	uint8_t i, k;
//	FSMGroup *group;
//	FSM *fsm;
//
//	_ofsmGroups = groups;
//	_ofsmGroupCount = groupCount;
//
//	//to over groups and initialize all their fsm
//	for (i = 0; i < _ofsmGroupCount; i++) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		ofsm_printf("OFSM: Initializing group %i...\n", i);
//#endif
//		group = (_ofsmGroups)[i];
//		for (k = 0; k < group->groupSize; k++) {
//			fsm = (group->fsms)[k];
//			_ofsm_fsm_initialize(fsm, k);
//		}
//	}
//
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	ofsm_printf("OFSM: Setting up timer...\n");
//#endif
//	_ofsm_initialize_heartbeat_timer();
//
//#ifndef OFSM_CONFIG_DEBUG_PRINT_FUNC 
//#	ifdef OFSM_CONFIG_DEBUG
//#		ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	ofsm_printf("OFSM: Configuring Serial api...\n");
//#		endif
//	_ofsm_initialize_serial_for_debug_print();
//#	endif
//#endif
//
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	ofsm_printf("OFSM: Configuration is complete.\n");
//#endif
//}
//
//void _ofsm_queue_group_event(FSMGroup *group, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) {
//	uint8_t copyFreeIdx;
//	FSMEventData *event;
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	bool bufferOverflow = 1;
//#endif
//	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//		//set event queued flag, so that ofsm_start() knows if it need to continue processing
//		_ofsmFlags |= OFSM_FLAG_ORCHESTRA_EVENT_QUEUED;
//		if (!(group->flags & OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//			bufferOverflow = 0;
//#endif
//			if (group->freeIdx == group->inprocessIdx) {
//				forceNewEvent = true;
//			}
//			else if (0 == eventCode) {
//				forceNewEvent = false; //always replace timeout event
//			}
//			//update previous event if previous event codes matches 
//			copyFreeIdx = group->freeIdx;
//			if (!forceNewEvent) {
//				if (copyFreeIdx == 0) {
//					copyFreeIdx = group->eventQueueSize;
//				}
//				event = &(group->eventQueue[copyFreeIdx - 1]);
//				if (event->eventCode != eventCode) {
//					forceNewEvent = 1;
//				}
//				else {
//					event->eventData = eventData;
//				}
//			}
//			if (forceNewEvent) {
//				group->freeIdx++;
//				if (group->freeIdx >= group->eventQueueSize) {
//					group->freeIdx = 0;
//				}
//				//event buffer overflow disable further events
//				if (group->freeIdx == group->inprocessIdx) {
//					group->flags |= OFSM_FLAG_GROUP_BUFFER_OVERFLOW; //set buffer overflow flag
//				}
//			}
//		}
//		//queue event.
//		if (forceNewEvent) {
//			event = &(group->eventQueue[copyFreeIdx]);
//			event->eventCode = eventCode;
//			event->eventData = eventData;
//		}
//	}
//	_ofsm_wakeup();
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	if (bufferOverflow) {
//		ofsm_printf("GRP: Buffer overflow. Event %i event data 0x%08X dropped.\n", eventCode, eventData);
//	}
//	else {
//		ofsm_printf("GRP: Queued event %i event data 0x%08X.\n", eventCode, eventData);
//#if OFSM_CONFIG_DEBUG > 2
//		ofsm_printf("GRP: inprocessIdx %i, freeIdx %i.\n", group->inprocessIdx, group->freeIdx);
//#endif
//	}
//#endif
//}
//
//
//#define ofsm_queue_group_event(groupId, forceNewEvent, eventCode, eventData) _ofsm_queue_group_event(_ofsmGroups[groupId], forceNewEvent, eventCode, eventData)
//
//void ofsm_queue_global_event(bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) {
//	uint8_t i;
//	FSMGroup *group;
//
//	for (i = 0; i < _ofsmGroupCount; i++) {
//		group = (_ofsmGroups)[i];
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		ofsm_printf("OFSM: Event queuing group %i...\n", i);
//#endif
//		_ofsm_queue_group_event(group, forceNewEvent, eventCode, eventData);
//	}
//}
//
////returns: no pending events indicator (queue is empty indicator), 
//static void inline _ofsm_group_process_pending_event(FSMGroup *group, uint8_t groupId, unsigned long currentTimeMs, unsigned long currentTimeUs, unsigned long *groupEarliestWakeupTime, uint8_t *groupTotalFsmFlags) {
//	FSMEventData event;
//	FSM *fsm;
//	uint8_t totalFsmFlags = (uint8_t)0xFFFF;
//	unsigned long earliestWakeupTime = 0xFFFF0000;
//	uint8_t i;
//	uint8_t eventPending = 1;
//	unsigned long currentTime;
//
//	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//		if (group->inprocessIdx == group->freeIdx && !(group->flags & OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
//			eventPending = 0;
//		}
//		else {
//			event = ((group->eventQueue)[group->inprocessIdx]);
//		}
//		group->inprocessIdx++;
//		if (group->inprocessIdx == group->eventQueueSize) {
//			group->inprocessIdx = 0;
//		}
//		group->flags &= ~OFSM_FLAG_GROUP_BUFFER_OVERFLOW; //clear buffer overflow
//	}
//
//	//Queue is empty when no new event exist (freeidx == inprocessIdx) and buffer overflow flag is not set
//	if (!eventPending) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		ofsm_printf("GRP(%i): Event queue is empty.\n", groupId);
//#endif
//	}
//	else {
//#ifdef OFSM_CONFIG_TAKE_NEW_TIME_SNAPSHOT_FOR_EACH_GROUP
//		currentTimeMs = ofsm_millis();
//		currentTime = currentTimeUs = ofsm_micros();
//#endif
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		ofsm_printf("GRP(%i): Event/Data %i/0x%08X processing ...\n", groupId, event.eventCode, event.eventData);
//#endif
//	}
//	//it is outside of queueIsEmpty, so that we can gather fsm info about next wakeup time
//	for (i = 0; i < group->groupSize; i++) {
//		fsm = (group->fsms)[i];
//		//if queue is empty don't call fsm just collect info
//		if (eventPending) {
//			if (fsm->flags & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS) {
//				currentTime = currentTimeMs;
//			}
//			else {
//				currentTime = currentTimeUs;
//			}
//			//skip early timeout event
//			if (0 == event.eventCode && (fsm->flags & OFSM_FLAG_INFINITE_SLEEP || OFSM_TIME_A_GT_B(fsm->wakeupTime, currentTime))) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//#	if (OFSM_CONFIG_DEBUG > 1)
//				ofsm_printf("G(%i): Event ignored for FSM %i is still asleep.\n", groupId, fsm->fsmId);
//#	endif
//#endif
//			}
//			else {
//				_ofsm_fsm_process_event(fsm, groupId, event.eventCode, event.eventData, currentTime);
//			}
//		}
//		totalFsmFlags &= fsm->flags;
//		//if fsm DID NOT requested infinite sleep,then take sleep period
//		if (!(totalFsmFlags & OFSM_FLAG_INFINITE_SLEEP)) {
//			if (totalFsmFlags & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS) {
//				if (fsm->flags & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS) {
//					//compare milliseconds to milliseconds
//					earliestWakeupTime = OFSM_MIN(earliestWakeupTime, fsm->wakeupTime);
//				}
//				else {
//					earliestWakeupTime = fsm->wakeupTime;
//				}
//			}
//			else {
//				if (!(fsm->flags & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS)) {
//					//compare micros to micros
//					earliestWakeupTime = OFSM_MIN(earliestWakeupTime, fsm->wakeupTime);
//				}
//			}
//		}
//	}
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	if (eventPending) {
//		ofsm_printf("GRP(%i): Event/Data %i/0x%08X processing complete.\n", groupId, event.eventCode, event.eventData);
//#if OFSM_CONFIG_DEBUG > 2
//		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//			ofsm_printf("GRP: inprocessIdx %i, freeIdx %i.\n", group->inprocessIdx, group->freeIdx);
//		}
//#endif
//	}
//#endif
//
//	//signal if another event is awaiting processing.  New event could have arrived while were calling FSMs
//	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//		if (group->flags & OFSM_FLAG_GROUP_BUFFER_OVERFLOW || group->freeIdx != group->inprocessIdx) {
//			_ofsmFlags |= OFSM_FLAG_ORCHESTRA_EVENT_QUEUED;
//		}
//	}
//
//	*groupEarliestWakeupTime = earliestWakeupTime;
//	*groupTotalFsmFlags = totalFsmFlags;
//}
//
//void ofsm_start() {
//	uint8_t i;
//	FSMGroup *group;
//	uint8_t totalFsmFlag = (uint8_t)0xFFFF;
//	unsigned long earliestWakeupTime = 0xFFFF0000;
//	uint8_t groupTotalFsmFlags;
//	unsigned long groupEarliestWakeupTime;
//	unsigned long currentTimeMs;
//	unsigned long currentTimeUs;
//
//
//#ifndef OFSM_CONFIG_NO_TIMEOUT_EVENT_AT_START
//	ofsm_queue_global_event(1, 0, 0);
//#endif
//	//start main loop
//	_ofsmFlags |= OFSM_FLAG_INFINITE_SLEEP; //prevents _ofsm_check_timeout() to ever accessing _ofsmWakeupTime while in process
//	while (1) {
//		_ofsmFlags &= ~OFSM_FLAG_ORCHESTRA_EVENT_QUEUED; //reset event queued flag
//		currentTimeMs = ofsm_millis();
//		currentTimeUs = ofsm_micros();
//		totalFsmFlag = (uint8_t)0xFFFF;
//		earliestWakeupTime = 0x000F0000;
//		for (i = 0; i < _ofsmGroupCount; i++) {
//			group = (_ofsmGroups)[i];
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//#	if (OFSM_CONFIG_DEBUG > 1)
//			ofsm_printf("OFSM: Processing event for group %i...\n", i);
//#	endif
//#endif
//			_ofsm_group_process_pending_event(group, i, currentTimeMs, currentTimeUs, &groupEarliestWakeupTime, &groupTotalFsmFlags);
//			totalFsmFlag &= groupTotalFsmFlags;
//			earliestWakeupTime = OFSM_MIN(earliestWakeupTime, groupEarliestWakeupTime);
//		}
//
//		//if have pending events in either of group, repeat the step
//		if (_ofsmFlags & OFSM_FLAG_ORCHESTRA_EVENT_QUEUED) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//#	if (OFSM_CONFIG_DEBUG > 1)
//			ofsm_printf("OFSM: At least one group has pending event(s). Re-process all groups.\n", i);
//#	endif
//#endif
//			continue;
//		}
//
//		if (!(totalFsmFlag & OFSM_FLAG_INFINITE_SLEEP)) {
//			//check if we reached wakeup time
//			if (totalFsmFlag & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS) {
//				currentTimeUs = ofsm_millis();
//			}
//			else {
//				currentTimeUs = ofsm_micros();
//			}
//			if (OFSM_TIME_A_GTE_B(currentTimeUs, earliestWakeupTime)) {
//				ofsm_queue_global_event(false, 0, 0);
//				continue;
//			}
//		}
//		//There is no need for ATOMIC, because _ofsm_check_timeout() will never access _ofsmWakeupTime while in process
//		//		ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
//		_ofsmWakeupTime = earliestWakeupTime;
//		_ofsmFlags = totalFsmFlag;
//		//		}
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//#	if (OFSM_CONFIG_DEBUG > 1)
//		ofsm_printf("OFSM: Entering sleep. Wakeup Time %i...\n", (totalFsmFlag & OFSM_FLAG_INFINITE_SLEEP) ? -1 : _ofsmWakeupTime);
//#	endif
//#endif
//		_ofsm_enter_sleep();
//
//		_ofsmFlags |= OFSM_FLAG_INFINITE_SLEEP; //prevents _ofsm_check_timeout() to ever accessing _ofsmWakeupTime while in process
//
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//#	if (OFSM_CONFIG_DEBUG > 1)
//		ofsm_printf("OFSM: Waking up...\n");
//#	endif
//#endif
//
//	}
//}
//
//static void inline _ofsm_fsm_process_event(FSM *fsm, uint8_t groupId, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData, unsigned long currentTime) {
//	Transition *t;
//	FSMStateInfo *ctrl;
//	uint8_t prevState;
//	uint8_t oldFlags;
//	unsigned long oldWakeupTime;
//
//#ifdef OFSM_CONFIG_DEBUG
//	unsigned long delay = -1;
//#endif
//
//	if (eventCode >= fsm->transitionTableRowSize) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		ofsm_printf("G(%i)FSM(%i): Unexpected event %i ignored.\n", groupId, fsm->fsmId, eventCode);
//#endif
//		return;
//	}
//
//	//check if wake time has been reached, wake up immediately if not timeout event, ignore non-handled   events.
//	t = OFSM_TRANSTION_GET(fsm, eventCode);
//	if (!t->eventHandler || (((fsm->flags & OFSM_FLAG_INFINITE_SLEEP) || OFSM_TIME_A_GT_B(fsm->wakeupTime, currentTime)) && 0 == eventCode)) {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//#	if (OFSM_CONFIG_DEBUG > 1)
//		if (!t->eventHandler) {
//			ofsm_printf("G(%i)FSM(%i): Handler is not specified. Infinite sleep.\n", groupId, fsm->fsmId);
//		}
//		else if (fsm->flags & OFSM_FLAG_INFINITE_SLEEP) {
//			ofsm_printf("G(%i)FSM(%i): State Machine is in infinite sleep.\n", groupId, fsm->fsmId);
//		}
//		else if (OFSM_TIME_A_GT_B(fsm->wakeupTime, currentTime)) {
//			ofsm_printf("G(%i)FSM(%i): State Machine is asleep. Wakeup is scheduled in %i ticks.\n", groupId, fsm->fsmId, fsm->wakeupTime - currentTime);
//		}
//#	endif 
//#endif
//		return;
//	}
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	ofsm_printf("G(%i)FSM(%i): State: %i, processing event %i...\n", groupId, fsm->fsmId, fsm->currentState, eventCode);
//#endif 
//	//call handler
//	oldFlags = fsm->flags;
//	oldWakeupTime = fsm->wakeupTime;
//	fsm->wakeupTime = 0;
//	fsm->flags &= ~(OFSM_FLAG_INFINITE_SLEEP | OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS | OFSM_FLAG_FSM_PREVENT_TRANSITION); //clear flags
//	fsm->groupId = groupId;
//	fsm->eventData = eventData;
//	fsm->eventCode = eventCode;
//	(t->eventHandler)(fsm);
//
//	//check if error was reported, restore original FSM state
//	if (fsm->flags & OFSM_FLAG_FSM_PREVENT_TRANSITION) {
//		fsm->flags = oldFlags;
//		fsm->wakeupTime = oldWakeupTime;
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		ofsm_printf("G(%i)FSM(%i): Handler requested no transition. FSM state is restored.\n", groupId, fsm->fsmId);
//#endif
//		return;
//	}
//
//	//make a transition
//#ifdef OFSM_CONFIG_DEBUG
//	prevState = fsm->currentState;
//#endif
//	fsm->currentState = t->newState;
//
//
//	//check transition delay, assume infinite (> 0xFFFF) if new state doesn't accept Timeout Event
//	t = OFSM_TRANSTION_GET(fsm, 0);
//	if (!t->eventHandler) {
//		fsm->flags |= OFSM_FLAG_INFINITE_SLEEP; //set infinite sleep
//	}
//	else if (0 == fsm->wakeupTime) {
//		fsm->wakeupTime = currentTime + OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY_IN_MICROSECONDS;
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		delay = OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY_IN_MICROSECONDS;
//#endif
//	}
//	else {
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//		delay = fsm->wakeupTime;
//#endif
//		// re-use delay, to set actual wakeup time. 
//		// we have to accept handler's decision about delay and set it relative to immediate current time instead of provided 'currentTime' which may be a bit behind.
//#ifdef OFSM_CONFIG_WAKEUP_TIME_RELATIVE_TO_MOST_CURRENT_TIME
//		if (fsm->flags & OFSM_FLAG_WAKEUP_TIME_IN_MILLISECONDS) {
//			fsm->wakeupTime += ofsm_millis();
//		}
//		else {
//			fsm->wakeupTime += ofsm_micros();
//		}
//#else 
//		ctrl->wakeupTime += currentTime;
//#endif
//	}
//#ifndef OFSM_CONFIG_DEBUG_NO_TRACE
//	ofsm_printf("G(%i)FSM(%i): Event %i processing is complete. Transitioning from state %i ==> %i. State transition delay: %i\n", groupId, fsm->fsmId, eventCode, prevState, fsm->currentState, delay);
//#endif
//
//}
//
////---------------------------------------------
////Transition Handlers
////---------------------------------------------
//void handleInit(FSM *fsm) {
//	ofsm_printf("H(%i-%i): handleInit\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
//}
//
//void handleS0E1(FSM *fsm) {
//	ofsm_printf("H(%i-%i): handleS0E1\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
//}
//
//void handleS0E2(FSM *fsm) {
//	ofsm_printf("H(%i-%i): handleS0E2\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
//	fsm_set_transition_us_delay(fsm, 1500);
//}
//
//void handleS1E1(FSM *fsm) {
//	ofsm_printf("H(%i-%i): handleS1E1\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
//}
//
//void handlerTimeout(FSM *fsm) {
//	ofsm_printf("H(%i-%i): handleTimeout\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
//}
//
//void handlerE3Failure(FSM *fsm) {
//	ofsm_printf("H(%i-%i): handlerE3Failure\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
//	fsm_prevent_transition(fsm);
//}
//
////------------------------------------------
////State and Events
////------------------------------------------
//
////event are shared between all FSMs.
//enum Event { Timeout, //AKA: EnteringState, Default etc
//			 E1,	E2,	E3 };
//
//enum StateCode {S0,	S1};
//
////-----------------------------------
////Initialization
////---------------------------------
//Transition transitionTable1[][1 + E3] = {
//	/* timeout,                          E1,                            E2              E3*/
//	{ { handlerTimeout, S0 },{ handleS0E1, S1 },{ handleS0E2, S0 },{ handlerE3Failure, S1 } }, //S0
//	{ { null,           S0 },{ handleS1E1, S0 },{ null,       S0 },{ handlerE3Failure, S1 } }, //S2
//};
//
//FSM fsm1 = {
//	0,								//state machine id
//	(Transition*)transitionTable1,	//transition table
//	1 + E3,							//transition table row size
//	handleInit,						//initialization handler
//	null,							//state machine private info
//};
//
//FSM *fc1[] = { &fsm1 };
//FSMEventData eq1[10];
//FSMGroup fsmGroup1 = {
//	fc1,
//	sizeof(fc1) / sizeof(*fc1),
//	eq1,
//	sizeof(eq1) / sizeof(*eq1),
//};
//FSMGroup *groups[] = { &fsmGroup1 };
//
////------------------------------------------------------------
////EVENT GENERATOR
////------------------------------------------------------------
//void my_printf(const char* format, ...) {
//	char buf[256];
//	char *b = buf;
//#ifndef OFSM_CONFIG_DEBUG_NO_TIMESTAMP
//	sprintf_s(buf, sizeof(buf) / sizeof(*buf), "[%i]: ", ofsm_micros());
//	b += strlen(buf);
//#endif
//	va_list argp;
//	va_start(argp, format);
//	vsprintf_s(b, ((sizeof(buf) / sizeof(*buf)) - (b - buf)), format, argp);
//	va_end(argp);
//	printf(buf);
//}
//
//#define HEARBEAT_MS 1000
//DWORD WINAPI TimerThread(LPVOID lpParam) {
//	ofsm_printf("TIMER: started.\n");
//	while (1) {
//		Sleep(HEARBEAT_MS);
//		//since default delay is based on microseconds, we assume that PC's millisecond is OFSM microsecond
//		ofsm_heartbeat_callback(0, HEARBEAT_MS);
//	}
//}
//
//
//DWORD WINAPI OFSMThread(LPVOID lpParam) {
//	ofsm_printf("OFSM: Thread started.\n");
//	ofsm_start();
//	return 0;
//}
//
//DWORD timerThreadId;
//HANDLE timerThreadHandle;
//DWORD ofsmThreadId;
//HANDLE ofsmThreadHandle;
//
//
//void my_init_heartbeat_timer() {
//	timerThreadHandle = CreateThread(
//		NULL,                   // default security attributes
//		0,                      // use default stack size  
//		TimerThread,       // thread function name
//		NULL,          // argument to thread function 
//		0,                      // use default creation flags 
//		&timerThreadId);   // returns the thread identifier 
//}
//void my_enter_sleep() {
//	SuspendThread(ofsmThreadHandle);
//}
//void my_wakeup() {
//	ResumeThread(ofsmThreadHandle);
//}
//
//
//int main(int argc, char* argv[])
//{
//	ATOMIC_BLOCK_INIT;
//	ofsm_initialize(groups, sizeof(groups) / sizeof(*groups));
//
//	ofsmThreadHandle = CreateThread(
//		NULL,                   // default security attributes
//		0,                      // use default stack size  
//		OFSMThread,       // thread function name
//		NULL,          // argument to thread function 
//		0,                      // use default creation flags 
//		&ofsmThreadId);   // returns the thread identifier 
//
//
//	char str[2];
//	str[1] = '\0';
//	int in = 0;
//	int eventCode = 0;
//
//	while (in != 0x1B) {
//		in = _getch();
//		str[0] = (char)in;
//		eventCode = atoi(str);
//		ofsm_printf("INPUT: %i (%c)\n", str[0], eventCode);
//		if ((0 == eventCode && in != '0')) {
//			ofsm_printf("INPUT: Unexpected event, skipping\n");
//			continue;
//		}
//		//_ofsm_fsm_process_event(&fsm1, 0, eventCode, 0, ofsm_micros());
//		ofsm_queue_group_event(0, false, eventCode, 0);
//	}
//	ofsm_printf("INPUT: exiting...\n");
//
//	return 0;
//}
