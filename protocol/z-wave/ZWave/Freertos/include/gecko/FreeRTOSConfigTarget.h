#ifndef FREERTOS_CONFIG_TARGET_H
#define FREERTOS_CONFIG_TARGET_H

/* Specific information on low power for ARM Cortex-M:
   https://www.freertos.org/low-power-ARM-cortex-rtos.html */

/* Portions of this configuration file is inspired by:
   FreeRTOS/Demo/CORTEX_EFM32_Giant_Gecko_Simplicity_Studio/FreeRTOSConfig.h */

#include "em_cmu.h"

#define configUSE_PORT_OPTIMISED_TASK_SELECTION    (1)
//#define configCPU_CLOCK_HZ		               ((unsigned long)38400000)
#define configCPU_CLOCK_HZ                         (CMU_ClockFreqGet(cmuClock_CORE))


/* Interrupt nesting behaviour configuration */
/* Cortex-M specific definitions. */


#ifdef __NVIC_PRIO_BITS
    /* __BVIC_PRIO_BITS will be specified when CMSIS is being used. */
    #define configPRIO_BITS            __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS            3    /* 7 priority levels */
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         0x07
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    0x01

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* Special port specific configuration */
#define configCREATE_LOW_POWER_DEMO 1

#define vPortSVCHandler         SVC_Handler
#define xPortPendSVHandler      PendSV_Handler
#define xPortSysTickHandler     SysTick_Handler

/* For the linker. */
#define fabs __builtin_fabs

void enterPowerDown(uint32_t millis);
void exitPowerDown(uint32_t millis);

#define configPRE_SLEEP_PROCESSING enterPowerDown
#define configPOST_SLEEP_PROCESSING exitPowerDown

#endif /* FREERTOS_CONFIG_TARGET_H */
