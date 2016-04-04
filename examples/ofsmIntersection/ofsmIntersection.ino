/* OFSM Intersection example:
Problem statement:
    1. There is an intersection of road 1 and road 2
    2. Both roads have green signal light, which when turned on indicates that road is in "traffic mode"
    3. Only one green signal light can be turned on at any point of time
    4. Unless interrupted by crosswalk button, switching between green lights happens every GREEN_LIGHT_TIMEOUT + CLEAR_INTERSECTION_TIMEOUT
    5. There is a crosswalk on both roads
    6. Both crosswalks are controlled by the same button
    7. When button is pressed, currently in "traffic mode" road gets into "Clear Intersection" mode
    8. Once button is pressed and "active" road gets into "Clear Intersection" mode, further presses of the button should be ignored
    Please see ofsmIntersection.png for state machine design diagram.
Wiring:
    1. [Pin12] --> [Diode+]   [Diode-] --> [R(220 Ohm)] --> GND
    2. [Pin2]  --> [ButtonTerminalA]     [ButtonTerminalB] --> GND
    Note: on-board diode (pin 13) is used for one of the signal lights.
Interactive Simulation:
    1. Build command: g++ -Wall -std=c++11 -fexceptions -std=c++11 -I../../src -g  -o ofsmIntersection -x c++ ofsmIntersection.ino
    2. When running, to simulate button press enter: 1 (which is a shorthand for queue,1,0,0) (don't forget to hit <ENTER> to submit the event)
Note:
    1. Crosswalk machine is not strictly required, as it can be easily optimized out. And it is kept for demo purposes only.
*/

#include "ofsmIntersection.h"
#include <ofsm.impl.h>

#define road1LightPin 13
#define road2LightPin 12
#define buttonPin 2
#define GREEN_LIGHT_TIMEOUT  10 * 1000 /*number of ticks Green light is in ON state before intersection clearing */
#define CLEAR_INTERSECTION_TIMEOUT 2 * 1000 /*number of ticks to wait for intersection to clear*/

/*define events and states*/
enum RoadStates {GreenLightOn = 0, ClearIntersection, GreenLightOff};
enum RoadEvents {TIMEOUT = 0, PEDESTRIAN_AWAITS, GO_GREEN};

enum CrossWalkStates {WaitForButtonPress = 0, Disabled};
enum CrossWalkEvents {TIMEOUT_ = 0, BUTTON_PRESSED, DISABLE_BUTTON, ARM_BUTTON};

/* Handlers declaration */
void OnRoadGreen();
void OnRoadClearIntersection();
void OnRoadRed();

void OnCrosswalkButtonPressed();
void OnCrosswalkArmButton();
void OnCrosswalkDisableButton();

/* Road transition table */
#define NOP OFSM_NOP_HANDLER /*make NOP as an alias to OFSM_NOP_HANDLER*/
OFSMTransition roadTrasitionTable[][1 + GO_GREEN] = {
    /* TIMEOUT                                           PEDESTRIAN_AWAITS                   GO_GREEN*/
    { { OnRoadGreen,                ClearIntersection }, { 0,    0                 },   {0,        0} },     /*GreenLightOn*/
    { { OnRoadClearIntersection,    GreenLightOff },     { NOP,  ClearIntersection },   {0,        0} },     /*ClearIntersection*/
    { { OnRoadRed,                  GreenLightOff },     { 0,    0                 },   {NOP, GreenLightOn} }/*GreenLightOff*/
};

/* Crosswalk transition table */
OFSMTransition crosswalkTrasitionTable[][1 + ARM_BUTTON] = {
    /* TIMEOUT   BUTTON_PRESSED,                          DISABLE_BUTTON,                          ARM_BUTTON*/
    { { 0,  0 }, { OnCrosswalkButtonPressed,  Disabled }, { OnCrosswalkDisableButton,  Disabled }, { 0,                     0 } },                 /*WaitForButtonPress*/
    { { 0,  0 }, { 0,                         0 },        { 0,                         0 },        { OnCrosswalkArmButton,  WaitForButtonPress } } /*Disabled*/
};

/*road FSM private data*/
struct RoadInfo {
    uint8_t lightPin;
};
RoadInfo ri[2] = { {road1LightPin}, {road2LightPin} };

/*other defines*/
enum FsmId  {CrosswalkFsm = 0, RoadFsm1, RoadFsm2};
enum FsmGrpId {CrosswalkGrp = 0, RoadGrp};

OFSM_DECLARE_FSM(RoadFsm1, roadTrasitionTable, 1 + GO_GREEN, NULL, &ri[0], GreenLightOn);
OFSM_DECLARE_FSM(RoadFsm2, roadTrasitionTable, 1 + GO_GREEN, NULL, &ri[1], GreenLightOff);
OFSM_DECLARE_FSM(CrosswalkFsm, crosswalkTrasitionTable, 1 + ARM_BUTTON, NULL, NULL, Disabled);

