/* File: startup_ARMCM3.S
 * Purpose: startup file for Cortex-M3 devices. Should use with
 *   GCC for ARM Embedded Processors
 * Version: V1.3
 * Date: 08 Feb 2012
 *
 * Copyright (c) 2012, ARM Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the ARM Limited nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL ARM LIMITED BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
    .syntax unified
    .arch armv7-m

#ifdef NXP_CRP
    .section .NXP.CRP
    .align 2
    .globl __nxp_crp
__nxp_crp:
    .long NXP_CRP
    .size __nxp_crp, . - __nxp_crp
#endif

    .section .stack
    .align 3
#ifdef __STACK_SIZE
    .equ    Stack_Size, __STACK_SIZE
#else
    .equ    Stack_Size, 0x400
#endif
    .globl    __StackTop
    .globl    __StackLimit
__StackLimit:
    .space    Stack_Size
    .size __StackLimit, . - __StackLimit
__StackTop:
    .size __StackTop, . - __StackTop

    .section .heap
    .align 3
#ifdef __HEAP_SIZE
    .equ    Heap_Size, __HEAP_SIZE
#else
    .equ    Heap_Size, 0xC00
#endif
    .globl    __HeapBase
    .globl    __HeapLimit
__HeapBase:
    .if    Heap_Size
    .space    Heap_Size
    .endif
    .size __HeapBase, . - __HeapBase
__HeapLimit:
    .size __HeapLimit, . - __HeapLimit

    .section .isr_vector
    .align 2
    .globl __isr_vector
__isr_vector:
    .long    __StackTop            /* Top of Stack */
    .long    Reset_Handler         /* Reset Handler */
    .long    NMI_Handler           /* NMI Handler */
    .long    HardFault_Handler     /* Hard Fault Handler */
    .long    MemManage_Handler     /* MPU Fault Handler */
    .long    BusFault_Handler      /* Bus Fault Handler */
    .long    UsageFault_Handler    /* Usage Fault Handler */
    .long    0                     /* Reserved */
    .long    0                     /* Reserved */
    .long    0                     /* Reserved */
    .long    0                     /* Reserved */
#if HAVE_FREERTOS
    .long    vPortSVCHandler       /* SVCall Handler */
#else
    .long    SVC_Handler           /* SVCall Handler */
#endif
    .long    DebugMon_Handler      /* Debug Monitor Handler */
    .long    0                     /* Reserved */
#if HAVE_FREERTOS
    .long    xPortPendSVHandler    /* PendSV Handler */
    .long    xPortSysTickHandler   /* SysTick Handler */
#else
    .long    PendSV_Handler        /* PendSV Handler */
    .long    SysTick_Handler       /* SysTick Handler */
#endif

    /* External interrupts */

#if defined(CHIP_LPC1343)
#define HAVE_LPC_IRQ_HANDLERS
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.0
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.1
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.2
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.3
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.4
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.5
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.6
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.7
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.8
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.9
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.10
    .long   WAKEUP_IRQHandler         // Wakeup PIO0.11
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.0
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.1
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.2
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.3
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.4
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.5
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.6
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.7
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.8
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.9
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.10
    .long   WAKEUP_IRQHandler         // Wakeup PIO1.11
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.0
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.1
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.2
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.3
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.4
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.5
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.6
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.7
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.8
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.9
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.10
    .long   WAKEUP_IRQHandler         // Wakeup PIO2.11
    .long   WAKEUP_IRQHandler         // Wakeup PIO3.0
    .long   WAKEUP_IRQHandler         // Wakeup PIO3.1
    .long   WAKEUP_IRQHandler         // Wakeup PIO3.2
    .long   WAKEUP_IRQHandler         // Wakeup PIO3.3
    .long   I2C_IRQHandler            // I2C
    .long   TIMER_16_0_IRQHandler     // 16-bit Counter-Timer 0
    .long   TIMER_16_1_IRQHandler     // 16-bit Counter-Timer 1
    .long   TIMER_32_0_IRQHandler     // 32-bit Counter-Timer 0
    .long   TIMER_32_1_IRQHandler     // 32-bit Counter-Timer 1
    .long   SSP0_IRQHandler           // SSP0
    .long   UART_IRQHandler           // UART
    .long   USB_IRQHandler            // USB IRQ
    .long   USB_FIQHandler            // USB FIQ
    .long   ADC_IRQHandler            // A/D Converter
    .long   WDT_IRQHandler            // Watchdog Timer
    .long   BOD_IRQHandler            // Brown Out Detect
    .long   FMC_IRQHandler            // IP2111 Flash Memory Controller
    .long   PIOINT3_IRQHandler        // PIO INT3
    .long   PIOINT2_IRQHandler        // PIO INT2
    .long   PIOINT1_IRQHandler        // PIO INT1
    .long   PIOINT0_IRQHandler        // PIO INT0
    .long   SSP1_IRQHandler           // SSP1
