#define OFSM_CONFIG_DEBUG_LEVEL 2
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
		std::vector<std::string> tokens;

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


		int eventCode = 0;
		int eventData = 0;

		//parse command
		if (0 == t.find("e")) {				//e[xit]
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM> 3
			ofsm_debug_printf("EG: Exiting...\n");
#endif
			return;
		}
		else if (0 == t.find("s")) {		//s[leep]
			eventData = 0;
			if (tokens.size() > 1) {
				eventData = atoi(tokens[1].c_str());
			}
			if (0 == eventData) {
				eventData = 1000;
			}
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 3
			ofsm_debug_printf("EG: Entering sleep for %i milliseconds...\n", eventData);
#endif
			_ofsm_simulation_sleep(eventData);
			continue;

		}
		else if (0 == t.find("q")) {		//q[ueue], [eventCode[, eventData, [groupIndex | g]]]

		}
		else if (0 == t.find("hb") || 0 == t.find("heartbeat")) { //heartbeat

		}
		else if (0 == t.find("r")) {		//r[eport]

		}
		else {                              //start with digit, assume 'queue'
			int eventCode = atoi(t.c_str());
			if (eventCode == 0 && t != "0") { //invalid input
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 0
				ofsm_debug_printf("EG: Invalid command: %s. Ignored.\n", line.c_str());
#endif
				continue;
			}
		}



		std::string eventCodeStr;
		//int eventCode = 0;
		//int eventData = 0;
		std::string targetStr = "0";
		uint8_t eventTargetGroup = 0;
		bool forceNewEvent = false;

		//get components
		switch (tokens.size()) {
		case 3:
			targetStr = tokens[2];
			eventTargetGroup = atoi(targetStr.c_str());
		case 2:
			eventData = atoi(tokens[1].c_str());
		case 1:
			t = tokens[0];
			if (std::string::npos != t.find("exit")) {
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 3
				ofsm_debug_printf("EG: Exiting...\n");
#endif
				return;
			}
			if (std::string::npos != t.find("sleep")) {
				if (tokens.size() == 1) {
					eventData = 1000;
				}
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 3
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
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 3 
			ofsm_debug_printf("EG: Queuing eventCode: %i eventData: %i GLOBAL (force new event: %i)...\n", eventCode, eventData, forceNewEvent);
#endif
			ofsm_queue_global_event(forceNewEvent, eventCode, eventData);
		}
		else {
#if OFSM_CONFIG_DEBUG_LEVEL_OFSM > 3
			ofsm_debug_printf("EG: Queuing eventCode: %i eventData: %i; group %i (force new event: %i)...\n", eventCode, eventData, eventTargetGroup, forceNewEvent);
#endif
			ofsm_queue_group_event(eventTargetGroup, forceNewEvent, eventCode, eventData);
		}
#if OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS > 0 
		_ofsm_simulation_sleep(OFSM_CONFIG_PC_SIMULATION_SLEEP_BETWEEN_EVENTS_MS);
#endif
	}
}

void setup() {
	OFSM_SETUP();
}

void loop() {

	OFSM_LOOP();
}
