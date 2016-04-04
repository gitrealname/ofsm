#ifndef __OFSM_INTERSECTION_H_
#define __OFSM_INTERSECTION_H_

#ifdef UTEST
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
#	define OFSM_CONFIG_SIMULATION_TICK_MS 1					 /* make 1 ticks == 1 millisecond */
#   define OFSM_CONFIG_DEFAULT_STATE_TRANSITION_DELAY 0      /* make 0 ticks as default delay between transitions */
#endif

#define EVENT_QUEUE_SIZE 3 /*event queue size*/

#include <ofsm.decl.h>

#endif /*__OFSM_INTERSECTION_H_*/
