var DEBUG = false;
var SIMULATE = false;
var version = 'v2.2';

/* Credit goes to https://github.com/pfeffed/liftmaster_myq for figuring out 
 * all the MyQ WebService URLs and parameters
 */
var WS_APPID = "Vj8pQggXLhLy0WHahglCD4N1nAkkXQtGYpq2HrHD7H1nvmbT55KqtN6RSF4ILB%2Fi";
var WS_Culture = "en";

var WS_HOST = "https://myqexternal.myqdevice.com/";
var WS_URL_Login = WS_HOST + "Membership/ValidateUserWithCulture?appId=" + WS_APPID + "&securityToken=null&username={username}&password={password}&culture=" + WS_Culture;
var WS_URL_Device_List = WS_HOST + "api/UserDeviceDetails?appId=" + WS_APPID + "&securityToken={securityToken}";
var WS_URL_Device_GetAttr = WS_HOST + "Device/getDeviceAttribute?appId=" + WS_APPID + "&securityToken={securityToken}&devId={deviceId}&name={attrName}";
var WS_URL_Device_SetAttr = WS_HOST + "Device/setDeviceAttribute";

var WS_SetAttr_Body = { AttributeName: "", DeviceId: "0", ApplicationId: WS_APPID, AttributeValue: 0, SecurityToken: "" };

// Watch app communication function enum
var Function_Key = {
  Error: -1,
  DeviceList: 1,
  DeviceDetails: 2,
  GetStatus: 3,
  SetStatus: 4
};

// MyQ device type enum (not the same as the type IDs returned from MyQ servers)
var Device_Type = {
  Unknown: 0,
  GarageDoor: 1,
  LightSwitch: 2,
  Gate: 3
};

// MyQ device status (same as MyQ Garage Door status at least)
var Device_Status = {
  Off: 0,
  OnOpen: 1,
  Closed: 2,
  Opening: 4,
  Closing: 5,
  VGDOOpen: 9
};

var salt = "WgGF^*(@!GJEK0fkjGIfy*&*^#&*TJKSFJK357HFQWYFF761YFPSDYbsnabMNBC&*";
var failCount = 0;
var loginCount = 0;
var raw_devices = "";

// Config object that is saved in localStorage (Password is encrypted with AES)
var config = {username: "", password: "", token: "", sessionStart: null, devices: null};

// Add an int as 4 bytes to an existing byte array to pass a c-style array to the Pebble
function appendInt32(byteArray, value) {
  byteArray.push(value&0xff);
  byteArray.push((value>>8)&0xff);
  byteArray.push((value>>16)&0xff);
  byteArray.push((value>>24)&0xff);
}

// Encrypts a string with AES (see aes.js) using Pebble account token and salt as the passphrase
function encrypt(input) {
  return CryptoJS.AES.encrypt(input, Pebble.getAccountToken() + salt).toString();
}

// Decrypts a string encrypted with AES (see aes.js) using Pebble account token and salt as the passphrase
function decrypt(input) {
  return CryptoJS.AES.decrypt(input, Pebble.getAccountToken() + salt).toString(CryptoJS.enc.Utf8)
}

// Send error message to watch app
function sendError(msg) {
  Pebble.sendAppMessage({"function_key": Function_Key.Error, "error_message": msg});
}

// Load saved config details (login, session token, devices)
function loadConfig() {
  if (localStorage.config) config = JSON.parse(localStorage.config);
}

// Save config details to phone
function saveConfig() {
  localStorage.config = JSON.stringify(config);
}

// Find a device in the locally saved device list by Device ID
function findDevice(deviceID) {
  if (config && config.devices && Array.isArray(config.devices)) {
    for (var i = 0; i < config.devices.length; i++) {
      if (config.devices[i].DeviceID == deviceID) return config.devices[i];
    }
    return null;
  } else {
    return null;
  }
}

// Indicates if it looks like we have a valid security token (the server still may reject it with a -3333 error)
function haveValidToken() {
  if (config.token && (((new Date()) - new Date(config.sessionStart)) < (1000 * 60 * 20))) {
    if (DEBUG) console.log("Token age: " + ((new Date()) - new Date(config.sessionStart)));
    return true;
  } else {
    return false;
  }
}

