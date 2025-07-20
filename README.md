
## 🛠️ Getting Started

### Hardware Required

- Ebyte, EoRa-S3-900TB (EoRa PI) Pair
- KY-002S bistable MOSFET switch (for remote load control)
- INA226 I2C current sensor (optional)
- 3.3V LDO power supply or battery
- Optional: Google Sheets or logging backend

### Wiring Guide

See [`docs/wiring.md`](docs/wiring.md) or check out the images in the `/images` folder.

### Configuration

Ebyte provided file “boards.h” sets up onboard peripherals Leds, I2C (more on this later), SD card, and OLED.  Another Ebyte provided file “utilites.h” configures GPIO IO for onboard peripherals 



### Flashing the Code

Used [Arduino IDE 2.3.6](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/) to upload the sketch.

1. Uses “RadioLib library (link to class members)
2. Open the desired sketch (`sender.ino`, `receiver.ino`)
3. Compile and upload

## 🔋 Power Optimization Tips

- Use modify duty cycle timing to optimize current consumption.
- “EoRa PI” uses a duty cycle, Sleep Mode wakes for 2 milliseconds and sleeps for 10,000           milliseconds. RadioLib
- Disconnect status LEDs and USB chips if not needed

## 📡 Communication Protocol

Messages are sent in this format:
"<command>,<timestamp>"

- `1` = Turn ON load
- `2` = Turn OFF load
- `3`  = Timestamp used for logging INA226 power monitoring data.

The receiver parses the command and toggles the bistable switch accordingly.

## 📈 Logging (Optional)

If desired, sender units can push sensor data to a Google Sheet using an HTTPClient POST request and Apps Script.

## 🧠 Credits & Acknowledgements

This project was built with guidance, testing, and development by:
- William Lucid (Founder & Developer)
- OpenAI ChatGPT (Engineering Assistant & Debug Partner)
- Community testers & feedback contributors

## 🤝 Contributing

Pull requests, issues, and suggestions are welcome! Fork this repo and submit your ideas or improvements.

## 📜 License

MIT License – see [`LICENSE`](LICENSE) for details.

---

> 73 and thanks for stopping by! This project started as a remote switch for a Wyze Cam and grew into a flexible foundation for LoRa + ESP32 innovation.


