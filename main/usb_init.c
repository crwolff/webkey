/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "bsp/board.h"
#include "tusb.h"

#include "usb_descriptors.h"

#include "esp_rom_gpio.h"
#include "hal/gpio_ll.h"
#include "hal/usb_hal.h"
#include "soc/usb_periph.h"

#include "driver/periph_ctrl.h"
#include "driver/rmt.h"

//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

// static task for usbd
#define USBD_STACK_SIZE     (3*configMINIMAL_STACK_SIZE/2)
StackType_t  usb_device_stack[USBD_STACK_SIZE];
StaticTask_t usb_device_taskdef;

// static task for hid
#define HID_STACK_SZIE      configMINIMAL_STACK_SIZE
StackType_t  hid_stack[HID_STACK_SZIE];
StaticTask_t hid_taskdef;

void usb_device_task(void* param);
void hid_task(void* params);

extern const char *TAG;

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

static void configure_pins(usb_hal_context_t *usb)
{
  /* usb_periph_iopins currently configures USB_OTG as USB Device.
   * Introduce additional parameters in usb_hal_context_t when adding support
   * for USB Host.
   */
  for (const usb_iopin_dsc_t *iopin = usb_periph_iopins; iopin->pin != -1; ++iopin) {
    if ((usb->use_external_phy) || (iopin->ext_phy_only == 0)) {
      esp_rom_gpio_pad_select_gpio(iopin->pin);
      if (iopin->is_output) {
        esp_rom_gpio_connect_out_signal(iopin->pin, iopin->func, false, false);
      } else {
        esp_rom_gpio_connect_in_signal(iopin->pin, iopin->func, false);
        if ((iopin->pin != GPIO_FUNC_IN_LOW) && (iopin->pin != GPIO_FUNC_IN_HIGH)) {
          gpio_ll_input_enable(&GPIO, iopin->pin);
        }
      }
      esp_rom_gpio_pad_unhold(iopin->pin);
    }
  }
  if (!usb->use_external_phy) {
    gpio_set_drive_capability(USBPHY_DM_NUM, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(USBPHY_DP_NUM, GPIO_DRIVE_CAP_3);
  }
}

void usb_init(void)
{
  // USB Controller Hal init
  periph_module_reset(PERIPH_USB_MODULE);
  periph_module_enable(PERIPH_USB_MODULE);

  usb_hal_context_t hal = {
    .use_external_phy = false // use built-in PHY
  };
  usb_hal_init(&hal);
  configure_pins(&hal);

  // Create a task for tinyusb device stack
  (void) xTaskCreateStatic( usb_device_task, "usbd", USBD_STACK_SIZE, NULL, configMAX_PRIORITIES-1, usb_device_stack, &usb_device_taskdef);

  // Create HID task
  (void) xTaskCreateStatic( hid_task, "hid", HID_STACK_SZIE, NULL, configMAX_PRIORITIES-2, hid_stack, &hid_taskdef);
}

// USB Device Driver task
// This top level thread process all usb events and invoke callbacks
void usb_device_task(void* param)
{
  (void) param;

  // This should be called after scheduler/kernel is started.
  // Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
  tusb_init();

  // RTOS forever loop
  while (1)
  {
    // tinyusb device task
    tud_task();
  }
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
uint32_t button_pressed = 0;

void hid_task(void* param)
{
  uint8_t sequence;
  uint8_t key_active;
  (void) param;

  while(1)
  {
    // Poll every 100ms
    vTaskDelay(pdMS_TO_TICKS(100));

    // Wait for command from web
    if ( button_pressed ) {
        // Record command so user can't change it mid-stream
        uint32_t const btn = button_pressed;

        // Send key sequence 20xSpace,(if b2) 2xDWN,1xENTER
        sequence = 43;
        key_active = 0;
        for(int i=0;(i < 1000) && (sequence != 0);i++) {
            if ( tud_suspended() ) {
                // Wake up host if we are in suspend mode
                // and REMOTE_WAKEUP feature is enabled by host
                tud_remote_wakeup();
            }
            if ( tud_hid_boot_mode() ) {
            }

            // Send next keypress in sequence
            if ( tud_hid_ready() ) {
                if ( key_active == 0 ) {
                    uint8_t keycode[6] = { 0 };
                    if ( sequence == 1 ) {
                        keycode[0] = HID_KEY_RETURN;
                    } else if ( sequence <= btn ) {
                        keycode[0] = HID_KEY_ARROW_DOWN;
                    } else {
                        keycode[0] = HID_KEY_SPACE;
                    }
                    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
                    key_active = 1;
                    vTaskDelay(pdMS_TO_TICKS(10));
                } else {
                    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
                    sequence = sequence - 1;
                    key_active = 0;
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        // Clear command and discard any that occurred during execution
        button_pressed = 0;
    }
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // TODO set LED based on CAPLOCK, NUMLOCK etc...
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}

