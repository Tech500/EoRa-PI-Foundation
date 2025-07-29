## 🛠️ Getting Started

### What You'll Need
#### Essential Hardware
- **Ebyte EoRa-S3-900TB (EoRa PI)** - Pair of transceivers for your country's ISM frequency band
- **3.3V Power Supply** - LDO regulator or battery pack
- **Development Environment** - [Arduino IDE 2.3.6](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/)

#### For Remote Control Applications
- **KY-002S bistable MOSFET switch** - Enables remote load switching
- **INA226 I2C current sensor** *(optional)* - For power monitoring and logging

### Quick Start Guide
1. **Install Dependencies**
   - Install [RadioLib](https://github.com/jgromes/RadioLib) library in Arduino IDE
   - Download this repository and open desired sketch

2. **Basic Setup** 
   - **Transmitter**: Uses EoRa PI board only (no additional wiring needed)
   - **Receiver**: See [wiring schematic](https://github.com/Tech500/EoRa-PI-Foundation/blob/main/EoRa-PI-Foundation%20--Receiver.png) for component connections

3. **Upload & Test**
   - Flash `EoRa_PI_WOR_Transmitter.ino` to transmitter board
   - Flash `EoRa_PI_WOR_Receiver.ino` to receiver board  
   - Power on both units and test basic communication

### Understanding the Platform
The EoRa-S3-900TB comes with helpful configuration files:
- **`boards.h`** - Configures onboard peripherals (LEDs, LoRa, I2C, SD card, OLED)
- **`utilities.h`** - Handles GPIO setup and hardware control logic

These files simplify code adaptation and reduce setup complexity.

## 🔋 Power Optimization

### Achieving Sub-1mA Operation
- **Default WOR cycle**: 2ms awake, 10,000ms sleep
- **Measured performance**: ~372µA average deep sleep current with 2% duty cycle, 5V Source voltage
- **Battery life**: Months to years depending on usage patterns

### Power-Saving Tips
- Disable status LEDs when not needed for debugging
- Power down unused onboard peripherals (USB chips, etc.)
- Use `radio.standby()` during sleep periods
- Implement on-demand INA226 monitoring

> **Real-world example**: Camera monitoring system extended from 24 hours to weeks of operation using web-activated power control.

##  📡  Communication Protocol

LoRa packet format: "1,<timestamp>"

Only one command: 1 (keep alive/turn ON)
No OFF command: Switch turns off automatically via ticker timeout
Timestamp: For logging purposes (NTP-based from transmitter)

How It Actually Works

Web request received → Preample message switches radio from standby to receive receive mode and awakens ESP32-S3   
Transmitter sends "1,timestamp" packet
Receiver gets packet → Resets 2-minute ticker countdown
No packet received → Ticker expires → Switch turns OFF automatically
Next web request → Process repeats

Dead Man's Switch Logic

- Active state: Camera powered after receiving periodic "1" packets
- Safety timeout: 2-minute countdown ensures automatic shutoff
- Simple & reliable: No complex OFF commands that could fail

This is much more elegant than bidirectional control! The ticker timeout provides the safety mechanism, and you only need reliable delivery of the "command, timestamp" packet. If LoRa communication fails, the system safely defaults to OFF state.

### Typical Use Cases
- **Remote switching**: Camera activation, equipment control
- **Monitoring**: Battery status, sensor readings  
- **Safety systems**: Dead man's switch, timeout protection

## 📈 Optional Features

### Data Logging
- **Local logging**: Store sensor data on SD card or flash.  Flash storage implemented
- Option by request; **Cloud integration**: POST INA226 data to Google Sheets via custom Apps Script
- **Power monitoring**: Track current consumption and battery health

### Advanced Applications
- **Web-activated systems**: HTTP requests trigger LoRa commands
- **Multi-node networks**: Scale to multiple sensor/control points
- **Integration ready**: Works with existing IoT infrastructure

## 🧠 Credits & Acknowledgements

This project was developed with testing and guidance from:
- **William Lucid** – Founder & Developer  
- **OpenAI ChatGPT** – Engineering Assistant & Debugging Partner
- **Claude** – Lead programmer & Debugger, "EoRa_PI_WOR_Receiver.ino"  
- **Copilot** and **Gemini** – Support and Contributions to coding
- Community testers and contributors

## 🤝 Contributing

We welcome contributions! Here's how to help:
- 🐛 **Found a bug?** Open an issue with details and steps to reproduce
- 💡 **Have an idea?** Submit a feature request or start a discussion  
- 🔧 **Want to code?** Fork the repo and submit a pull request
- 📚 **Documentation**: Help improve examples, guides, or troubleshooting

## 📜 License
MIT License – see [`LICENSE`](LICENSE) for details.

## Directory of \EoRa-PI-Foundation\Docs

- Complete Ebyte EoRa-S3-900TB (EoRa PI) Pin Mapping Guide.pdf
- DeepSleep Duty-Cycle.log
- Documents Dir.txt
- EoRa PI Power Profile.jpg 
- EoRa_PI_UserManual_EN_v1.1.pdf
- ESP32-S3 Power Consumption Reference Guides.pdf
- Pin callouts.jpg
            

## Directory of \EoRa-PI-Foundation\Docs\Deep Sleep Waveforms (Latest Nordic PPK2 Observations)

- Awake period.png
- Deep Sleep in Standby 5V Source .png
- incomplete init.png
- Going to Awake .png
- Going to Deep Sleep.png

---
> **73's de AB9NQ thanks for stopping by!**  
> What started as a simple Wyze Cam switch evolved into a flexible, low-power LoRa + ESP32 foundation.
