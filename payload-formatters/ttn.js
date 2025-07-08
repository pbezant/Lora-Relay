// Encoder function for downlinks
function encodeDownlink(input) {
  // Accept either a single object or an array of objects
  let data = input.data;
  if (Array.isArray(data) || (typeof data === 'object' && data !== null)) {
    // Convert to JSON string and then to bytes
    const jsonString = JSON.stringify(data);
    const bytes = [];
    for (let i = 0; i < jsonString.length; i++) {
      bytes.push(jsonString.charCodeAt(i));
    }
    return {
      bytes: bytes,
      fPort: input.fPort || 2
    };
  }
  // Default empty response
  return {
    bytes: [],
    fPort: input.fPort || 2
  };
}

// Decoder function for downlinks
function decodeDownlink(input) {
  // Try to parse as JSON (array or object)
  try {
    let str = "";
    for (let i = 0; i < input.bytes.length; i++) {
      str += String.fromCharCode(input.bytes[i]);
    }
    const data = JSON.parse(str);
    return {
      data: data
    };
  } catch (err) {
    // If parsing fails, return raw bytes
    return {
      data: {
        bytes: input.bytes,
        type: "binary"
      }
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
    
    return {
      data: {
        hex: hexStr
      }
    };
  }