// Convert time to a phrase used to summarize how long it has been since the time
function timeToDesc(time) {
  if (time) {
    var now = new Date();
    var today = (new Date(now)).setHours(0,0,0,0);
    var timeDate = (new Date(time)).setHours(0,0,0,0);
    if (today == timeDate) {
      var hours = time.getHours();
      var mins = time.getMinutes();
      var ampm = hours >= 12 ? 'pm' : 'am';
      hours = hours % 12;
      hours = hours ? hours : 12; // the hour '0' should be '12'
      mins = mins < 10 ? '0'+mins : mins;
      return "since " + hours + ':' + mins + ampm;
    } else {
      var dayDiff = (today-timeDate)/(1000*60*60*24);
      if (dayDiff == 1)
        return "since yesterday";
      else
        return "for " + dayDiff + " days";
    }
  } else {
    return "";
  }
}

// Make HTTP GET request to a URL. Call 'success' with response JSON on success. Call error on HTTP error
function getData(url, success, error) {
  var req = new XMLHttpRequest();
  var timeout = null;
  
  req.onload = function(e) {
    if (req.readyState == 4) {
      // HTTP request completed
      clearTimeout(timeout);
      
      if (req.status == 200) {
        if (DEBUG) console.log("GET Response: " + req.responseText);
        success(JSON.parse(req.responseText));
      } else {
        if (DEBUG) console.log("GET Error: " + req.status);
        error("HTTP error: " + req.status);
      }
    }
  };
  
  // Start 45 second timeout for HTTP request
  timeout = setTimeout(function() { req.abort(); error("Server communication timed out"); }, 45000);
  
  req.open("GET", url, true);
  req.send();
}

// Send a JSON object to a URL using a HTTP PUT request. Call 'success' on success and 'error' on HTTP error
function putData(url, data, success, error) {
  var req = new XMLHttpRequest();
  var timeout = null;
  
  req.onload = function(e) {
    if (req.readyState == 4) {
      // HTTP request completed
      clearTimeout(timeout);
      
      if (req.status == 200) {
        if (DEBUG) console.log("PUT Response: " + req.responseText);
        success(JSON.parse(req.responseText));
      } else {
        if (DEBUG) console.log("PUT Error: " + req.status);
        error("HTTP error: " + req.status);
      }
    }
  };
  
  // Start 45 seconds timeout for HTTP request
  timeout = setTimeout(function() { req.abort(); error("Server communication timed out"); }, 45000);
  
  if (DEBUG) console.log("Putting data: " + JSON.stringify(data));
  req.open("PUT", url, true);
  // Send JSON data to server
  req.setRequestHeader("Content-Type", "application/json");
  req.send(JSON.stringify(data));
}

// Login to the MyQ server to get the security token
// On success, function passed as 'success' is called with 'param' as the single parameter
// On error, function passed as 'error' is called with a string error message
function login(success, param, error) {
  if (!config.username || !config.password) {
    error("Enter both a username and password in the HomeP settings on your phone.");
  } else {
    if (DEBUG) console.log("Logging in to MyQ server...");
    try {
      if (loginCount >= 5) {
        // Stop any possibility of getting stuck in a login loop
        error("Too many login attempts");
      } else {
        loginCount++;
        
        getData(WS_URL_Login.replace("{username}", 
                                     encodeURIComponent(config.username)).replace("{password}",
                                                              encodeURIComponent(decrypt(config.password))),
               function(data) {
                 // HTTP success
                 if (data.ReturnCode) {
                   switch (data.ReturnCode) {
                     case "0":
                       // Login success
                       if (DEBUG) console.log("...Login successful");
                       if (data.SecurityToken) {
                         config.token = data.SecurityToken;
                         config.sessionStart = new Date();
                         // Security token is valid for around 20 minutes, so save it to speed up app reloads within 20 minutes
                         saveConfig();
                         success(param);
                       } else {
                         error("Missing security token");
                       }
                       break;
                     case "203":
                       // Invalid username or password
                       if (DEBUG) console.log("...invalid username/password");
                       error("Wrong username or password");
                       break;
                     default:
                       // Unknown response
                       if (DEBUG) console.log("...unknown login error - " + data.ErrorMessage + " (" + data.ReturnCode + ")");
                       if (data.ErrorMessage)
                         error(data.ErrorMessage);
                       else
                         error("Unknown server error: " + data.ReturnCode);
                       break;
                   }
                 } else {
                   error("Unexpected server response while logging in");
                 }
               }, function(msg) { error(msg); });
      }
    } catch (err) {
      error("Login error: " + err.message);
    }
  }
}

