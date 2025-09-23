#ifndef CONFIG_H
#define CONFIG_H

// MTN/Vodacom South Africa APN settings
const char* apn = "internet";
const char* gprsUser = "guest";
const char* gprsPass = "";

// // Kenya's Safaricom APN settings
// const char* apn = "safaricom";
// const char* gprsUser = "";
// const char* gprsPass = "";


/* To get more details about the Access Point names of your country
check https://www.teltonika.org/apn-list/
*/

// SIM PIN (change this to your actual PIN)
const char* simPIN = "1234"; // Replace with your actual PIN

// Server URL for sending water meter data
const char* serverUrl = "http://192.168.1.105:9000/upload"; // Replace with your actual server URL

#endif
