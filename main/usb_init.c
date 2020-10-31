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

//--------------------------------------------------------------------+
// Main
//--------------------------------------------------------------------+

void usb_init(void)
{
//  board_init();

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

void hid_task(void* param)
{
  (void) param;

  while(1)
  {
    // Poll every 10ms
    vTaskDelay(pdMS_TO_TICKS(10));

    // TODO: Do something(s) in response to POST on webpage
    uint32_t const btn = 0; // board_button_read();

    // Remote wakeup
    if ( tud_suspended() && btn )
    {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      tud_remote_wakeup();
    }

#ifdef NEVER
    /*------------- Mouse -------------*/
    if ( tud_hid_ready() )
    {
      if ( btn )
      {
        int8_t const delta = 5;

        // no button, right + down, no scroll pan
        tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta, delta, 0, 0);

        // delay a bit before attempt to send keyboard report
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
#endif

    /*------------- Keyboard -------------*/
    if ( tud_hid_ready() )
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_key = false;

      if ( btn )
      {
        uint8_t keycode[6] = { 0 };
        keycode[0] = HID_KEY_A;

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);

        has_key = true;
      }else
      {
        // send empty key report if previously has key pressed
        if (has_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        has_key = false;
      }
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