#endif

#ifdef CHIP_LPC1347
#define HAVE_LPC_IRQ_HANDLERS
    .long   PIN_INT0_IRQHandler       // All GPIO pin can be routed to PIN_INTx
    .long   PIN_INT1_IRQHandler          
    .long   PIN_INT2_IRQHandler                       
    .long   PIN_INT3_IRQHandler                         
    .long   PIN_INT4_IRQHandler                        
    .long   PIN_INT5_IRQHandler
    .long   PIN_INT6_IRQHandler
    .long   PIN_INT7_IRQHandler                       
    .long   GINT0_IRQHandler                         
    .long   GINT1_IRQHandler          // PIO0 (0:7)              
    .long   Reserved_IRQHandler
    .long   Reserved_IRQHandler
    .long   RIT_IRQHandler       
    .long   Reserved_IRQHandler
    .long   SSP1_IRQHandler           // SSP1
    .long   I2C_IRQHandler            // I2C
    .long   TIMER_16_0_IRQHandler     // 16-bit Counter-Timer 0
    .long   TIMER_16_1_IRQHandler     // 16-bit Counter-Timer 1
    .long   TIMER_32_0_IRQHandler     // 32-bit Counter-Timer 0
    .long   TIMER_32_1_IRQHandler     // 32-bit Counter-Timer 1
    .long   SSP0_IRQHandler           // SSP0
    .long   UART_IRQHandler           // UART
    .long   USB_IRQHandler            // USB IRQ
    .long   USB_FIQHandler            // USB FIQ
    .long   ADC_IRQHandler            // A/D Converter
    .long   WDT_IRQHandler            // Watchdog Timer
    .long   BOD_IRQHandler            // Brown Out Detect
    .long   FMC_IRQHandler            // IP2111 Flash Memory Controller
    .long   OSCFAIL_IRQHandler        // OSC FAIL
    .long   PVTCIRCUIT_IRQHandler     // PVT CIRCUIT
    .long   USBWakeup_IRQHandler      // USB wake up
    .long   Reserved_IRQHandler
#endif

#if defined(CHIP_LPC175X_6X) || defined(CHIP_LPC177X_8X)
#define HAVE_LPC_IRQ_HANDLERS
    .long   WDT_IRQHandler              /* 16: Watchdog Timer               */
    .long   TIMER0_IRQHandler           /* 17: Timer0                       */
    .long   TIMER1_IRQHandler           /* 18: Timer1                       */
    .long   TIMER2_IRQHandler           /* 19: Timer2                       */
    .long   TIMER3_IRQHandler           /* 20: Timer3                       */
    .long   UART0_IRQHandler            /* 21: UART0                        */
    .long   UART1_IRQHandler            /* 22: UART1                        */
    .long   UART2_IRQHandler            /* 23: UART2                        */
    .long   UART3_IRQHandler            /* 24: UART3                        */
    .long   PWM1_IRQHandler             /* 25: PWM1                         */
    .long   I2C0_IRQHandler             /* 26: I2C0                         */
    .long   I2C1_IRQHandler             /* 27: I2C1                         */
    .long   I2C2_IRQHandler             /* 28: I2C2                         */
    .long   SPI_IRQHandler              /* 29: SPI                          */
    .long   SSP0_IRQHandler             /* 30: SSP0                         */
    .long   SSP1_IRQHandler             /* 31: SSP1                         */
    .long   PLL0_IRQHandler             /* 32: PLL0 Lock (Main PLL)         */
    .long   RTC_IRQHandler              /* 33: Real Time Clock              */
    .long   EINT0_IRQHandler            /* 34: External Interrupt 0         */
    .long   EINT1_IRQHandler            /* 35: External Interrupt 1         */
    .long   EINT2_IRQHandler            /* 36: External Interrupt 2         */
    .long   EINT3_IRQHandler            /* 37: External Interrupt 3         */
    .long   ADC_IRQHandler              /* 38: A/D Converter                */
    .long   BOD_IRQHandler              /* 39: Brown-Out Detect             */
    .long   USB_IRQHandler              /* 40: USB                          */
    .long   CAN_IRQHandler              /* 41: CAN                          */
    .long   DMA_IRQHandler              /* 42: General Purpose DMA          */
    .long   I2S_IRQHandler              /* 43: I2S                          */
    .long   ENET_IRQHandler             /* 44: Ethernet                     */
#if defined(CHIP_LPC175X_6X)
    .long   RIT_IRQHandler              /* 45: Repetitive Interrupt Timer   */
#else
    .long   SDIO_IRQHandler 		/* 45: SD/MMC card I/F              */
