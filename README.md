# webkey
This project combines a web server and a USB keyboard to allow control over the boot sequence of a dual-boot machine.

The key sequence set in response to a POST command is deliberately limited to a sequence of F8's, 0-3 Down Arrows, followed by Enter. This enters the boot selection window, and blindly selects one of the items.

The code could be extended to allow more control over the key sequence being sent, but this is a fairly severe security hole. A malicious user could then send any sequence of keystrokes to your computer.

## Tool chain
```
#rm -rf esp-idf ~/.espressif
git clone -bv4.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git reset --hard 178b122
git submodule update --init --recursive
bash ./install.sh
```

## Project configuration
```
source ../esp-idf/export.sh
idf.py set-target esp32s2
idf.py menuconfig
-> Webkey Configuration -> SSID/Password
-> Compont -> LWIP -> netif hostname
```

## Build instructions
```
source ../esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 erase_flash
idf.py -p /dev/ttyUSB0 flash monitor
```

## JTAG wiring
| Wire Color | Saola Pin    | WROOM Name | JTAG             | JTAG  | JTAG           | WROOM Name | Saola Pin     | Wire Color |
|:----------:|:------------:|:----------:|:----------------:|:-----:|:--------------:|:----------:|:-------------:|:----------:|
| Orange     | J2&#x2011;1  | 3.3V       | 1&#x2011;VtRef   |       | 2&#x2011;NC    |            |               |            |
|            |              |            | 3&#x2011;nTRST   |       | 4&#x2011;GND   |            |               |            |
| Yellow     | J3&#x2011;8  | IO41       | 5&#x2011;TDI     |       | 6&#x2011;GND   |            |               |            |
| Green      | J3&#x2011;7  | IO42       | 7&#x2011;TMS     |       | 8&#x2011;GND   |            |               |            |
| Blue       | J3&#x2011;10 | IO39       | 9&#x2011;TCK     | <-key | 10&#x2011;GND  |            |               |            |
|            |              |            | 11&#x2011;RTCK   | <-key | 13&#x2011;GND  |            |               |            |
| Purple     | J3&#x2011;9  | IO40       | 13&#x2011;TDO    |       | 14&#x2011;GND  |            |               |            |
| Grey       | J3&#x2011;2  | nRST       | 15&#x2011;RESET  |       | 16&#x2011;GND  |            |               |            |
|            |              |            | 17&#x2011;DBGRQ  |       | 18&#x2011;GND  |            |               |            |
| White      | J2&#x2011;20 | VDD        | 19&#x2011;VDD    |       | 20&#x2011;GND  | GND        | J2&#x2011;21  | Black      |

## OpenOCD use
Two iterations of init are required to power up the board prior to programming
```
openocd -f interface/jlink.cfg -f target/esp32s2.cfg -c"adapter_khz 1000; init; jlink targetpower on; exit"
openocd -f interface/jlink.cfg -f target/esp32s2.cfg -c"adapter_khz 1000; init; reset halt; program_esp build/webkey.bin 0x10000 verify exit"
```

## Operation
To use programatically:
```
curl -X POST http://webkey/ctrl?key=b1
curl -X POST http://webkey/ctrl?key=b2
curl -X POST http://webkey/ctrl?key=b3
curl -X POST http://webkey/ctrl?key=b4
```

There is also a lovely web page at http://webkey/index.html that provides pushbuttons.
