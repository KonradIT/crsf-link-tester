## CRSF Link tester and packet monitor

### A testing tool for Crossfire and ExpressLRS receivers.

Based on ESP32 microcontroller, multiple boards and displays are supported.

#### Features

**Realtime visualization of**:

- RSSI, LQ, Link Rate and TX power
- Up to 10 RC channels
- Scrolling colored indicator of received, bad and missed packets 

**Fake flight controller telemetry**: simulated GPS, battery, attitude, vario, barometric altitude and flight mode frames are sent back to the receiver, so the handset gets telemetry sensors without a real flight controller. See [Fake telemetry](#fake-telemetry).

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

- GPIO27 (RX) -> receiver TX
- GPIO26 (TX) -> receiver RX (telemetry)

#### Fake telemetry

The tester pretends to be a flight controller and continuously sends CRSF telemetry on its TX pin to the receiver's RX pad. It plays back a flight path bundled into the firmware, by default [data/madrid-demo-flight.kml](data/madrid-demo-flight.kml): a simulated quadcopter with a 4S 3000 mAh pack flying from Bernabeu to Atocha in Madrid, then 30 seconds on the ground. The flight restarts after its last point; build with `-D MISSION_REPEAT=0` to hold the last point instead.

| Frame | Type | UART rate |
| --- | --- | --- |
| GPS | `0x02` | 5 Hz |
| Battery | `0x08` | 2 Hz |
| Attitude | `0x1E` | 5 Hz |
| Vario | `0x07` | 2 Hz |
| Barometric altitude + vertical speed | `0x09` | 2 Hz |
| Flight mode | `0x21` | 1 Hz |

##### Using another flight path

Set `custom_flight_path` in `platformio.ini` (in `[env]` for all boards, or in a single board's env) to a KML file, relative to the project folder. Before every build, [scripts/kml_to_flight_path.py](scripts/kml_to_flight_path.py) converts it to a table in the build folder, so the board doesn't parse KML. The build output shows which fields were derived, and the build stops with an error if the KML can't be used. To check a file without building, run it by hand: `python scripts/kml_to_flight_path.py my-flight.kml flight_path_data.h`.

The KML needs exactly one `gx:Track` (it may sit inside a `gx:MultiTrack`) with a `<when>` timestamp (strictly increasing) and a `<gx:coord>` (longitude, latitude and optionally GPS altitude in m above mean sea level) for every point. A plain track from a GPS logger is enough. Everything else is optional, one entry per point, and derived when it's missing:

| Element | Content | When missing |
| --- | --- | --- |
| `<gx:angles>` | heading = yaw (0-360°), tilt = pitch (°, nose up positive), roll (°, right wing down positive) | yaw follows the course; pitch and roll are the tilt needed for the acceleration (nose down to speed up, banked in turns), at most 45° |
| `<gx:SimpleArrayData name="ground_speed">` | m/s | from the distance between points |
| `course` | course over ground, ° | from the direction between points, held while slower than 1 m/s |
| `vertical_speed` | m/s | from the altitude change |
| `baro_altitude` | m above takeoff | altitude above the first point |
| `battery_voltage`, `battery_current`, `battery_used`, `battery_remaining` | V, A, mAh, % (integer) | 4S 3000 mAh pack: 12 A flying plus more for speed and climb, 0.4 A on the ground |
| `satellites` | integer | 12 |
| `flight_mode` | text sent in the flight mode frame, up to 15 characters | `ANGL` |
| `phase` | text shown on the telemetry page, may be empty | `FLYING`, or `LANDED` when less than 2 m above takeoff and slower than 1 m/s |

Speeds and accelerations are taken over about 2 seconds of track to smooth out GPS noise. Other arrays (heart rate and so on) are ignored.

Placemarks with a `<Point>` and a `<TimeStamp>` are waypoints: the telemetry page shows the next one. Values are interpolated linearly between points (angles the short way round), text changes at the point that sets it. The Madrid demo has one point per second.

To see it on the radio:

1. Connect the board's TX pin (`PIN_TX` in `platformio.ini`) to the receiver's RX pad.
2. Set the receiver's serial protocol to CRSF.
3. On the handset, set the ExpressLRS **Telem Ratio** to something other than Off or Race (for example 1:128).
4. Run **Discover new sensors** in the radio's telemetry page.

The telemetry page shows what is currently being sent: flight mode, mission phase and number, battery voltage/current/remaining, used mAh, satellites, relative altitude, speed, vertical speed, GPS position, next waypoint, mission time and the number of frames written to the UART. To open it:

| Board | Button | Pages |
| --- | --- | --- |
| M5Core2 | BtnA (left touch button) | main <-> telemetry |
| ESP32 2432-S028, cyd2usb, custom ILI9341 | BOOT (GPIO0) | main <-> telemetry |
| M5Stack Sticks3 | BtnA | channels -> stats -> telemetry |
| TTGO T-Display | external button between GPIO13 and GND | channels -> stats -> telemetry |

On the small screen boards the telemetry page uses the whole display, so RSSI and LQ are hidden until you switch back.