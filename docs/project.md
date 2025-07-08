# Project Overview: LoRaWAN Class C Relay Controller

## Core Architecture

### System Components
1. **Hardware Platform**: Heltec WiFi LoRa 32 V3 (ESP32S3 + SX1262)
2. **LoRaWAN Stack**: LoraManager2 library (RadioLib-based)
3. **Relay Control**: 8-channel relay board interface
4. **Communication**: The Things Network (TTN) integration

### Tech Stack Details

#### Hardware
- **MCU**: ESP32S3 (dual-core, 240MHz, WiFi/Bluetooth)
- **LoRa Radio**: SX1262 (sub-GHz transceiver)
- **GPIO**: 8 relay control pins (21, 26, 48, 47, 33, 34, 35, 36)
- **Interfaces**: USB (programming/debugging), UART (serial commands)

#### Software Libraries
- **LoraManager2**: Custom LoRaWAN management library
- **RadioLib**: Low-level radio communication
- **ArduinoJson**: JSON parsing for downlink commands
- **DisplayManager**: OLED display interface (disabled in current implementation)
- **Heltec ESP32 Dev-Boards**: Board support package

#### Development Environment
- **IDE**: PlatformIO (preferred) / Arduino IDE
- **Framework**: Arduino for ESP32
- **Build System**: PlatformIO/CMake
- **Upload Protocol**: ESPTool via USB

### API Patterns

#### LoRaWAN Communication
- **Class**: C (continuous receive windows)
- **Activation**: OTAA (Over-The-Air-Activation)
- **Frequency**: US915 (configurable)
- **Subband**: 2 (channels 8-15)

#### Command Interface
```cpp
// JSON Command Format
{
  "relay": 1-8,        // Relay number
  "state": 0|1,        // OFF|ON
  "duration": 3600     // Optional: seconds
}

// HEX Command Format (legacy)
[0x01][relay_data][duration]
```

#### Serial Interface
```
relay,<number>,<state>[,<duration>] - Control relay
status                              - Show system status
join                               - Force LoRaWAN join
test_json                          - Test JSON processing
test_hex                           - Test HEX processing
```

### Database Schema Overview
**Note**: This is an embedded system with no persistent database. State is maintained in volatile memory:

```cpp
// Runtime State Structure
bool relayStates[8];           // Current relay states
unsigned long relayTimers[8];  // Auto-off timers
bool loraConnected;            // LoRaWAN connection status
String lastCommand;            // Last processed command
```

## Integration Points

### The Things Network (TTN)
- **Uplinks**: Periodic status packets (5-minute intervals)
- **Downlinks**: JSON commands for relay control
- **Payload Formatter**: Custom JavaScript encoder/decoder

### Relay Board Interface
- **Control Logic**: Inverted (LOW=ON, HIGH=OFF)
- **Power**: External supply for relay coils
- **Isolation**: Optical isolation recommended for safety

## Configuration Management
- **LoRaWAN Credentials**: Compiled into firmware (DevEUI, AppEUI, AppKey)
- **Pin Assignments**: Configurable in source code
- **Network Settings**: Frequency band and subband selection
- **Debug Mode**: Compile-time flag for verbose logging 