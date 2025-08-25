This is a sketch for A Zigbee comor dimmable light, using a M5Stack Nano 26.
THIS IS THE DEVICE: https://docs.m5stack.com/en/core/M5NanoC6

Coded using the Arduino IDE

Under Boards Manager, Make sure you are supporting the board: 
https://github.com/m5stack

Includes: 
* Zigbee.h from the M5Stack hardware package
* Adafruit_NeoPixel.h fromt he Library Manager

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
