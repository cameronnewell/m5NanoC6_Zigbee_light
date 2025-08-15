// m5nanoc6_light.js
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const extend = require('zigbee-herdsman-converters/lib/extend');
const ota = require('zigbee-herdsman-converters/lib/ota');
const e = exposes.presets;
const ea = exposes.access;

const device = {
    zigbeeModel: ['ESP32C6.Light'], // This MUST exactly match your 'modelid' from Arduino sketch (without length prefix)
    model: 'M5NanoC6-Light', // A user-friendly model name for Zigbee2MQTT
    vendor: 'M5Stack / Espressif', // A user-friendly vendor name
    description: 'M5NanoC6 Zigbee On/Off Light',
    // The manufacturerName MUST exactly match your 'manufname' from Arduino sketch (without length prefix)
    // Note: The 'manufname' in your code was {9, 'E', 's', 'p', 'r', 'e', 's', 's', 'i', 'f'} -> "Espressif"
    // So, it's 'Espressif' here. Case-sensitive.
    manufacturerName: 'Espressif',
    extend: extend.switch(), // This uses a built-in converter for simple on/off switches
    configure: async (device, coordinator, leptopology) => {
        // This 'configure' section is important for the device to report its state.
        // It tells the device to send reports when its on/off state changes.
        // Adjust endpoint if your device is not on endpoint 1. Your code uses HA_ESP_LIGHT_ENDPOINT 10.
        const endpoint = device.getEndpoint(10); // Get endpoint 10 as defined in your code
        await reporting.bind(endpoint, coordinator, ['genOnOff']);
        await reporting.onOff(endpoint);
    },
};

module.exports = device;
