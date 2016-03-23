/*IMPORTANT! Include this file once into any c/cpp file within the project*/
#ifndef __OFSM_IMPL_H_
#define __OFSM_IMPL_H_

/*---------------------------------------
Global variables
-----------------------------------------*/

OFSMGroup**				_ofsmGroups;
uint8_t                 _ofsmGroupCount;
volatile uint8_t        _ofsmFlags;
volatile _OFSM_TIME_DATA_TYPE  _ofsmWakeupTime;
volatile _OFSM_TIME_DATA_TYPE  _ofsmTime;

/*--------------------------------------
Common (simulation and non-simulation code)
----------------------------------------*/

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
            _ofsm_debug_printf(4,  "F(%i)G(%i): State Machine is asleep. Wakeup is scheduled in %lu ticks.\n", fsmIndex, groupIndex, (long unsigned int)(fsm->wakeupTime - currentTime));
#endif
        }
        return;
    }

#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
    _ofsm_debug_printf(2,  "F(%i)G(%i): State: %i. Processing eventCode %i eventData %i(0x%08X)...\n", fsmIndex, groupIndex, fsm->currentState, e->eventCode, e->eventData, (int)e->eventData);
#else
    _ofsm_debug_printf(2,  "F(%i)G(%i): State: %i. Processing eventCode %i...\n", fsmIndex, groupIndex, fsm->currentState, e->eventCode);
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
    else if (!(fsm->flags & _OFSM_FLAG_FSM_HANDLER_SET_TRANSITION_DELAY)) {
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
    _ofsm_debug_printf(2,  "F(%i)G(%i): Transitioning from state %i ==> %c%i. Transition delay: %ld\n", fsmIndex, groupIndex,  prevState, overridenState, fsm->currentState, delay);
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

void _ofsm_setup() {

    /*In simulation mode, we need to allow to reset OFSM to initial state, so that intermediate test case can start clean.*/
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
            fsm->currentState = 0;
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
    OFSMEventData e;
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
        _ofsm_debug_printf(4,  "O: Entering sleep... Wakeup Time %ld.\n", _ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP ? -1 : (long int)_ofsmWakeupTime);
        OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC();

        _ofsm_debug_printf(4,  "O: Waked up.\n");
    } while (1);
#else
#	if OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE > 2
        break; /*process single event per step*/
#	endif
    } while (_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED);
    _ofsm_debug_printf(4, "O: Step through OFSM is complete.\n");
#endif

}/*_ofsm_start*/

