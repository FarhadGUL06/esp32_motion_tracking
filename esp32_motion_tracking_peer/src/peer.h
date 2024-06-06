#ifndef PEER_H
#define PEER_H

#include "BMI085.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SD.h>
#include <SPI.h>
#include <Wifi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <time.h>

// Header file with SSID and passsword for WiFi Connection
#include "secrets.h"

const bool DEBUG_MODE = false;

#define SERIAL_BEGIN Serial.begin
#define SERIAL_PRINTLN Serial.println
#define SERIAL_PRINT Serial.print

uint8_t CURRENT_ID = -1; // ID ESP32_ID - auto
const uint8_t number_sensors = 12;

const int size_of_entry = 80;
const int one_second = 1000000;
const int frames_per_second = 60;
// const int framePerSecondBias = 0;
// const int frame_delay = 1000 / frames_per_second;
const int seconds_to_SD = 35;

// https://lastminuteengineers.com/esp32-sleep-modes-power-consumption/

const char *header =
	"Timestamp;Index;AccelX;AccelY;AccelZ;gyroX;GyroY;GyroZ;Temperature";

char *payload_combined;
char *payload_entry;

long long current_time;

String file_name;

enum state {
	COLLECTING_STATE = 0,
	DOWNLOAD_STATE = 1,
};

RTC_NOINIT_ATTR state program_state = COLLECTING_STATE;

/*
	program_state == 0 => collecting mode
	program_state == 1 => download mode
*/
volatile int collecting_state = 0;
volatile int start_blocker = 0;
volatile int init_blocker = 0;
volatile int frame_on_timer = 0;
volatile int frame_delay_index = 0;

long index_of_frame = 0;

File file;

hw_timer_t *timer_frame = NULL;

// For ESP-NOW
typedef struct struct_message {
	uint8_t id;		 // id for each esp32
	uint8_t command; // command to be executed
	char text[112];	 // text to be send

} struct_message;

struct_message message_send;
struct_message message_recv;

esp_now_peer_info_t peer_info;

uint8_t server_mac[6] = {
	// 60:55:F9:7E:A7:B8 - server
	0x60, 0x55, 0xF9, 0x7E, 0xA7, 0xB8};

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

// Battery level
int analog_volts;
float voltage;

// Download-serve mode
int32_t wifi_channel;
const char *WIFI_SSID = DEBUG_MODE == true ? HIDDEN_SSID1 : HIDDEN_SSID2;
const char *SSID1 = DEBUG_MODE == true ? HIDDEN_SSID1 : HIDDEN_SSID2;
const char *PASS1 = DEBUG_MODE == true ? HIDDEN_PASS1 : HIDDEN_PASS2;
const int asyncport = 80;
const char *FILE_INPUT = "file";

// All function headers
void light_off();
void light_green_on();
void light_red_on();
void send_message(uint8_t command, String text);
int connect_to_wifi(const char *ssid, const char *password);
long long get_current_time();
void get_entry_info(long index);
void IRAM_ATTR on_timer();
void read_battery_level();
void get_multiple_frames();
void on_data_recv(const uint8_t *mac, const uint8_t *incomingData, int len);
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);
void print_time_nice();
void initialise_stuff();
void start_trial();
void enable_server_mode();
void enable_espnow();
void init_block();
void start_trial();

#endif
