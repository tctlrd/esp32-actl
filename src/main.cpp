/*
MIT License

Augmented by tctlrd 2025
Copyright (c) 2024 pvtex
Copyright (c) 2018 esp-rfid Community
Copyright (c) 2017 Ömer Şiar Baysal

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#define VERSION "0.3.0"

#ifdef ETHERNET
bool eth_connected = false;
#endif

#include "Arduino.h"
#include <WiFi.h>
#include <SPI.h>
#include <ESPmDNS.h>
#define ARDUINOJSON_DECODE_UNICODE 0
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include "esp_flash.h" 
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TimeLib.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <Bounce2.h>
#include <Desfire.h>
#include <PN532.h>
//#include <esp_task_wdt.h>
#include <Update.h>
#include "magicnumbers.h"
#include "config.h"

Config config;

#include <WiegandNG.h>

File fsUploadFile;

WiegandNG wg;

// relay specific variables
#if MAX_NUM_RELAYS == 4
	bool activateRelay[MAX_NUM_RELAYS] = {false, false, false, false};
	bool deactivateRelay[MAX_NUM_RELAYS] = {false, false, false, false};
#endif
#if MAX_NUM_RELAYS == 3
	bool activateRelay[MAX_NUM_RELAYS] = {false, false, false};
	bool deactivateRelay[MAX_NUM_RELAYS] = {false, false, false};
#endif
#if MAX_NUM_RELAYS == 2
	bool activateRelay[MAX_NUM_RELAYS] = {false, false};
	bool deactivateRelay[MAX_NUM_RELAYS] = {false, false};
#endif
#if MAX_NUM_RELAYS == 1
	bool activateRelay[MAX_NUM_RELAYS] = {false};
	bool deactivateRelay[MAX_NUM_RELAYS] = {false};
#endif

Desfire desfire;

// The PICC master key.
// This 3K3DES or AES key is the "god key".
// It allows to format the card and erase ALL it's content (except the PICC master key itself).
// This key will be stored on your Desfire card when you execute the command "ADD {Username}" in the terminal.
// To restore the master key to the factory default DES key use the command "RESTORE" in the terminal.
// If you set the compiler switch USE_AES = true, only the first 16 bytes of this key will be used.
// IMPORTANT: Before changing this key, please execute the RESTORE command on all personalized cards!
// IMPORTANT: When you compile for DES, the least significant bit (bit 0) of all bytes in this key 
//            will be modified, because it stores the key version.
const byte SECRET_PICC_MASTER_KEY[24] = { 0xAA, 0x08, 0x57, 0x92, 0x1C, 0x76, 0xFF, 0x65, 0xE7, 0xD2, 0x78, 0x44, 0xF8, 0x0F, 0x8D, 0x1B, 0xE7, 0xC2, 0xF0, 0x89, 0x04, 0xC0, 0xC3, 0xE3 };

// This 3K3DES key is used to derive a 16 byte application master key from the UID of the card and the user name.
// The purpose is that each card will have it's unique application master key that can be calculated from known values.
const byte SECRET_APPLICATION_KEY[24] = { 0x81, 0xDF, 0x6A, 0xD9, 0x89, 0xE9, 0xA2, 0xD1, 0xC5, 0xB3, 0xE3, 0x9D, 0xE9, 0x60, 0x43, 0xE3, 0x5B, 0x60, 0x85, 0x8B, 0x99, 0xD8, 0xD3, 0x5B };

// This 3K3DES key is used to derive the 16 byte store value from the UID of the card and the user name.
// This value is stored in a standard data file on the card.
// The purpose is that each card will have it's unique store value that can be calculated from known values.
const byte SECRET_STORE_VALUE_KEY[24] = { 0x1E, 0x5D, 0x78, 0x57, 0x68, 0xFC, 0xEE, 0xC9, 0x40, 0xEC, 0x30, 0xDE, 0xEC, 0xA9, 0x8B, 0x3C, 0x7F, 0x8A, 0xC9, 0xC3, 0xAA, 0xD7, 0x4F, 0x17 };

// -----------------------------------------------------------------------------------------------------------

// The ID of the application to be created
// This value must be between 0x000001 and 0xFFFFFF (NOT zero!)
const uint32_t CARD_APPLICATION_ID = 0xAA401F;

// The ID of the file to be created in the above application
// This value must be between 0 and 31
const byte CARD_FILE_ID = 0;

// This 8 bit version number is uploaded to the card together with the key itself.
// This version is irrelevant for encryption. 
// It is just a version number for the key that you can obtain with Desfire::GetKeyVersion().
// The key version can always be obtained without authentication.
// You can theoretically have multiple master keys and by obtaining the version you know which one to use for authentication.
// This value must be between 1 and 255 (NOT zero!)
const byte CARD_KEY_VERSION = 0x10;

// these are from vendors
#include "webh/glyphicons-halflings-regular.woff.gz.h"
#include "webh/required.css.gz.h"
#include "webh/required.js.gz.h"

// these are from us which can be updated and changed
#include "webh/esprfid.js.gz.h"
#include "webh/boards.js.gz.h"
#include "webh/esprfid.htm.gz.h"
#include "webh/index.html.gz.h"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;
Ticker wsMessageTicker;
Bounce openLockButton;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

#define LEDoff HIGH
#define LEDon LOW

#define BEEPERoff HIGH
#define BEEPERon LOW

// Variables for whole scope
unsigned long cooldown = 0;
unsigned long currentMillis = 0;
unsigned long deltaTime = 0;
bool doEnableWifi = false;
bool doEnableEth = false;
bool formatreq = false;
const char *httpUsername = "admin";
unsigned long keyTimer = 0;
uint8_t lastDoorbellState = 0;
uint8_t lastDoorState = 0;
uint8_t lastTamperState = 0;
unsigned long nextbeat = 0;
time_t epoch;
time_t lastNTPepoch;
unsigned long lastNTPSync = 0;
unsigned long openDoorMillis = 0;
unsigned long previousLoopMillis = 0;
unsigned long previousMillis = 0;
bool shouldReboot = false;
tm timeinfo;
unsigned long uptimeSeconds = 0;
unsigned long wifiPinBlink = millis();
unsigned long wiFiUptimeMillis = 0;


#include "led.esp"
#include "beeper.esp"
#include "log.esp"
#include "mqtt.esp"
#include "helpers.esp"
#include "wsResponses.esp"
#include "rfid.esp"
#include "wifi.esp"
#ifdef ETHERNET
#include "ethernet.esp"
#endif
#include "config.esp"
#include "websocket.esp"
#include "webserver.esp"
#include "door.esp"
#include "doorbell.esp"

char* numberToHexStr(char* out, unsigned char* in, size_t length)
{
        char* ptr = out;
        for (int i = length-1; i >= 0 ; i--)
            ptr += sprintf(ptr, "%02X", in[i]);
        return ptr;
}

void ICACHE_FLASH_ATTR setup()
{
#ifdef DEBUG
	Serial.begin(115200);
	Serial.println();

	Serial.print(F("[ INFO ] ESP32-ACTL v"));
	Serial.print(VERSION);
#ifdef ETHERNET
	Serial.print(" eth");
#endif
#ifdef DEBUG
	Serial.print(" debug");
#endif
	Serial.println("");

	uint32_t realSize;
    esp_flash_get_size(NULL, &realSize);
	uint32_t ideSize = ESP.getFlashChipSize();
	FlashMode_t ideMode = ESP.getFlashChipMode();
	
	Serial.print("ESP32 Model:      ");
	Serial.print(ESP.getChipModel());
	Serial.print(" rev");
	Serial.println(ESP.getChipCores());
	Serial.print("ESP32 Cores:       ");
	Serial.println(ESP.getChipCores());
	uint32_t chipID = 0;
	for (int i = 0; i < 17; i = i + 8) {
    	chipID |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  	}
	Serial.printf("ESP32 Chip ID:   %d\n", chipID);

	Serial.printf("Flash real size: %u\n\n", realSize);
	Serial.printf("Flash ide  size: %u\n", ideSize);
	Serial.printf("Flash ide speed: %u\n", ESP.getFlashChipSpeed());
	Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT"
																	: ideMode == FM_DIO	   ? "DIO"
																	: ideMode == FM_DOUT   ? "DOUT"
																						   : "UNKNOWN"));
	if (ideSize != realSize)
	{
		Serial.println(F("Flash Chip configuration wrong!\n"));
	}
	else
	{
		Serial.println(F("Flash Chip configuration ok.\n"));
	}
#endif
	if (!LittleFS.begin(true))
	{
#ifdef DEBUG
		Serial.println(F("[ ERROR ] Filesystem ERROR!"));
#endif
	} else
	{
#ifdef DEBUG
			Serial.println(F("[ INFO ] Filesystem OK"));
#endif
	}

	File root = LittleFS.open("/P");
	if(!root.isDirectory())
	{
        LittleFS.mkdir("/P");
    }
	
	bool configured = false;
	configured = loadConfiguration(config);
#ifdef ETHERNET
	bool configuredeth = false;
	configuredeth = configured;
	eth_connected = false;
	setupEth(configuredeth);

	config.ipAddressEth = ETH.localIP();
	config.gatewayIpEth = ETH.gatewayIP();
	config.subnetIpEth = ETH.subnetMask();
	config.dnsIpEth = ETH.dnsIP();
	config.ethmac = ETH.macAddress();
	
    String linkduplex = "HD";
	if (ETH.fullDuplex() == true) 
	{
		linkduplex = "FD";
	}
	char spd[12]; 
	sprintf(spd, "%dMbps %s", ETH.linkSpeed(), linkduplex);
	config.ethlink = (String)spd;
#endif
	setupWifi(configured);

	setupMqtt();
	setupWebServer();
	writeEvent("INFO", "sys", "System setup completed, running", "");
#ifdef DEBUG
	Serial.println(F("[ INFO ] System setup completed, running"));
#endif
	desfire.setPiccMasterKey(SECRET_PICC_MASTER_KEY);
	desfire.setApplicationKey(SECRET_APPLICATION_KEY);
	desfire.setStoreValueKey(SECRET_STORE_VALUE_KEY);
	desfire.setCardApplicationId(CARD_APPLICATION_ID);
	desfire.setCardFileId(CARD_FILE_ID);
	desfire.setCardKeyVersion(CARD_KEY_VERSION);
}

void ICACHE_RAM_ATTR loop()
{
	currentMillis = millis();
	deltaTime = currentMillis - previousLoopMillis;
	uptimeSeconds = currentMillis / 1000;
	previousLoopMillis = currentMillis;
	
	trySyncNTPtime(10);
	
	
	if (config.openlockpin != 255)
	{
		openLockButton.update();
		if (openLockButton.fell())
		{
			writeLatest(" ", "Button", 1);
			mqttPublishAccess(epoch, "true", "Always", "Button", " ", " ");
			activateRelay[0] = true;
			beeperValidAccess();
			// TODO: handle other relays
		}
	}

	ledWifiStatus();
	ledAccessDeniedOff();
	beeperBeep();
	doorStatus();
	doorbellStatus();

	if (currentMillis >= cooldown)
	{
		rfidLoop();
	}

	for (int currentRelay = 0; currentRelay < config.numRelays; currentRelay++)
	{
		if (config.lockType[currentRelay] == LOCKTYPE_CONTINUOUS) // Continuous relay mode
		{
			if (activateRelay[currentRelay])
			{
				if (digitalRead(config.relayPin[currentRelay]) == !config.relayType[currentRelay]) // currently OFF, need to switch ON
				{
					mqttPublishIo("lock" + String(currentRelay), "UNLOCKED");
#ifdef DEBUG
					Serial.print(F("mili : "));
					Serial.println(millis());
					Serial.printf("activating relay %d now\n", currentRelay);
#endif
					digitalWrite(config.relayPin[currentRelay], config.relayType[currentRelay]);
				}
				else // currently ON, need to switch OFF
				{
					mqttPublishIo("lock" + String(currentRelay), "LOCKED");
#ifdef DEBUG
					Serial.print(F("mili : "));
					Serial.println(millis());
					Serial.printf("deactivating relay %d now\n", currentRelay);
#endif
					digitalWrite(config.relayPin[currentRelay], !config.relayType[currentRelay]);
				}
				activateRelay[currentRelay] = false;
			}
		}
		else if (config.lockType[currentRelay] == LOCKTYPE_MOMENTARY) // Momentary relay mode
		{
			if (activateRelay[currentRelay])
			{
				mqttPublishIo("lock" + String(currentRelay), "UNLOCKED");
#ifdef DEBUG
				Serial.print(F("mili : "));
				Serial.println(millis());
				Serial.printf("activating relay %d now\n", currentRelay);
#endif
				digitalWrite(config.relayPin[currentRelay], config.relayType[currentRelay]);
				previousMillis = millis();
				activateRelay[currentRelay] = false;
				deactivateRelay[currentRelay] = true;
#ifdef DEBUG
				Serial.printf("relay %d active\n", currentRelay);
#endif
			}
			else if (((currentMillis - previousMillis) >= config.activateTime[currentRelay]) && (deactivateRelay[currentRelay]))
			{
				mqttPublishIo("lock" + String(currentRelay), "LOCKED");
#ifdef DEBUG
				Serial.println(currentMillis);
				Serial.println(previousMillis);
				Serial.println(config.activateTime[currentRelay]);
				Serial.println(activateRelay[currentRelay]);
				Serial.println(F("deactivate relay after this"));
				Serial.print("mili : ");
				Serial.println(millis());
#endif
				digitalWrite(config.relayPin[currentRelay], !config.relayType[currentRelay]);
				deactivateRelay[currentRelay] = false;
			}
		}
	}
	if (formatreq)
	{
#ifdef DEBUG
		Serial.println(F("[ WARN ] Factory reset initiated..."));
#endif
		LittleFS.end();
		ws.enable(false);
		LittleFS.format();
		ESP.restart();
	}

	if (config.autoRestartIntervalSeconds > 0 && uptimeSeconds > config.autoRestartIntervalSeconds)
	{
		writeEvent("WARN", "sys", "Auto restarting...", "");
#ifdef DEBUG
		Serial.println(F("[ WARN ] Auto retarting..."));
#endif
		shouldReboot = true;
	}

	if (shouldReboot)
	{
		writeEvent("INFO", "sys", "System is going to reboot", "");
#ifdef DEBUG
		Serial.println(F("[ INFO ] System is going to reboot..."));
#endif
		LittleFS.end();
		ESP.restart();
	}

	if (WiFi.isConnected())
	{
		wiFiUptimeMillis += deltaTime;
	}

	if (config.wifiTimeout > 0 && wiFiUptimeMillis > (config.wifiTimeout * 1000) && WiFi.isConnected())
	{
		writeEvent("INFO", "wifi", "WiFi is going to be disabled", "");
#ifdef DEBUG
		Serial.println(F("[ INFO ] WiFi is going to be disabled..."));
#endif
		disableWifi();

	}

	// don't try connecting to WiFi when waiting for pincode
	if (doEnableWifi == true && keyTimer == 0 && activateRelay[0] == true)
	{
		if (!WiFi.isConnected())
		{
			enableWifi();
			writeEvent("INFO", "wifi", "Enabling WiFi", "");
			doEnableWifi = false;
#ifdef DEBUG
		Serial.println(F("[ INFO ] Enabling WiFi..."));
#endif
		}
	}

	if (config.mqttEnabled && mqttClient.connected())
	{
		if ((unsigned)epoch > nextbeat)
		{
			mqttPublishHeartbeat(epoch, uptimeSeconds);
			nextbeat = (unsigned)epoch + config.mqttInterval;
#ifdef DEBUG
			Serial.print("[ INFO ] Nextbeat=");
			Serial.println(nextbeat);
#endif
		}
		processMqttQueue();
	}
	if (config.mqttEnabled && !mqttClient.connected())
	{
		writeEvent("INFO", "mqtt", "MQTT connect", "");
		mqttClient.connect();
		if ((unsigned)epoch > nextbeat)
		{
			mqttPublishHeartbeat(epoch, uptimeSeconds);
			nextbeat = (unsigned)epoch + config.mqttInterval;
#ifdef DEBUG
			Serial.print("[ INFO ] Nextbeat=");
			Serial.println(nextbeat);
#endif
		}
		processMqttQueue();
	}

	processWsQueue();

	// clean unused websockets
	ws.cleanupClients();
}
