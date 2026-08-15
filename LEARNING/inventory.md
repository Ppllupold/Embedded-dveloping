# Hardware Inventory

Reference list of everything on hand, as of 2026-08-12. Not a project plan —
just what exists and where it came from, so future sessions don't have to
re-derive it from chat scrollback.

Note: names below are taken directly from the kit's own product listing.
Exact chip/protocol/pinout for anything not yet wired is unconfirmed until
physically inspected — don't trust a name alone (the DS18B20 saga started
exactly that way).

## 45-in-1 sensor kit

- Soil module
- Infrared sensor receiver module (IR receiver)
- Infrared emission sensor module (IR emitter)
- Laser head sensor module
- Temperature and humidity sensor module (likely DHT11-style, single-wire-ish custom timing protocol)
- 5V relay module
- Gyro module (type/interface not yet confirmed — could be I2C like an MPU6050, could be analog; check before wiring)
- Finger heartbeat detect module (analog pulse sensor)
- Microphone sensitivity sensor module
- Microphone sound sensor module (likely a second, differently-branded mic module — check if it's actually a duplicate of the above)
- Metal touch sensor module
- Flame sensor module
- 3-color LED module
- Hunt sensor module (likely a single-eye IR reflectance/tracking sensor — presence/reflectivity only, no directional info by itself)
- Smart car avoid-obstacle infrared photoelectric switch (separate IR obstacle sensor, same category as Hunt)
- Linear magnetic Hall sensor (analog/proportional output)
- Analog Holzer magnetic sensor ("Holzer" = Hall, common kit-listing mistranslation — possibly the same type as the linear Hall above, worth checking for duplication)
- Hall magnetic sensor module (digital threshold output — different behavior from the analog ones above)
- Rotary encoder module
- Active buzzer module
- Small passive buzzer module
- Magic Light Cup module (mercury tilt switch)
- Tilt switch module
- Vibration switch module
- For-arduino "hit sensor" module (likely another vibration/shock sensor — check for duplication against the above)
- Digital temperature sensor module (name suggests true digital/OneWire, but could also be a comparator+trimpot analog-threshold board like the one already verified this session — confirm before assuming it's a DS18B20)
- Temperature sensor module x2 (listed twice in the original kit description — likely two identical bare analog thermistor boards, no comparator)
- Ultrasonic module (HC-SR04-style)
- Opening module / Optical breaking module (listed as two separate entries — possibly a photo-interrupter/slot sensor pair, or duplicates; check)
- RGB LED SMD module
- Mini reed module
- Large reed module
- Bicolor LED common cathode module (3mm)
- Two-color LED module
- Key switch module (tactile button)
- Photoresistor module
- Breadboard power module (barrel jack/USB-powered supply, jumper-selectable rails — useful given this session's power-rail debugging saga)
- MP1584EN buck module (adjustable buck converter — power component, not a sensor)
- SD card reader module (SPI)
- PS2 joystick game controller module (2-axis analog + button)
- Automatically flashing LED module (self-flashing, no code needed)
- DS1302 clock module, no battery (3-wire synchronous protocol: CE/SCLK/I-O — not I2C)
- Water level module

## From the previous Arduino kit

- 2x servo motors
- LCD1602 (bare header holes, needs soldering — blocked until soldering equipment is available)
- MPU6050 breakout (needs soldering)
- IR remote control (pairs with the 45-in-1 kit's IR receiver module)
- Stepper motor driver board (likely ULN2003 + 28BYJ-48 style — new actuator type, not yet used in this project)
- Relay module (spare/duplicate of the 45-in-1 kit's relay)
- 74HC595 (bare DIP IC, not a breakout board — SIPO shift register, simplest realistic first real-SPI target, no MOSI-to-MISO loopback needed)
- MAX7219 (bare DIP IC, not a breakout board — LED matrix/7-segment driver, real command/register-based SPI protocol, a step up from raw 74HC595 bit-shifting; datasheet also calls for an ISET resistor and a decoupling cap near VCC)
- L293D (bare DIP IC, not a breakout board — dual H-bridge motor driver; the "D" suffix means it has internal flyback diodes, unlike the plain L293)
- Small brushed DC motor (confirmed on hand — 2-terminal, no gearbox visible)
