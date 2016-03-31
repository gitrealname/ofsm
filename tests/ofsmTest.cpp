#include "ofsmTest.h"
#include <ofsm.impl.h>

/*define events*/
enum Events {Timeout = 0, NormalTransition, PreventTransition, InfiniteDelay};
enum States {S0 = 0, S1};
enum FsmId	{DefaultFsm = 0};
enum FsmGrpId {MainGroup = 0};

/* Handlers declaration */
void DummyHandler();
void PreventTransitionHandler();
void InifiniteDelayHandler();

/* OFSM configuration */
OFSMTransition transitionTable[][1 + InfiniteDelay] = {
    /* timeout,               NormalTransition,    PreventTransition,               InifiniteDelay*/
    { { DummyHandler, S1 },{ DummyHandler, S1 },{ PreventTransitionHandler, S1 },{ InifiniteDelayHandler, S1 } }, //S0
    { { 0,			  0  },{ DummyHandler, S0 },{ PreventTransitionHandler, S0 },{ InifiniteDelayHandler, S0 } }, //S1
};

OFSM_DECLARE_FSM(DefaultFsm, transitionTable, 1 + InfiniteDelay, handleInit, NULL);
OFSM_DECLARE_GROUP_1(MainGroup, EVENT_QUEUE_SIZE, DefaultFsm);
OFSM_DECLARE_1(MainGroup);


/* Setup */
void setup() {
    OFSM_SETUP();
}

void loop() {
    OFSM_LOOP();
}


/* Handler implementation */
void DummyHandler() {
}

void PreventTransitionHandler() {
    fsm_prevent_transition();
}

void InifiniteDelayHandler() {
    fsm_set_infinite_delay();
}

