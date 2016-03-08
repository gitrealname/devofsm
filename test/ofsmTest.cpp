#ifdef TEST
/*configure script (unit test) mode simulation*/
#   define OFSM_CONFIG_SIMULATION                            /* turn on simulation mode */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE                /* run main loop synchronously */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 2  /* 0 - wakeup when event queued; 1 - wakeup on timeout from heartbeat; 2 - manual wakeup (use command 'wakeup') */
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 0              /* turn off sketch debug print */
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 0         /* turn off ofsm debug print */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS 0 /* don't sleep between script commands */
#   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 1     /* make 1 tick as default delay between transitions */
#else 
/*configure interactive simulation*/
#   define OFSM_CONFIG_SIMULATION                            /* turn on simulation mode */             
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 4              /* turn on sketch debug print up to level 4 */
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 4         /* turn on ofsm debug print up to level 4*/
#   define OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP  /* add time stamps for debug print output */
#   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 5      /* make 5 ticks (5 seconds in interactive mode) as default delay between transitions */
#endif

#include "../ofsm.h"

/*define events*/
enum Events {Timeout, };
enum States {};

/*define transition table*/
//OFSMTransition transitionTable[][1 + E3] = {
//	/* timeout,               E1,               E2                  E3*/
//	{ { handlerTimeout, S0 },{ handleS0E1, S1 },{ handleS0E2, S0 },{ handlerE3Failure, S1 } }, //S0
//	{ { 0,				S0 },{ handleS1E1, S0 },{ 0,		  S0 },{ handlerE3Failure, S1 } }, //S2
//};
//
//OFSM_DECLARE_FSM(0, transitionTable, 1 + E3, handleInit, NULL);
//OFSM_DECLARE_GROUP_1(0, 3, 0);
//OFSM_DECLARE_1(0);


void setup() {
	OFSM_SETUP();
}

void loop() {

	OFSM_LOOP();
}


