
## Three-Phase Energy Monitor on ESP32

### Overview
This repository contains the design and implementation details for a three-phase energy monitor based on the ESP32 microcontroller. This project is intended to provide an accurate, cost-effective solution for monitoring electrical energy usage in real-time across three-phase power systems.

### Key Features
- **Real-time Monitoring:** Displays real-time energy consumption for three-phase systems.
- **Wi-Fi Connectivity:** Allows remote monitoring and control via the ESP32’s Wi-Fi capabilities.
- **User-Friendly Interface:** The system comes with a simple, intuitive web interface for easy interaction.
- **Customizable Settings:** Users can easily reset to factory settings, change the password, configure time zones, and set up Wi-Fi connections.

### Hardware
The heart of the project is a custom-designed PCB interfacing with the ESP32 microcontroller. You can find the design files and more details in the following link:

[Three-Phase Energy Monitor PCB Design on OSHWLab](https://oshwlab.com/kamil.adaskamil.adas/adas)

### Software Installation

1. **Clone the Repository:**
   ```bash
   git clone https://github.com/yourusername/three-phase-energy-monitor.git
   ```

2. **SPIFFS Upload:**
   Use SPIFFS to upload the data folder.

3. **Upload Firmware via Arduino IDE:**
   Open the project in Arduino IDE and upload the firmware to your ESP32.

### Application Interface

#### Connected Device Page
Displays the current IP address and provides options to disconnect, return to the homepage, or access settings.
![Connected Device Page](screenshots/connected2.PNG)

#### Main Dashboard
Shows the real-time energy consumption in kilowatt-hours (kWh).
![Main Dashboard](screenshots/MDNS.PNG)

#### Settings Page
Allows users to reset to factory settings, change passwords, and configure time zones.
![Settings Page](screenshots/settings_page.PNG)

#### Wi-Fi Configuration
Users can configure the Wi-Fi settings directly from the interface.
![Wi-Fi Configuration](screenshots/wifi_con.PNG)
![Wi-Fi Configuration - SSID Selection](screenshots/wifi_con2.PNG)

### Video Demonstration
For a live demonstration of the system's functionality, refer to the following video:

![Video Demo](videos/screen-20240821-120257.mp4)

### Future Work and Contributions
- Implementing MQTT for cloud-based data logging and analysis.
- Adding support for more sensors and peripherals.
- Improving the UI/UX of the web interface.

Contributions are welcome! Please feel free to fork this project and submit pull requests.

### License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
