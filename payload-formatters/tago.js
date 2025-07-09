/**
 * Tago Payload Formatter for LoRa Relay Control System
 * 
 * This formatter handles:
 * - Uplink decoding: Device data → Tago variables
 * - Downlink encoding: Hex-encoded JSON → Binary format for TTN
 * 
 * Usage Examples:
 * 
 * UPLINK (Device → Tago):
 * Device sends relay status data, gets decoded into structured variables
 * 
 * DOWNLINK (Tago → Device):
 * Tago sends hex-encoded JSON like:
 * "7B2272656C617973223A5B7B2272656C6179223A312C227374617465223A747275652C226475726174696F6E223A353030307D5D7D"
 * Which decodes to: {"relays":[{"relay":1,"state":true,"duration":5000}]}
 */

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * Convert hex string to regular string
 * @param {string} hex - Hex string to convert
 * @returns {string} - Decoded string
 */
function hexToString(hex) {
  let str = '';
  for (let i = 0; i < hex.length; i += 2) {
    const hexChar = hex.substr(i, 2);
    str += String.fromCharCode(parseInt(hexChar, 16));
  }
  return str;
}

/**
 * Convert string to hex string
 * @param {string} str - String to convert
 * @returns {string} - Hex encoded string
 */
function stringToHex(str) {
  let hex = '';
  for (let i = 0; i < str.length; i++) {
    hex += str.charCodeAt(i).toString(16).padStart(2, '0');
  }
  return hex.toUpperCase();
}

/**
 * Convert bytes array to hex string
 * @param {number[]} bytes - Array of bytes
 * @returns {string} - Hex string
 */
function bytesToHex(bytes) {
  return bytes.map(b => b.toString(16).padStart(2, '0')).join('').toUpperCase();
}

/**
 * Convert hex string to bytes array
 * @param {string} hex - Hex string
 * @returns {number[]} - Array of bytes
 */
function hexToBytes(hex) {
  const bytes = [];
  for (let i = 0; i < hex.length; i += 2) {
    bytes.push(parseInt(hex.substr(i, 2), 16));
  }
  return bytes;
}

// ============================================================================
// TAGO UPLINK DECODER (Device → Tago)
// ============================================================================

/**
 * Decode uplink payload from device
 * @param {Object} payload - Tago payload object
 * @returns {Object[]} - Array of Tago variables
 */
