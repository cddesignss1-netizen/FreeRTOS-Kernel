/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

/* *INDENT-OFF* */
#ifdef __cplusplus
    extern "C" {
#endif
/* *INDENT-ON* */

/*------------------------------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the given hardware
 * and compiler.
 *
 * These settings should not be altered.
 *------------------------------------------------------------------------------
 */

/**
 * Architecture specifics.
 */
#define portARCH_NAME                    "Cortex-M33"
#define portHAS_ARMV8M_MAIN_EXTENSION    1
#define portARMV8M_MINOR_VERSION         0
#define portDONT_DISCARD                 __attribute__( ( used ) )
/*-----------------------------------------------------------*/

/* ARMv8-M common port configurations. */
#include "portmacrocommon.h"
#include "pico/platform.h"
#include "hardware/sync.h"
/*-----------------------------------------------------------*/

#ifndef configENABLE_MVE
    #define configENABLE_MVE    0
#elif ( configENABLE_MVE != 0 )
    #error configENABLE_MVE must be left undefined, or defined to 0 for the Cortex-M33.
#endif
/*-----------------------------------------------------------*/

#if ( ( configNUMBER_OF_CORES > 1 ) && ( configENABLE_MPU == 1 ) )
    #error This CM33 RP2350 SMP integration currently supports configENABLE_MPU == 0 only.
#endif

#define portMAX_CORE_COUNT    2

#if ( configNUMBER_OF_CORES < 1 || portMAX_CORE_COUNT < configNUMBER_OF_CORES )
    #error "Invalid number of cores specified in config!"
#endif

#if ( configNUMBER_OF_CORES > 1 )
    #ifndef configTICK_CORE
        #define configTICK_CORE    0
    #endif

    #if ( configTICK_CORE < 0 || configTICK_CORE >= configNUMBER_OF_CORES )
        #error "Invalid tick core specified in config!"
    #endif
#endif

#ifndef configSMP_SPINLOCK_0
    #define configSMP_SPINLOCK_0    PICO_SPINLOCK_ID_OS1
#endif

#ifndef configSMP_SPINLOCK_1
    #define configSMP_SPINLOCK_1    PICO_SPINLOCK_ID_OS2
#endif

#if ( configNUMBER_OF_CORES > 1 )
    #define portGET_CORE_ID()    get_core_num()
#else
    #define portGET_CORE_ID()    0
#endif

void vYieldCore( BaseType_t xCoreID );
#define portYIELD_CORE( xCoreID )    vYieldCore( xCoreID )

#if ( configNUMBER_OF_CORES > 1 )
    #undef portENTER_CRITICAL
    #undef portEXIT_CRITICAL

    #define portCRITICAL_NESTING_IN_TCB    1

    extern void vTaskEnterCritical( void );
    extern void vTaskExitCritical( void );
    extern UBaseType_t vTaskEnterCriticalFromISR( void );
    extern void vTaskExitCriticalFromISR( UBaseType_t uxSavedInterruptStatus );
#endif

/**
 * @brief Critical section management.
 */
#define portSET_INTERRUPT_MASK()    ulSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK( x ) vClearInterruptMask( x )
#define portDISABLE_INTERRUPTS()    ulSetInterruptMask()
#define portENABLE_INTERRUPTS()     vClearInterruptMask( 0 )

#if ( configNUMBER_OF_CORES > 1 )
    #define portENTER_CRITICAL()               vTaskEnterCritical()
    #define portEXIT_CRITICAL()                vTaskExitCritical()
    #define portENTER_CRITICAL_FROM_ISR()      vTaskEnterCriticalFromISR()
    #define portEXIT_CRITICAL_FROM_ISR( x )    vTaskExitCriticalFromISR( x )
#endif

#define portRTOS_SPINLOCK_COUNT    2

#if PICO_SDK_VERSION_MAJOR < 2
__force_inline static bool spin_try_lock_unsafe( spin_lock_t * pxLock )
{
    return *pxLock;
}
#endif

static inline void vPortRecursiveLock( BaseType_t xCoreID,
                                       uint32_t ulLockNum,
                                       spin_lock_t * pxSpinLock,
                                       BaseType_t xAcquire )
{
    static volatile uint8_t ucOwnedByCore[ portMAX_CORE_COUNT ][ portRTOS_SPINLOCK_COUNT ];
    static volatile uint8_t ucRecursionCountByLock[ portRTOS_SPINLOCK_COUNT ];

    configASSERT( ulLockNum < portRTOS_SPINLOCK_COUNT );

    if( xAcquire != pdFALSE )
    {
        if( !spin_try_lock_unsafe( pxSpinLock ) )
        {
            if( ucOwnedByCore[ xCoreID ][ ulLockNum ] != 0U )
            {
                configASSERT( ucRecursionCountByLock[ ulLockNum ] != 255U );
                ucRecursionCountByLock[ ulLockNum ]++;
                return;
            }

            spin_lock_unsafe_blocking( pxSpinLock );
        }

        configASSERT( ucRecursionCountByLock[ ulLockNum ] == 0U );
        ucRecursionCountByLock[ ulLockNum ] = 1U;
        ucOwnedByCore[ xCoreID ][ ulLockNum ] = 1U;
    }
    else
    {
        configASSERT( ucOwnedByCore[ xCoreID ][ ulLockNum ] != 0U );
        configASSERT( ucRecursionCountByLock[ ulLockNum ] != 0U );

        ucRecursionCountByLock[ ulLockNum ]--;

        if( ucRecursionCountByLock[ ulLockNum ] == 0U )
        {
            ucOwnedByCore[ xCoreID ][ ulLockNum ] = 0U;
            spin_unlock_unsafe( pxSpinLock );
        }
    }
}

#if ( configNUMBER_OF_CORES == 1 )
    #define portGET_ISR_LOCK( xCoreID )
    #define portRELEASE_ISR_LOCK( xCoreID )
    #define portGET_TASK_LOCK( xCoreID )
    #define portRELEASE_TASK_LOCK( xCoreID )
#else
    #define portGET_ISR_LOCK( xCoreID )         vPortRecursiveLock( ( xCoreID ), 0, spin_lock_instance( configSMP_SPINLOCK_0 ), pdTRUE )
    #define portRELEASE_ISR_LOCK( xCoreID )     vPortRecursiveLock( ( xCoreID ), 0, spin_lock_instance( configSMP_SPINLOCK_0 ), pdFALSE )
    #define portGET_TASK_LOCK( xCoreID )        vPortRecursiveLock( ( xCoreID ), 1, spin_lock_instance( configSMP_SPINLOCK_1 ), pdTRUE )
    #define portRELEASE_TASK_LOCK( xCoreID )    vPortRecursiveLock( ( xCoreID ), 1, spin_lock_instance( configSMP_SPINLOCK_1 ), pdFALSE )
#endif
/*-----------------------------------------------------------*/

/* *INDENT-OFF* */
#ifdef __cplusplus
    }
#endif
/* *INDENT-ON* */

#endif /* PORTMACRO_H */