void _ofsm_queue_group_event(uint8_t groupIndex, OFSMGroup *group, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) {
    uint8_t copyNextEventIndex;
    OFSMEventData *event;
#ifdef OFSM_CONFIG_SIMULATION
    uint8_t debugFlags = 0x1; //set buffer overflow
#endif
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
        copyNextEventIndex = group->nextEventIndex;

        if (!(group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
#ifdef OFSM_CONFIG_SIMULATION
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
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
                event->eventData = eventData;
#endif
#ifdef OFSM_CONFIG_SIMULATION
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
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
                event->eventData = eventData;
#endif

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
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
        _ofsm_debug_printf(1,  "G(%i): Buffer overflow. eventCode %i eventData %i(0x%08X) dropped.\n", groupIndex, eventCode, eventData, eventData);
#else
        _ofsm_debug_printf(1,  "G(%i): Buffer overflow. eventCode %i dropped.\n", groupIndex, eventCode );
#endif
    }
    else {
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
        _ofsm_debug_printf(3,  "G(%i): Queued eventCode %i eventData %i(0x%08X) (Updated %i, Set buffer overflow %i).\n", groupIndex, eventCode, eventData, eventData, (debugFlags & 0x2) > 0, (group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW) > 0);
#else
        _ofsm_debug_printf(3,  "G(%i): Queued eventCode %i (Updated %i, Set buffer overflow %i).\n", groupIndex, eventCode, (debugFlags & 0x2) > 0, (group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW) > 0);
#endif

        _ofsm_debug_printf(4,  "G(%i): currentEventIndex %i, nextEventIndex %i.\n", groupIndex, group->currentEventIndex, group->nextEventIndex);
    }
#endif
}/*_ofsm_queue_group_event*/

void ofsm_queue_group_event(uint8_t groupIndex, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData)
{
#ifdef OFSM_CONFIG_SIMULATION
    if (groupIndex >= _ofsmGroupCount) {
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
        _ofsm_debug_printf(1,  "O: Invalid Group Index %i!!! Dropped eventCode %i eventData %i(0x%08X). \n", groupIndex, eventCode, eventData, eventData);
#else
        _ofsm_debug_printf(1,  "O: Invalid Group Index %i!!! Dropped eventCode %i. \n", groupIndex, eventCode);
#endif
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

/*--------------------------------------
SIMULATION specific code
----------------------------------------*/

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
        , (long unsigned int)r->ofsmTime
        , (r->ofsmTimerOverflow ? '!' : '.')
        , (long unsigned int)r->ofsmScheduledWakeupTime
        , (r->ofsmScheduledTimeOverflow ? '!' : '.')
        , (long unsigned int)r->fsmScheduledWakeupTime
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

#ifdef _OFSM_IMPL_SIMULATION_ENTER_SLEEP
void _ofsm_simulation_enter_sleep() {
        std::unique_lock<std::mutex> lk(cvm);

        cv.wait(lk);
        _ofsmFlags &= ~_OFSM_FLAG_OFSM_EVENT_QUEUED;
        lk.unlock();
}
#endif /* _OFSM_IMPL_SIMULATION_ENTER_SLEEP */

#ifdef _OFSM_IMPL_SIMULATION_WAKEUP
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
#endif /* _OFSM_IMPL_SIMULATION_WAKEUP */

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

int lineNumber = 0;
std::ifstream fileStream;

int _ofsm_simulation_event_generator(const char *fileName) {
    std::string line;
    std::string token;
    int exitCode = 0;
    std::string assertCompareString;

    //in case of reset we don't need to open file again
    if (fileName && !lineNumber) {
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
        std::string t;
        std::deque<std::string> tokens;

        //strip comments
        std::size_t commentPos = line.find("//");
        if (commentPos != std::string::npos) {
            line.replace(commentPos, line.length(), "");
        }

        //skip empty lines
        if (!line.length()) {
            continue;
        }

        //get assert string or print string (preserving case)
        t = line;
        toLower(t);
        assertCompareString = "";
        if (0 != t.find("p")) {
            /*it is not print, get assert*/
            std::size_t assertComparePos = line.find("=");
            if (assertComparePos != std::string::npos) {
                assertCompareString = line.substr(assertComparePos + 1);
                trim(assertCompareString);
                line.replace(assertComparePos, line.length(), "");
            }
        }
        //handle p[rint][,<string to be printed>] ...... command (preserver case)
        else {
            std::size_t commaPos = line.find(",");
            if (commaPos != std::string::npos) {
                assertCompareString = line.substr(commaPos + 1);
            }
            std::cout << assertCompareString << std::endl;
            continue;
        }

        //convert commands to lower case
        toLower(line);
        std::stringstream strStream(line);

        //parse by tokens
        while (std::getline(strStream, token, ',')) {
            if (token.find_first_not_of(' ') >= 0) {
                tokens.push_back(trim(token));
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
            _ofsm_debug_printf(4,  "G: Entering sleep for %lu milliseconds...\n", (long unsigned int)sleepPeriod);
            _ofsm_simulation_sleep(sleepPeriod);
            continue;
        }
        break;
//        case 'p':			//p[rint]
//        break;
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

#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
        _ofsm_debug_printf(3, "Waiting for %i milliseconds for all threads to exit...\n", OFSM_CONFIG_SIMULATION_TICK_MS);
        _ofsm_simulation_sleep(OFSM_CONFIG_SIMULATION_TICK_MS + 10); /*let heartbeat provider thread to exit before exiting*/
#endif

        if (retCode < 0) {
            _ofsm_debug_printf(3, "Reseting...\n");
        }

    } while (retCode < 0);

    return retCode;
}

#endif /* OFSM_CONFIG_SIMULATION */

/*--------------------------------------
MCU specific code
----------------------------------------*/

#ifndef OFSM_CONFIG_SIMULATION
static inline void _ofsm_setup_hardware() {
    //TBI:
}/*_ofsm_setup_hardware*/

static inline void _ofsm_wakeup() {
}

static inline void _ofsm_enter_sleep() {
    
    cli(); /*disable interrupts*/
    while(!_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED) {
        /* determine if we need to use watch dog timer */
        unsigned long sleepPeriodTick = _ofsmWakeupTime - _ofsmTime;
    #if OFSM_CONFIG_TICK_US > 1000
        unsigned long sleepPeriodMs = sleepPeriodTick * ((unsigned long)(OFSM_CONFIG_TICK_US/1000));
    #else
        unsigned long sleepPeriodMs = (unsigned long)(sleepPeriodTick * OFSM_CONFIG_TICK_US)/1000;
    #endif
        if(_ofsmFlags & _OFSM_FLAG_ALLOW_DEEP_SLEEP && sleepPeriodMs >= 16) {
            _ofsm_enter_deep_sleep(sleepPeriodMs);
        } else {
            _ofsm_enter_idle_sleep();
        }
        //waked up continues here

    }
    //disable sleep
    sti();
}

static inline void _ofsm_enter_idle_sleep() {

}

extern volatile unsigned long timer0_millis; /*Arduino timer0 variable*/
volatile uint16_t watchDogDelayMs;

static inline void _ofsm_wdt_vector() {
    /*try to update Arduino timer variables, to keep time relatively correct.
    if deep sleep was interrupted by external source, 
    then time will not be updated as we don't know how big of a delay was between getting to sleep and external interrupt
    */
    if(_ofsm_flags &= ~_OFSM_FLAG_OFSM_IN_DEEP_SLEEP) {
        timer0_millis += watchDogDelayMs;
    }
}

ISR(WDT_vect) {
    _ofsm_wdt_vector();
}

static inline void _ofsm_enter_deep_sleep(unsigned long sleepPeriodMs) {
    uint8_t wdtMask = 0;

    if(sleepPeriodMs > 8000) {
		wdtMask = B1001 /*8 sec*/
	} else {
		uint16_t probe = 16;
		for(wdtMask = 1; wdtMask < B1001 && probe < sleepPeriodMs; wdtMask++) {
			probe  = probe << wdtMask;
		}
		wdtMask--;
        watchDogDelayMs = prob >> 1;
	}
    
    power_adc_disable();
    power_spi_disable();
    power_timer0_disable();
    power_timer1_disable();
    power_timer2_disable();
    power_twi_disable();
    
    MCUSR = 0;     /*clear various "reset" flags*/
    sbi(WDTCSR, WDCE); /* allow changes */
    sbi(WDTCSR, WDE); /* disable reset */
    WDTCSR &= (B11111000 | wdtMask); /* set interval */
    wdt_reset();  /* prepare */

    _ofsm_flags |= _OFSM_FLAG_OFSM_IN_DEEP_SLEEP; /*set flag indicating deep sleep. It must be removed on wakeup. Event wakeup happened due to external interrupt! */
    set_sleep_mode (SLEEP_MODE_PWR_DOWN);  

    sleep_enable();

    /* turn off brown-out */
    MCUCR = (1 << BODS) | (1 << BODSE);
    MCUCR = (1 << BODS);

    sti();
    sleep_cpu();
    /*------------------------------------------*/
    /*waked up here*/
    cli();
    cbi(WDTCSR, WDIE); /*disable watch dog interrupt*/
    wdt_disable();
    _ofsm_flags &= ~_OFSM_FLAG_OFSM_IN_DEEP_SLEEP;
    sti();

    /*restore hardware*/
    sleep_disable();

    power_twi_enable();
    power_timer2_enable();
    power_timer1_enable();
    power_timer0_enable();
    power_spi_enable();
    power_adc_enable();
}

#endif /* not OFSM_CONFIG_SIMULATION */

#endif /* __OFSM_IMPL_H_ */