function decodeUplink(payload) {
  const data = [];
  
  try {
    // Get the raw payload data
    const rawPayload = payload.payload_raw || payload.payload || payload.data;
    
    if (!rawPayload) {
      data.push({
        variable: 'error',
        value: 'No payload data received',
        unit: 'text'
      });
      return data;
    }
    
    // Convert hex string to bytes if needed
    let bytes;
    if (typeof rawPayload === 'string') {
      bytes = hexToBytes(rawPayload);
    } else if (Array.isArray(rawPayload)) {
      bytes = rawPayload;
    } else {
      bytes = Object.values(rawPayload);
    }
    
    // Add raw hex data for debugging
    data.push({
      variable: 'raw_hex',
      value: bytesToHex(bytes),
      unit: 'hex'
    });
    
    // Check if this is a multi-relay binary response (starts with 0xFF)
    if (bytes.length >= 2 && bytes[0] === 0xFF) {
      const relayCount = bytes[1];
      const expectedLength = 2 + (relayCount * 4);
      
      data.push({
        variable: 'message_type',
        value: 'multi_relay_binary',
        unit: 'text'
      });
      
      data.push({
        variable: 'relay_count',
        value: relayCount,
        unit: 'count'
      });
      
      if (bytes.length === expectedLength) {
        // Decode each relay
        for (let i = 0; i < relayCount; i++) {
          const offset = 2 + (i * 4);
          const relayNum = bytes[offset];
          const state = bytes[offset + 1];
          const durationLow = bytes[offset + 2];
          const durationHigh = bytes[offset + 3];
          const duration = durationLow + (durationHigh << 8);
          
          data.push({
            variable: `relay_${relayNum}_state`,
            value: state,
            unit: 'bool'
          });
          
          data.push({
            variable: `relay_${relayNum}_duration`,
            value: duration,
            unit: 'ms'
          });
        }
      } else {
        data.push({
          variable: 'error',
          value: `Invalid binary format: expected ${expectedLength} bytes, got ${bytes.length}`,
          unit: 'text'
        });
      }
    } else {
      // Try to parse as JSON (single-relay or legacy format)
      try {
        const jsonString = bytes.map(b => String.fromCharCode(b)).join('');
        const jsonData = JSON.parse(jsonString);
        
        data.push({
          variable: 'message_type',
          value: 'json',
          unit: 'text'
        });
        
        // Handle single relay command
        if (jsonData.relay !== undefined) {
          data.push({
            variable: `relay_${jsonData.relay}_state`,
            value: jsonData.state ? 1 : 0,
            unit: 'bool'
          });
          
          if (jsonData.duration !== undefined) {
            data.push({
              variable: `relay_${jsonData.relay}_duration`,
              value: jsonData.duration,
              unit: 'ms'
            });
          }
        }
        
        // Handle multi-relay array
        if (jsonData.relays && Array.isArray(jsonData.relays)) {
          data.push({
            variable: 'relay_count',
            value: jsonData.relays.length,
            unit: 'count'
          });
          
          jsonData.relays.forEach(relay => {
            data.push({
              variable: `relay_${relay.relay}_state`,
              value: relay.state ? 1 : 0,
              unit: 'bool'
            });
            
            if (relay.duration !== undefined) {
              data.push({
                variable: `relay_${relay.relay}_duration`,
                value: relay.duration,
                unit: 'ms'
              });
            }
          });
        }
        
      } catch (jsonError) {
        // Not JSON, treat as raw sensor data or status
        data.push({
          variable: 'message_type',
          value: 'raw_data',
          unit: 'text'
        });
        
        // Add individual bytes for analysis
        bytes.forEach((byte, index) => {
          data.push({
            variable: `byte_${index}`,
            value: byte,
            unit: 'int'
          });
        });
      }
    }
    
    // Add timestamp
    data.push({
      variable: 'timestamp',
      value: new Date().toISOString(),
      unit: 'datetime'
    });
    
  } catch (error) {
    data.push({
      variable: 'decode_error',
      value: error.message,
      unit: 'text'
    });
  }
  
  return data;
}

// ============================================================================
// TAGO DOWNLINK ENCODER (Tago → Device via TTN)
// ============================================================================

/**
 * Encode downlink payload for device
 * @param {Object} payload - Tago payload object containing hex-encoded JSON
 * @returns {Object} - Encoded payload for TTN
 */
function encodeDownlink(payload) {
  try {
    // Get the command data from Tago
    const commandData = payload.payload_raw || payload.payload || payload.data || payload;
    
    let jsonData;
    
    // Check if the data is hex-encoded JSON
    if (typeof commandData === 'string' && commandData.length > 0) {
      try {
        // Try to decode as hex-encoded JSON
        const jsonString = hexToString(commandData);
        jsonData = JSON.parse(jsonString);
      } catch (hexError) {
        // If hex decoding fails, try direct JSON parsing
        try {
          jsonData = JSON.parse(commandData);
        } catch (jsonError) {
          return {
            error: `Failed to parse command data: ${hexError.message} / ${jsonError.message}`,
            bytes: [],
            fPort: 1
          };
        }
      }
    } else if (typeof commandData === 'object') {
      jsonData = commandData;
    } else {
      return {
        error: 'Invalid command data format',
        bytes: [],
        fPort: 1
      };
    }
    
    // Use the same encoding logic as TTN formatter
    return encodeRelayCommand(jsonData);
    
  } catch (error) {
    return {
      error: `Encoding error: ${error.message}`,
      bytes: [],
      fPort: 1
    };
  }
}

/**
 * Encode relay command using TTN logic
 * @param {Object} data - Command data object
 * @returns {Object} - Encoded bytes and metadata
 */
