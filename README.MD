## CRSF Link tester and packet monitor

### A testing tool for Crossfire and ExpressLRS receivers.

Based on ESP32 microcontroller, multiple boards and displays are supported.

#### Features

**Realtime visualization of**:

- RSSI, LQ, Link Rate and TX power
- Up to 10 RC channels
- Scrolling colored indicator of received, bad and missed packets 

#### Limitations

- ELRS full resolution switch modes not supported

#### Supported boards

- M5Core2
- ESP32 2432-S028 HMI Module
- M5Stack Sticks3
- TTGO T-Display
- cyd2usb

#### Supported displays

- 320x240 ILI9341 SPI displays
- 240x135 TFT displays

#### User interface

**M5Core2** | **ESP32 2432-S028 HMI Module** | **M5Stack Sticks3** |
| :-------------------------------------------: | :-----------------------------------------: | :-----------------------------------------: |
| ![M5Core2 photo](https://i.ibb.co/XLLzwKn/crsf-tester-m5-photo.jpg) | ![HMI photo](https://i.ibb.co/x6TcJYh/IMG-20241130-184013.jpg)    | ![Sticks3 photo](https://i.imgur.com/B6bEZPT.png) |

- RSSI bar, minimum range is based on RX sensitivity for given packet rate (ELRS)
- LQ: Link quality bar
- PW: Current power of the transmitter
- PR: Current packet rate in Hz
- Numeric values and graphical bars for 10 RC channels
- Text and scrolling packet indicator: each vertical line reppresents a received (or not) RC packet:
 
Green: ✅ Good RC packet received

Red: ❌ RC packet timeout

Gray: ⚪ Bad serial RX frame or CRC error

Update rate and packet timeout are based on:

> packet timeout = (1 / PacketRateHz * 1.2) seconds

It's OK to see red missed packets at regular intervals with telemetry turned on, because these time slots are used by receiver to transmit back the telemetry packet.

#### Programming and flashing

Basically the process is the same as for ExpressLRS and VSCode, just modify the platformio.ini file and select the correct serial port for flashing
[How to compile with VSCode for ESP32](https://randomnerdtutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino "How to compile with VSCode for ESP32")

#### Wiring diagram for M5 Core2

![M5Core2 diagram](https://i.ibb.co/9p8ZG4k/crsf-tester-m5-wiring.jpg)

#### Wiring for M5Stack Sticks3:

Uses Grove hat2 connector.

ROW 1:

- GND -> GND
- EXT 5V -> 5V

ROW 2:

- G6 -> TX
- G7 -> RX but unused


#### Wiring for TTGO T-Display:

PIN 32 for RX