/*
ofsm.h - "Orchestrated" Finite State Machines library (OFSM) for Micro-controllers (MCU)
Copyright (c) 2016 Maksym Moyseyev.  All right reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
.............................................................................

WHAT IT IS
==========
OFSM is a lightweight Finite State Machine macro library to:
* Help writing MCU power efficient code;
* Encourage more efficient code organization;

GOALS
=====
Main goals of this implementation are:
* Testability and easy of debugging.
* Easy of customization and support of different MCUs;
* Re-usability of logic and simplicity of scaling up and down;
* Prevention of "dead" code, only code that ever gets executes gets generated;

DESIGN/USAGE NOTES
==================
Important design decisions and usage notes:
* Multiple state machines should peacefully co-exist and play together;
* Structure of application should resemble Arduino sketch, namely it should have void setup() {...} and void loop() {...} functions. main(...) function should not be defined or
  it needs to be surrounded by #ifndef OFSM_CONFIG_SIMULATION ... main(...) #endif for simulation to work, as it declares its own main() function.
* OFSM is relaying on external "heartbeat" provider, so it can be easily replaced and configured independently.
  Default provider piggybacks Arduino Timer0, millis() and micros() functions.
* Internal time within OFSM is measured in ticks. Tick may represent microseconds, milliseconds or any other time dimensions depending on "heartbeat" provider. (see above).
  Thus, if rules (stated in this section) are followed. The same implementation can run with different speeds and environments.
* As outcome of previous statement, it is recommended that event handlers rely on relative time (in ticks) supplied by OFSM (or in worse case on ofsm_get_time());
* It is recommended that handlers never do a time math on their own, and rely completely on relative delays. As time math should account for timer overflow to work in all cases.
* Event handlers are not expected to know transition states, and should rely on events to move FSM. (see Handlers API for details);
* FSMs can be organized into groups of dependent state machines. NOTE: One group shares the same set of events. Each queued event processed by all FSMs within the group;
* Events are never directly processed but queued, allowing better parallelism between multiple FSM.
* Event queues are protected from overflow. Thus if events get generated faster than FSMs can process them, the events will be compressed (see ofsm_queue...()) or dropped;
* The same event handler can be shared between the states of the same FSM, within different FSMs or even FSMs of different groups;
* FSMs can be shared between different groups.
* OFSM is driven by time (transition delays) and events (from interrupt handlers) and TIMEOUT event become a very important concept.
  Therefore, event id 0 is reserved for TIMEOUT/STATE ENTERING event and must be declared in all FSMs;

TIMEOUT EVENT NOTES
====================
Even though timeout event can be queued to all groups, it doesn't mean that all FSM instances will need to handle it.
OFSM makes sure that if FSM instance requested timeout that expires in the future (relatively to current timeout event) or it is in infinite sleep,
 then that FSM instance handler will NOT be called.

API NOTE
========
* API related documentation sections are created to provide with quick reference of functionality and application structure.
 More detailed documentation (when needed) can be found above implementation/declaration of concrete function, define etc;

FSM DESCRIPTION/DECLARATION API
===============================
* All event handlers are: typedef void(*FSMHandler)(); Example: void MyHandler(){...};
* Initialization handler(s) (when provided) act as regular event handler in terms of access to fsm_... function and makes the same impact to FSM state;
* Example of Transition Table declaration (Ex - event X, Sx - state X, Hxy - state X Event Y handler) (in reality all names are much more meaningful):
OFSMTransition transitionTable1[][1 + E3] = {
//timeout,      E1,         E2                          E3
{ { H00, S0 }, { H01, S1 }, { H02, S0 },               { H03, S1 } },  //S0
{ { 0   ,S0 }, { H11, S0 }, { OFSM_NOP_HANDLER  ,S0 }, { H11, S1 } },  //S1
};
* OFSM_NOP_HANDLER can be used if no action is needed and only transition is required.
* Set of preprocessor macros to help to declare state machine with list amount of effort. These macros include:
    OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr)
    OFSM_DECLARE_GROUP_1(eventQueueSize, fsmId0) //setup group of 1 FSM
    ...
    OFSM_DECLARE_GROUP_5(eventQueueSize, fsmId0, ....,fsmId4) //setup group of 5 FSMs
    OFSM_DECLARE_1(grpId0) //OFSM with 1 group
    ...
    OFSM_DECLARE_5(grpId0,....grpId4) //OFSM with 5 groups
    OFSM_DECLARE_BASIC(transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr) //single FSM single Group declaration

IMPORTANT:
* When handler calls either of fsm_set_transition_delay... or fsm_set_infinite_delay...,
    internally transition happens immediately but target event gets called after timeout.
    Therefore, if there is a need to handle non-timeout event at the state that is requesting transition (source state),
    the corresponding handler needs to be defined in the state that FSM is transitioning to (target state).
* ...DECLARE... macros should be called in the global scope. Outside of setup() and loop() functions.
Example:
 ...
 ...OFSM_DECLARE...
 void setup() {
     OFSM_SETUP();
     ...
 }
 void loop() {
    OFSM_LOOP(); //this call never returns
 }
 ...

INITIALIZATION AND START API
===========================
* All configuration switches (#define OFSM_CONFIG...)  should appear before "ofsm.h" ("ofsm.decl.h") gets included;
* call OFSM_SETUP() to initialize hardware, setup interrupts. During initialization phase all registered FSM initialization handlers will be called;
* call OFSM_LOOP() to start OFSM, this function never returns;
* When FSM is declared, it gets configured with INFINITE_SLEEP flag set. To wakeup FSM at start time the following methods can be used:
  1) When FSM initialization handler is supplied it acts like regular FSM event handler  and can adjust FSM state by means of fsm_... API. For example: it can set transition delay as need.
  2) ofsm_queue... API calls can be used right before OFSM_LOOP() to queue desired events. Including TIMEOUT event.
  NOTE: TIMEOUT event doesn't wake FSM that is in INFINITE_TIMEOUT by default. But it will do it once if event was queued before OFSM_LOOP().

INTERRUPT HANDLERS API
======================
Interrupt handlers may queue an event into any FSM group(s).
To queue an event the following API can be used by interrupt handler:
* ofsm_queue_group_event(groupIndex, eventCode, eventData)
* ofsm_queue_global_event(eventCode, eventData) //queue the same event to all groups

FSM EVENT HANDLERS API
======================
The following set of functions can be called from any event handler:
* fsm_prevent_transition()

* fsm_set_transition_delay(unsigned long delayTicks)
* fsm_set_transition_delay_deep_sleep(unsigned long delayTicks)
* fsm_set_infinite_delay()
* fsm_set_infinite_delay_deep_sleep()
* fsm_set_next_state(uint8_t nextStateId)            //TRY TO AVOID IT! overrides default transition state from the handler

* fsm_get_private_data()
* fsm_get_private_data_cast(castType)                // example: MyPrivateStruct_t *data = fsm_get_private_data_cast(fsm, MyPrivateStruct_t*)
* fsm_get_state()
* fsm_get_time_left_before_timeout()
* fsm_get_fsm_ndex()                                   //index of fsm in the group
* fsm_get_group_index()                                //index of group in OFSM
* fsm_get_event_code()
* fsm_get_event_data()

* fsm_queue_group_event(uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) //queue event into current group
* fsm_queue_group_event_exclude_self(uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData) //queue event into current group, but exclude current FSM from handling the queued event

* ofsm_queue_global_event(uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData)
* ofsm_debug_printf(level,format, ....)	                       //Simulation mode debug print

CONFIGURATION
=============
OFSM can be "shaped" in many different ways using configuration switches. NOTE: all needed configuration switches must be defined before #include <ofsm.h> (<ofsm.decl.h>):
#define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY              //Default 0. Specifies default transition delay, used if event handler didn't set one.
#define OFSM_CONFIG_EVENT_DATA_TYPE uint8_t                     //Default uint8_t (8 bits). Event data type.
#define OFSM_CONFIG_ATOMIC_BLOCK ATOMIC_BLOCK                   //Default: ATOMIC_BLOCK; Retain compatibility with original: see implementation details in <util/atomic.h> from AVR SDK.
#define OFSM_CONFIG_ATOMIC_RESTORESTATE ATOMIC_RESTORESTATE     //Default: ATOMIC_RESTORESTATE see <util/atomic.h>
#define OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER              //Default: undefined. When defined OFMS implements supports for initialization handlers. This will consume a little bit of memory, as handler place holder and initialization logic will be implemented.
#define OFSM_CONFIG_SUPPORT_EVENT_DATA                          //Default: undefined. When defined OFMS will support event data.
#define OFSM_CONFIG_TICK_US                                     //Default: 1000 (1 millisecond). OFSM tick size in microseconds

//By default OFSM piggybacks Arduino timer0 interrupt and micros()/millis() function to call heartbeat,
//Custom heartbeat provider is expected to call ofsm_hearbeat(unsigned long currentTicktime);
// See TIME MANAGEMENT AND SLEEP STRATEGY section for further details.
#define OFSM_CONFIG_CUSTOM_HEARTBEAT_PROVIDER                   //If defined, custom timing function is expected to call: ofsm_hearbeat(unsigned long currentTicktime)

// --------------Simulation Specific Macros -------------------
#define OFSM_CONFIG_SIMULATION									//Default undefined. Turn SIMULATION mode on. See PC SIMULATION section for additional info
#define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 3                    //Default undefined. Enables debug print functionality. Also is used as debug message level filter.
#define OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP        //Default undefined. When defined ofsm_debug_print() it will prefix debug messages with [<current time in ticks>]<debug message>.

//If OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM is undefined, it get the same value as OFSM_CONFIG_SIMULATION_DEBUG_LEVEL.
//OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM value determines level of debug/trace output from OFSM. The following are debug output categories levels used by OFSM:
// 0 - no trace/debug messages from OFSM
// 1 - sever errors; if you getting one of those there is a good chance that definition of the State Machine is incorrect.
// 2 - state transition messages.
// 3 - information messages.
// 4 - debug messages.
#define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM OFSM_CONFIG_SIMULATION_DEBUG_LEVEL    //Default OFSM_CONFIG_SIMULATION_DEBUG_LEVEL. when level 0, all debug prints from OFSM will be disabled.
#define OFSM_CONFIG_SIMULATION_TICK_MS 1000                  //Default 1000 milliseconds in one tick.
#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS 0     //Default 0. Sleep period (in milliseconds) before reading new simulation event. May be helpful in batch processing mode.
#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE					//Default undefined, When defined heartbeat is manually invoked. see PC SIMULATION SCRIPT MODE for details.

//Default: 0 - (wakeup when queued, including timeout);
//	Other values:
//		1 - wakeup explicitly by 'w[akeup]' command and timeout via 'h[eartbeat] command;
//		2 - wakeup only by 'w[akeup]'
//		3 - wakeup only by 'w[akeup]' and process only one event per step
#define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 3

CUSTOMIZATION
=============
#define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_enter_sleep                   //typedef: void _ofsm_enter_sleep().
//Is almost never needs to be implemented, as wakeup naturally occurs when interrupt is called.
//It is Mostly used for simulation mode where more work needs to be done to wakeup the main loop.
#define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_wakeup												//typedef: void _ofsm_wakeup().
#define OFSM_CONFIG_CUSTOM_IDLE_SLEEP_DISABLE_PERIPHERAL_FUNC _ofsm_idle_sleep_disable_peripheral //typedef: void _ofsm_idle_sleep_disable_peripheral()
#define OFSM_CONFIG_CUSTOM_IDLE_SLEEP_ENABLE_PERIPHERAL_FUNC _ofsm_idle_sleep_enable_peripheral //typedef: void _ofsm_idle_sleep_disable_peripheral()
#define OFSM_CONFIG_CUSTOM_DEEP_SLEEP_DISABLE_PERIPHERAL_FUNC _ofsm_deep_sleep_disable_peripheral //typedef: void _ofsm_deep_sleep_disable_peripheral()
#define OFSM_CONFIG_CUSTOM_DEEP_SLEEP_ENABLE_PERIPHERAL_FUNC _ofsm_deep_sleep_enable_peripheral //typedef: void _ofsm_deep_sleep_disable_peripheral()
#define OFSM_CONFIG_CUSTOM_WATCHDOG_INTERRUPT_HANDLER_FUNC _ofsm_wdt_vector //typedef: void _ofsm_wdt_vector()

// --------------Simulation Specific Macros -------------------
#define OFSM_CONFIG_CUSTOM_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER_FUNC _ofsm_simulation_status_report_printer // typedef: void custom_func(OFSMSimulationStatusReport *r)
#define OFSM_CONFIG_CUSTOM_SIMULATION_COMMAND_HOOK_FUNC						    //Default: undefined; typedef: bool func(std::deque<std::string> &tokens); see PC SIMULATION EVENT GENERATOR for details.
#define OFSM_CONFIG_CUSTOM_SIMULATION_EVENT_GENERATOR_FUNC _ofsm_simulation_event_generator //typedef: int _ofsm_simulation_event_generator(const char* fileName). Function: expected to call: ofsm_hearbeat(unsigned long currentTicktime)

TIME MANAGEMENT AND SLEEP STRATEGIES
====================================
Once all FSMs requested delay, OFSM calculates earliest wakeup time and goes to sleep until specified wakeup time is reached.
There are two sleep modes: idle sleep and deep sleep (using watch dog timer)
Deep sleep is never used unless all registered FSMs requested  the "deep sleep" (see fsm_....deep_sleep()).
If there where no external or pin change interrupts, then Deep Sleep interrupt handler will attempt to sync Arduino time-keeping variables to be reflect time spent in the "deep sleep".
	which will also cause default heartbeat provider to advance.
However, if "deep sleep" was interrupted by external or pin change interrupt, there is no way to calculate time passed since beginning of the sleep and interrupt
	thus Arduino clock variables will not be updated and timing may become unreliable.
Deep Sleep will not trigger for delays that are less then 16 milliseconds regardless of the FSMs requests.
For long delays that do not fit into possible Deep sleep intervals, the rest of delay will be filled with "Idle Sleep".
For Example:
	wakeup time is 100ms from now: first watchdog timer will be set for 64ms, then for 32ms and the rest ~4ms will be using idle sleep mode.
NOTE: When implementing custom heartbeat provider or not using Arduino environment, consider to define OFSM_CONFIG_CUSTOM_WATCHDOG_INTERRUPT_HANDLER_FUNC along with
	implementation of custom heartbeat functionality.


PC SIMULATION
=============
Ultimate goal is to be able to run properly formatted project in simulation mode on any PC using GCC or other C+11 compatible compiler (including VS012) without any change.
To do that,  the sketch code should follow the following rules:
* OFSM_CONFIG_SIMULATION definition should appear at the very top of the sketch file before any other includes
* All MCU specific code should be surrounded by
    #ifndef OFSM_CONFIG_SIMULATION
        <MCU specific code>
    #endif
    Or
    #if OFMS_MCU_BLOCK
        <MCU specific code>
    #endif
* All simulation specific code should be similarly surrounded with #ifdef OFSM_CONFIG_SIMULATION.... (fsm_debug_printf() can be left as is, it will be automatically optimized out by compiler)
    Default simulation implementation is based on C++ threads. First thread is running state machine and Second is running heartbeat provider with resolution of  OFSM_CONFIG_SIMULATION_TICK_MS.
    Main program's thread is used as event generator. Default implementation reads input in infinite loop and queues events into OFSM. see PC SIMULATION EVENT GENERATOR section for details.
    IMPORTANT NOTE: For simulation to work, it re-defines almost all OFSM_CONFIG_CUSTOM.... macros.
    Therefore, if making customization for MCU purposes, all declarations should be surrounded by #ifndef OFSM_CONFIG_SIMULATION .... #endif construct.
    At the same time one can completely override all parts of simulation as well.

PC SIMULATION EVENT GENERATOR
=============================
Event generator is implemented as infinite loop which reads event information from the input and queues it in OFSM.
Simulation command format: <command>,<parameters>; Value surrounded by '[]' denotes optional part(s).
Any command can optionally be followed by '=' <assert compare string>.
    When specified, event generator will compare <assert compare string> and last output (usually from 'status' or custom command) and issue an assert if there is a mismatch.
'//' designates comment. Event generator will ignore any text appearing after '//'.
The following are commands supported by event generator out of box;
* e[exit] - exit simulation;
* d[elay][,<sleep_milliseconds>] - forces event generator to sleep for <sleep_milliseconds> before reading next command; default sleep is 1000 milliseconds.
    -Example:
        1) sleep,2000		//sleep for 2 seconds
        2) s,2000			// the same as above
* q[ueue][,<modifiers>][,<event code>[,<event data>[,<group index>]]] - queue <event code> into OFSM.
    -<modifiers> - (optional) either 'g' or 'f' or both; where: 'g' - if specified causes event to be queued for all groups (global event), 'f' - forces new event vs. possible replacement of previously queued
    -Examples:
        1) queue,g,0,0,1	//queue global event code 0 event data 0 into all groups;
        2) q,1				//queue event code 1 event data 0 into group 0;
        3) q,f,2,1,1		//queue event code 2 event data 1 into group 1, force new event.
* h[eartbeat][,<current time (in ticks)>] // calls OFSM heartbeat with specified time; see also PC SIMULATION SCRIPT MODE;
    -Examples:
        1) heartbeat,1000	//set current OFSM time to 1000 ticks
        2) h,1000			//same as above
* s[tatus][,<group index>[,<fsm index>] // prints out status report about current state of specified FSM, if not specified <group index> and <fsm index> assumed to be 0
    - see PC SIMULATION REPORT FORMAT for details about produced output.

* p[rint][,<string>]		// prints out <string>
* w[akup]					// explicitly wakeup OFSM; ignored unless OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE > 0
* r[eset]					// reset and restart OFSM; mostly used in script mode for creating of test case.

You can define OFSM_CONFIG_CUSTOM_SIMULATION_COMMAND_HOOK_FUNC that will be called before command gets processed by the event generator.
This way you can extend standard set of commands or change their default behavior.
By returning true, the hook signals event generator that command was processed. Otherwise, event generator will continue processing the command as usual.
Custom hook may call: ofsm_simulation_set_assert_compare_string(const char* assertCompareString) to allow support for test asserts.

PC SIMULATION SCRIPT MODE
=========================
* By default (OFSM_CONFIG_SIMULATION_SCRIPT_MODE is undefined). simulation process runs three threads:
        1) FSM thread: where FSM is running it's loop (in MCU world it corresponds to loop())
        2) Timer thread: this thread is running timer which periodically calls heartbeat to supply time into OFSM and wakes up OFSM in case of Timeout. (in MCU world it corresponds to timer interrupt handler or other time supplying mechanism)
        3) Main thread: used by event generator. (in MCU world: external interrupts, such as button click ....)
    Such a mode allows dynamic/interactive testing of the OFSM.
* Script Mode can be used for thorough state machine logic testing. In this mode everything runs in single thread;
    Heartbeat is expected to be explicitly called from the script (see heartbeat command), that allows to setup precise test scenarios with predetermined outcome
    and creation of repeatable test cases.
*There are couple of ways to supply test data for Script mode:
    1) piping input data into simulated sketch executable (example: mysketch < TestScript.txt)
    2) or by specifying <script file>  on the command line. (example: mysketch TestScript.txt)

PC SIMULATION REPORT FORMAT
===========================
see implementation of _ofsm_simulation_create_status_report() and _ofsm_simulation_status_report_printer() in ofsm.impl.h for details.

LIMITATIONS
============
* Number of events in single FSM, number of FSMs in single group, number of groups within OFSM must not exceed 255!

*/
#ifndef __OFSM_H_
#define __OFSM_H_