OFSM_DECLARE_GROUP_1(CrosswalkGrp, 2, CrosswalkFsm);
OFSM_DECLARE_GROUP_2(RoadGrp, 2, RoadFsm1, RoadFsm2);

OFSM_DECLARE_2(CrosswalkGrp, RoadGrp);

/* Setup */
void setup() {
#if OFSM_MCU_BLOCK
	/* set up Serial library at 9600 bps */
	//Serial.begin(9600);
    pinMode(road1LightPin, OUTPUT);
    pinMode(road2LightPin, OUTPUT);
#endif
    OFSM_SETUP();
}

void loop() {
    ofsm_queue_global_event(false, 0, 0); /*queue timeout to wakeup FSMs*/
    OFSM_LOOP();
}


/* Road(s) FSM Handlers */
void OnRoadGreen() {
    RoadInfo *ri = fsm_get_private_data_cast(RoadInfo*);
    ofsm_queue_group_event(CrosswalkGrp, false, ARM_BUTTON, 0); /* notify crosswalk FSM that it needs to arm the button */
    fsm_set_transition_delay_deep_sleep(GREEN_LIGHT_TIMEOUT);
#if OFSM_MCU_BLOCK
    digitalWrite(ri->lightPin, HIGH); /* turn on light */
    //Serial.print("R("); Serial.print(fsm_get_fsm_index() + 1); Serial.println(") On.");
#endif /* OFSM_MCU_BLOCK */
    ofsm_debug_printf(1, "Road(%i): Light ON. (pin: %i).\n", fsm_get_fsm_index() + 1, ri->lightPin);
}
void OnRoadClearIntersection() {
    ofsm_queue_group_event(CrosswalkGrp, false, DISABLE_BUTTON, 0); /* notify crosswalk FSM that it must disable button */
    fsm_set_transition_delay_deep_sleep(CLEAR_INTERSECTION_TIMEOUT);
#if OFSM_MCU_BLOCK
	//Serial.print("R("); Serial.print(fsm_get_fsm_index() + 1); Serial.println(") Clearing intersection...");
#endif /* OFSM_MCU_BLOCK */
    ofsm_debug_printf(1, "Road(%i): Clearing intersection...\n", fsm_get_fsm_index() + 1);
}
void OnRoadRed() {
    RoadInfo *ri = fsm_get_private_data_cast(RoadInfo*);
    /* notify "inactive" road FSM that it has to go into "traffic mode", exclude this FSM instance from processing of this event */
    fsm_queue_group_event_exclude_self(false, GO_GREEN, 0);
    fsm_set_infinite_delay_deep_sleep();
#if OFSM_MCU_BLOCK
    digitalWrite(ri->lightPin, LOW); /* turn off light */
    //Serial.print("R("); Serial.print(fsm_get_fsm_index() + 1); Serial.println(") Off.");
#endif /* OFSM_MCU_BLOCK */
    ofsm_debug_printf(1, "Road(%i): Light OFF. (pin: %i).\n", fsm_get_fsm_index() + 1, ri->lightPin);
}

/* Button FSM Handlers */
void OnCrosswalkButtonPressed(){
    /* queue event into Road FSM group to indicates that pedestrian is awaiting */
    ofsm_queue_group_event(RoadGrp, false, PEDESTRIAN_AWAITS, 0);
#if OFSM_MCU_BLOCK
    //Serial.println("Crosswalk: Button pressed.");
#endif /* OFSM_MCU_BLOCK */
    ofsm_debug_printf(1, "Crosswalk: Button pressed.\n");
}

void ISR_ButtonPress() {
    ofsm_queue_group_event(CrosswalkGrp, false, BUTTON_PRESSED, 0);
}

void OnCrosswalkArmButton(){
#if OFSM_MCU_BLOCK
    /* attach button press interrupt */
    pinMode(buttonPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonPin), ISR_ButtonPress, FALLING);
    //Serial.println("Crosswalk: Arming button...");
#endif /* OFSM_MCU_BLOCK */
    ofsm_debug_printf(1, "Crosswalk: Arming button...\n");
}

void OnCrosswalkDisableButton(){
#if OFSM_MCU_BLOCK
    /* attach button press interrupt */
    detachInterrupt(digitalPinToInterrupt(buttonPin));
    //Serial.println("Crosswalk: Disarming button...");
#endif /* OFSM_MCU_BLOCK */
    ofsm_debug_printf(1, "Crosswalk: Disarming button...\n");
}


