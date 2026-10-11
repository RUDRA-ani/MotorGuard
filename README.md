# Motor Guard — ESP32 Motor Protection System

A modular embedded firmware project that simulates a motor protection and monitoring system using an ESP32, developed with **ESP-IDF and FreeRTOS** and tested in the **Wokwi simulator**.

Motor Guard monitors simulated current and temperature sensor readings, detects abnormal conditions, and transitions the motor into a fault state when protection limits are exceeded. It also supports fault latching, controlled fault clearing, stale sensor-data detection, and periodic system diagnostics.

> **Project status:** Core functionality implemented and seven functional test scenarios passed in Wokwi simulation. Physical hardware validation has not yet been performed.

## Features

- **Overcurrent protection:** Trips the motor when the simulated current reading reaches the configured threshold.
- **Overtemperature protection:** Trips the motor when the simulated temperature reading reaches the configured threshold.
- **Fault latching:** Keeps the motor in a fault state until an explicit, valid fault-clear operation is performed.
- **Sensor error detection:** Detects sensor read failures and reports a sensor fault.
- **Stale sensor-data detection:** Detects when sensor snapshots stop updating beyond the configured timeout.
- **Safe fault recovery:** Allows fault clearing only when sensor data is valid and readings are within the configured recovery limits.
- **Motor state management:** Separates motor control from protection and fault-management logic.
- **Periodic diagnostics:** Reports motor state, fault information, occurrence count, and task stack high-water marks.
- **Modular architecture:** Separates motor control, sensor acquisition, protection logic, fault management, and system monitoring.
- **RTOS-based execution:** Uses FreeRTOS tasks, queues, task scheduling, and synchronization mechanisms.

 
 

### Component responsibilities

| Component | Responsibility |
|---|---|
| `main` | Initializes components, handles button input, consumes sensor snapshots, and detects stale data. |
| `motor_controller` | Manages motor states and simulated motor/fault indicator outputs. |
| `sensor_manager` | Reads simulated current and temperature inputs. |
| `protection` | Evaluates readings, applies protection thresholds, and coordinates fault reporting and recovery. |
| `fault_manager` | Records fault codes, timestamps, active status, and occurrence counts. |
| `system_monitor` | Periodically reports system status and task stack diagnostics. |

## Hardware and Simulation Setup

The project uses an ESP32 DevKit V1 in Wokwi. Sensor values are simulated using potentiometers rather than physical current and temperature sensors.

| Signal | ESP32 GPIO | Purpose |
|---|---:|---|
| Motor indicator LED | GPIO 18 | Represents the simulated motor output |
| Fault indicator LED | GPIO 19 | Indicates a motor fault |
| Push button | GPIO 23 | Motor control and fault recovery |
| Current sensor input | GPIO 34 | Simulated current reading |
| Temperature sensor input | GPIO 35 | Simulated temperature reading |

### Simulated controls

- Adjust the current potentiometer to test overcurrent protection.
- Adjust the temperature potentiometer to test overtemperature protection.
- Press the button briefly to start or stop the simulated motor.
- Hold the button for approximately two seconds to request fault injection or fault clearing, depending on the current motor state.

The push button uses active-low logic with an internal pull-up. Button input is debounced in software.

## Protection Logic

The current implementation uses raw ADC values as simulated sensor measurements.

| Condition | Configured behavior |
|---|---|
| Current ADC ≥ 3000 | Trigger overcurrent fault |
| Temperature ADC ≥ 3000 | Trigger overtemperature fault |
| Sensor read failure | Trigger sensor error fault |
| Sensor snapshot older than 500 ms | Trigger stale sensor-data fault |
| Current or temperature ADC ≥ 2800 during recovery | Reject fault clearing |

The recovery threshold provides a margin below the trip threshold. This helps prevent fault clearing while readings remain close to the configured trip point.

**Important:** These are development thresholds expressed as raw ADC counts, not calibrated amperes or degrees Celsius. Real motor protection would require appropriate sensors, calibration, electrical isolation, and validated safety limits.

 

## Fault Management

Each fault record contains:

- Fault code
- Fault timestamp in microseconds since boot
- Number of recorded fault occurrences
- Active/inactive status

Supported fault codes:

- `FAULT_NONE`
- `FAULT_OVERCURRENT`
- `FAULT_OVERTEMPERATURE`
- `FAULT_SENSOR_ERROR`

Faults remain latched until an explicit recovery request succeeds. Restoring sensor readings alone does not automatically clear a fault.

## Functional Testing

The following seven scenarios have passed in Wokwi simulation.

| # | Test scenario | Expected result | Status |
|---:|---|---|---|
| 1 | Short button press | Toggles motor operation | PASS |
| 2 | Overcurrent condition | Motor trips and fault is latched | PASS |
| 3 | Overtemperature condition | Motor trips and fault is latched | PASS |
| 4 | Restore safe sensor readings after a trip | Fault remains latched | PASS |
| 5 | Long press with valid, safe readings | Fault clears and motor remains stopped | PASS |
| 6 | Simulated sensor read failure | Sensor fault is latched | PASS |
| 7 | Sensor task stops publishing snapshots | Stale-data fault is triggered | PASS |

These results represent functional tests in simulation. They do not establish physical hardware reliability, electrical safety, or real-world protection performance.

## Technology Stack

- **Language:** C
- **Framework:** ESP-IDF 5.5.5
- **RTOS:** FreeRTOS
- **Target:** ESP32 DevKit V1
- **Simulation:** Wokwi
- **Build system:** CMake
- **Development tools:** ESP-IDF command-line tools

## Project Structure

```text
MotorGuard/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── components/
    ├── motor_controller/
    │   ├── CMakeLists.txt
    │   ├── include/
    │   │   └── motor_controller.h
    │   └── motor_controller.c
    ├── sensor_manager/
    │   ├── CMakeLists.txt
    │   ├── include/
    │   │   └── sensor_manager.h
    │   └── sensor_manager.c
    ├── protection/
    │   ├── CMakeLists.txt
    │   ├── include/
    │   │   └── protection.h
    │   └── protection.c
    ├── fault_manager/
    │   ├── CMakeLists.txt
    │   ├── include/
    │   │   └── fault_manager.h
    │   └── fault_manager.c
    └── system_monitor/
        ├── CMakeLists.txt
        ├── include/
        │   └── system_monitor.h
        └── system_monitor.c
```

## Build Instructions

### Prerequisites

- ESP-IDF 5.5.5
- ESP32 build target configuration
- Wokwi simulator or Wokwi extension for your development environment

### Build the firmware

From the project directory, with ESP-IDF configured in your shell:

```bash
idf.py set-target esp32
idf.py build
```

### Run the simulation

Open the project in your Wokwi environment, configure the ESP32 DevKit V1 and the required LEDs, button, and potentiometers, then run the simulation.

Use the Wokwi serial monitor to inspect motor-state transitions, fault events, sensor readings, and periodic system diagnostics.

The firmware can be built locally, but simulation is the project's current execution and validation environment.
 
## Author

**Anirudra Dahal**

Electronics, Communication & Information Engineering graduate interested in embedded systems, firmware development, PCB design, automation, and intelligent electronic systems.

---

*Motor Guard is an educational and portfolio project. Its simulated protection logic is not a certified safety system and should not be used to protect real motors or people without appropriate engineering validation.*
