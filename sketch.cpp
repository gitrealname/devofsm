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
void handleInit(OFSM *fsm) {
	ofsm_debug_printf("H(%i-%i): handleInit\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handleS0E1(OFSM *fsm) {
	ofsm_debug_printf("H(%i-%i): handleS0E1\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handleS0E2(OFSM *fsm) {
	ofsm_debug_printf("H(%i-%i): handleS0E2\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
	fsm_set_transition_delay(fsm, 10);
}

void handleS1E1(OFSM *fsm) {
	ofsm_debug_printf("H(%i-%i): handleS1E1\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handlerTimeout(OFSM *fsm) {
	ofsm_debug_printf("H(%i-%i): handleTimeout\n", fsm_get_state(fsm), fsm_get_event_code(fsm));
}

void handlerE3Failure(OFSM *fsm) {
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

static void inline _ofsm_fsm_process_event(OFSM *fsm, uint8_t groupIndex, uint8_t fsmIndex, OFSMEventData *e, unsigned long currentTime) {
	//TBI:
}

void setup() {
	OFSM_SETUP();
}

void loop() {

	OFSM_LOOP();
}
