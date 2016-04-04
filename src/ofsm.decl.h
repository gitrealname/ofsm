#ifndef __OFSM_DECL_H_
#define __OFSM_DECL_H_

/*Visual C/C++ support*/
#ifdef _MSC_VER
#   ifndef __attribute__
#       define __attribute__(xxx)
#   endif
#else
#endif /*_MSC_VER*/


#include <stdint.h> /*for uint8_t support*/
#ifdef OFSM_CONFIG_SIMULATION
#   include <iostream>
#	include <fstream>
#   include <thread>
#   include <string>
#   include <deque>
#   include <sstream>
#   include <algorithm>
#   include <mutex>
#   include <condition_variable>
#	include <functional>
#	include <cctype>
#	include <locale>
#   include <string.h>
#	include <stdio.h>
#   define _OFSM_TIME_DATA_TYPE uint32_t //make it the same size as AVR unsigned long
#else
#   define _OFSM_TIME_DATA_TYPE unsigned long
#endif

/*default event data type*/
#ifndef OFSM_CONFIG_EVENT_DATA_TYPE
#	define OFSM_CONFIG_EVENT_DATA_TYPE uint8_t
#endif

/*--------------------------------
Type definitions
----------------------------------*/
struct OFSMTransition;
struct OFSMEventData;
struct OFSM;
struct OFSMState;
struct OFSMGroup;
typedef void(*OFSMHandler)();

/*#define ofsm_get_time(time,timeFlags) //see implementation below */

/*------------------------------------------------
The ofsm_debug_printf() available in both simulation and non-simulated modes.
In non-simulated mode it does nothing and consumes no memory. Compiler will optimize if out.
Sketch code that uses this function may be left untouched when compiled for MCU and it will NOT cause any memory consumption.
-------------------------------------------------*/
/*#define ofsm_debug_printf(...) //see implementation below*/

void ofsm_queue_global_event(bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
void ofsm_queue_group_event(uint8_t groupIndex, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
static inline void ofsm_heartbeat(_OFSM_TIME_DATA_TYPE currentTime)  __attribute__((__always_inline__));

void _ofsm_queue_group_event(uint8_t groupIndex, OFSMGroup *group, bool forceNewEvent, uint8_t eventCode, OFSM_CONFIG_EVENT_DATA_TYPE eventData);
static inline void _ofsm_group_process_pending_event(OFSMGroup *group, uint8_t groupIndex, _OFSM_TIME_DATA_TYPE *groupEarliestWakeupTime, uint8_t *groupAndedFsmFlags) __attribute__((__always_inline__));
static inline void _ofsm_fsm_process_event(OFSM *fsm, uint8_t groupIndex, uint8_t fsmIndex, OFSMEventData *e) __attribute__((__always_inline__));
static inline void _ofsm_check_timeout() __attribute__((__always_inline__));
void _ofsm_setup();
void _ofsm_start();

/*see declaration of fsm_... macros below*/


/*---------------------
Common defines
-----------------------*/
#ifndef OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY
#	define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0
#endif

#ifndef OFSM_CONFIG_TICK_US
#   define OFSM_CONFIG_TICK_US 1000L /* 1 millisecond */
#endif

#define OFSM_NOP_HANDLER (OFSMHandler)(-1)

/*---------------------
Simulation defines
-----------------------*/
#ifdef OFSM_CONFIG_SIMULATION

struct OFSMSimulationStatusReport;
volatile char _ofsm_simulation_assert_compare_string[1024];

#undef OFSM_MCU_BLOCK

#define ofsm_simulation_set_assert_compare_string(str) \
OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
    std::string s = str; \
    s = trim(s); \
    _ofsm_strncpy((char*)_ofsm_simulation_assert_compare_string, s.c_str(), 1023); \
}

/*DEBUG should be turned on during simulation*/
#ifndef OFSM_CONFIG_SIMULATION_DEBUG_LEVEL
#	define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL 0
#	ifndef OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM
#		define OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM OFSM_CONFIG_SIMULATION_DEBUG_LEVEL
#	endif
#endif

#ifdef _MSC_VER
#	define _ofsm_strncpy(dest, src, size) strcpy_s(dest, size, src)
#   define _ofsm_snprintf(dest, size, format, ...) sprintf_s(dest, size, format, __VA_ARGS__)
#else
#   define _ofsm_snprintf(dest, size, format, ...) snprintf(dest, size, format, __VA_ARGS__)
#	define _ofsm_strncpy(dest, src, size) strncpy(dest, src, size)
#endif /*_MSC_VER*/

#ifdef OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP
#   define ofsm_debug_printf(level,  ...) \
        if( level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL ) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                _OFSM_TIME_DATA_TYPE __time; \
                ofsm_get_time(__time, __time); \
                printf("[%lu] ", (long unsigned int)__time); \
                printf(__VA_ARGS__); \
            } \
        }