// Get an attribute value from the MyQ device object with the given name
function getAttrVal(device, name) {
  if (device && device.Attributes && Array.isArray(device.Attributes)) {
    for (var i = 0; i < device.Attributes.length; i++) {
      if (device.Attributes[i].Name == name) return device.Attributes[i].Value;
    }
    return null;
  } else {
    return "Bad Attr";
  }
}

// Get an attribute updated time from the MyQ device object with the given name
function getAttrUpdatedTime(device, name) {
  if (device && device.Attributes && Array.isArray(device.Attributes)) {
    for (var i = 0; i < device.Attributes.length; i++) {
      if (device.Attributes[i].Name == name) return new Date(parseInt(device.Attributes[i].UpdatedTime));
    }
    return null;
  } else {
    return "Bad Attr";
  }
}

// Get the name of the MyQ device that is the parent matching the parent ID from the MyQ device list
// (This is the location name of the child device)
function getParentDeviceName(devices, parentid) {
  if (devices && Array.isArray(devices)) {
    for (var i = 0; i < devices.length; i++) {
      if (devices[i].DeviceId == parentid) return getAttrVal(devices[i], "desc");
    }
    return "";
  } else {
    return "Bad Devices";
  }
}

// Get the list of devices under the MyQ account
function getDeviceList() {
  try {
    if (config.devices && Array.isArray(config.devices) && config.devices.length > 0) {
      if (DEBUG) console.log("Getting SAVED device list");
      var deviceids = [];
      // If device list has been saved, just build C-style array of device IDs for passing to the Pebble
      for (var i = 0; i < config.devices.length; i++) {
        appendInt32(deviceids, config.devices[i].DeviceID);
      }
      // Send device IDs to Pebble (which will then request individual device details)
      Pebble.sendAppMessage({"function_key": Function_Key.DeviceList, "device_list": deviceids});
    } else {
      if (DEBUG) console.log("Getting LATEST device list");
      
      // If simulating, build fake list of devices and return that
      if (SIMULATE) {
        config.devices = [];
        var updated = new Date((new Date()).getTime() - (30 * 60000));
        config.devices.push({DeviceID: 1, Type: Device_Type.GarageDoor, Location: "Home", Name: "Garage Door 1",
                             Status: Device_Status.Closed, StatusUpdated: new Date(), StatusChanged: updated});
        config.devices.push({DeviceID: 2, Type: Device_Type.GarageDoor, Location: "Home", Name: "Garage Door 2",
                             Status: Device_Status.Closed, StatusUpdated: new Date(), StatusChanged: updated});
        config.devices.push({DeviceID: 3, Type: Device_Type.LightSwitch, Location: "Home", Name: "Hall",
                             Status: Device_Status.Off, StatusUpdated: new Date(), StatusChanged: updated});
        config.devices.push({DeviceID: 4, Type: Device_Type.LightSwitch, Location: "Home", Name: "Side Door",
                             Status: Device_Status.Off, StatusUpdated: new Date(), StatusChanged: updated});
        config.devices.push({DeviceID: 5, Type: Device_Type.GarageDoor, Location: "Holiday Home", Name: "Garage Door",
                             Status: Device_Status.Closed, StatusUpdated: new Date(), StatusChanged: updated});

        var ids = [];

        // Build C-style array of device IDs for passing to the Pebble
        appendInt32(ids, 1);
        appendInt32(ids, 2);
        appendInt32(ids, 3);
        appendInt32(ids, 4);
        appendInt32(ids, 5);

        // Send FAKE device IDs to Pebble during simulation (which will then request individual device details)
        Pebble.sendAppMessage({"function_key": Function_Key.DeviceList, "device_list": ids});

        return;
      }
      
      // Else fetch the latest device list from the MyQ server
      if (haveValidToken()) {
        getData(WS_URL_Device_List.replace("{securityToken}", encodeURIComponent(config.token)),
               function(data) {
                 // HTTP Success
                 if (data.ReturnCode) {
                   switch (data.ReturnCode) {
                     case "0":
                       // Parse MyQ device list
                       raw_devices = JSON.stringify(data);
                       var deviceids = [];
                       config.sessionStart = new Date();
                       config.devices = [];
                       if (data.Devices && Array.isArray(data.Devices)) {
                         for (var i = 0; i < data.Devices.length; i++) {
                           if (data.Devices[i].DeviceId && (data.Devices[i].TypeName || data.Devices[i].TypeId)) {
                             // MyQ Garage Door openers have "garage door" in the TypeName or TypeID of 47.
                             // "MyQ Garage" devices for 3rd party devices have VGDO in the TypeName or TypeID of 259 and a 
                             //  'oemtransmitter' attribute value that is not 255 (filters out the duplicate)
                             if ((data.Devices[i].TypeName && data.Devices[i].TypeName.search(/garage\s*door/i) != -1) || 
                                 (data.Devices[i].TypeId && data.Devices[i].TypeId == 47) ||
                                 (((data.Devices[i].TypeName && data.Devices[i].TypeName.search(/gdo/i) != -1 && 
                                    data.Devices[i].TypeName.search(/gateway/i) == -1) || 
                                    (data.Devices[i].TypeId && data.Devices[i].TypeId == 259)) &&
                                       getAttrVal(data.Devices[i], "oemtransmitter") != 255 &&
                                       getAttrVal(data.Devices[i], "desc"))) {
                               
                               if (DEBUG) {
                                 console.log("Adding Garage Door - DeviceID: " + data.Devices[i].DeviceId + 
                                             ", gatewayID: " + getAttrVal(data.Devices[i], "gatewayID") + 
                                             ", desc: " + getAttrVal(data.Devices[i], "desc") + 
                                             ", doortstate: " + getAttrVal(data.Devices[i], "doorstate") + 
                                             ", stateUpdatedTime: " + getAttrUpdatedTime(data.Devices[i], "doorstate"));
                               }
                               
                               // Add Garage Door Openers devices to JS array
                               config.devices.push({DeviceID: parseInt(data.Devices[i].DeviceId),
                                                    Type: Device_Type.GarageDoor,
                                                    Location: getParentDeviceName(data.Devices, getAttrVal(data.Devices[i], "gatewayID")),
                                                    Name: getAttrVal(data.Devices[i], "desc"),
                                                    Status: parseInt(getAttrVal(data.Devices[i], "doorstate")),
                                                    StatusUpdated: new Date(),
                                                    StatusChanged: getAttrUpdatedTime(data.Devices[i], "doorstate")});
    
                               // Build C-style array of device IDs for passing to the Pebble
                               appendInt32(deviceids, parseInt(data.Devices[i].DeviceId));
                               
                             } else if ((data.Devices[i].TypeName && data.Devices[i].TypeName.search(/light/i) != -1) || 
                                 (data.Devices[i].TypeId && data.Devices[i].TypeId == 48)) {
                               
                               if (DEBUG) {
                                 console.log("Adding Light Switch - DeviceID: " + data.Devices[i].DeviceId + 
                                             ", gatewayID: " + getAttrVal(data.Devices[i], "gatewayID") + 
                                             ", desc: " + getAttrVal(data.Devices[i], "desc") + 
                                             ", lightstate: " + getAttrVal(data.Devices[i], "lightstate") + 
                                             ", stateUpdatedTime: " + getAttrUpdatedTime(data.Devices[i], "lightstate"));
                               }
                               
                               // Add Light Switch devices to JS array
                               config.devices.push({DeviceID: parseInt(data.Devices[i].DeviceId),
                                                    Type: Device_Type.LightSwitch,
                                                    Location: getParentDeviceName(data.Devices, getAttrVal(data.Devices[i], "gatewayID")),
                                                    Name: getAttrVal(data.Devices[i], "desc"),
                                                    Status: parseInt(getAttrVal(data.Devices[i], "lightstate")),
                                                    StatusUpdated: new Date(),
                                                    StatusChanged: getAttrUpdatedTime(data.Devices[i], "lightstate")});
    
                               // Build C-style array of device IDs for passing to the Pebble
                               appendInt32(deviceids, parseInt(data.Devices[i].DeviceId));
                               
                             }
                           }
                         }
                       } 
                       // Save device list
                       saveConfig();
                       // Send device IDs to Pebble (which will then request individual device details)
                       Pebble.sendAppMessage({"function_key": Function_Key.DeviceList, "device_list": deviceids});
                       
                       // On successfully completing an operation, reset the login count
                       loginCount = 0;
                       break;
                     case "-3333":
                       // Security token failed, probably due to being too old
                       failCount++;
                       if (failCount >= 5)
                         sendError("Security failed too many times");
                       else {
                         // Login again and retry this function
                         login(getDeviceList, null, function(msg) { sendError(msg); });
                       }
                       break;
                     default:
                       if (data.ErrorMessage)
                         sendError(data.ErrorMessage);
                       else
                         sendError("Unknown server error: " + data.ReturnCode);
                       break;
                   }
                 } else {
                   sendError("Unexpected server response while listing devices");
                 }
               }, function(msg) { sendError(msg); });
      } else {
        // No valid security token, so login and try again
        login(getDeviceList, null, function(msg) { sendError(msg); });
      }
    }
  } catch (err) {
    sendError("Error getting devices: " + err.message);
  }
}

