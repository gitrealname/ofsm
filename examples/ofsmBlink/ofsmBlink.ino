#include "ofsmBlink.h"
#include <ofsm.impl.h>

/*define events*/
enum Events {Timeout = 0};
enum States {On = 0, Off};
enum FsmId	{DefaultFsm = 0};
enum FsmGrpId {MainGroup = 0};

/* Handlers declaration */
void OnHandler();
void OffHandler();

/* OFSM configuration */
OFSMTransition transitionTable[][1 + Timeout] = {
    /* Timeout*/
    { { OnHandler,  Off } }, /*On State*/
    { { OffHandler,	On  } }  /*Off State*/
};

OFSM_DECLARE_FSM(DefaultFsm, transitionTable, 1 + Timeout, NULL, NULL, On);
OFSM_DECLARE_GROUP_1(MainGroup, EVENT_QUEUE_SIZE, DefaultFsm);
OFSM_DECLARE_1(MainGroup);

#define ledPin 13
#define ticksOn  1000 /*number of ticks to be in On state*/
#define ticksOff 500 /*number of ticks to be in Off state*/

/* Setup */
void setup() {
    /*prevent MCU specific code during simulation*/
#if OFSM_MCU_BLOCK
    /* initialize digital ledPin as an output.*/
    pinMode(ledPin, OUTPUT);
	/* set up Serial library at 9600 bps */
	//Serial.begin(9600); 
#endif 
    OFSM_SETUP();
}

void loop() {
    ofsm_queue_global_event(false, 0, 0); /*queue timeout to wakeup FSM*/
    OFSM_LOOP();
}


/* Handler implementation */
void OnHandler() {
	ofsm_debug_printf(1, "Turning Led ON for %i ticks.\n", ticksOn);
	fsm_set_transition_delay_deep_sleep(ticksOn);
#if OFSM_MCU_BLOCK
	digitalWrite(ledPin, HIGH);
	//Serial.println("On");
	//delay(10);
#endif
}

void OffHandler() {
	ofsm_debug_printf(1, "Turning Led OFF for %i ticks.\n", ticksOff);
	fsm_set_transition_delay_deep_sleep(ticksOff);
#if OFSM_MCU_BLOCK
	digitalWrite(ledPin, LOW);
	//Serial.println("Off");
	//delay(10);
#endif
}

