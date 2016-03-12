#define TEST

#ifdef TEST
/*configure script (unit test) mode simulation*/
#   define OFSM_CONFIG_SIMULATION                            /* turn on simulation mode */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE                /* run main loop synchronously */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 3  /* 0 - wakeup when event queued; 1 - wakeup on timeout from heartbeat; 2 - manual wakeup (use command 'wakeup'); 3- manual, one event per step*/
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 0              /* turn off sketch debug print */
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 0         /* turn off ofsm debug print */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS 0 /* don't sleep between script commands */
#   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 1     /* make 1 tick as default delay between transitions */
#else 
/* When F_CPU is defined, we can be confident that sketch is being compiled to be flushed into MCU, otherwise we assume SIMULATION mode */
#   ifndef F_CPU
#       define OFSM_CONFIG_SIMULATION                        /* turn on simulation mode */  
#   endif /*F_CPU*/
/*configure interactive simulation*/
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 4              /* turn on sketch debug print up to level 4 */
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 4         /* turn on ofsm debug print up to level 4*/
#   define OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP  /* add time stamps for debug print output */
#   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 5      /* make 5 ticks (5 seconds in interactive mode) as default delay between transitions */
#endif

#define EVENT_QUEUE_SIZE 3 /*event queue size*/

#include "../ofsm.h"

/*define events*/
enum Events {Timeout = 0, NormalTransition, PreventTransition, InfiniteDelay};
enum States {S0 = 0, S1};

/* Handlers declaration */
void DummyHandler(OFSMState *fsms);
void PreventTransitionHandler(OFSMState *fsms);
void InifiniteDelayHandler(OFSMState *fsms);

/* OFSM configuration */
OFSMTransition transitionTable[][1 + InfiniteDelay] = {
	/* timeout,               NormalTransition,    PreventTransition,               InifiniteDelay*/
	{ { DummyHandler, S1   },{ DummyHandler, S1 },{ PreventTransitionHandler, S1 },{ InifiniteDelayHandler, S1 } }, //S0
	{ { NULL,	      NULL },{ DummyHandler, S0 },{ PreventTransitionHandler, S0 },{ InifiniteDelayHandler, S0 } }, //S1
};

OFSM_DECLARE_FSM(0, transitionTable, 1 + InfiniteDelay, handleInit, NULL);
OFSM_DECLARE_GROUP_1(0, 3, 0);
OFSM_DECLARE_1(0);


/* Setup */
void setup() {
	OFSM_SETUP();
}

void loop() {
	OFSM_LOOP();
}


/* Handler implementation */
void DummyHandler(OFSMState *fsms) {
}

void PreventTransitionHandler(OFSMState *fsms) {
    fsm_prevent_transition(fsms);
}

void InifiniteDelayHandler(OFSMState *fsms) {
    fsm_set_inifinite_delay(fsms);
}