#   define _ofsm_debug_printf(level,  ...) \
        if( level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM ) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                _OFSM_TIME_DATA_TYPE __time; \
                ofsm_get_time(__time, __time); \
                printf("[%lu] ", (long unsigned int)__time); \
                printf(__VA_ARGS__); \
            } \
        }
#else
#   define ofsm_debug_printf(level,  ...) \
        if (level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                printf(__VA_ARGS__); \
            } \
        }
#   define _ofsm_debug_printf(level,  ...) \
        if (level <= OFSM_CONFIG_SIMULATION_DEBUG_LEVEL_OFSM) { \
            OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
                printf(__VA_ARGS__); \
            } \
        }
#endif // OFSM_CONFIG_SIMULATION_DEBUG_PRINT_ADD_TIMESTAMP



/*other overrides*/
#ifndef OFSM_CONFIG_SIMULATION_TICK_MS
#	define OFSM_CONFIG_SIMULATION_TICK_MS 1000
#endif

#ifndef OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE
#   define OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE 3
#endif

#ifndef OFSM_CONFIG_CUSTOM_SIMULATION_EVENT_GENERATOR_FUNC
    int _ofsm_simulation_event_generator(const char *fileName);
#	define OFSM_CONFIG_CUSTOM_SIMULATION_EVENT_GENERATOR_FUNC _ofsm_simulation_event_generator
#	define _OFSM_IMPL_EVENT_GENERATOR
#endif

#ifndef OFSM_CONFIG_CUSTOM_WAKEUP_FUNC
    void _ofsm_simulation_wakeup();
#	define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_simulation_wakeup
#	define _OFSM_IMPL_SIMULATION_WAKEUP
#endif

#ifndef OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC
    void _ofsm_simulation_enter_sleep();
#	define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_simulation_enter_sleep
#	define _OFSM_IMPL_SIMULATION_ENTER_SLEEP
#endif

#ifndef OFSM_CONFIG_ATOMIC_BLOCK
    static std::recursive_mutex _ofsm_simulation_mutex;
    static uint8_t _ofsm_simulation_atomic_counter;
#	ifdef OFSM_CONFIG_ATOMIC_RESTORESTATE
#		undef OFSM_CONFIG_ATOMIC_RESTORESTATE
#	endif
#	define OFSM_CONFIG_ATOMIC_RESTORESTATE _ofsm_simulation_mutex
#	define OFSM_CONFIG_ATOMIC_BLOCK(type) for(type.lock(), _ofsm_simulation_atomic_counter = 0; _ofsm_simulation_atomic_counter++ < 1; type.unlock())
#endif /*OFSM_CONFIG_ATOMIC_BLOCK*/

#ifndef OFSM_CONFIG_SIMULATION_SLEEP_BETWEEN_EVENTS_MS
#	define OFSM_CONFIG_SIMULATION_SLEEP_BETWEEN_EVENTS_MS 0
#endif

#ifndef OFSM_CONFIG_CUSTOM_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER_FUNC
    void _ofsm_simulation_status_report_printer(OFSMSimulationStatusReport* r);
#   define OFSM_CONFIG_CUSTOM_SIMULATION_CUSTOM_STATUS_REPORT_PRINTER_FUNC _ofsm_simulation_status_report_printer
#   define _OFSM_IMPL_SIMULATION_STATUS_REPORT_PRINTER
#endif

#else /*is NOT OFSM_CONFIG_SIMULATION*/

static inline void _ofsm_enter_idle_sleep(unsigned long sleepPeriodUs) __attribute__((__always_inline__));
static inline void _ofsm_enter_deep_sleep(unsigned long sleepPeriodMs) __attribute__((__always_inline__));

