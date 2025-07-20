
- `1` = Turn ON load  
- `2` = Turn OFF load  
- `3` = Timestamp-only message, used to trigger INA226 power monitoring/logging  

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



