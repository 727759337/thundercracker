/*
 * Thundercracker Firmware -- Confidential, not for redistribution.
 * Copyright <c> 2012 Sifteo, Inc. All rights reserved.
 */

#include "macros.h"
#include "usart.h"
#include "flash_stack.h"
#include "hardware.h"
#include "board.h"
#include "gpio.h"
#include "systime.h"
#include "radio.h"
#include "tasks.h"
#include "audiomixer.h"
#include "audiooutdevice.h"
#include "volume.h"
#include "usb/usbdevice.h"
#include "homebutton.h"
#include "svmloader.h"
#include "powermanager.h"
#include "crc.h"
#include "sampleprofiler.h"
#include "bootloader.h"
#include "cubeconnector.h"
#include "neighbor_tx.h"
#include "led.h"
#include "batterylevel.h"

static void ensureMinimumBatteryLevel();

/*
 * Application specific entry point.
 * All low level init is done in setup.cpp.
 */
int main()
{
    /*
     * Nested Vectored Interrupt Controller setup.
     *
     * This won't actually enable any peripheral interrupts yet, since
     * those need to be unmasked by the peripheral's driver code.
     *
     * If we've gotten bootloaded, relocate the vector table to account
     * for offset at which we're placed into MCU flash.
     */

#ifdef BOOTLOADABLE
    NVIC.setVectorTable(NVIC.VectorTableFlash, Bootloader::SIZE);
#endif

    NVIC.irqEnable(IVT.RF_EXTI_VEC);                // Radio interrupt
    NVIC.irqPrioritize(IVT.RF_EXTI_VEC, 0x80);      //  Reduced priority

    NVIC.irqEnable(IVT.DMA2_Channel1);              // Radio SPI DMA2 channels 1 & 2
    NVIC.irqPrioritize(IVT.DMA1_Channel1, 0x75);    //  higher than radio
    NVIC.irqEnable(IVT.DMA2_Channel2);
    NVIC.irqPrioritize(IVT.DMA1_Channel2, 0x75);

    NVIC.irqEnable(IVT.FLASH_DMA_CHAN_RX);          // Flash SPI DMA channels
    NVIC.irqPrioritize(IVT.FLASH_DMA_CHAN_RX, 0x75);//  higher than radio
    NVIC.irqEnable(IVT.FLASH_DMA_CHAN_TX);
    NVIC.irqPrioritize(IVT.FLASH_DMA_CHAN_TX, 0x75);

    NVIC.irqEnable(IVT.UsbOtg_FS);
    NVIC.irqPrioritize(IVT.UsbOtg_FS, 0x70);        //  A little higher than radio

    NVIC.irqEnable(IVT.BTN_HOME_EXTI_VEC);          //  home button

    NVIC.irqEnable(IVT.AUDIO_SAMPLE_TIM);           // sample rate timer
    NVIC.irqPrioritize(IVT.AUDIO_SAMPLE_TIM, 0x50); //  pretty high priority! (would cause audio jitter)

    NVIC.irqEnable(IVT.LED_SEQUENCER_TIM);          // LED sequencer timer
    NVIC.irqPrioritize(IVT.LED_SEQUENCER_TIM, 0x75);

    NVIC.irqEnable(IVT.USART3);                     // factory test uart
    NVIC.irqPrioritize(IVT.USART3, 0x99);           //  loooooowest prio

    NVIC.irqEnable(IVT.VOLUME_TIM);                 // volume timer
    NVIC.irqPrioritize(IVT.VOLUME_TIM, 0x55);       //  just below sample rate timer

    NVIC.irqEnable(IVT.PROFILER_TIM);               // sample profiler timer
    NVIC.irqPrioritize(IVT.PROFILER_TIM, 0x0);      //  highest possible priority

    NVIC.irqEnable(IVT.NBR_TX_TIM);                 // Neighbor transmit
    NVIC.irqPrioritize(IVT.NBR_TX_TIM, 0x60);       //  just below sample rate timer

    /*
     * For SVM to operate properly, SVC needs to have a very low priority
     * (we'll be inside it most of the time) and any fault handlers which have
     * meaning in userspace need to be higher priority than it.
     *
     * We disable the local fault handlers, (should be disabled by default anyway)
     * so all faults will get routed through HardFault and handled by SvmCpu.
     */

    NVIC.sysHandlerPrioritize(IVT.SVCall, 0x96);
    NVIC.sysHandlerPrioritize(IVT.HardFault, 0x00);
    NVIC.sysHandlerControl = 0;

    /*
     * High-level hardware initialization
     *
     * Avoid reinitializing periphs that the bootloader has already init'd.
     */
#ifndef BOOTLOADER
    SysTime::init();
    PowerManager::init();
    Crc32::init();
#endif

    // This is the earliest point at which it's safe to use Usart::Dbg.
    Usart::Dbg.init(UART_RX_GPIO, UART_TX_GPIO, 115200);

#ifdef REV2_GDB_REWORK
    DBGMCU_CR |= (1 << 30) |        // TIM14 stopped when core is halted
                 (1 << 29) |        // TIM13 ""
                 (1 << 28) |        // TIM12 ""
                 (1 << 27) |        // TIM11 ""
                 (1 << 26) |        // TIM10 ""
                 (1 << 25) |        // TIM9 ""
                 (1 << 20) |        // TIM8 ""
                 (1 << 19) |        // TIM7 ""
                 (1 << 18) |        // TIM6 ""
                 (1 << 17) |        // TIM5 ""
                 (1 << 13) |        // TIM4 ""
                 (1 << 12) |        // TIM3 ""
                 (1 << 11) |        // TIM2 ""
                 (1 << 10);         // TIM1 ""
#endif

    LED::init();
    Tasks::init();
    FlashStack::init();
    HomeButton::init();
    NeighborTX::init();

    /*
     * NOTE: NeighborTX & BatteryLevel share a timer - Battery level expects
     *      it to have already been init'd.
     *
     *      Also, CubeConnector is currently the only system to initiate neighbor
     *      transmissions, so wait until we do our battery check before init'ing
     *      him to avoid any conflicts.
     */

    BatteryLevel::init();
    if (PowerManager::state() == PowerManager::BatteryPwr) {
        ensureMinimumBatteryLevel();
    }

    // wait until after we know we're going to continue starting up before
    // showing signs of life :)
    UART(("Firmware " TOSTRING(SDK_VERSION) "\r\n"));

    CubeConnector::init();

    Volume::init();
    AudioOutDevice::init();
    AudioOutDevice::start();

    PowerManager::beginVbusMonitor();
    SampleProfiler::init();

    // Includes radio power-on delay. Initialize this last.
    Radio::init();

    /*
     * Start the game runtime, and execute the Launcher app.
     */

    SvmLoader::runLauncher();
}

void ensureMinimumBatteryLevel()
{
    /*
     * Kick off our first sample and wait for it to complete.
     */

    BatteryLevel::beginCapture();

    int batteryLevel;
    do {
        batteryLevel = BatteryLevel::currentLevel();
    } while (batteryLevel == BatteryLevel::UNINITIALIZED);

    /*
     * To be a bit conservative, we assume that any sample we take is skewed by
     * our MAX_JITTER in the positive direction. Correct for this, and see if
     * we're still above the required thresh to continue powering on.
     *
     * If not, shut ourselves down, and hope our batteries get replaced soon.
     */
    if (batteryLevel - BatteryLevel::MAX_JITTER < BatteryLevel::STARTUP_THRESHOLD) {

        PowerManager::batteryPowerOff();
        /*
         * wait to for power to drain. if somebody keeps their finger
         * on the homebutton, we may be here a little while, so don't
         * get zapped by the watchdog on our way out
         */
        for (;;) {
            Atomic::Barrier();
            Tasks::resetWatchdog();
        }
    }
}