#ifndef OFSM_CONFIG_CUSTOM_HEARTBEAT_PROVIDER
static inline void _ofsm_heartbeat_proxy_set_time() __attribute__((__always_inline__));
static inline void _ofsm_heartbeat_ms_us_proxy() __attribute__((__always_inline__));
#endif

#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/power.h>

#define OFSM_MCU_BLOCK 1

#undef OFSM_CONFIG_SIMULATION_SCRIPT_MODE
#undef OFSM_CONFIG_SIMULATION_SCRIPT_MODE_WAKEUP_TYPE


#ifndef sbi
# define sbi(reg, bitToSet) (reg |= (1 << bitToSet))
#endif
#ifndef cbi
# define cbi(reg, bitToClear) (reg &= ~(1 << bitToClear))
#endif

#define ofsm_debug_printf(level,  ...)
#define _ofsm_debug_printf(level,  ...)

#ifndef OFSM_CONFIG_CUSTOM_IDLE_SLEEP_DISABLE_PERIPHERAL_FUNC
static inline void  _ofsm_idle_sleep_disable_peripheral() __attribute__((__always_inline__));
#	define OFSM_CONFIG_CUSTOM_IDLE_SLEEP_DISABLE_PERIPHERAL_FUNC _ofsm_idle_sleep_disable_peripheral
#	define OFSM_IMPL_IDLE_SLEEP_DISABLE_PERIPHERAL
#endif

#ifndef OFSM_CONFIG_CUSTOM_IDLE_SLEEP_ENABLE_PERIPHERAL_FUNC
static inline void  _ofsm_idle_sleep_enable_peripheral() __attribute__((__always_inline__));
#	define OFSM_CONFIG_CUSTOM_IDLE_SLEEP_ENABLE_PERIPHERAL_FUNC _ofsm_idle_sleep_enable_peripheral
#	define OFSM_IMPL_IDLE_SLEEP_ENABLE_PERIPHERAL
#endif

#ifndef OFSM_CONFIG_CUSTOM_DEEP_SLEEP_DISABLE_PERIPHERAL_FUNC
static inline void  _ofsm_deep_sleep_disable_peripheral() __attribute__((__always_inline__));
#	define OFSM_CONFIG_CUSTOM_DEEP_SLEEP_DISABLE_PERIPHERAL_FUNC _ofsm_deep_sleep_disable_peripheral
#	define OFSM_IMPL_DEEP_SLEEP_DISABLE_PERIPHERAL
#endif

#ifndef OFSM_CONFIG_CUSTOM_DEEP_SLEEP_ENABLE_PERIPHERAL_FUNC
static inline void  _ofsm_deep_sleep_enable_peripheral() __attribute__((__always_inline__));
#	define OFSM_CONFIG_CUSTOM_DEEP_SLEEP_ENABLE_PERIPHERAL_FUNC _ofsm_deep_sleep_enable_peripheral
#	define OFSM_IMPL_DEEP_SLEEP_ENABLE_PERIPHERAL
#endif

#ifndef OFSM_CONFIG_CUSTOM_WATCHDOG_INTERRUPT_HANDLER_FUNC
static inline void _ofsm_wdt_vector() __attribute__((__always_inline__));
#	define OFSM_CONFIG_CUSTOM_WATCHDOG_INTERRUPT_HANDLER_FUNC _ofsm_wdt_vector
#	define OFSM_IMPL_WATCHDOG_INTERRUPT_HANDLER
#endif

#ifndef OFSM_CONFIG_CUSTOM_WAKEUP_FUNC
static inline void _ofsm_wakeup() __attribute__((__always_inline__));
#	define OFSM_CONFIG_CUSTOM_WAKEUP_FUNC _ofsm_wakeup
#	define _OFSM_IMPL_WAKEUP
#endif

#ifndef OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC
static inline void _ofsm_enter_sleep() __attribute__((__always_inline__));
#	define OFSM_CONFIG_CUSTOM_ENTER_SLEEP_FUNC _ofsm_enter_sleep
#	define _OFSM_IMPL_ENTER_SLEEP
#endif

#ifndef OFSM_CONFIG_ATOMIC_BLOCK
#	undef __inline__
#	define __inline__ inline
#	include <util/atomic.h>
#	define OFSM_CONFIG_ATOMIC_BLOCK ATOMIC_BLOCK
#endif

#ifndef OFSM_CONFIG_ATOMIC_RESTORESTATE
#	define OFSM_CONFIG_ATOMIC_RESTORESTATE ATOMIC_RESTORESTATE
#endif