/*NOTE: In case of multi-file projects, ofsm.h should not be included directly into any c/cpp file.
Instead project specific .h file fill contain OFSM configuration section and include of the ofsm.decl.h right after that.
And ofsm.impl.h will be include just once into one of the c/cpp project files after project specific .h file.
Example:
    myproject.h:
        #ifdef UTEST
        / *configure script (unit test) mode simulation* /
        #   define OFSM_CONFIG_SIMULATION                            / * turn on simulation mode * /
        #   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE                / * run main loop synchronously * /
        #   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 3  / * 0 - wakeup when event queued; 1 - wakeup on timeout from heartbeat; 2 - manual wakeup (use command 'wakeup'); 3- manual, one event per step* /
        #   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 0              / * turn off sketch debug print * /
        #   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 0         / * turn off ofsm debug print * /
        #   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_SLEEP_BETWEEN_EVENTS_MS 0 / * don't sleep between script commands * /
        #   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0     / * make 0 tick as default delay between transitions * /
        #else
        / * When F_CPU is defined, we can be confident that sketch is being compiled to be flushed into MCU, otherwise we assume SIMULATION mode * /
        #   ifndef F_CPU
        #       define OFSM_CONFIG_SIMULATION                        / * turn on simulation mode * /
        #   endif / *F_CPU* /
        / *configure interactive simulation* /
        #   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 4              / * turn on sketch debug print up to level 4 * /
        #   define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM 0         / * turn on ofsm debug print up to level 4* /
        #   define OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP  / * add time stamps for debug print output * /
        #   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0      / * make 0 ticks as default delay between transitions * /
        #endif
        #include <ofsm.decl.h>
        ...
    main.c:
        #include "myproject.h"
        #include "ofsm.impl.h"
        ...
    fileX.c:
        #include "myproject.h"
        <... your code goes here ...>
*/

#include "ofsm.decl.h"
#include "ofsm.impl.h"

#endif /*__OFSM_H_*/