// Get status of a specified device by ID
function getDeviceStatus(deviceID) {
  if (DEBUG) console.log("getDeviceStatus(" + deviceID + ")");
  try {
    var device = findDevice(deviceID);
    
    if (device) {
      if (((new Date()) - device.StatusUpdated) < 2000 || SIMULATE) {
        // If the device status is less that 2 seconds old or SIMULATING, return the saved status
        if (DEBUG) {
          if (SIMULATE)
            console.log("Simulating. Returning fake status");
          else
            console.log("Status LESS than 2 seconds old. Returning save status");
        } 
        Pebble.sendAppMessage({"function_key": Function_Key.GetStatus, 
                               "device_id": deviceID,
                               "device_status": device.Status,
                               "status_changed": timeToDesc(device.StatusChanged)});
      } else {
        // Fetch latest device status
        if (DEBUG) console.log("Status MORE than 2 seconds old. Fetching latest status");
        if (haveValidToken()) {
          var attrName = null;
          // Determine attribute used for this device's status
          if (device.Type == Device_Type.GarageDoor) {
            attrName = "doorstate";
          } else if (device.Type == Device_Type.LightSwitch) {
            attrName = "lightstate";
          }
          
          if (attrName) {
            getData(WS_URL_Device_GetAttr.replace("{securityToken}", 
                                                 encodeURIComponent(config.token)).replace("{deviceId}", 
                                                                       deviceID).replace("{attrName}", 
                                                                                    encodeURIComponent(attrName)),
                   function(data) {
                     // HTTP Success
                     if (data.ReturnCode) {
                       switch (data.ReturnCode) {
                         case "0":
                           // Success
                           if (DEBUG) console.log("Status successfully fetched");
                           config.sessionStart = new Date();
                           device.StatusUpdated = new Date();
                           if (data.AttributeValue) {
                             // Send MyQ device status to watch app
                             device.Status = parseInt(data.AttributeValue);
                             device.StatusChanged = new Date(parseInt(data.UpdatedTime));
                             Pebble.sendAppMessage({"function_key": Function_Key.GetStatus, 
                                                    "device_id": deviceID,
                                                    "device_status": device.Status,
                                                    "status_changed": timeToDesc(device.StatusChanged)});
                           } else {
                             device.Status = -2; // Missing status attribute
                             Pebble.sendAppMessage({"function_key": Function_Key.GetStatus, 
                                                    "device_id": deviceID,
                                                    "device_status": device.Status,
                                                    "status_changed": ""});
                           }
                           // Save latest status and when it was last updated
                           saveConfig();
                           
                           // On successfully completing an operation, reset the login count
                           loginCount = 0;
                           break;
                         case "-3333":
                           if (DEBUG) console.log("Security token failure - Fail count: " + failCount);
                           // Security token failed, probably due to being too old
                           failCount++;
                           if (failCount >= 5)
                             sendError("Security failed too many times");
                           else {
                             // Login again and retry this function
                             login(getDeviceStatus, deviceID, function(msg) { sendError(msg); });
                           }
                           break;
                         default:
                           if (DEBUG) console.log("Unknown status fetch error: " + data.ErrorMessage + " (" + data.ReturnCode + ")");
                           if (data.ErrorMessage)
                             sendError(data.ErrorMessage);
                           else
                             sendError("Unknown server error: " + data.ReturnCode);
                           break;
                       }
                     } else {
                       sendError("Unexpected server response while getting device status");
                     }
                   }, function(msg) { sendError(msg); });
          }
        } else {
          if (DEBUG) console.log("No valid security token, logging in and with then get device status");
          // No valid security token, so login and try again
          login(getDeviceStatus, deviceID, function(msg) { sendError(msg); });
        }
      }
    }
  } catch (err) {
    sendError("Error getting status: " + err.message);
  }
}

