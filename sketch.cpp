#define OFSM_CONFIG_DEBUG_LEVEL 4
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
	/* timeout,               E1,               E2                  E3*/
	{ { handlerTimeout, S0 },{ handleS0E1, S1 },{ handleS0E2, S0 },{ handlerE3Failure, S1 } }, //S0
	{ { 0,				S0 },{ handleS1E1, S0 },{ 0,		  S0 },{ handlerE3Failure, S1 } }, //S2
};


OFSM_DECLARE_FSM(0, transitionTable1, 1 + E3, handleInit, NULL);
OFSM_DECLARE_GROUP_1(0, 3, 0);
OFSM_DECLARE_1(0);

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

struct OFSMSimulationStatusReport {
    uint8_t grpIndex;
    uint8_t fsmIndex;
    //OFSM status
    bool ofsmInfiniteSleep;
    bool ofsmTimerOverflow;
    bool ofsmScheduledTimeOverflow;
    unsigned long ofsmScheduledWakeupTime;
    //Group status
    bool grpEventBufferOverflow;
    uint8_t grpPendingEventCount;
    //FSM status
	bool fsmInfiniteSleep;
	bool fsmTransitionPrevented;
    bool fsmTransitionStateOverriden;
    bool fsmScheduledTimeOverflow;
    unsigned long fsmScheduledWakeupTime;
    uint8_t fsmCurrentState;
};
#ifdef _OFSM_IMPL_SIMULATION_STATUS_REPORT_PRINTER
void _ofsm_simulation_status_report_printer(OFSMSimulationStatusReport *r) {
	char buf[80];
	sprintf_s(buf, (sizeof(buf) / sizeof(*buf)), "-O[%c%c%c]-G(%i)[%c,%03d]-F(%i)[%c%c%c%c]-S(%i)-T[O:%010lu,F:%010lu]" 
		//OFSM
		, (r->ofsmInfiniteSleep ? 'I' : 'i')
		, (r->ofsmTimerOverflow ? 'T' : 't')
		, (r->ofsmScheduledTimeOverflow ? 'S' : 's')
		//Group
		, r->grpIndex
		, (r->grpEventBufferOverflow ? 'B' :  'b')
		, (r->grpPendingEventCount)
		//Fsm
		, r->fsmIndex
		, (r->fsmInfiniteSleep ? 'I' : 'i')
		, (r->fsmTransitionPrevented ? 'P' : 'p')
		, (r->fsmTransitionStateOverriden ? 'O' : 'o')
		, (r->fsmScheduledTimeOverflow ? 'S' : 's')
		//Current State
		, r->fsmCurrentState
        //wakeup-time
   		, r->ofsmScheduledWakeupTime
		, r->fsmScheduledWakeupTime
	);
	std::cout << buf << std::endl;
}
#endif