#endif /*OFSM_CONFIG_SIMULATION*/

/*--------------------------------
Type definitions
----------------------------------*/
struct OFSMTransition {
    OFSMHandler eventHandler;
    uint8_t newState;
};

struct OFSMEventData {
    uint8_t                     eventCode;
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
    OFSM_CONFIG_EVENT_DATA_TYPE eventData;
#endif
};

struct OFSM {
    OFSMTransition**    transitionTable;
    uint8_t             transitionTableEventCount;  /*number of elements in each row (number of events defined)*/
#ifdef OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER
    OFSMHandler         initHandler;                /*optional, can be null*/
#endif
    void*               fsmPrivateInfo;
    uint8_t             flags;
    _OFSM_TIME_DATA_TYPE wakeupTime;
    uint8_t             currentState;
#ifdef OFSM_CONFIG_SIMULATION
    uint8_t             simulationInitialState; /* store initial state, so that it can be restored during simulation reset*/
#endif /* OFSM_CONFIG_SIMULATION */
    uint8_t             skipNextEventCode;     /* skip (once) processing of specified event event code */
};

struct OFSMState {
    OFSM					*fsm;
    OFSMEventData*			e;
    _OFSM_TIME_DATA_TYPE	timeLeftBeforeTimeout;      /*time left before timeout set by previous transition*/
    uint8_t					groupIndex;                 /*group index where current fsm is registered*/
    uint8_t					fsmIndex;	                /*fsm index within group*/
};

struct OFSMGroup {
    OFSM**					fsms;
    uint8_t					groupSize;
    OFSMEventData*			eventQueue;
    uint8_t					eventQueueSize;

    volatile uint8_t		flags;
    volatile uint8_t		nextEventIndex; //queue cell index that is available for new event
    volatile uint8_t		currentEventIndex; //queue cell that is being processed by ofsm
};

/*defined typedef void(*OFSMHandler)(OFSMState *fsmState);*/

/*------------------------------------------------
Flags
-------------------------------------------------*/
//Common flags
#define _OFSM_FLAG_INFINITE_SLEEP			0x1
#define _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW	0x2 /*indicate if wakeup time is scheduled after post overflow of time register*/
#define _OFSM_FLAG_ALLOW_DEEP_SLEEP         0x4
#define _OFSM_FLAG_ALL (_OFSM_FLAG_INFINITE_SLEEP | _OFSM_FLAG_SCHEDULED_TIME_OVERFLOW | _OFSM_FLAG_ALLOW_DEEP_SLEEP)

//FSM Flags
#define _OFSM_FLAG_FSM_PREVENT_TRANSITION			0x10
#define _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE			0x20
#define _OFSM_FLAG_FSM_HANDLER_SET_TRANSITION_DELAY	0x40
#define _OFSM_FLAG_FSM_FLAG_ALL (_OFSM_FLAG_ALL | _OFSM_FLAG_FSM_PREVENT_TRANSITION | _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE | _OFSM_FLAG_FSM_HANDLER_SET_TRANSITION_DELAY )

//GROUP Flags
#define _OFSM_FLAG_GROUP_BUFFER_OVERFLOW	0x10

//Orchestra Flags
#define _OFSM_FLAG_OFSM_IN_DEEP_SLEEP   0x8   /*watch dog timer is running*/
#define _OFSM_FLAG_OFSM_EVENT_QUEUED	0x10
#define _OFSM_FLAG_OFSM_TIMER_OVERFLOW	0x20
#define _OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT	0x40 /*used at startup to allow queued timeout event to wakeup FSM*/
#define _OFSM_FLAG_OFSM_SIMULATION_EXIT		0x80

/*------------------------------------------------
Macros
-------------------------------------------------*/
#define fsm_prevent_transition()					((_ofsmCurrentFsmState->fsm)[0].flags |= _OFSM_FLAG_FSM_PREVENT_TRANSITION)

