# LoRaWAN Class C Relay Controller

This project implements a LoRaWAN Class C relay controller using a Heltec WiFi LoRa 32 V3 board. It allows remote control of up to 8 relays via The Things Network (TTN) downlink messages using JSON commands.

**Built with LoraManager2** - This implementation uses the advanced LoraManager2 library for TRUE Class C operation with continuous receive windows, providing low-latency remote control capabilities.

## Key Features

- ✅ **TRUE Class C Operation** - Continuous receive windows for instant downlink processing
- ✅ **8-Channel Relay Control** - Individual control with timer support for automated shutoff
- ✅ **Flexible JSON Commands** - Supports both numeric and string state values
- ✅ **Enhanced Serial Interface** - Comprehensive status reporting and test commands
- ✅ **Signal Quality Monitoring** - RSSI and SNR reporting for received downlinks
- ✅ **Event-Driven Architecture** - Separate callbacks for different LoRaWAN events
- ✅ **Memory Optimized** - Efficient resource usage (8.3% RAM, 12.2% Flash)
- ✅ **TTN Compatible** - Full compatibility with The Things Network
- ✅ **Debug Friendly** - Extensive serial output for troubleshooting

> **Note:** The current implementation has temporarily disabled the OLED display functionality to focus on the core relay control and LoRaWAN connectivity. Serial output provides status information instead.

## Hardware Requirements

