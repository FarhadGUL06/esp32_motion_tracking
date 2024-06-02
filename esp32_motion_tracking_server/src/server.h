#ifndef SERVER_H
#define SERVER_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Wifi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Header file with SSID and passsword for WiFi Connection
#include "secrets.h"

#define SERIAL_BEGIN Serial.begin
#define SERIAL_PRINTLN Serial.println
#define SERIAL_PRINT Serial.print

/*
 * If DEBUG_MODE == true then the program will run in debug mode
 * and the SSID and PASSWORD will be the ones for the test network
 * else the program will run in production mode
 */

const bool DEBUG_MODE = true;

int32_t wifi_channel;
const char *WIFI_SSID = DEBUG_MODE == true ? HIDDEN_SSID1 : HIDDEN_SSID2;
const char *SSID1 = DEBUG_MODE == true ? HIDDEN_SSID1 : HIDDEN_SSID2;
const char *PASS1 = DEBUG_MODE == true ? HIDDEN_PASS1 : HIDDEN_PASS2;
// time stuff
const char *ntp_server1 = "ro.pool.ntp.org";
const char *ntp_server2 = "time.nist.gov";
const long gmt_offset_sec = 7200;
const int daylight_offset_sec = 3600; // Bucharest is UTC+2
const char *time_zone = "EET-2EEST,M3.5.0/3,M10.5.0/4";

const uint8_t CURRENT_ID = 0; // ID Server
const uint8_t number_sensors = 12;

int start_point = 1;//DEBUG_MODE == true ? 0 : 1;
int end_point = 12;//DEBUG_MODE == true ? 2 : number_sensors;

const int asyncport = 80;
const char *PARAM_INPUT_VBAT = "sensor";
const char *FILE_INPUT = "file";

volatile int init_state = 0;
volatile int start_state = 0;

String file_name;
int sensorid;

// Array with all MAC for 11 ESP32 devices
uint8_t all_macs[][6] = {
	// 60:55:F9:7C:AE:DC - ESP32_TEST
	{0x60, 0x55, 0xF9, 0x7C, 0xAE, 0xDC},
	// 60:55:F9:7C:AE:D8 - ESP32_1
	{0x60, 0x55, 0xF9, 0x7C, 0xAE, 0xD8},
	// 60:55:F9:7E:A7:DC - ESP32_2
	{0x60, 0x55, 0xF9, 0x7E, 0xA7, 0xDC},
	// 60:55:F9:7E:9D:84 - ESP32_3
	{0x60, 0x55, 0xF9, 0x7E, 0x9D, 0x84},
	// 60:55:F9:7E:A7:E4 - ESP32_4
	{0x60, 0x55, 0xF9, 0x7E, 0xA7, 0xE4},
	// 60:55:F9:7E:A8:28 - ESP32_5
	{0x60, 0x55, 0xF9, 0x7E, 0xA8, 0x28},
	// 60:55:F9:7C:AE:6C - ESP32_6
	{0x60, 0x55, 0xF9, 0x7C, 0xAE, 0x6C},
	// 60:55:F9:7C:AE:F0 - ESP32_7
	{0x60, 0x55, 0xF9, 0x7C, 0xAE, 0xF0},
	// 60:55:F9:7C:AE:50 - ESP32_8
	{0x60, 0x55, 0xF9, 0x7C, 0xAE, 0x50},
	// 60:55:F9:7E:A8:58 - ESP32_9
	{0x60, 0x55, 0xF9, 0x7E, 0xA8, 0x58},
	// 60:55:F9:7E:A8:70 - ESP32_10
	{0x60, 0x55, 0xF9, 0x7E, 0xA8, 0x70},
	// 60:55:F9:7C:AE:D4 - ESP32_11
	{0x60, 0x55, 0xF9, 0x7C, 0xAE, 0xD4}};

typedef struct struct_message {
	uint8_t id;		 // id for each esp32
	uint8_t command; // command to be executed
	char text[112];	 // text to be send

} struct_message;


char battery_level[number_sensors + 1][10];
String battery_level_str;

char timestamps[number_sensors + 1][128];
String timestamp_str;
long long timestamp;

char file_exists[number_sensors + 1][128];
String file_exists_str;

int sensors_ready[number_sensors + 1] = {0};

struct_message message;
esp_now_peer_info_t peer_info;
esp_err_t result;

// All function headers
void light_off();
void light_green_on();
void light_red_on();
void send_message(int id, uint8_t command, String text);
int connect_to_wifi(const char *ssid, const char *password);
int32_t get_WiFi_channel(const char *ssid);
void on_data_recv(const uint8_t *mac, const uint8_t *incomingData, int len);
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);
void initialise_stuff();
int decide_send_id(const uint8_t *current_mac);

#endif