#define fsm_set_transition_delay(delayTicks)		((_ofsmCurrentFsmState->fsm)[0].wakeupTime = delayTicks, (_ofsmCurrentFsmState->fsm)[0].flags |= _OFSM_FLAG_FSM_HANDLER_SET_TRANSITION_DELAY)
#define fsm_set_transition_delay_deep_sleep(delayTicks) (fsm_set_transition_delay(delayTicks), (_ofsmCurrentFsmState->fsm)[0].flags |= _OFSM_FLAG_ALLOW_DEEP_SLEEP)
#define fsm_set_infinite_delay()					((_ofsmCurrentFsmState->fsm)[0].flags |= _OFSM_FLAG_INFINITE_SLEEP)
#define fsm_set_infinite_delay_deep_sleep()         (fsm_set_infinite_delay(), (_ofsmCurrentFsmState->fsm)[0].flags |= _OFSM_FLAG_ALLOW_DEEP_SLEEP)
#define fsm_set_next_state(nextStateId)			    ((_ofsmCurrentFsmState->fsm)[0].flags |= _OFSM_FLAG_FSM_NEXT_STATE_OVERRIDE, (_ofsmCurrentFsmState->_ofsmCurrentFsmState)[0].currentState = nextStateId)

#define fsm_get_private_data()						((_ofsmCurrentFsmState->fsm)[0].fsmPrivateInfo)
#define fsm_get_private_data_cast(castType)		    ((castType)((_ofsmCurrentFsmState->fsm)[0].fsmPrivateInfo))
#define fsm_get_state()								((_ofsmCurrentFsmState->fsm)[0].currentState)
#define fsm_get_time_left_before_timeout()          (_ofsmCurrentFsmState->timeLeftBeforeTimeout)
#define fsm_get_fsm_index()							(_ofsmCurrentFsmState->fsmIndex)
#define fsm_get_group_index()						(_ofsmCurrentFsmState->groupIndex)
#define fsm_get_event_code()						((_ofsmCurrentFsmState->e)[0].eventCode)
#ifdef OFSM_CONFIG_SUPPORT_EVENT_DATA
#   define fsm_get_event_data()						((_ofsmCurrentFsmState->e)[0].eventData)
#else
#	define fsm_get_event_data()						0
#endif
#define fsm_queue_group_event(forceNewEvent, eventCode, eventData) \
    ofsm_queue_group_event(fsm_get_group_index(), forceNewEvent, eventCode, eventData)
#define fsm_queue_group_event_exclude_self(forceNewEvent, eventCode, eventData) \
    (ofsm_queue_group_event(fsm_get_group_index(), forceNewEvent, eventCode, eventData), (_ofsmCurrentFsmState->fsm)[0].skipNextEventCode = eventCode)


#define ofsm_get_time(outCurrentTime, outTimeFlags) \
    OFSM_CONFIG_ATOMIC_BLOCK(OFSM_CONFIG_ATOMIC_RESTORESTATE) { \
        outTimeFlags = _ofsmFlags & _OFSM_FLAG_OFSM_TIMER_OVERFLOW; \
        outCurrentTime = _ofsmTime; \
    }

#define _OFSM_GET_TRANSTION(fsm, eventCode) ((OFSMTransition*)( (fsm->transitionTableEventCount * fsm->currentState +  eventCode) * sizeof(OFSMTransition) + (char*)fsm->transitionTable) )

/*time comparison*/
/*ao, bo - 'o' means overflow*/
#define _OFSM_TIME_A_GT_B(a, ao, b, bo)  ( (a  >  b) && (ao || !bo) )
#define _OFSM_TIME_A_GTE_B(a, ao, b, bo) ( (a  >=  b) && (ao || !bo) )

/*------------------------------------------------
Global variables
-------------------------------------------------*/
extern OFSMGroup**				        _ofsmGroups;
extern uint8_t                          _ofsmGroupCount;
extern OFSMState*						_ofsmCurrentFsmState;
extern volatile uint8_t                 _ofsmFlags;
extern volatile _OFSM_TIME_DATA_TYPE    _ofsmWakeupTime;
extern volatile _OFSM_TIME_DATA_TYPE    _ofsmTime;

extern volatile uint16_t				_ofsmWatchdogDelayMs;

/*----------------------------------------------
Setup helper macros
-----------------------------------------------*/

#define _OFSM_DECLARE_GET(name, id) (name##id)
#define _OFSM_DECLARE_GROUP_EVENT_QUEUE(grpId, eventQueueSize) OFSMEventData _ofsm_decl_grp_eq_##grpId[eventQueueSize];

