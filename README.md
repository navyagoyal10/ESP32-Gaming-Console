# 🎮 ESP32 Standalone Multiplayer Game Console

**A custom-engineered, 3D-printed local multiplayer gaming console powered by an ESP32 microcontroller.**

This project is a fully functional, standalone hardware and software gaming system developed as part of academic coursework (under the mentorship of Prof. Sunil Jha). It features custom server-client architecture for local multiplayer gaming, housed in a bespoke, 3D-printed enclosure.

## 🚀 Key Features

* **Microcontroller Architecture:** Built on the ESP32 platform, utilizing its integrated Wi-Fi capabilities to handle network communication.
* **Local Multiplayer System:** Employs a custom server-client network architecture that allows multiple devices/players to connect and interact locally without external internet dependency.
* **CAD & 3D Fabrication:** The protective enclosure was custom-designed using Autodesk Fusion 360 and fabricated via FDM 3D printing, featuring tailored cutouts for tactile inputs and battery integration.
* **Hardware Integration:** Integrates distinct electronic modules including a TFT LCD display, tactile push buttons (D-pad and action buttons), and a battery power module to deliver an arcade-like standalone experience.

## 🛠️ Tech Stack & Components

* **Microcontroller:** ESP32 development board
* **Display:** TFT LCD Screen
* **Input Peripherals:** Tactile push buttons (Micro-switches)
* **Power Source:** Battery module / AA Battery holder
* **Programming Language:** C / C++ (Arduino IDE / ESP-IDF)
* **Design Software:** Autodesk Fusion 360
* **Fabrication Method:** FDM 3D Printing

## 📷 Hardware & Visual Assets

* **Enclosure Design:** (Custom yellow 3D-printed shell with "BLAZE CORE" branding visible in project photos)
* **Circuit Assembly:** (Hand-soldered perfboard integrating ESP32, TFT screen, and tactile buttons)
* **Gameplay Demo:** (Standalone local multiplayer server-client operations)

## ⚙️ Installation & Setup

### Option A: Local Environment
1. **Clone the repository:**
   ```bash
   git clone [https://github.com/navyagoyal10/ESP32-Gaming-Console.git](https://github.com/navyagoyal10/ESP32-Gaming-Console.git)
   cd ESP32-Gaming-Console
   ```
2. **Flash the ESP32:**
   * Open the `.ino` or C++ source files in the Arduino IDE or PlatformIO.
   * Ensure you have the ESP32 board definitions installed.
   * Connect your ESP32 via USB and flash the code onto the board.
3. **Hardware Wiring:**
   Ensure your tactile push buttons, TFT display, and power modules are connected to the corresponding GPIO pins mapped in the source code.
4. **Operation:**
   Power on the console. The master console will initialize the local server, allowing client consoles to connect over the local network.

## 🖥️ Usage

1. Power the console via the integrated battery module.
2. Navigate the menu system and game interface using the physical tactile push buttons.
3. Select multiplayer mode to establish the server-client connection and begin gameplay.
