#include "peer.h"

// For NEOPIXEL
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// FOR BMI085
BMI085Gyro gyro(Wire, 0x68);
BMI085Accel accel(Wire, 0x18);
AsyncWebServer server(asyncport);

void light_off()
{
	pixel.setBrightness(0);
	pixel.fill(0x000000);
	pixel.show();
}

void light_green_on()
{
	pixel.setBrightness(5);
	pixel.fill(0x4CBB17);
	pixel.show();
}

void light_red_on()
{
	pixel.setBrightness(5);
	pixel.fill(0xFF0000);
	pixel.show();
}

void enable_server_mode()
{
	WiFi.mode(WIFI_STA);
	int status = 0;
	while (!status) {
		status = connect_to_wifi(SSID1, PASS1);
	}

	delay(100);

	// <ESP_IP>/download?file=<file_name>
	server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request) {
		SERIAL_PRINTLN("Download started!");
		if (request->hasParam(FILE_INPUT)) {
			file_name = request->getParam(FILE_INPUT)->value();
			file_name = "/" + file_name;
		}
		File to_send = SD.open(file_name.c_str(), FILE_READ);
		if (!to_send) {
			SERIAL_PRINTLN("File doesn't exist!");
			request->send(200, "text/plain", "File doesn't exist!");
			return;
		}

		SERIAL_PRINTLN("File exists!");
		AsyncWebServerResponse *response = request->beginChunkedResponse(
			"text/plain",
			[to_send](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
				maxLen = maxLen >> 1;
				File SDLambdaFile = to_send;
				return SDLambdaFile.read(buffer, maxLen);
			});
		request->send(response);
	});

	// <ESP_IP>/disable_download
	server.on(
		"/disable_download", HTTP_GET, [](AsyncWebServerRequest *request) {
			SERIAL_PRINTLN("Download disabled!");
			request->send(200, "text/plain", "Download mode disabled!");
			program_state = COLLECTING_STATE;
			SERIAL_PRINTLN("Program state is switching to COLLECTING_STATE");
			delay(1000);
			ESP.restart();
		});
	SERIAL_PRINTLN("[ESP32] Free memory: " + String(esp_get_free_heap_size()) +
				   " bytes");
	server.begin();
}

void enable_espnow()
{
	// Init ESP-NOW
	if (esp_now_init() != ESP_OK) {
		SERIAL_PRINTLN("Error initializing ESP-NOW");
		return;
	}
	esp_now_register_recv_cb(on_data_recv);
	esp_now_register_send_cb(on_data_sent);

	// Register server as peer
	memcpy(peer_info.peer_addr, server_mac, 6);

	// if (CURRENT_ID == 3 || CURRENT_ID == 10 || CURRENT_ID == 11) {
	// 	// If it's the first ESP32, then it's the server
	// 	peer_info.channel = 2;
	// } else {
	// If it's not the first ESP32, then it's a client
	peer_info.channel = 0;
	// }

	// Add peer
	if (esp_now_add_peer(&peer_info) != ESP_OK) {
		SERIAL_PRINTLN("Failed to add peer");
		return;
	}
	// Added Peer with MAC
	char macStr[18];
	snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
			 peer_info.peer_addr[0], peer_info.peer_addr[1],
			 peer_info.peer_addr[2], peer_info.peer_addr[3],
			 peer_info.peer_addr[4], peer_info.peer_addr[5]);
	SERIAL_PRINTLN("Added peer with MAC: " + String(macStr));
}

int connect_to_wifi(const char *ssid, const char *password)
{
	SERIAL_PRINTLN("Connecting to WIFI");
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(1000);
		SERIAL_PRINTLN(WiFi.status());
	}
	if (WiFi.status() == WL_CONNECTED) {
		SERIAL_PRINTLN("Connected to WIFI");
		SERIAL_PRINTLN(WiFi.localIP());
		return 1;
	} else {
		return 0;
	}
}

void send_message(uint8_t command, String text)
{
	memset(&message_send, 0, sizeof(message_send));
	message_send.id = CURRENT_ID;
	message_send.command = command;
	strcpy(message_send.text, text.c_str());
	if (esp_now_send(NULL, (uint8_t *)&message_send, sizeof(message_send)) !=
		ESP_OK) {
		SERIAL_PRINTLN("Error sending message");
	}
}

long long get_current_time()
{
	// Calculate current time
	struct tm timeinfo;
	long long timestamp = 0;

	struct timeval tv_now;
	gettimeofday(&tv_now, NULL);

	// Get the seconds
	timestamp = tv_now.tv_sec;
	return timestamp;
}

void get_entry_info(long index)
{
	// Get BMI085 data and timestamp
	accel.readSensor();
	gyro.readSensor();

	float accelX = accel.getAccelX_mss();
	float accelY = accel.getAccelY_mss();
	float accelZ = accel.getAccelZ_mss();

	float gyroX = gyro.getGyroX_rads();
	float gyroY = gyro.getGyroY_rads();
	float gyroZ = gyro.getGyroZ_rads();
	float temp = accel.getTemperature_C();

	current_time = get_current_time();
	memset(payload_entry, 0, size_of_entry);
	sprintf(payload_entry, "%lli;%li;%f;%f;%f;%f;%f;%f;%d\n", current_time,
			index, accelX, accelY, accelZ, gyroX, gyroY, gyroZ, (int)temp);
}