#endif
    .long   MCPWM_IRQHandler            /* 46: Motor Control PWM            */
    .long   QEI_IRQHandler              /* 47: Quadrature Encoder Interface */
    .long   PLL1_IRQHandler             /* 48: PLL1 Lock (USB PLL)          */
    .long   USBActivity_IRQHandler	/* 49: USB Activity                 */
    .long   CANActivity_IRQHandler	/* 50: CAN Activity                 */
#if defined(CHIP_LPC177X_8X)
    .long   UART4_IRQHandler            /* 51: UART4                        */
    .long   SSP2_IRQHandler             /* 52: SSP2                         */
    .long   LCD_IRQHandler              /* 53: LCD                          */
    .long   GPIO_IRQHandler             /* 54: GPIO                         */
    .long   PWM0_IRQHandler             /* 55: PWM0                         */
    .long   EEPROM_IRQHandler           /* 56: EEPROM                       */
#endif
#endif

#ifndef HAVE_LPC_IRQ_HANDLERS
#error "No LPC IRQ handlers defined for this chip type."
#endif

    .size    __isr_vector, . - __isr_vector

    .text
    .thumb
    .thumb_func
    .align 2
    .globl    Reset_Handler
    .type    Reset_Handler, %function
Reset_Handler:
/*     Loop to copy data from read only memory to RAM. The ranges
 *      of copy from/to are specified by following symbols evaluated in
 *      linker script.
 *      __etext: End of code section, i.e., begin of data sections to copy from.
 *      __data_start__/__data_end__: RAM address range that data should be
 *      copied to. Both must be aligned to 4 bytes boundary.  */

    ldr    r1, =__etext
    ldr    r2, =__data_start__
    ldr    r3, =__data_end__

#if 1
/* Here are two copies of loop implemenations. First one favors code size
 * and the second one favors performance. Default uses the first one.
 * Change to "#if 0" to use the second one */
.flash_to_ram_loop:
    cmp     r2, r3
    ittt    lt
    ldrlt   r0, [r1], #4
    strlt   r0, [r2], #4
    blt    .flash_to_ram_loop
#else
    subs    r3, r2
    ble    .flash_to_ram_loop_end
.flash_to_ram_loop:
    subs    r3, #4
    ldr    r0, [r1, r3]
    str    r0, [r2, r3]
    bgt    .flash_to_ram_loop
.flash_to_ram_loop_end:
#endif

#ifndef __NO_SYSTEM_INIT
    ldr    r0, =SystemInit
    blx    r0
#endif

    ldr    r0, =_start
    bx    r0
    .pool
    .size Reset_Handler, . - Reset_Handler

/*    Macro to define default handlers. Default handler
 *    will be weak symbol and just dead loops. They can be
 *    overwritten by other handlers */
    .macro    def_irq_handler    handler_name
    .align 1
    .thumb_func
    .weak    \handler_name
    .type    \handler_name, %function
\handler_name :
    b    .
    .size    \handler_name, . - \handler_name
    .endm

    def_irq_handler    NMI_Handler
    def_irq_handler    HardFault_Handler
    def_irq_handler    MemManage_Handler
    def_irq_handler    BusFault_Handler
    def_irq_handler    UsageFault_Handler
    def_irq_handler    SVC_Handler
    def_irq_handler    DebugMon_Handler
    def_irq_handler    PendSV_Handler
    def_irq_handler    SysTick_Handler
    def_irq_handler    Default_Handler

#if defined(CHIP_LPC1343)
    def_irq_handler   WAKEUP_IRQHandler         // Wakeup
    def_irq_handler   I2C_IRQHandler            // I2C
    def_irq_handler   TIMER_16_0_IRQHandler     // 16-bit Counter-Timer 0
    def_irq_handler   TIMER_16_1_IRQHandler     // 16-bit Counter-Timer 1
    def_irq_handler   TIMER_32_0_IRQHandler     // 32-bit Counter-Timer 0
    def_irq_handler   TIMER_32_1_IRQHandler     // 32-bit Counter-Timer 1
    def_irq_handler   SSP0_IRQHandler           // SSP0
    def_irq_handler   UART_IRQHandler           // UART
    def_irq_handler   USB_IRQHandler            // USB IRQ
    def_irq_handler   USB_FIQHandler            // USB FIQ
    def_irq_handler   ADC_IRQHandler            // A/D Converter
    def_irq_handler   WDT_IRQHandler            // Watchdog Timer
    def_irq_handler   BOD_IRQHandler            // Brown Out Detect
    def_irq_handler   FMC_IRQHandler            // IP2111 Flash Memory Controller
    def_irq_handler   PIOINT3_IRQHandler        // PIO INT3
    def_irq_handler   PIOINT2_IRQHandler        // PIO INT2
    def_irq_handler   PIOINT1_IRQHandler        // PIO INT1
    def_irq_handler   PIOINT0_IRQHandler        // PIO INT0
    def_irq_handler   SSP1_IRQHandler           // SSP1