function encodeRelayCommand(data) {
  // Check if this is a multi-relay command with 'relays' array
  if (data && data.relays && Array.isArray(data.relays)) {
    // Convert multi-relay JSON to compact binary format
    const relays = data.relays;
    const bytes = [];
    
    // Magic byte to indicate multi-relay binary command
    bytes.push(0xFF);
    
    // Relay count
    bytes.push(relays.length);
    
    // Encode each relay (4 bytes per relay)
    for (const relay of relays) {
      bytes.push(relay.relay || 0);           // Relay number (1-8)
      bytes.push(relay.state ? 1 : 0);        // State (0/1)
      
      const duration = relay.duration || 0;
      bytes.push(duration & 0xFF);            // Duration low byte
      bytes.push((duration >> 8) & 0xFF);     // Duration high byte
    }
    
    return {
      bytes: bytes,
      fPort: 1,
      confirmed: false
    };
  }
  
  // Single object or other format - use JSON
  if (typeof data === 'object' && data !== null) {
    const jsonString = JSON.stringify(data);
    const bytes = [];
    for (let i = 0; i < jsonString.length; i++) {
      bytes.push(jsonString.charCodeAt(i));
    }
    return {
      bytes: bytes,
      fPort: 1,
      confirmed: false
    };
  }
  
  // Default empty response
  return {
    bytes: [],
    fPort: 1,
    confirmed: false
  };
}

// ============================================================================
// MAIN TAGO FUNCTION
// ============================================================================

/**
 * Main Tago payload parser function
 * @param {Object} payload - Tago payload object
 * @returns {Object[]} - Array of Tago variables or encoded downlink
 */
function main(payload) {
  // Determine if this is an uplink or downlink
  const isDownlink = payload.downlink_url || payload.command || payload.type === 'downlink';
  
  if (isDownlink) {
    // This is a downlink command from Tago to device
    return encodeDownlink(payload);
  } else {
    // This is an uplink message from device to Tago
    return decodeUplink(payload);
  }
}

// Export for different environments
if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    main,
    decodeUplink,
    encodeDownlink,
    hexToString,
    stringToHex,
    bytesToHex,
    hexToBytes
  };
}

// ============================================================================
// USAGE EXAMPLES AND DOCUMENTATION
// ============================================================================

/*
UPLINK EXAMPLES:

1. Multi-relay binary response (device status):
   Input: [0xFF, 0x02, 0x01, 0x01, 0x88, 0x13, 0x02, 0x00, 0x10, 0x27]
   Output: [
     {variable: 'message_type', value: 'multi_relay_binary', unit: 'text'},
     {variable: 'relay_count', value: 2, unit: 'count'},
     {variable: 'relay_1_state', value: 1, unit: 'bool'},
     {variable: 'relay_1_duration', value: 5000, unit: 'ms'},
     {variable: 'relay_2_state', value: 0, unit: 'bool'},
     {variable: 'relay_2_duration', value: 10000, unit: 'ms'}
   ]

2. JSON response:
   Input: '{"relay":1,"state":true,"duration":5000}'
   Output: [
     {variable: 'message_type', value: 'json', unit: 'text'},
     {variable: 'relay_1_state', value: 1, unit: 'bool'},
     {variable: 'relay_1_duration', value: 5000, unit: 'ms'}
   ]

DOWNLINK EXAMPLES:

1. Single relay command:
   Tago sends: "7B2272656C6179223A312C227374617465223A747275652C226475726174696F6E223A353030307D"
   Decodes to: {"relay":1,"state":true,"duration":5000}
   Output: {bytes: [123,34,114,101,108,97,121,34,58,49,44,34,115,116,97,116,101,34,58,116,114,117,101,44,34,100,117,114,97,116,105,111,110,34,58,53,48,48,48,125], fPort: 1}

2. Multi-relay command:
   Tago sends: "7B2272656C617973223A5B7B2272656C6179223A312C227374617465223A747275652C226475726174696F6E223A353030307D2C7B2272656C6179223A322C227374617465223A66616C73652C226475726174696F6E223A307D5D7D"
   Decodes to: {"relays":[{"relay":1,"state":true,"duration":5000},{"relay":2,"state":false,"duration":0}]}
   Output: {bytes: [0xFF, 0x02, 0x01, 0x01, 0x88, 0x13, 0x02, 0x00, 0x00, 0x00], fPort: 1}

HEX ENCODING HELPER:
To convert JSON to hex for Tago:
1. JSON: {"relay":1,"state":true,"duration":5000}
2. Hex: 7B2272656C6179223A312C227374617465223A747275652C226475726174696F6E223A353030307D

Use an online JSON to hex converter or the stringToHex() function in this file.
*/ 