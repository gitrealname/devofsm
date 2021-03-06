//GCC build cmd: g++ -Wall -std=c++11 -fexceptions -std=c++11 -g  -o sketch sketch.cpp

#define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 5

#define OFSM_CONFIG_SIMULATION

//#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE
//#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 2
//#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS 500

#define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 4
#define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 4

#define OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP

#include "ofsm.h"


/*--------------------------------
Handlers
--------------------------------*/
void handleInit() {
	ofsm_debug_printf(1, "H(%i-%i): handleInit\n", fsm_get_state(), fsm_get_event_code());
}

void handleS0E1() {
	ofsm_debug_printf(1, "H(%i-%i): handleS0E1\n", fsm_get_state(), fsm_get_event_code());
}

void handleS0E2() {
	ofsm_debug_printf(1, "H(%i-%i): handleS0E2\n", fsm_get_state(), fsm_get_event_code());
	fsm_set_transition_delay(10);
}

void handleS1E1() {
	ofsm_debug_printf(1, "H(%i-%i): handleS1E1\n", fsm_get_state(), fsm_get_event_code());
}

void handlerTimeout() {
	ofsm_debug_printf(1, "H(%i-%i): handleTimeout\n", fsm_get_state(), fsm_get_event_code());
}

void handlerE3Failure() {
	ofsm_debug_printf(1, "H(%i-%i): handlerE3Failure\n", fsm_get_state(), fsm_get_event_code());
	fsm_prevent_transition();
}

enum Event { Timeout, E1, E2, E3 };
enum State { S0, S1 };

/*AAA*/
//_OFSM_DECLARE_FSM(0, 2, 4, NULL, NULL);
//_OFSM_DECLARE_FSM(0, 2, 4, NULL, NULL);
//	OFSM_DECLARE_FSM_TRANSITION(0, S0, Timeout, handlerTimeout, S0);
//	OFSM_DECLARE_FSM_TRANSITION(0, S0, Timeout, handlerTimeout, S0);

//_OFSM_DECLARE_FSM(0, 2, 4, NULL, NULL);

//OFSMTransition *a[2 * 4];
//uint8_t b = 4;
//OFSM _ofsm_declare_fsm_0 = { (OFSMTransition**)a, 4, 0, 0x1 };;
////	OFSM_DECLARE_FSM_TRANSITION(0, S0, Timeout, handlerTimeout, S0);
//OFSMTransition _ofsm_t_fsmId_stateId_Timeout = { handlerTimeout, S0 };
//{
//	a[b * S0 + Timeout] = &_ofsm_t_fsmId_stateId_Timeout;;
//}

char string_1[]  = "String 1";

char* string_table[]{
	string_1
};


/*-----------------------------------
Initialization
---------------------------------*/
OFSMTransition transitionTable1[][1 + E3] = {
	/* timeout,               E1,               E2                  E3*/
	{ { handlerTimeout, S0 },{ handleS0E1, S1 },{ handleS0E2, S0 },{ handlerE3Failure, S1 } }, //S0
	{ { 0,				S0 },{ handleS1E1, S0 },{ 0,		  S0 },{ handlerE3Failure, S1 } }, //S2
};


OFSM_DECLARE_FSM(0, transitionTable1, 1 + E3, handleInit, NULL, S0);
OFSM_DECLARE_GROUP_1(0, 3, 0);
OFSM_DECLARE_1(0);


void setup() {
	OFSM_SETUP();
}

void loop() {

	OFSM_LOOP();
}