void IRAM_ATTR on_timer()
{
	if (frame_on_timer == 1 && frame_delay_index == 0) {
		frame_delay_index = 1;
	}
}

void read_battery_level()
{
	analog_volts = analogReadMilliVolts(3);
	voltage = analogReadMilliVolts(3) / 500.0;
	SERIAL_PRINT("Battery level: ");
	SERIAL_PRINTLN(voltage);
}

int cv = 0, cv2 = 0;

void get_multiple_frames()
{
	SERIAL_PRINTLN("Getting multiple frames");
	frame_on_timer = 1;
	int i;
	// while (collecting_state == 1) {
	for (i = 0; i < seconds_to_SD * frames_per_second; i++) {
		frame_delay_index = 0;
		timerWrite(timer_frame, 0);

		get_entry_info(index_of_frame);
		index_of_frame++;
		strcat(payload_combined, payload_entry);

		if (collecting_state == 0) {
			// Exit if received stop command
			// cv2 = micros();
			break;
		}
		if (i == seconds_to_SD * frames_per_second - 1) {
			// Skip waiting if it's the last iteration
			break;
		}
		while (frame_delay_index == 0) {
			// Wait for timer to trigger
		}
		//}

		/* Print to SD Card in chunks of 512 bytes
		char to_print[512];
		memset(to_print, 0, 512);
		for (i = 0; i < strlen(payload_combined); i += 511) {
			strncpy(to_print, payload_combined + i, 511);
			file.print(to_print);
		}*/
	}
	file.print(payload_combined);
	memset(payload_combined, 0,
		   size_of_entry * seconds_to_SD * frames_per_second);
	// Close file
	file.close();

	// Reset timer
	timerAlarmDisable(timer_frame);
	frame_on_timer = 0;

	SERIAL_PRINTLN("Program stopped!");
	SERIAL_PRINTLN("[ESP32] Free memory: " + String(esp_get_free_heap_size()) +
				   " bytes");
	// send_message(7, "");
	// Serial.print("Time taken: ");
	// Serial.println(cv2 - cv);
}

// callback function that will be executed when data is received
void on_data_recv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
	memcpy(&message_recv, incomingData, sizeof(message_recv));
	SERIAL_PRINT("Bytes received: ");
	SERIAL_PRINTLN(len);
	SERIAL_PRINT("Received from: ");
	SERIAL_PRINTLN(mac[5], HEX);
	SERIAL_PRINT("ID: ");
	SERIAL_PRINTLN(message_recv.id);
	SERIAL_PRINT("Command: ");
	SERIAL_PRINTLN(message_recv.command);
	SERIAL_PRINT("Text: ");
	SERIAL_PRINTLN(message_recv.text);
	struct timeval custom_time;
	switch (message_recv.command) {
	case 0:
		// Init data collect
		init_blocker = 1;
		break;
	case 1:
		// Start data collect
		start_blocker = 1;
		break;
	case 2:
		// Stop
		// cv = micros();
		collecting_state = 0;
		break;
	case 3:
		// Get battery level
		read_battery_level();
		// Send a message to server with battery level
		send_message(3, String(voltage));
		break;
	case 4:
		// Get current time
		current_time = atoll(message_recv.text);
		// Update timeinfo
		custom_time.tv_sec = current_time;
		custom_time.tv_usec = 0;
		settimeofday(&custom_time, NULL);

		// Print nice current_time in DD/MM/YYYY HH:MM
		current_time = get_current_time();
		print_time_nice();
		send_message(4, "Time set");
		break;

	case 5:
		// Get current time
		current_time = get_current_time();
		// Send a message to server with current time
		send_message(5, String(current_time));
		break;

	case 6:
		// Check if filename from command exists on SD Card
		file_name = message_recv.text;
		file_name = "/" + file_name;
		if (SD.exists(file_name)) {
			send_message(6, String(file_name) + " exists");
		} else {
			send_message(6, String(file_name) + " does not exist");
		}
		break;

	case 7:
		// Open server mode for downloading
		SERIAL_PRINTLN("[ESP32] Free memory: " +
					   String(esp_get_free_heap_size()) + " bytes");
		program_state = DOWNLOAD_STATE;
		SERIAL_PRINTLN("Program state is switching to DOWNLOAD_STATE");

		ESP.restart();
		break;
	default:
		break;
	}
}

// callback when data is sent
void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
	SERIAL_PRINT("\r\nLast Packet Send Status:\t");
	SERIAL_PRINTLN(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success"
												  : "Delivery Fail");
}

