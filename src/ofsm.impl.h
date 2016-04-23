/*IMPORTANT! Include this file once into any c/cpp file within the project*/
#ifndef __OFSM_IMPL_H_
#define __OFSM_IMPL_H_

#ifndef OFSM_CONFIG_SIMULATION
/*#include <HardwareSerial.h> //may be included for debugging purposes*/
#	ifndef OFSM_CONFIG_CUSTOM_HEARTBEAT_PROVIDER
#		include <Arduino.h>
#	endif
#endif

#include "ofsm.decl.h"

/*---------------------------------------
Global variables
-----------------------------------------*/

OFSMGroup**				_ofsmGroups;
uint8_t                 _ofsmGroupCount;
OFSMState*				_ofsmCurrentFsmState;
volatile uint16_t       _ofsmFlags;
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

    if(e->eventCode == fsm->skipNextEventCode) {
        fsm->skipNextEventCode = (uint8_t)-1; /*reset skip event*/
        _ofsm_debug_printf(1,  "F(%i)G(%i): eventCode %i is set to be skipped for this FSM instance.\n", fsmIndex, groupIndex, e->eventCode);
        return;
    }

    ofsm_get_time(currentTime, timeFlags);

    //check if wake time has been reached, wake up immediately if not timeout event, ignore non-handled   events.
    t = _OFSM_GET_TRANSTION(fsm, e->eventCode);
    wakeupTimeGTcurrentTime = _OFSM_TIME_A_GT_B(fsm->wakeupTime, (fsm->flags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW), currentTime, (timeFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW));
    if (!t->eventHandler || (0 == e->eventCode && (((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) && !(_ofsmFlags & _OFSM_FLAG_OFSM_FIRST_ITERATION)) || wakeupTimeGTcurrentTime))) {
        if (!t->eventHandler) {
#ifdef OFSM_CONFIG_SIMULATION
            _ofsm_debug_printf(4,  "F(%i)G(%i): Handler is not specified, state %i event code %i. Event is ignored.\n", fsmIndex, groupIndex, fsm->currentState, e->eventCode);
        }
        else if ((fsm->flags & _OFSM_FLAG_INFINITE_SLEEP) && !(_ofsmFlags & _OFSM_FLAG_OFSM_FIRST_ITERATION)) {
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

    oldFlags = fsm->flags;
    oldWakeupTime = fsm->wakeupTime;
    fsm->wakeupTime = 0;
    fsm->flags &= ~_OFSM_FLAG_FSM_FLAG_ALL; //clear flags

    if(t->eventHandler != OFSM_NOP_HANDLER) {
        //call handler
        fsmState.fsm = fsm;
        fsmState.e = e;
        fsmState.groupIndex = groupIndex;
        fsmState.fsmIndex = fsmIndex;
        fsmState.timeLeftBeforeTimeout = 0;

        if(oldFlags & _OFSM_FLAG_INFINITE_SLEEP) {
            fsmState.timeLeftBeforeTimeout = (_OFSM_TIME_DATA_TYPE)-1;
        } else {
            if(wakeupTimeGTcurrentTime) {
                fsmState.timeLeftBeforeTimeout = oldWakeupTime - currentTime; /* time overflow will be accounted for*/
            }
        }
        _ofsmCurrentFsmState = &fsmState;
        (t->eventHandler)();

        //check if transition prevention was requested, restore original FSM state
        if (fsm->flags & _OFSM_FLAG_FSM_PREVENT_TRANSITION) {
            fsm->flags = oldFlags | _OFSM_FLAG_FSM_PREVENT_TRANSITION;
            fsm->wakeupTime = oldWakeupTime;
            _ofsm_debug_printf(3,  "F(%i)G(%i): Handler requested no transition. FSM state was restored.\n", fsmIndex, groupIndex);
            return;
        }
    }

    /*make a transition*/
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

    /*check transition delay, assume infinite sleep if new state doesn't accept Timeout Event*/
    t = _OFSM_GET_TRANSTION(fsm, 0);
    if (!t->eventHandler) {
        fsm->flags |= _OFSM_FLAG_INFINITE_SLEEP; /*set infinite sleep*/
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
			_ofsmCurrentFsmState = &fsmState;
            if (fsm->initHandler) {
                (fsm->initHandler)();
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

	/*start main loop*/
    do
    {
        OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
            _ofsmFlags |= _OFSM_FLAG_OFSM_IN_PROCESS;   /*prevents _ofsm_check_timeout() to ever accessing _ofsmWakeupTime and queue timeout while in process*/
            _ofsmFlags &= ~(_OFSM_FLAG_OFSM_EVENT_QUEUED); /*reset event queued flag*/
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

        OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
			_ofsmWakeupTime = earliestWakeupTime;
			_ofsmFlags = (_ofsmFlags & ~_OFSM_FLAG_ALL) | (andedFsmFlags & _OFSM_FLAG_ALL);
			//if scheduled time is in overflow and timer is in overflow reset timer overflow flag
			if ((_ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP) || ((_ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW) && (_ofsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW)))
            {
                _ofsmFlags &= ~(_OFSM_FLAG_OFSM_TIMER_OVERFLOW | _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW);
            }
			_ofsmFlags &= ~(_OFSM_FLAG_OFSM_FIRST_ITERATION | _OFSM_FLAG_OFSM_IN_PROCESS);
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
    uint8_t debugFlags = 0x1; /*set buffer overflow*/
#endif
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) {
        copyNextEventIndex = group->nextEventIndex;

		/*since even is queued we must erase deep sleep flag to indicate that deep sleep was interrupted and infinite timeout */
		_ofsmFlags &= ~(_OFSM_FLAG_OFSM_IN_DEEP_SLEEP);

        if (!(group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
#ifdef OFSM_CONFIG_SIMULATION
            debugFlags = 0; /*remove buffer overflow*/
#endif
            if (group->nextEventIndex == group->currentEventIndex) {
                forceNewEvent = true; /*all event are processed by FSM and event should never reuse previous event slot.*/
            }
            else if (0 == eventCode) {
                forceNewEvent = false; /*always replace timeout event*/
            }
        }

        /*update previous event if previous event codes matches*/
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
                debugFlags |= 0x2; /*set event replaced flag*/
#endif
            }
        }

        if (!(group->flags & _OFSM_FLAG_GROUP_BUFFER_OVERFLOW)) {
            if (forceNewEvent) {
                group->nextEventIndex++;
                if (group->nextEventIndex >= group->eventQueueSize) {
                    group->nextEventIndex = 0;
                }

                /*queue event*/
                event = &(group->eventQueue[copyNextEventIndex]);
                event->eventCode = eventCode;
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
                event->eventData = eventData;
#endif

                /*set event queued flag, so that _ofsm_start() knows if it need to continue processing*/
                _ofsmFlags |= (_OFSM_FLAG_OFSM_EVENT_QUEUED);

                /*event buffer overflow disable further events*/
                if (group->nextEventIndex == group->currentEventIndex) {
                    group->flags |= _OFSM_FLAG_GROUP_BUFFER_OVERFLOW; /*set buffer overflow flag, so that no new events get queued*/
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
    /*do nothing if in process*/
    if (_ofsmFlags & (_OFSM_FLAG_OFSM_IN_PROCESS | _OFSM_FLAG_INFINITE_SLEEP)) {
        return;
    }
    if (_OFSM_TIME_A_GTE_B(_ofsmTime, (_ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW), _ofsmWakeupTime, (_ofsmFlags & _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW))) {
        ofsm_queue_global_event(false, 0, 0); /*this call will wakeup main loop*/

#if OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE == 1 /*in this mode ofsm_queue_... will not wakeup*/
        OFSM_CONFIG_CUSTOM_WAKEUP_FUNC();
#endif
    }
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
	bool ofsmDeepSleepMode;
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
    _ofsm_snprintf(buf, (sizeof(buf) / sizeof(*buf)), "-O[%c%c]-G(%i)[%c,%03d]-F(%i)[%c%c%c]-S(%i)-TW[%010lu%c,O:%010lu%c,F:%010lu%c]"
        //OFSM (-O)
        , (r->ofsmInfiniteSleep ? 'I' : 'i')
		, (r->ofsmDeepSleepMode ? 'D' : 'd')
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
		r->ofsmDeepSleepMode = (bool)((_ofsmFlags & _OFSM_FLAG_ALLOW_DEEP_SLEEP) > 0);
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
        _ofsmFlags &= ~_OFSM_FLAG_OFSM_IN_PROCESS; /*enable wakeup on timeout*/
        std::unique_lock<std::mutex> lk(cvm);

        cv.wait(lk);
        _ofsmFlags &= ~(_OFSM_FLAG_OFSM_EVENT_QUEUED | _OFSM_FLAG_INFINITE_SLEEP);
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
#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
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
#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
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

			/*In simulation mode, we need to allow to reset OFSM to initial state, so that intermediate test case can start clean.*/
			uint8_t i, k;
			OFSMGroup *group;
			OFSM *fsm;
			/*reset GLOBALS*/
			_ofsmFlags = (_OFSM_FLAG_INFINITE_SLEEP | _OFSM_FLAG_OFSM_FIRST_ITERATION);
			_ofsmTime = 0;
			_ofsmWakeupTime = 0;
			/*reset groups and FSMs*/
			for (i = 0; i < _ofsmGroupCount; i++) {
				group = (_ofsmGroups)[i];
				group->flags = 0;
				group->currentEventIndex = group->nextEventIndex = 0;
				for (k = 0; k < group->groupSize; k++) {
					fsm = (group->fsms)[k];
					fsm->flags = (_OFSM_FLAG_INFINITE_SLEEP);
					fsm->currentState = fsm->simulationInitialState;
					fsm->skipNextEventCode = (uint8_t)-1;
					fsm->wakeupTime = 0;
				}
			}
        }

    } while (retCode < 0);

    return retCode;
}

#endif /* OFSM_CONFIG_SIMULATION */

/*--------------------------------------
MCU specific code
----------------------------------------*/

#ifndef OFSM_CONFIG_SIMULATION

static inline void _ofsm_wakeup() {
}

#ifdef OFSM_IMPL_IDLE_SLEEP_DISABLE_PERIPHERAL
static inline void  _ofsm_idle_sleep_disable_peripheral() {
    power_adc_disable();
    power_spi_disable();
    power_twi_disable();
}
#endif

#ifdef OFSM_IMPL_IDLE_SLEEP_ENABLE_PERIPHERAL
static inline void _ofsm_idle_sleep_enable_peripheral() {
    power_twi_enable();
    power_spi_enable();
    power_adc_enable();
}
#endif

#ifdef OFSM_IMPL_DEEP_SLEEP_DISABLE_PERIPHERAL
static inline void  _ofsm_deep_sleep_disable_peripheral() {
    power_adc_disable();
    power_spi_disable();
    power_twi_disable();
#ifdef power_usart1_disable
    power_usart1_disable();
#endif
    power_usart0_disable();
}
#endif

#ifdef OFSM_IMPL_DEEP_SLEEP_ENABLE_PERIPHERAL
static inline void _ofsm_deep_sleep_enable_peripheral() {
    power_usart0_enable();
#ifdef power_usart1_disable
    power_usart1_enable();
#endif
    power_twi_enable();
    power_spi_enable();
    power_adc_enable();
}
#endif

volatile unsigned long _ofsmWatchdogDelayUs;

/*chip specific MAX watchdog sleep period*/
#ifndef WDP3
#	define _OFSM_MAX_SLEEP_MASK  0B111 /*2 sec*/
#else
#   define _OFSM_MAX_SLEEP_MASK 0B1001 /*8 sec*/
#endif

static inline void _ofsm_enter_sleep() {
uint8_t sleepFlag = 0;
unsigned long sleepPeriodUs;

	cli(); /*disable interrupts*/
	_ofsmFlags &= ~_OFSM_FLAG_OFSM_IN_PROCESS; /*enable wakeup on timeout*/

    while(!(_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED)) {
        /*when in infinite sleep make wakeup time far in the future (128 * 16ms (16384000 us))*/
        if(_ofsmFlags & _OFSM_FLAG_INFINITE_SLEEP) {
            _ofsmWakeupTime = _ofsmTime + (128 * 16384000L)/(unsigned long)OFSM_CONFIG_TICK_US;
        }
        sleepPeriodUs = OFSM_CONFIG_CUSTOM_SLEEP_TIMER_SET_FUNC();
        while(!(_ofsmFlags & _OFSM_FLAG_OFSM_EVENT_QUEUED) && sleepPeriodUs > 0L) {
            if(_ofsmFlags & _OFSM_FLAG_ALLOW_DEEP_SLEEP && sleepPeriodUs >= 16000L) {
                sleepFlag |= 2;
                OFSM_CONFIG_CUSTOM_DEEP_SLEEP_DISABLE_PERIPHERAL_FUNC();
                _ofsm_enter_deep_sleep(sleepPeriodUs);
                /*when coming out of deep sleep we always re-enable peripherals right away, as we cannot guarantee that next sleep will be a deep sleep again*/
                OFSM_CONFIG_CUSTOM_DEEP_SLEEP_ENABLE_PERIPHERAL_FUNC();
            } else {
                if(!(sleepFlag & 1)) {
                    OFSM_CONFIG_CUSTOM_IDLE_SLEEP_DISABLE_PERIPHERAL_FUNC();
                }
                sleepFlag |= 1;
                _ofsm_enter_idle_sleep(sleepPeriodUs);
                /*after idle sleep we still don't know if we are going to continue sleep or not, postpone enable peripherals until after the sleep*/
            }
            /*waked up continues here*/

            /*next call will be calling heartbeat via milliseconds/microseconds proxy
            unless custom heartbeat provider is implemented*/
            sleepPeriodUs = OFSM_CONFIG_CUSTOM_SLEEP_TIMER_GET_TIME_LEFT_US_FUNC();
        }
    }

    /* enable interrupts; disable sleep */
    _ofsmFlags |= _OFSM_FLAG_OFSM_IN_PROCESS;
	sei();
	sleep_disable();
    if(sleepFlag & 1) {
		OFSM_CONFIG_CUSTOM_IDLE_SLEEP_ENABLE_PERIPHERAL_FUNC();
    }
}

#ifndef OFSM_CONFIG_CUSTOM_HEARTBEAT_PROVIDER

static unsigned long _ofsmTimerStartTimeUs;
static unsigned long _ofsmSleepPeriodUs;
static unsigned long _ofsmTimeBeforeSleep;

static inline unsigned long _ofsm_sleep_timer_set() {
    _ofsmTimerStartTimeUs = OFSM_CONFIG_CUSTOM_MICROS_FUNC();
    _ofsmSleepPeriodUs = (_ofsmWakeupTime - _ofsmTime) * OFSM_CONFIG_TICK_US;
    _ofsmTimeBeforeSleep = _ofsmTime;
    return _ofsmSleepPeriodUs;
}

static inline unsigned long _ofsm_sleep_timer_get_time_left_us() {
    unsigned long diff = OFSM_CONFIG_CUSTOM_MICROS_FUNC() - _ofsmTimerStartTimeUs;
    unsigned long fullTicks = diff / OFSM_CONFIG_TICK_US;
    /*call heartbeat when we have at least 1 full ticks, adjust timer start time*/
    ofsm_heartbeat(_ofsmTimeBeforeSleep + fullTicks);
    if(diff >= _ofsmSleepPeriodUs) {
        return 0;
    }
    return _ofsmSleepPeriodUs - diff;
}
#endif /*OFSM_CONFIG_CUSTOM_HEARTBEAT_PROVIDER*/

static inline void _ofsm_enter_idle_sleep(unsigned long sleepPeriodUs) {

	/* timing debug helper */
    /*
	Serial.print("ISus: ");
	Serial.print(sleepPeriodUs);
	Serial.println();
	delay(50);
	cli();
    */

	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();

#ifdef OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_IDLE_SLEEP
    sleep_bod_disable();
#endif // OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_IDLE_SLEEP
	sei();
	sleep_cpu();

	/*wakeup here*/
    cli();
#ifdef OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_IDLE_SLEEP
    sleep_bod_enable();
#endif // OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_IDLE_SLEEP
}

#ifndef MICROSECONDS_PER_TIMER0_OVERFLOW
	/* In Arduino environment the prescaler is set so that timer0 ticks every 64 clock cycles, and the
	 the overflow handler is called every 256 ticks. */
#	define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(64 * 256))
    /* the fractional number of milliseconds per timer0 overflow. we shift right
    * by three to fit these numbers into a byte. (for the clock speeds we care
    * about - 8 and 16 MHz - this doesn't lose precision.) */
#   define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#   define FRACT_MAX (1000 >> 3)
    /* the whole number of milliseconds per timer0 overflow*/
#   define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)
#endif

#ifdef OFSM_IMPL_WATCHDOG_INTERRUPT_HANDLER
extern volatile unsigned long timer0_millis; /*Arduino timer0 variable*/
extern volatile unsigned long timer0_overflow_count; /*Arduino timer0 overflow counter*/

static inline void _ofsm_wdt_vector() {
    /*try to update Arduino timer variables, to keep time relatively correct.
    if deep sleep was interrupted by external source,
    then time will not be updated as we don't know how big of a delay was between getting to sleep and external interrupt
    */
	if(_ofsmFlags & _OFSM_FLAG_OFSM_IN_DEEP_SLEEP) {
		unsigned long overflowCount = _ofsmWatchdogDelayUs / MICROSECONDS_PER_TIMER0_OVERFLOW;
		timer0_millis += overflowCount * MILLIS_INC + (unsigned long)(overflowCount/FRACT_MAX);
		/*update Arduino overflow counter*/
		timer0_overflow_count += overflowCount;
	}
}
#endif

ISR(WDT_vect) {
	OFSM_CONFIG_CUSTOM_WATCHDOG_INTERRUPT_HANDLER_FUNC();
}

static inline void _ofsm_enter_deep_sleep(unsigned long sleepPeriodUs) {
    uint16_t wdtMask = 0;

    if(sleepPeriodUs >= 8192000L) {
        wdtMask = _OFSM_MAX_SLEEP_MASK; /*8 sec*/
        _ofsmWatchdogDelayUs = 16000L << _OFSM_MAX_SLEEP_MASK;
    } else {
        unsigned long probe = 16000L;
        for (wdtMask = 0; probe <= (sleepPeriodUs-(16000L << wdtMask)); wdtMask++) {
            probe = probe << 1;
        }
        _ofsmWatchdogDelayUs = probe;
    }

    /*adjust wdtMask: shift bit: 3 into WDP3 position (bit: 5)*/
    wdtMask = (((wdtMask & 0B1000) << 2) | (wdtMask & 0B111));

    /* timing debug helper */
    /*
    sei();
    Serial.print("DSus: ");
    Serial.print(sleepPeriodUs, DEC);
    Serial.print(" wdtMask: ");
    Serial.print(wdtMask, BIN);
    Serial.println();
    delay(100);
    cli();
    */

    /* NOTE: without this delay, deep sleep may fail.
    * Having this 2us timeout causes yielding to outstanding interrupt handlers, to let them finish.
    * This is pure speculation at this point as I don't understand what really is going on.
    * And why it does not work without this delay
    * Further research is needed.*/
	sei();
    delayMicroseconds(2);
	cli();

    _ofsmFlags |= _OFSM_FLAG_OFSM_IN_DEEP_SLEEP; /*set flag indicating deep sleep. It must be cleared on wakeup or event queuing.*/
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	MCUSR &= ~(1 << WDRF); /*Watchdog Reset Flag*/
	wdt_reset();  /* prepare */
	WDTCSR |= ((1 << WDCE) | (1 << WDE)); /*enable change; timing sequence goes from here*/
	WDTCSR = ((1 << WDIE) | wdtMask);   /* set interrupt mode, set prescaler */

#ifdef OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_DEEP_SLEEP
	/* turn off brown-out detector*/
	sleep_bod_disable();
#endif // OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_DEEP_SLEEP

	sei(); /*next instruction is guaranteed to be executed*/
	sleep_cpu();
	/*------------------------------------------*/
    /*waked up here*/
	cli();
#ifdef OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_DEEP_SLEEP
	/* turn on brown-out detector*/
	sleep_bod_enable();
#endif // OFSM_CONFIG_DISABLE_BROWN_OUT_DETECTOR_ON_DEEP_SLEEP

    wdt_disable();
    _ofsmFlags &= ~_OFSM_FLAG_OFSM_IN_DEEP_SLEEP;
}

#endif /* not OFSM_CONFIG_SIMULATION */

#endif /* __OFSM_IMPL_H_ */
