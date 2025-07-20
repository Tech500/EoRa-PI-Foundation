## 🛠️ Getting Started

### Hardware Required

- Ebyte EoRa-S3-900TB (EoRa PI) pair
- KY-002S bistable MOSFET switch (for remote load control)
- INA226 I2C current sensor (optional)
- 3.3V LDO power supply or battery
- Optional: Google Sheets or logging backend

### Wiring Guide

See [`docs/wiring.md`](docs/wiring.md) or refer to images in the `/images` folder.

### Configuration

Ebyte provides two key files:
- `boards.h`: Sets up onboard peripherals including LEDs, I2C, SD card, and OLED
- `utilites.h`: Configures GPIOs and other hardware control logic

These files simplify adapting the code to the EoRa-S3-900TB platform.

### Flashing the Code

Use [Arduino IDE 2.3.6](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/) to upload the sketch.

1. Ensure the [RadioLib](https://github.com/jgromes/RadioLib) library is installed
2. Open the desired sketch (`sender.ino` or `receiver.ino`)
3. Compile and upload to the EoRa PI boards

## 🔋 Power Optimization Tips

- Adjust duty cycle timing to reduce average current draw
- The EoRa PI defaults to a WOR duty cycle: awake for 2 ms, asleep for 10,000 ms
- Disable status LEDs and onboard USB chips (if not required) to save power

## 📡 Communication Protocol

LoRa packet follows this format:

"<command>,<timestamp>"

- `1` = Turn ON load  
- `2` = Turn OFF load  
- Timestamp-only message, used for logging  date and time

The receiver interprets the command and toggles the bistable switch or logs data as appropriate.

## 📈 Logging (Optional)  

If desired, sender units can POST INA226 data to a Google Sheet using `HTTPClient` and a custom Google Apps Script endpoint.

## 🧠 Credits & Acknowledgements

This project was developed with testing and guidance from:
- **William Lucid** – Founder & Developer  
- **OpenAI ChatGPT** – Engineering Assistant & Debugging Partner
- **Claude**– Lead programmer & Debugger, “EoRa_PI_WOR_Receiver.ino”  
- **Copilot** and **Gemini** Support and Contributions to coding
- Community testers and contributors

## 🤝 Contributing

Pull requests, issues, and suggestions are welcome! Fork this repo and submit your ideas or improvements.

## 📜 License

MIT License – see [`LICENSE`](LICENSE) for details.

---

> 73 and thanks for stopping by!  
> What started as a simple Wyze Cam switch evolved into a flexible, low-power LoRa + ESP32 foundation.



