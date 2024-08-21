# Three-Phase Energy Monitor on ESP32

## Overview
This repository contains the design and implementation details for a three-phase energy monitor based on the ESP32 microcontroller. This project is intended to provide an accurate, cost-effective solution for monitoring electrical energy usage in real-time across three-phase power systems.

## Key Features
- Real-time energy measurement for three-phase systems.
- Utilizes ESP32 for Wi-Fi connectivity and advanced processing capabilities.
- Compact and efficient PCB design.
- Open source software for data logging and analysis.


### Application Interface

#### Connected Device Page
Displays the current IP address and provides options to disconnect, return to the homepage, or access settings.
<div align="center"><img src="https://github.com/user-attachments/assets/e1e79304-1e8b-4a55-9eae-2ac1e46adad8" alt="connected2" style="max-width: 100%;"></div>


#### Main Dashboard with MDNS
Shows the real-time energy consumption in kilowatt-hours (kWh).
<div align="center"><img src="https://github.com/user-attachments/assets/5b3e1c0e-d8a3-46d7-b4a6-5f39a85f1ebb" alt="MDNS" style="max-width: 100%;"></div>


#### Settings Page
Allows users to reset to factory settings, change passwords, and configure time zones.
<div align="center"><img src="https://github.com/user-attachments/assets/b60d5ae9-3526-4136-86b2-c2e2661aa731" alt="settings_page" style="max-width: 100%;"></div>


#### Wi-Fi Configuration
Users can configure the Wi-Fi settings directly from the interface.
<div align="center"><img src="https://github.com/user-attachments/assets/26290412-9373-4b2c-92ba-633fbc3281a4" alt="wifi_con" style="max-width: 100%;"></div>
<div align="center"><img src="https://github.com/user-attachments/assets/336f6a2c-9dcc-4a77-8e94-208e3ae50b82" alt="wifi_con2" style="max-width: 100%;"></div>


### Video Demonstration
For a live demonstration of the system's functionality, refer to the following video:


https://github.com/user-attachments/assets/12034a56-c6ff-4372-9617-42a8840f2156



                                                                                             


### Future Work and Contributions
- Implementing MQTT for cloud-based data logging and analysis.
- Adding support for more sensors and peripherals.
- Improving the UI/UX of the web interface.

Contributions are welcome! Please feel free to fork this project and submit pull requests.


## Hardware
The core of the energy monitor is a custom-designed PCB that interfaces with the ESP32 microcontroller. The design files for this PCB are available for viewing and modification at the following link:

[Three-Phase Energy Monitor PCB Design on OSHWLab](https://oshwlab.com/kamil.adaskamil.adas/adas)

Please refer to the PCB project for a detailed list of components and assembly instructions.

## Software Installation
1. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/three-phase-energy-monitor.git
   ```
2. Use spiffs to burn data folder.

3. Upload Firmware via Arduino IDE

<div align="center"><img src="https://github.com/kamiladas/Digital_multimeter/assets/58427794/6b9421fc-af99-4d5c-b5da-9a12be77cae9" alt="ESP32 PCB " style="max-width: 100%;"></div>



<div align="center"><img src="https://github.com/kamiladas/Digital_multimeter/assets/58427794/29ea7a18-3429-40fb-94ca-34e23a53a571" alt="Schematic Overview" style="max-width: 100%;"></div>



<div align="center"><img src="https://github.com/kamiladas/Digital_multimeter/assets/58427794/68c54c70-47e8-45c6-815a-6fbbb4a0eb6e" alt="Assembled Device" style="max-width: 100%;"></div>




<div align="center"><img src="https://github.com/kamiladas/Digital_multimeter/assets/58427794/9468b386-d8fb-49d6-8b63-8a7d2e5a2e6e" alt="Application UI" style="max-width: 100%;"></div>





PCB and diagram
[PCB and Diagram on OSHWLab](https://oshwlab.com/kamil.adaskamil.adas/adas)


### License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
