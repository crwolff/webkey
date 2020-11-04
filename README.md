# webkey
This project combines a web server and a USB keyboard to allow control over the boot sequence of a dual-boot machine.

The key sequence set in response to a POST command is deliberately limited to a sequence of F8's, 0-3 Down Arrows, followed by Enter. This enters the boot selection window, and blindly selects one of the items.

The code could be extended to allow more control over the key sequence being sent, but this is a fairly severe security hole. A malicious user could then send any sequence of keystrokes to your computer.

```
get_idf
idf.py set-target esp32s2
idf.py menuconfig
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

To use programatically:
```
curl -X POST http://webkey/ctrl?key=b0
curl -X POST http://webkey/ctrl?key=b1
curl -X POST http://webkey/ctrl?key=b2
curl -X POST http://webkey/ctrl?key=b3
```

There is also a lovely web page at http://webkey/index.html that provides pushbuttons.
