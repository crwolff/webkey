idf_component_register(SRCS "main.c" "wifi_init_sta.c" "web_server.c" "usb_init.c" "usb_descriptors.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "www-data/favicon.ico" "www-data/index.html" "www-data/update.html"
)

target_compile_options(${COMPONENT_TARGET} PUBLIC
  "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2"
)

# TOP is absolute path to root directory of TinyUSB git repo
set(TOP "../tinyusb")
get_filename_component(TOP "${TOP}" REALPATH)

idf_component_get_property( FREERTOS_ORIG_INCLUDE_PATH freertos ORIG_INCLUDE_PATH)
target_include_directories(${COMPONENT_TARGET} PUBLIC
  "${FREERTOS_ORIG_INCLUDE_PATH}"
  "${TOP}/hw"
  "${TOP}/src"
)

target_sources(${COMPONENT_TARGET} PUBLIC
  "${TOP}/src/tusb.c"
  "${TOP}/src/common/tusb_fifo.c"
  "${TOP}/src/device/usbd.c"
  "${TOP}/src/device/usbd_control.c"
  "${TOP}/src/class/cdc/cdc_device.c"
  "${TOP}/src/class/dfu/dfu_rt_device.c"
  "${TOP}/src/class/hid/hid_device.c"
  "${TOP}/src/class/midi/midi_device.c"
  "${TOP}/src/class/msc/msc_device.c"
  "${TOP}/src/class/net/net_device.c"
  "${TOP}/src/class/usbtmc/usbtmc_device.c"
  "${TOP}/src/class/vendor/vendor_device.c"
  "${TOP}/src/portable/espressif/esp32s2/dcd_esp32s2.c"
)