void print_time_nice()
{
	// Print nice current_time in DD/MM/YYYY HH:MM
	// current_time = mktime() so reverse it
	struct tm timeinfo;
	time_t timestamp = current_time;
	localtime_r(&timestamp, &timeinfo);
	SERIAL_PRINT(timeinfo.tm_mday);
	SERIAL_PRINT("/");
	SERIAL_PRINT(timeinfo.tm_mon + 1);
	SERIAL_PRINT("/");
	SERIAL_PRINT(timeinfo.tm_year + 1900);
	SERIAL_PRINT(" ");
	SERIAL_PRINT(timeinfo.tm_hour);
	SERIAL_PRINT(":");
	SERIAL_PRINT(timeinfo.tm_min);
	SERIAL_PRINT(":");
	SERIAL_PRINTLN(timeinfo.tm_sec);
}

void decide_current_id()
{
	int i, j, ok;
	uint8_t current_mac[6];
	esp_read_mac(current_mac, ESP_MAC_WIFI_STA);
	// Verify which ESP32 is this (CURRENT_ID)
	for (i = 0; i < number_sensors; i++) {
		ok = 1;

		for (j = 0; j < 6; j++) {
			if (current_mac[j] != all_macs[i][j]) {
				ok = 0;
				break;
			}
		}

		if (ok == 1) {
			CURRENT_ID = i;
			break;
		}
	}
	SERIAL_PRINT("Current ID: ");
	SERIAL_PRINTLN(CURRENT_ID);
}

void initialise_bmi()
{
	while (accel.begin() != 1 || gyro.begin() != 1) {
		SERIAL_PRINTLN("Initialising...");
		delay(100);
	}
}

void initialise_sd()
{
	int statussd = SD.begin(4);
	while (!statussd) {
		SERIAL_PRINTLN("Initialising SD Card: status = ");
		SERIAL_PRINTLN(statussd);
		delay(10);
		statussd = SD.begin(4);
	}
}

void initialise_timer()
{
	// Init timer
	timer_frame = timerBegin(0, 80, true);
	timerAttachInterrupt(timer_frame, &on_timer, true);
	timerAlarmWrite(timer_frame, one_second / frames_per_second, true);
}

void initialise_memory()
{
	// Initialise memory
	payload_combined = (char *)malloc(size_of_entry * (seconds_to_SD + 1) *
									  frames_per_second * sizeof(char));

	payload_entry = (char *)malloc(size_of_entry * sizeof(char));
}

void initialise_espnow()
{
	WiFi.mode(WIFI_STA);

	// Print MAC
	SERIAL_PRINTLN(WiFi.macAddress());

	// Based on WIFI.macAddress decide CURRENT_ID
	decide_current_id();

	// Init ESP-NOW
	enable_espnow();
}

void initialise_collect()
{
	SERIAL_PRINTLN("Initialising collect");
	initialise_bmi();

	initialise_timer();

	initialise_memory();

	initialise_espnow();
}

void initialise_stuff()
{
	// Set init value of program_state
	esp_reset_reason_t reason = esp_reset_reason();
	if (reason != ESP_RST_SW) {
		// If not software reset, then start collecting
		program_state = COLLECTING_STATE;
	}

	// Initialise common stuff - SD Card
	initialise_sd();
	SERIAL_PRINTLN("Current program state is " + String(program_state) + " " +
				   ((program_state == DOWNLOAD_STATE)
						? String("DOWNLOAD_STATE")
						: String("COLLECTING_STATE")));

	switch (program_state) {
	case COLLECTING_STATE:
		initialise_collect();
		break;
	case DOWNLOAD_STATE:
		enable_server_mode();
		break;
	default:
		break;
	}
}

void start_trial()
{
	SERIAL_PRINTLN("Starting trial on file " + file_name);
	memset(payload_combined, 0,
		   size_of_entry * seconds_to_SD * frames_per_second);
	index_of_frame = 0;
	collecting_state = 1;
	get_multiple_frames();
}

void setup()

{
	SERIAL_BEGIN(9600);
	SERIAL_PRINTLN("Turning on!");
	pixel.begin();

	// When loading => red
	light_red_on();

	initialise_stuff();

	// When ready => green
	light_green_on();

	// Turn the light off
	light_off();
	SERIAL_PRINTLN("Initialised!");
	SERIAL_PRINTLN("[ESP32] Free memory: " + String(esp_get_free_heap_size()) +
				   " bytes");
}

void init_block()
{
	// Init - open file
	SERIAL_PRINTLN("Init block");
	file_name = message_recv.text;
	file_name = "/" + file_name;

	int time_to_open_start = millis();
	file = SD.open(file_name.c_str(), FILE_WRITE);
	int time_to_open_end = millis();

	SERIAL_PRINT("Time to open file: ");
	SERIAL_PRINTLN(time_to_open_end - time_to_open_start);

	int time_to_write_start = millis();
	if (!file) {
		// If the file didn't open, print an error:
		SERIAL_PRINTLN("Error opening file!");
	} else {
		// Print header
		file.println(header);
	}
	int time_to_write_end = millis();
	SERIAL_PRINT("Time to write to file: ");
	SERIAL_PRINTLN(time_to_write_end - time_to_write_start);
	// Enable timer
	timerAlarmEnable(timer_frame);
}

void loop()
{
	if (init_blocker == 1) {
		init_blocker = 0;
		init_block();
	}
	if (start_blocker == 1) {
		start_blocker = 0;
		start_trial();
	}
}