// Set the status of a device
// DeviceID and Status passed as params object so that it can be called as login success function
function setDeviceStatus(params) {
  try {
    var device = findDevice(params.DeviceID);
    
    if (device) {
      // If simulating, just update the device status without contacting a server
      if (SIMULATE) {
        device.Status = params.Status;
        device.StatusUpdated = new Date();
        device.StatusChanged = new Date();
        
        // Success. Let watch app know so it can update the status
        Pebble.sendAppMessage({"function_key": Function_Key.SetStatus, 
                                                "device_id": device.DeviceID});
        
        return;
      }
      
      // Else send the new status to the real MyQ server
      if (haveValidToken()) {
        var attrName = null;
        var desiredStatus = null;
        // Determine attribute and status value used for this device type
        if (device.Type == Device_Type.GarageDoor) {
          attrName = "desireddoorstate";
          desiredStatus = (params.Status == Device_Status.OnOpen) ? 1 : 0;
        } else if (device.Type == Device_Type.LightSwitch) {
          attrName = "desiredlightstate";
          desiredStatus = (params.Status == Device_Status.OnOpen) ? 1 : 0;
        } 
        if (attrName) {
          // Setup object for setting device attribute
          WS_SetAttr_Body.SecurityToken = config.token;
          WS_SetAttr_Body.DeviceId = params.DeviceID.toString();
          WS_SetAttr_Body.AttributeName = attrName;
          WS_SetAttr_Body.AttributeValue = desiredStatus;
          
          // Send attribute data to server
          putData(WS_URL_Device_SetAttr, WS_SetAttr_Body, 
                 function(data) {
                   // HTTP Success
                   if (data.ReturnCode) {
                     switch (data.ReturnCode) {
                       case "0":
                         // Success. Let watch app know so it can start checking for status change
                         Pebble.sendAppMessage({"function_key": Function_Key.SetStatus, 
                                                "device_id": device.DeviceID});
                         
                         config.sessionStart = new Date();
                         saveConfig();
                         break;
                       case "-3333":
                         // Security token failed, probably due to being too old
                         failCount++;
                         if (failCount >= 5)
                           sendError("Security failed too many times");
                         else {
                           // Login again and retry this function after a brief pause
                           login(setDeviceStatus, params, function(msg) { sendError(msg); });
                         }
                         
                         // On successfully completing an operation, reset the login count
                         loginCount = 0;
                         break;
                       default:
                         if (data.ErrorMessage)
                           sendError(data.ErrorMessage);
                         else
                           sendError("Unknown server error: " + data.ReturnCode);
                         break;
                     }
                   } else {
                     sendError("Unexpected server response while setting device status");
                   }
                 }, function(msg) { sendError(msg); });
        }
      } else {
        // No valid security token, so login and try again
        login(setDeviceStatus, params, function(msg) { sendError(msg); });
      }
    }
  } catch (err) {
    sendError("Error setting status: " + err.message);
  }
}