#endif

#ifdef CHIP_LPC1347
    def_irq_handler   PIN_INT0_IRQHandler       // All GPIO pin can be routed to PIN_INTx
    def_irq_handler   PIN_INT1_IRQHandler          
    def_irq_handler   PIN_INT2_IRQHandler                       
    def_irq_handler   PIN_INT3_IRQHandler                         
    def_irq_handler   PIN_INT4_IRQHandler                        
    def_irq_handler   PIN_INT5_IRQHandler
    def_irq_handler   PIN_INT6_IRQHandler
    def_irq_handler   PIN_INT7_IRQHandler                       
    def_irq_handler   GINT0_IRQHandler                         
    def_irq_handler   GINT1_IRQHandler          // PIO0 (0:7)              
    def_irq_handler   RIT_IRQHandler
    def_irq_handler   SSP1_IRQHandler           // SSP1
    def_irq_handler   I2C_IRQHandler            // I2C
    def_irq_handler   TIMER_16_0_IRQHandler     // 16-bit Counter-Timer 0
    def_irq_handler   TIMER_16_1_IRQHandler     // 16-bit Counter-Timer 1
    def_irq_handler   TIMER_32_0_IRQHandler     // 32-bit Counter-Timer 0
    def_irq_handler   TIMER_32_1_IRQHandler     // 32-bit Counter-Timer 1
    def_irq_handler   SSP0_IRQHandler           // SSP0
    def_irq_handler   UART_IRQHandler           // UART
    def_irq_handler   USB_IRQHandler            // USB IRQ
    def_irq_handler   USB_FIQHandler            // USB FIQ
    def_irq_handler   ADC_IRQHandler            // A/D Converter
    def_irq_handler   WDT_IRQHandler            // Watchdog Timer
    def_irq_handler   BOD_IRQHandler            // Brown Out Detect
    def_irq_handler   FMC_IRQHandler            // IP2111 Flash Memory Controller
    def_irq_handler   OSCFAIL_IRQHandler        // OSC FAIL
    def_irq_handler   PVTCIRCUIT_IRQHandler     // PVT CIRCUIT
    def_irq_handler   USBWakeup_IRQHandler      // USB wake up
    def_irq_handler   Reserved_IRQHandler
#endif

#if defined(CHIP_LPC175X_6X) || defined(CHIP_LPC177X_8X)
    def_irq_handler     WDT_IRQHandler
    def_irq_handler     TIMER0_IRQHandler
    def_irq_handler     TIMER1_IRQHandler
    def_irq_handler     TIMER2_IRQHandler
    def_irq_handler     TIMER3_IRQHandler
    def_irq_handler     UART0_IRQHandler
    def_irq_handler     UART1_IRQHandler
    def_irq_handler     UART2_IRQHandler
    def_irq_handler     UART3_IRQHandler
    def_irq_handler     PWM1_IRQHandler
    def_irq_handler     I2C0_IRQHandler
    def_irq_handler     I2C1_IRQHandler
    def_irq_handler     I2C2_IRQHandler
    def_irq_handler     SPI_IRQHandler
    def_irq_handler     SSP0_IRQHandler
    def_irq_handler     SSP1_IRQHandler
    def_irq_handler     PLL0_IRQHandler
    def_irq_handler     RTC_IRQHandler
    def_irq_handler     EINT0_IRQHandler
    def_irq_handler     EINT1_IRQHandler
    def_irq_handler     EINT2_IRQHandler
    def_irq_handler     EINT3_IRQHandler
    def_irq_handler     ADC_IRQHandler
    def_irq_handler     BOD_IRQHandler
    def_irq_handler     USB_IRQHandler
    def_irq_handler     CAN_IRQHandler
    def_irq_handler     DMA_IRQHandler
    def_irq_handler     I2S_IRQHandler
    def_irq_handler     ENET_IRQHandler
#if defined(CHIP_LPC175X_6X)
    def_irq_handler     RIT_IRQHandler
#else
    def_irq_handler     SDIO_IRQHandler
#endif
    def_irq_handler     MCPWM_IRQHandler
    def_irq_handler     QEI_IRQHandler
    def_irq_handler     PLL1_IRQHandler
    def_irq_handler     USBActivity_IRQHandler
    def_irq_handler     CANActivity_IRQHandler
#if defined(CHIP_LPC177X_8X)
    def_irq_handler     UART4_IRQHandler
    def_irq_handler     SSP2_IRQHandler
    def_irq_handler     LCD_IRQHandler
    def_irq_handler     GPIO_IRQHandler
    def_irq_handler     PWM0_IRQHandler
    def_irq_handler     EEPROM_IRQHandler
#endif
#endif

    .end