- [Heltec WiFi LoRa 32 V3](https://heltec.org/project/wifi-lora-32-v3/) board (ESP32S3 with SX1262 LoRa chip)
- Relay board with up to 8 relay channels
- USB cable for programming
- Power supply appropriate for your relay board

## Relay Pin Configuration

The relay pins are configured as follows:

| Relay Number | GPIO Pin |
|--------------|----------|
| 1            | 36       |
| 2            | 35       |
| 3            | 34       |
| 4            | 33       |
| 5            | 47       |
| 6            | 48       |
| 7            | 26       |
| 8            | 21       |

You can modify these pin assignments in the `main.cpp` file if needed.

## LoRaWAN Configuration

Before using this code, you must configure your LoRaWAN credentials using the secrets system:

### Setting up Credentials

1. **Copy the example file:**
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```

2. **Edit the secrets file:**
   Open `include/secrets.h` and replace the placeholder values with your actual TTN credentials:
   ```cpp
   // LoRaWAN credentials - Replace with your values from TTN
   const char* devEui = "YOUR_DEVICE_EUI";     // Device EUI as hex string
   const char* appEui = "YOUR_APPLICATION_EUI"; // Application EUI as hex string  
   const char* appKey = "YOUR_APP_KEY";         // App Key as hex string
   ```

3. **Get your credentials from TTN:**
   - Log into [The Things Network Console](https://console.cloud.thethings.network/)
   - Go to your application and device
   - Copy the Device EUI, Application EUI, and App Key exactly as shown
   - Paste them as hex strings (no spaces, no 0x prefix)

These values must be entered as hex strings in MSB (Most Significant Byte) format, exactly as they appear in the TTN console.

> **Security Note:** The `secrets.h` file is automatically ignored by git to prevent accidental commits of your real credentials. Never commit real network keys to version control!

## TTN Setup

1. Create an account on [The Things Network Console](https://console.cloud.thethings.network/)
2. Create a new application
3. Register a new device within your application
4. Set the device to use OTAA activation
5. Copy the Device EUI, Application EUI, and App Key to the corresponding variables in the code
6. Configure your device for Class C operation in the TTN console

## TRUE Class C Operation

This device operates as a **TRUE Class C** LoRaWAN device using LoraManager2, which provides:
- **Continuous receive windows** - Can receive downlink messages at any time (not just after sending an uplink)
- **Low-latency control** - Near-instant response to TTN downlink commands
- **Enhanced reliability** - Better downlink reception with automatic retry handling
- **Signal quality reporting** - RSSI and SNR information for received downlinks
- **Event-driven architecture** - Separate callbacks for join, downlink, and transmission events

> **Power Consumption:** Class C operation consumes more power than Class A devices due to continuous receive windows, but provides the best user experience for remote control applications.

## Building and Uploading

This project uses PlatformIO for development and deployment.

### PlatformIO

1. Install [VSCode](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
2. Open this project folder in VSCode
3. Update the LoRaWAN credentials in `src/main.cpp`
4. Connect your Heltec board via USB
5. Enter download mode on the board:
   - Press and hold the BOOT button
   - Press the RESET button briefly
   - Release the BOOT button
6. Click the PlatformIO upload button or run the following command:
   ```
   platformio run -t upload
   ```

### Arduino IDE

If you prefer using Arduino IDE:

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 board support via Boards Manager
3. Install the required libraries:
   - LoraManager2 (from https://github.com/pbezant/LoraManager2.git)
   - beegee-tokyo/SX126x-Arduino@^2.0.0
   - ArduinoJson by Benoit Blanchon (v7.0.0+)
   - RadioLib by Jan Gromeš
   - Heltec ESP32 Dev-Boards by Heltec Automation
4. Open the `src/main.cpp` file (rename to .ino if needed)
5. Select the correct board: "Heltec WiFi LoRa 32(V3)"
6. Select the correct port
7. Enter download mode as described above
8. Click Upload

## Controlling Relays via TTN

### JSON Command Format

Relays can be controlled via TTN downlink messages using a JSON format:

```json
{
  "relay": 1,
  "state": 1,
  "duration": 3600
}
```

- `relay`: The relay number (1-8)
- `state`: 1 for ON, 0 for OFF (also supports "on"/"off", "true"/"false" strings)
- `duration`: (Optional) Duration in seconds to keep the relay ON

### Multi-Relay Control (NEW)

You can now control multiple relays at once by sending an array of relay command objects. Each object can include `relay`, `state`, and optional `duration` (in seconds):

```json
[
  {"relay": 1, "state": 1, "duration": 60},
  {"relay": 2, "state": 0},
  {"relay": 3, "state": 1, "duration": 10}
]
```

- `relay`: The relay number (1-8)
- `state`: 1 for ON, 0 for OFF (also supports "on"/"off", "true"/"false" strings)
- `duration`: (Optional) Duration in seconds to keep the relay ON (defaults to 0 if omitted)

**You can still use the old single-relay command format as well.**

#### Example: Turn ON relays 1 and 3 for 1 minute, turn OFF relay 2
```json
[
  {"relay": 1, "state": 1, "duration": 60},
  {"relay": 2, "state": 0},
  {"relay": 3, "state": 1, "duration": 60}
]
```

### JSON Downlink Examples

Here are various examples of JSON downlink messages you can use:

#### Basic Relay Control

Turn ON relay #1:
```json
{
  "relay": 1,
  "state": 1
}
```

Turn OFF relay #1:
```json
{
  "relay": 1,
  "state": 0
}
```

Turn ON relay #1 (using string state):
```json
{
  "relay": 1,
  "state": "on"
}
```

#### Timed Relay Control

Turn ON relay #2 for 10 minutes (600 seconds):
```json
{
  "relay": 2,
  "state": 1,
  "duration": 600
}
```

Turn ON relay #3 for 1 hour (3600 seconds):
```json
{
  "relay": 3,
  "state": 1,
  "duration": 3600
}
```

#### Examples for Different Relays

Turn ON relay #4:
```json
{
  "relay": 4,
  "state": 1
}
```

Turn OFF relay #5:
```json
{
  "relay": 5,
  "state": 0
}
```

Turn ON relay #6 for 30 seconds:
```json
{
  "relay": 6,
  "state": 1,
  "duration": 30
}
```

Turn ON relay #7 for 24 hours:
```json
{
  "relay": 7,
  "state": 1,
  "duration": 86400
}
```

### TTN Payload Formatter

Add this payload formatter in the TTN console to encode your downlink messages:

```javascript
// Encoder function for downlinks
function encodeDownlink(input) {
  // Check if input is a JSON object
  if (typeof input.data === 'object') {
    // Convert JSON to string and then to bytes
    const jsonString = JSON.stringify(input.data);
    const bytes = [];
    for (let i = 0; i < jsonString.length; i++) {
      bytes.push(jsonString.charCodeAt(i));
    }
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  } 
  // If input.data is already an array of bytes, use it directly
  else if (Array.isArray(input.data)) {
    return {
      bytes: input.data,
      fPort: input.fPort || 1
    };
  }
  // If it's a string but not JSON
  else if (typeof input.data === 'string') {
    // Convert string to bytes
    const bytes = [];
    for (let i = 0; i < input.data.length; i++) {
      bytes.push(input.data.charCodeAt(i));
    }
    return {
      bytes: bytes,
      fPort: input.fPort || 1
    };
  }
  // Default empty response
  return {
    bytes: [],
    fPort: input.fPort || 1
  };
}

// Decoder function for downlinks
function decodeDownlink(input) {
  // If there are less than 4 bytes, it's likely a command
  if (input.bytes.length < 4) {
    return {
      data: {
        bytes: input.bytes,
        type: "command"
      },
      warnings: [],
      errors: []
    };
  }
  
  // Try to parse as JSON
  try {
    // Convert bytes to string
    let str = "";
    for (let i = 0; i < input.bytes.length; i++) {
      str += String.fromCharCode(input.bytes[i]);
    }
    
    // Parse as JSON
    const data = JSON.parse(str);
    
    return {
      data: data,
      warnings: [],
      errors: []
    };
  } catch (err) {
    // If parsing fails, return raw bytes
    return {
      data: {
        bytes: input.bytes,
        type: "binary"
      },
      warnings: [`Failed to parse as JSON: ${err.message}`],
      errors: []
    };
  }
}

// Decoder function for uplinks
function decodeUplink(input) {
  // Convert bytes to hex string
  let hexStr = "";
  for (let i = 0; i < input.bytes.length; i++) {
    const hex = input.bytes[i].toString(16).padStart(2, '0');
    hexStr += hex;
  }
  
  // Try to parse as JSON first
  try {
    // Convert bytes to string
    let str = "";
    for (let i = 0; i < input.bytes.length; i++) {
      str += String.fromCharCode(input.bytes[i]);
    }
    
    // Try parsing as JSON
    const data = JSON.parse(str);
    
    return {
      data: data,
      warnings: [],
      errors: []
    };
  } catch (err) {
    // If JSON parsing fails, return the hex string
    return {
      data: {
        hex: hexStr
      },
      warnings: [],
      errors: []
    };
  }
}
```

## Display Information

The OLED display shows:
- LoRaWAN connection status (Connected/Disconnected)
- Last received command
- Current state of all relays (1=ON, 0=OFF)

## Serial Commands

You can also control the relays via the serial port for testing:

- `relay,<number>,<state>[,<duration>]` - Control a relay
  - Example: `relay,1,1,10` turns relay 1 ON for 10 seconds
- `status` - Show comprehensive device status including:
  - LoRaWAN initialization and join status
  - Current device class and EUI information
  - All relay states and timers
  - Last command received
  - Device uptime
- `send` - Force send a status packet to TTN
- `test_json` - Run JSON command processing test
- `test_hex` - Run HEX command processing test

## JSON Command Examples

The device accepts JSON commands through LoRaWAN downlinks. Here are various examples:

### Basic Relay Control

Turn ON relay #1:
```json
{
  "relay": 1,
  "state": 1
}
```

Turn OFF relay #1:
```json
{
  "relay": 1,
  "state": 0
}
```

### Timed Relay Control

Turn ON relay #2 for 10 minutes (600 seconds):
```json
{
  "relay": 2,
  "state": 1,
  "duration": 600
}
```
Turn ON relay #2 for 10 minutes (600 seconds):
```json
{
  "relay": 2,
  "state": "on",
  "duration": 5
}
```

Turn ON relay #3 for 1 hour (3600 seconds):
```json
{
  "relay": 3,
  "state": 1,
  "duration": 3600
}
```

### Examples for Different Relays

Turn ON relay #4:
```json
{
  "relay": 4,
  "state": 1
}
```

Turn OFF relay #5:
```json
{
  "relay": 5,
  "state": 0
}
```

Turn ON relay #6 for 30 seconds:
```json
{
  "relay": 6,
  "state": 1,
  "duration": 30
}
```

Turn ON relay #7 for 24 hours:
```json
{
  "relay": 7,
  "state": 1,
  "duration": 86400
}
```

### Testing JSON Commands

You can test JSON commands through the Serial Monitor using the `test_json` command. This will run two test commands:
1. Turn relay 1 ON
2. Wait 2 seconds
3. Turn relay 1 OFF

The device will output detailed debug information including:
- Raw JSON string
- Character codes
- Parsed JSON contents
- Relay and state values
- Duration (if specified)

## Technical Specifications

### Library Stack
- **LoraManager2**: Advanced LoRaWAN library with TRUE Class C support
- **SX126x-Arduino**: Hardware abstraction for SX126x LoRa chips
- **ArduinoJson v7**: Enhanced JSON processing with flexible state handling
- **RadioLib**: Core radio communication functionality

### LoRaWAN Configuration
- **Device Class**: TRUE Class C with continuous receive windows
- **Region**: US915 (configurable)
- **Sub-band**: 2 (channels 8-15)
- **Data Rate**: DR_3
- **TX Power**: 14 dBm
- **ADR**: Disabled for consistent performance
- **Public Network**: Enabled for TTN compatibility

### Memory Usage
- **RAM**: ~8.3% (27KB used)
- **Flash**: ~12.2% (407KB used)
- **Optimized**: For embedded deployment

### Uplink Data Format

The device sends status packets periodically (every 5 minutes by default) with the following format:
- Byte 0: Bit field of relay states (bit 0 = relay 1, bit 1 = relay 2, etc.)
- Byte 1: Reserved for future use

### Event Callbacks
- **onJoined()**: Successful network join
- **onJoinFailed()**: Join attempt failed
- **onDownlink()**: Downlink received with RSSI/SNR data
- **onClassChanged()**: Device class change notification
- **onTxComplete()**: Transmission completion status

## Troubleshooting

### Upload Issues

If you have trouble uploading:

1. Make sure you've entered download mode correctly (BOOT button held while pressing RESET)
2. Check that you have the correct USB drivers installed
3. Verify you're using a data-capable USB cable, not just a charging cable
4. Try a different USB port or cable
5. If using Windows, check Device Manager for unknown devices that might need drivers

### LoRaWAN Connection Issues

If the device fails to join the network:

1. Verify your EUI and key values are correct and in hex string format
2. Check that your device is configured correctly in TTN
3. Ensure you're within range of a LoRaWAN gateway
4. Try moving the device closer to a window or higher location
5. Verify that your region is set correctly in the code (US915 with sub-band 2 is the default)
6. Use the `status` serial command to check join attempts and device state
7. Monitor serial output for detailed join process information

### Relay Control Issues

If relays don't respond to commands:

1. Check your wiring connections
2. Verify the relay board is receiving adequate power
3. Use the serial monitor to send direct commands for testing
4. Check the TTN console for any errors in your downlink messages

## License

This project is licensed under the MIT License - see the LICENSE file for details. 