void _ofsm_simulation_create_status_report(OFSMSimulationStatusReport *r, uint8_t groupIndex, uint8_t fsmIndex) {
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
        r->grpIndex = groupIndex;
        r->fsmIndex = fsmIndex;
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
		r->grpEventBufferOverflow = (bool)((grp->flags & _OFSM_FLAG_INFINITE_SLEEP) > 0);
		if (grp->nextEventIndex > grp->currentEventIndex) {
			r->grpPendingEventCount = grp->nextEventIndex - grp->currentEventIndex;
		}
		else {
			if (r->grpEventBufferOverflow && (grp->currentEventIndex == grp->nextEventIndex)) {
				if(grp->currentEventIndex == grp->nextEventIndex) {
                    r->grpPendingEventCount = grp->eventQueueSize;
                } else {
                    r->grpPendingEventCount = grp->eventQueueSize - (grp->currentEventIndex - grp->nextEventIndex);
                }
			}
			else {
				r->grpPendingEventCount = 0;
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
void _ofsm_simulation_event_generator() {
	std::string line;
	while (!std::cin.eof())
	{

		//read line from stdin
		std::getline(std::cin, line);
		trim(line);
		std::transform(line.begin(), line.end(), line.begin(), ::tolower);

		//strip comments
		std::size_t commentPos = line.find("//");
		if (commentPos != std::string::npos) {
			line.replace(commentPos, line.length(), "");
		}

		//prepare for parsing
		std::stringstream strStream(line);
		std::string t;
		std::deque<std::string> tokens;

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

		//if line starts with digit, assume shorthand of 'queue' command: eventCode[,eventData[,groupIndex]]
		std::string digits = "0123456789";
		if (std::string::npos != digits.find(tokens[0])) {
			tokens.push_front("queue");
		}

		//parse command
		t = tokens[0];
        uint8_t tCount = (uint8_t)tokens.size();

		if (0 == t.find("e")) {				//e[xit]
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM> 3
			ofsm_debug_printf("EG: Exiting...\n");
#endif
			return;
		}
		else if (0 == t.find("s")) {		//s[leep][,sleepPeriod]
			unsigned long sleepPeriod = 0;
			if (tCount > 1) {
				sleepPeriod = atoi(tokens[1].c_str());
			}
			if (0 == sleepPeriod) {
				sleepPeriod = 1000;
			}
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 3
			ofsm_debug_printf("EG: Entering sleep for %i milliseconds...\n", sleepPeriod);
#endif
			_ofsm_simulation_sleep(sleepPeriod);
			continue;

		}
		else if (0 == t.find("q")) {		//q[ueue][,[mods],eventCode[,eventData[,groupIndex]]]
            uint8_t eventCode = 0;
            uint8_t eventData = 0;
            uint8_t eventCodeIndex = 1;
            uint8_t groupIndex = 0;
            bool isGlobal = false;
            bool forceNew = false;
            if(tCount > 1) {
                t = tokens[1];
                isGlobal = std::string::npos != t.find("g");
                forceNew = std::string::npos != t.find("f");
                if(isGlobal || forceNew) {
                    eventCodeIndex = 2;
                }
            }
            //get eventCode
            if(tCount > eventCodeIndex) {
                t = tokens[eventCodeIndex];
                eventCodeIndex++;
                eventCode = atoi(t.c_str());
            }
            //get eventData
            if(tCount > eventCodeIndex) {
                t = tokens[eventCodeIndex];
                eventCodeIndex++;
                eventData = atoi(t.c_str());
            }
            //get groupIndex
            if(tCount > eventCodeIndex) {
                t = tokens[eventCodeIndex];
                eventCodeIndex++;
                groupIndex = atoi(t.c_str());
            }
            //queue event
            if(isGlobal) {
                ofsm_queue_global_event(forceNew, eventCode, eventData);
            } else {
                ofsm_queue_group_event(groupIndex, forceNew, eventCode, eventData); 
            }
		}
		else if (0 == t.find("h")) { // h[eartbeat][,currentTime]
            unsigned long currentTime;
            if(tCount > 1) {
                t = tokens[1];
                unsigned long time = atoi(t.c_str());
            } else {
                OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
                    currentTime = _ofsmTime;
                }
                currentTime++;
            }
            ofsm_heartbeat(currentTime);
		}
		else if (0 == t.find("r")) {		//r[eport][,groupIndex[,fsmIndex]]
            OFSMSimulationStatusReport report;
            uint8_t groupIndex = 0;
            uint8_t fsmIndex = 0;
            //get group index
            if(tCount > 1) {
                t = tokens[1];
                groupIndex = atoi(t.c_str());
            }
            //get fsm index
            if(tCount > 2) {
                t = tokens[2];
                fsmIndex = atoi(t.c_str());
            }
            _ofsm_simulation_create_status_report(&report, groupIndex, fsmIndex);

			OFSM_CONFIG_CUSTOM_PC_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER(&report);
		}
		else {
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
				ofsm_debug_printf("EG: Invalid command: %s. Ignored.\n", line.c_str());
#endif
				continue;
			}
		}
}

void setup() {
	OFSM_SETUP();
}

void loop() {

	OFSM_LOOP();
}
