/*
 * Copyright (C) 2024 Kieran Simkin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ESPNoiseSource.h"
#include "RNG.h"
#include "Crypto.h"
#include <Arduino.h>
#include "esp_phy_init.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "phy.h"
#include "phy_init_data.h"
#include "esp_private/wifi.h"
/**
 * \class ESPNoiseSource ESPNoiseSource.h <ESPNoiseSource.h>
 * \brief Processes the signal from a ESP-based noise source.
 *
/*

Theory of operation:

From the ESP32 technical manual:

25.3 Functional Description
Every 32-bit value that the system reads from the RNG_DATA_REG register of the random number generator is a
true random number. These true random numbers are generated based on the thermal noise in the system and
the asynchronous clock mismatch.
Thermal noise comes from the high-speed ADC or SAR ADC or both. Whenever the high-speed ADC or SAR ADC
is enabled, bit streams will be generated and fed into the random number generator through an XOR logic gate as
random seeds.

When there is noise coming from the SAR ADC, the random number generator is fed with a 2-bit entropy in one
clock cycle of RC_FAST_CLK (8 MHz), which is generated from an internal RC oscillator (see Chapter Reset and
Clock for details). Thus, it is advisable to read the RNG_DATA_REG register at a maximum rate of 500 kHz to
obtain the maximum entropy.
When there is noise coming from the high-speed ADC, the random number generator is fed with a 2-bit entropy
in one APB clock cycle, which is normally 80 MHz. Thus, it is advisable to read the RNG_DATA_REG register at a
maximum rate of 5 MHz to obtain the maximum entropy.
A data sample of 2 GB, which is read from the random number generator at a rate of 5 MHz with only the highspeed ADC being enabled, has been tested using the Dieharder Random Number Testsuite (version 3.31.1). The
sample passed all tests.

When using the random number generator, make sure at least either the SAR ADC or high-speed ADC is enabled.
Otherwise, pseudo-random numbers will be returned.
• SAR ADC can be enabled by using the DIG ADC controller. For details, please refer to Chapter 29 On-Chip
Sensors and Analog Signal Processing.
• High-speed ADC is enabled automatically when the Wi-Fi or Bluetooth modules is enabled.
Note:
Note that, when the Wi-Fi module is enabled, the value read from the high-speed ADC can be saturated in some extreme
cases, which lowers the entropy. Thus, it is advisable to also enable the SAR ADC as the noise source for the random
number generator for such cases.


TLDR - If you don't have Wifi or Bluetooth enabled, you're definitely going to want to enableDigitalADC 

*/
#include <driver/adc_common.h>

static uint32_t s_module_phy_rf_init = 0;

/**
 * \brief Constructs a new ESP-based noise source handler.
 */
ESPNoiseSource::ESPNoiseSource(bool enableDigitalADC=true) : enableDigitalADC(enableDigitalADC)
{
    #if defined(ESP32)    
        if (enableDigitalADC) {
 
            #if CONFIG_IDF_TARGET_ESP32C3
                adc_digi_initialize(adc_init);
                           adc_digi_init_config_t adc_init = { 
                adc1_chan_mask= ADC1_CHANNEL_MAX,
                adc2_chan_mask =ADC2_CHANNEL_MAX,
                max_store_buf_size= ADC_WIDTH_MAX-1,
                conv_num_each_intr = 2
                };
            #endif
            adc_digi_start()

        }
    #endif  
}

ESPNoiseSource::~ESPNoiseSource()
{
    #if defined(ESP32)    
        if (enableDigitalADC) adc_digi_stop();
    #endif
    
}

void ESPNoiseSource::added()
{
    // I was going to stir in the serial numbers here but it's already done in the base class :)
}

void ESPNoiseSource::stir()
{
    uint32_t s_module_phy_rf_init_old = s_module_phy_rf_init;
    uint32_t phy_bt_wifi_mask = BIT(PHY_BT_MODULE) | BIT(PHY_WIFI_MODULE);
    bool is_wifi_or_bt_enabled = !!(s_module_phy_rf_init_old & phy_bt_wifi_mask);
    if (!is_wifi_or_bt_enabled && !enableDigitalADC) return;
    uint8_t buffer[32];
    esp_fill_random(buffer,sizeof(buffer));
    output(buffer,sizeof(buffer),sizeof(buffer)*2);
}