#define _OFSM_DECLARE_GROUP_FSM_ARRAY_1(grpId, fsmId0) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_2(grpId, fsmId0, fsmId1) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_3(grpId, fsmId0, fsmId1, fsmId2) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1, &_ofsm_decl_fsm_##fsmId2 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_4(grpId, fsmId0, fsmId1, fsmId2, fsmId3) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1, &_ofsm_decl_fsm_##fsmId2, &_ofsm_decl_fsm_##fsmId3 };
#define _OFSM_DECLARE_GROUP_FSM_ARRAY_5(grpId, fsmId0, fsmId1, fsmId2, fsmId3, fsmId4) OFSM *_ofsm_decl_grp_fsms_##grpId[] = { &_ofsm_decl_fsm_##fsmId0, &_ofsm_decl_fsm_##fsmId1, &_ofsm_decl_fsm_##fsmId2, &_ofsm_decl_fsm_##fsmId3, &_ofsm_decl_fsm_##fsmId4 };

#define _OFSM_DECLARE_GROUP(grpId) \
    OFSMGroup _ofsm_decl_grp_##grpId = {\
        _OFSM_DECLARE_GET(_ofsm_decl_grp_fsms_, grpId),\
        sizeof(_OFSM_DECLARE_GET(_ofsm_decl_grp_fsms_, grpId))/sizeof(*_OFSM_DECLARE_GET(_ofsm_decl_grp_fsms_, grpId)),\
        _OFSM_DECLARE_GET(_ofsm_decl_grp_eq_, grpId),\
        sizeof(_OFSM_DECLARE_GET(_ofsm_decl_grp_eq_, grpId))/sizeof(*_OFSM_DECLARE_GET(_ofsm_decl_grp_eq_, grpId))\
    }

#define _OFSM_DECLARE_GROUP_ARRAY_1(grpId0) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0) };
#define _OFSM_DECLARE_GROUP_ARRAY_2(grpId0, grpId1) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1) };
#define _OFSM_DECLARE_GROUP_ARRAY_3(grpId0, grpId1, grpId2) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId2) };
#define _OFSM_DECLARE_GROUP_ARRAY_4(grpId0, grpId1, grpId2, grpId3) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId2), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId3) };
#define _OFSM_DECLARE_GROUP_ARRAY_5(grpId0, grpId1, grpId2, grpId3, grpId4) OFSMGroup *_ofsm_decl_grp_arr[] = { &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId0), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId1), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId2), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId3), &_OFSM_DECLARE_GET(_ofsm_decl_grp_, grpId4) };


#define _OFSM_DECLARE_GROUP_N(n, grpId, eventQueueSize, ...) \
    _OFSM_DECLARE_GROUP_EVENT_QUEUE(grpId, eventQueueSize);\
    _OFSM_DECLARE_GROUP_FSM_ARRAY_##n(grpId, __VA_ARGS__);\
    _OFSM_DECLARE_GROUP(grpId);

#define _OFSM_DECLARE_N(n, ...)\
    _OFSM_DECLARE_GROUP_ARRAY_##n(__VA_ARGS__);

#ifdef OFSM_CONFIG_SIMULATION
#   ifdef OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER
#       define OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr, initialState) \
        OFSM _ofsm_decl_fsm_##fsmId = {\
                (OFSMTransition**)transitionTable, 	/*transitionTable*/ \
                transitionTableEventCount,			/*transitionTableEventCount*/ \
                initializationHandler,				/*initHandler*/ \
                fsmPrivateDataPtr,					/*fsmPrivateInfo*/ \
                _OFSM_FLAG_INFINITE_SLEEP,          /*flags*/ \
                0,                                  /*wakeup time*/ \
                initialState,                       /*initial state*/ \
                initialState,                       /*simulation initial state*/ \
                (uint8_t)-1                         /*skipNextEventCode*/ \
        };
#   else
#       define OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr, initialState) \
        OFSM _ofsm_decl_fsm_##fsmId = {\
                (OFSMTransition**)transitionTable, 	/*transitionTable*/ \
                transitionTableEventCount,			/*transitionTableEventCount*/ \
                fsmPrivateDataPtr,					/*fsmPrivateInfo*/ \
                _OFSM_FLAG_INFINITE_SLEEP,          /*flags*/ \
                0,                                  /*wakeup time*/ \
                initialState,                       /*current state*/ \
                initialState,                       /*simulation initial state*/ \
                (uint8_t)-1                         /*skipNextEventCode*/ \
        };
