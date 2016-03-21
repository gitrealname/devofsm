#ifdef TEST
/*configure script (unit test) mode simulation*/
#   define OFSM_CONFIG_SIMULATION                            /* turn on simulation mode */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE                /* run main loop synchronously */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 3  /* 0 - wakeup when event queued; 1 - wakeup on timeout from heartbeat; 2 - manual wakeup (use command 'wakeup'); 3- manual, one event per step*/
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 0              /* turn off sketch debug print */
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 0         /* turn off ofsm debug print */
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS 0 /* don't sleep between script commands */
#   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0     /* make 0 tick as default delay between transitions */
#else 
/* When F_CPU is defined, we can be confident that sketch is being compiled to be flushed into MCU, otherwise we assume SIMULATION mode */
#   ifndef F_CPU
#       define OFSM_CONFIG_SIMULATION                        /* turn on simulation mode */  
#   endif /*F_CPU*/
/*configure interactive simulation*/
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 4              /* turn on sketch debug print up to level 4 */
#   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 0         /* turn on ofsm debug print up to level 4*/
#   define OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP  /* add time stamps for debug print output */
#   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0      /* make 0 ticks as default delay between transitions */
#endif

#define EVENT_QUEUE_SIZE 1 /*event queue size*/

#include <ofsm.h>

/*define events*/
enum Events {Timeout = 0};
enum States {On = 0, Off};
enum FsmId	{DefaultFsm = 0};
enum FsmGrpId {MainGroup = 0};

/* Handlers declaration */
void OnHandler(OFSMState *fsms);
void OffHandler(OFSMState *fsms);

/* OFSM configuration */
OFSMTransition transitionTable[][1 + Timeout] = {
    /* Timeout*/
    { { OnHandler,  Off } }, /*On State*/
    { { OffHandler,	On  } }  /*Off State*/
};

OFSM_DECLARE_FSM(DefaultFsm, transitionTable, 1 + Timeout, NULL, NULL);
OFSM_DECLARE_GROUP_1(MainGroup, EVENT_QUEUE_SIZE, DefaultFsm);
OFSM_DECLARE_1(MainGroup);

#define ledPin 13
#define ticksOn  2 /*number of ticks to be in On state*/
#define ticksOff 1 /*number of ticks to be in Off state*/

/* Setup */
void setup() {
    /*prevent MCU specific code during simulation*/
#if OFSM_MCU_BLOCK
    /* initialize digital ledPin as an output.*/
    pinMode(ledPin, OUTPUT);
#endif 
    OFSM_SETUP();
}

void loop() {
    ofsm_queue_global_event(false, 0, 0); /*queue timeout to wakeup FSM*/
    OFSM_LOOP();
}


/* Handler implementation */
void OnHandler(OFSMState *fsms) {
	ofsm_debug_printf(1, "Turning Led ON for %i ticks.\n", ticksOn);
	fsm_set_transition_delay(fsms, ticksOn);
#if OFSM_MCU_BLOCK
	digitalWrite(led, HIGH);
#endif
}

void OffHandler(OFSMState *fsms) {
	ofsm_debug_printf(1, "Turning Led OFF for %i ticks.\n", ticksOff);
	fsm_set_transition_delay(fsms, ticksOff);
#if OFSM_MCU_BLOCK
	digitalWrite(led, LOW);
#endif
}

