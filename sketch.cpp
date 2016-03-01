//VC shim
#include <stdint.h> //to define uint8_t


#define OFSM_CONFIG_DEBUG_LEVEL 3
#define OFSM_CONFIG_PC_SIMULATION
#define OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS 500
#define OFSM_CONFIG_TAKE_NEW_TIME_SNAPSHOT_FOR_EACH_GROUP
#define OFSM_CONFIG_DEBUG_PRINT_ADD_TIMESTAMP
#define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 5

#include "Ofsm.h"

/*--------------------------------
Handlers
--------------------------------*/
void handleInit(OFSMState *fsm) {
	ofsm_debug_printf("H(%i-%i): handleInit\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handleS0E1(OFSMState *fsm) {
	ofsm_debug_printf("H(%i-%i): handleS0E1\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handleS0E2(OFSMState *fsm) {
	ofsm_debug_printf("H(%i-%i): handleS0E2\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
	fsm_set_transition_delay(fsm, 10);
}

void handleS1E1(OFSMState *fsm) {
	ofsm_debug_printf("H(%i-%i): handleS1E1\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handlerTimeout(OFSMState *fsm) {
	ofsm_debug_printf("H(%i-%i): handleTimeout\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handlerE3Failure(OFSMState *fsm) {
	ofsm_debug_printf("H(%i-%i): handlerE3Failure\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
	fsm_prevent_transition(fsm);
}

enum Event { Timeout, E1, E2, E3 };
enum State { S0, S1 };

/*-----------------------------------
Initialization
---------------------------------*/
OFSMTransition transitionTable1[][1 + E3] = {
	/* timeout,                          E1,                            E2              E3*/
	{ { handlerTimeout, S0 },{ handleS0E1, S1 },{ handleS0E2, S0 },{ handlerE3Failure, S1 } }, //S0
	{ { 0,				S0 },{ handleS1E1, S0 },{ 0,		  S0 },{ handlerE3Failure, S1 } }, //S2
};


OFSM_DECLARE_FSM(0, transitionTable1, 1 + E3, handleInit, NULL);
OFSM_DECLARE_GROUP_1(0, 3, 0);
OFSM_DECLARE_1(0);

static void inline _ofsm_fsm_process_event(OFSM *fsm, uint8_t groupIndex, uint8_t fsmIndex, OFSMEventData *e, unsigned long currentTime, uint8_t timeFlags) {
	//TBI:
	OFSMTransition *t;
	uint8_t prevState;
	uint8_t oldFlags;
	unsigned long oldWakeupTime;
	uint8_t wakeupGTcurrent;
	OFSMState fsmState;
	unsigned long mostCurrentTime;
	uint8_t mostCurrentTimeFlags;

#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	long delay = -1;
	char overrideStateFlag[] = { '\0', '\0' };
#endif
	
	if (e->eventCode >= fsm->transitionTableEventCount) {
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
		ofsm_debug_printf("G(%i)FSM(%i): Unexpected event %i ignored.\n", groupIndex, fsmIndex, e->eventCode);
#endif
		return;
	}
	
	//check if wake time has been reached, wake up immediately if not timeout event, ignore non-handled   events.
	t = _OFSM_GET_TRANSTION(fsm, e->eventCode);
	wakeupGTcurrent = _OFSM_TIME_A_GT_B(fsm->wakeupTime, (fsm->flags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW), currentTime, (timeFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW));
	if (!t->eventHandler || (0 == e->eventCode && ((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) || wakeupGTcurrent))) {
		if (!t->eventHandler) {
			fsm->flags |= _OFSM_FLAG_INFINITE_SLEEP;
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 2
			ofsm_debug_printf("G(%i)FSM(%i): Handler is not specified. Assuming infinite sleep.\n", groupIndex, fsmIndex);
		}
		else if (fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) {
			ofsm_debug_printf("G(%i)FSM(%i): State Machine is in infinite sleep.\n", groupIndex, fsmIndex);
		}
		else if (wakeupGTcurrent) {
			ofsm_debug_printf("G(%i)FSM(%i): State Machine is asleep. Wakeup is scheduled in %i ticks.\n", groupIndex, fsmIndex, fsm->wakeupTime - currentTime);
#endif
		}
		return;
	}
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
	ofsm_debug_printf("G(%i)FSM(%i): State: %i, processing eventCode %i eventData %i(0x%8X)...\n", groupIndex, fsmIndex, fsm->currentState, e->eventCode, e->eventData, e->eventData);
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

	OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
		mostCurrentTime = ofsm_get_time();
		mostCurrentTimeFlags = _ofsmFlags | _OFSM_FLAG_OFSM_TIMER_OVERFLOW;
	}

	fsmState.timeLeftBeforeTimeout = 0; //TBI:
	(t->eventHandler)(&fsmState);
	
	//check if error was reported, restore original FSM state
	if (fsm->flags & _OFSM_FLAG_FSM_PREVENT_TRANSITION) {
		fsm->flags = oldFlags;
		fsm->wakeupTime = oldWakeupTime;
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		ofsm_debug_printf("G(%i)FSM(%i): Handler requested no transition. FSM state was restored.\n", groupIndex, fsmIndex);
#endif
		return;
	}
	
	//make a transition
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	prevState = fsm->currentState;
#endif
	if (!(fsm->flags & _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE)) {
		fsm->currentState = t->newState;
	}
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	else {
		overrideStateFlag[0] = 'O';
	}
#endif
	
	//check transition delay, assume infinite sleep if new state doesn't accept Timeout Event
	t = _OFSM_GET_TRANSTION(fsm, 0);
	if (!t->eventHandler) {
		fsm->flags |= _OFSM_FLAG_INFINITE_SLEEP; //set infinite sleep
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		delay = -1;
#endif
	}
	else if (0 == fsm->wakeupTime) {
		fsm->wakeupTime = currentTime + OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY;
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		delay = OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY;
#endif
	}
	else {
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
		delay = fsm->wakeupTime;
#endif
		if (fsm->flags & _OFSM_FLAG_FSM_DELAY_RELATIVE_TO_MOST_CURRENT_TIME) {
			currentTime = mostCurrentTime;
			timeFlags = mostCurrentTimeFlags;
		}

		fsm->wakeupTime += currentTime;
		if (fsm->wakeupTime < currentTime) {
			fsm->flags |= _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW;
		}
	}
#ifdef OFSM_CONFIG_DEBUG_LEVEL_OFSM > 1
	ofsm_debug_printf("G(%i)FSM(%i): Transitioning from state %i ==> %s%i. Transition delay: %i\n", groupIndex, fsmIndex, prevState, overrideStateFlag, fsm->currentState, delay);
#endif

}

void setup() {
	OFSM_SETUP();
}

void loop() {

	OFSM_LOOP();
}