// Initialize app
function init() {
  if (!SIMULATE && (!config.username || !config.password || !decrypt(config.password))) {
    Pebble.sendAppMessage({"function_key": Function_Key.Error,
                           "error_message": "Enter both a username and password in the HomeP settings on your phone."});
  } else {
    // Trigger device list to be sent to watch app
    getDeviceList();
  }
}

Pebble.addEventListener("ready",
                        function(e) {
                          if (DEBUG) console.log("JS Ready");
                          // Application startup,
                          loadConfig();
                          init();
                        });

Pebble.addEventListener("appmessage",
                        function(e) {
                          if (DEBUG) console.log("Pebble App Message! - " + JSON.stringify(e.payload));
                          
                          if (e.payload && e.payload.function_key) {
                            // Received valid message from watch app
                            switch (e.payload.function_key) {
                              case Function_Key.DeviceDetails:
                                // Watch app requesting device details
                                if (e.payload.device_id) {
                                  try {
                                  var device = findDevice(e.payload.device_id);
                                  
                                  if (device) {
                                      if (DEBUG) console.log("Sending details for device ID: " + device.DeviceID);
                                      Pebble.sendAppMessage({"function_key": Function_Key.DeviceDetails, 
                                                             "device_id": device.DeviceID,
                                                             "device_location": device.Location,
                                                             "device_name": device.Name,
                                                             "device_type": device.Type});
                                  } else {
                                    // Unknown device
                                    Pebble.sendAppMessage({"function_key": Function_Key.DeviceDetails, 
                                                               "device_id": e.payload.device_id,
                                                               "device_location": "Unknown",
                                                               "device_name": "Device",
                                                               "device_type": 0});
                                  }
                                  } catch (err) {
                                    sendError("Error getting device details: " + err.message);
                                  }
                                }
                                break;
                                
                              case Function_Key.GetStatus:
                                // Watch app requesting device status
                                if (e.payload.device_id) {
                                  if (DEBUG) console.log("Sending status for ID: " + e.payload.device_id);
                                  
                                  getDeviceStatus(e.payload.device_id);
                                  
                                }
                                break;
                                
                              case Function_Key.SetStatus:
                                // Watch app setting device status
                                if (e.payload.device_id && e.payload.device_status !== null) {
                                  if (DEBUG) console.log("Setting status for ID " + e.payload.device_id + " to: " + e.payload.device_status);
                                  
                                  setDeviceStatus({DeviceID: e.payload.device_id, Status: e.payload.device_status});
                                }
                                break;
                            }
                          }
                        });