#   endif
#else
#   ifdef OFSM_CONFIG_SUPPORT_INITIALIZATION_HANDLER
#       define OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr, initialState) \
        OFSM _ofsm_decl_fsm_##fsmId = {\
                (OFSMTransition**)transitionTable, 	/*transitionTable*/ \
                transitionTableEventCount,			/*transitionTableEventCount*/ \
                initializationHandler,				/*initHandler*/ \
                fsmPrivateDataPtr,					/*fsmPrivateInfo*/ \
                _OFSM_FLAG_INFINITE_SLEEP,          /*flags*/ \
                0,                                  /*wakeup time*/ \
                initialState,                       /*current state*/ \
                (uint8_t)-1                         /*skipNextEventCode*/ \
        };
#   else
#       define OFSM_DECLARE_FSM(fsmId, transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr, initialState) \
        OFSM _ofsm_decl_fsm_##fsmId = {\
                (OFSMTransition**)transitionTable, 	/*transitionTable*/ \
                transitionTableEventCount,			/*transitionTableEventCount*/ \
                fsmPrivateDataPtr,					/*fsmPrivateInfo*/ \
                _OFSM_FLAG_INFINITE_SLEEP,          /*flags*/ \
                0,                                  /*wakeup time*/ \
                initialState                        /*current state*/ \
                (uint8_t)-1                         /*skipNextEventCode*/ \
        };
#   endif
#endif /* OFSM_CONFIG_SIMULATION*/
#define OFSM_DECLARE_GROUP_1(grpId, eventQueueSize, fsmId0) _OFSM_DECLARE_GROUP_N(1, grpId, eventQueueSize, fsmId0);
#define OFSM_DECLARE_GROUP_2(grpId, eventQueueSize, fsmId0, fsmId1) _OFSM_DECLARE_GROUP_N(2, grpId, eventQueueSize, fsmId0, fsmId1);
#define OFSM_DECLARE_GROUP_3(grpId, eventQueueSize, fsmId0, fsmId1, fsmId2) _OFSM_DECLARE_GROUP_N(3, grpId, eventQueueSize, fsmId0, fsmId1, fsmId2);
#define OFSM_DECLARE_GROUP_4(grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3) _OFSM_DECLARE_GROUP_N(4, grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3);
#define OFSM_DECLARE_GROUP_5(grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3, fsmId4) _OFSM_DECLARE_GROUP_N(5, grpId, eventQueueSize, fsmId0, fsmId1, fsmId2, fsmId3, fsmId4);

#define OFSM_DECLARE_1(grpId0) _OFSM_DECLARE_N(1, grpId0);
#define OFSM_DECLARE_2(grpId0, grpId1) _OFSM_DECLARE_N(2, grpId0, grpId1);
#define OFSM_DECLARE_3(grpId0, grpId1, grpId2) _OFSM_DECLARE_N(3, grpId0, grpId1, grpId2);
#define OFSM_DECLARE_4(grpId0, grpId1, grpId2, grpId3) _OFSM_DECLARE_N(4, grpId0, grpId1, grpId2, grpId3);
#define OFSM_DECLARE_5(grpId0, grpId1, grpId2, grpId3, grpId4) _OFSM_DECLARE_N(5, grpId0, grpId1, grpId2, grpId3, grpId4);

#define OFSM_DECLARE_BASIC(transitionTable, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr) \
    OFSM_DECLARE_FSM(0, transitionTable1, transitionTableEventCount, initializationHandler, fsmPrivateDataPtr); \
    OFSM_DECLARE_GROUP_1(0, 10, 0); \
    OFSM_DECLARE_1(0);

#define OFSM_SETUP() \
    _ofsmGroups = (OFSMGroup**)_ofsm_decl_grp_arr; \
    _ofsmGroupCount = sizeof(_ofsm_decl_grp_arr) / sizeof(*_ofsm_decl_grp_arr); \
    _ofsmFlags |= (_OFSM_FLAG_INFINITE_SLEEP |_OFSM_FLAG_OFSM_INTERRUPT_INFINITE_SLEEP_ON_TIMEOUT);\
    _ofsmTime = 0; \
    _ofsm_setup();
#define OFSM_LOOP() _ofsm_start();

#endif /*__OFSM_DECL_H_*/