Pebble.addEventListener("showConfiguration", 
                         function() {
                           if (DEBUG) console.log("Showing Settings...");
                           
                           // Settings page HTML, which will be used in a Data URI
                           var html = '<html>\
	<head>\
		<meta charset="utf-8" /><meta name="viewport" content="width=device-width, initial-scale=1" />\
		<script type="text/javascript" language="Javascript">\
			function login(refreshDevices) {\
				var username = document.getElementById("username");\
				var password = document.getElementById("password");\
				if (username.value.trim() == "") {\
					alert("A username must be entered.");\
					username.focus();\
					return false;\
				}\
				if (password.value.trim() == "") {\
					alert("A password must be entered.");\
					password.focus();\
					return false;\
				}\
				document.location = "pebblejs://close#" + encodeURIComponent(JSON.stringify({"username":username.value,"password":password.value,"refreshDevices":refreshDevices}));\
				return true;\
			}\
		</script>\
	</head>\
<body style="font-family: sans-serif;">\
		<p>Setup your account and devices as per the MyQ&#8482; device manufacturer&apos;s instructions first.</p>\
    <p>Enter your MyQ&#8482; username and password below and tap "Login". These will be saved on your phone and only ever sent to the official MyQ&#8482; servers. If you lose your phone, change your password as soon as possible. If you lose your Pebble, delete the Bluetooth connection on your phone.</p>\
		<p>If you add devices to your account at a later date, tap "Refresh Devices" below.</p>\
		<p>NOTE: Most garage door openers will beep for a period before starting to close when operated remotely. This is functionality built into the garage door opener and is done for safety under UL standards. Always operate garage doors with caution.</p>\
		<fieldset>\
			<label for="username">Username:</label><br>\
			<input type="email" id="username" style="width: 90%; font-size: larger;" value="' + config.username + '" ><br>\
			<label for="password">Password:</label><br>\
			<input type="password" id="password" style="width: 90%; font-size: larger;" autocomplete="off" value="' + decrypt(config.password) + '" >\
		</fieldset>\
		<fieldset>\
			<input type="button" value="Login" style="font-size: larger;" onclick="login(false);" />\
			<input type="button" value="Refresh Devices" style="font-size: larger;" onclick="login(true);" />\
			<div style="float: right; font-size: xx-small;">' + version + '</div>\
		</fieldset><br>\
		<fieldset>\
			<label>Raw Device Data:</label>\
			<p>If "No devices found" or "Unknown device" is displayed on your watch and you have used the correct username and password above and tapped <b>Refresh Devices</b>, then the raw device data will need to be examined to determine the cause.</p>';
      
      if (!raw_devices) {
        html += '<p>To get the raw device data, tap <b>Refresh Devices</b> (which will close these settings), wait for the watchapp to finish updating and then come back to the HomeP settings while keeping the HomeP watch app open. If this message does not change, then the device data is not being retrieved at all - Check your username and password and make sure it is working in the MyQ phone app or website and try again.</p>';
      } else {
        // Remove email address
        var anon_devices = raw_devices.replace(/"[^"@]+@[^"@]+\.[^"@]+"/g, '""');
        // Remove serial numbers
        anon_devices = anon_devices.replace(/"(CG|GW)[A-Z0-9]{10}"/g, '""');
        // Remove correlation ID
        anon_devices = anon_devices.replace(/"CorrelationId":"[^"]+"/gi, '"CorrelationId":""');
        // Replace Device IDs (generally between 5 and 10 digits) with unique numbers (starting at 1)
        var reID = new RegExp("[0-9]{5,10}", "g");
        var reNew;
        var id, newID = 0;
        while ((id = reID.exec(anon_devices)) !== null) {
          reNew = new RegExp("([^0-9])" + id[0] + "([^0-9])", "g");
          newID++;
          anon_devices = anon_devices.replace(reNew, "$1" + newID.toString() + "$2");
        }
        
			  html += '<p>Tap <b>Show Raw Device Data</b> and then select and copy all the text in the box below and paste it into the email started by going to the HomeP Pebble app store page and selecting <i>Email Developer for Support</i> (Or find the HomeP thread on the Pebble forums and PM the data to the developer - While best efforts have been made to anonymize the device data, DO NOT post the raw data to public forums)</p>\
			<p><input type="button" value="Show Raw Device Data" style="font-size: larger;" onclick="document.getElementById(&#39;rawdevicedata&#39;).style.display = &#39;block&#39;;" /></p>\
			<div id="rawdevicedata" style="display: none;"><textarea rows="4" cols="40">' + anon_devices + '</textarea></div>\
		</fieldset>';
      }
      html += '</body></html><!--.html'; // Open .html comment is for some versions of Android to show this correctly
                           
                           // Open above HTML as a Data URI
                           Pebble.openURL("data:text/html," + encodeURIComponent(html));
                          });

Pebble.addEventListener("webviewclosed",
                         function(e) {
                           if (e.response) {
                             // 'Login' or 'Refresh Devices' tapped
                             if (DEBUG) console.log("Webview closed");
                             var settings = null;
                             try {
                               settings = JSON.parse(e.response);
                             } catch(ex) {
                               settings = JSON.parse(decodeURIComponent(e.response));
                             }
                             // Username and Password should always be returned
                             config.username = settings.username;
                             config.password = encrypt(settings.password);
                             // Reset login session
                             config.token = null;
                             config.sessionStart = null;
                             // Always force refresh of device list after tapping 'Login' or 'Refresh Devices'
                             config.devices = null;
                             
                             saveConfig();
                             
                             // Trigger fetching device list
                             init();
                           }
                           else {
                             // Cancel tapped
                             if (DEBUG) console.log("Settings cancelled");
                           }
                         });