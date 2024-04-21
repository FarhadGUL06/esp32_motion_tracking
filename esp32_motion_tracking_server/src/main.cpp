#include "server.h"

// For NEOPIXEL
Adafruit_NeoPixel pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// For Async Web Server
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

void send_message(int id, uint8_t command, String text)
{
	memset(&message, 0, sizeof(message));
	message.id = CURRENT_ID;
	message.command = command;
	strcpy(message.text, text.c_str());
	switch (id) {
	case -1:
		// Send to all peers
		if (esp_now_send(NULL, (uint8_t *)&message, sizeof(message)) !=
			ESP_OK) {
			SERIAL_PRINTLN("Error sending message");
		}
		break;
	default:
		// Send message to specific device (id)
		if (esp_now_send(mac[id], (uint8_t *)&message, sizeof(message)) !=
			ESP_OK) {
			SERIAL_PRINTLN("Error sending message");
		}
	}
}

int connect_to_wifi(const char *ssid, const char *password)
{
	SERIAL_PRINTLN("Connecting to WIFI");
	WiFi.begin(ssid, password);
	while (WiFi.status() != WL_CONNECTED) {
		delay(10);
	}
	if (WiFi.status() == WL_CONNECTED) {
		SERIAL_PRINTLN("Connected to WIFI");
		SERIAL_PRINTLN(WiFi.localIP());
		SERIAL_PRINTLN(WiFi.channel());
		wifi_channel = WiFi.channel();

		return 1;
	} else {
		return 0;
	}
}

int32_t get_WiFi_channel(const char *ssid)
{
	if (int32_t n = WiFi.scanNetworks()) {
		for (uint8_t i = 0; i < n; i++) {
			if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
				return WiFi.channel(i);
			}
		}
	}
	return 0;
}

// callback function that will be executed when data is received
void on_data_recv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
	memset(&message, 0, sizeof(message));
	memcpy(&message, incomingData, sizeof(message));
	SERIAL_PRINT("Bytes received: ");
	SERIAL_PRINTLN(len);
	SERIAL_PRINT("Received from: ");
	SERIAL_PRINTLN(mac[5], HEX);
	SERIAL_PRINT("ID: ");
	SERIAL_PRINTLN(message.id);
	SERIAL_PRINT("Command: ");
	SERIAL_PRINTLN(message.command);
	SERIAL_PRINT("Text: ");
	SERIAL_PRINTLN(message.text);

	switch (message.command) {
	case 3:
		// Print Battery level
		SERIAL_PRINTLN(message.text);
		memset(&battery_level[message.id], 0,
			   sizeof(battery_level[message.id]));
		strcpy(battery_level[message.id], message.text);
		break;

	case 4:
		SERIAL_PRINTLN(message.text);
		break;

	case 5:
		// Print Timestamp
		SERIAL_PRINTLN(message.text);
		memset(&timestamps[message.id], 0, sizeof(timestamps[message.id]));
		strcpy(timestamps[message.id], message.text);
		break;

	case 6:
		// Send if file exists
		SERIAL_PRINTLN(message.text);
		memset(&file_exists[message.id], 0, sizeof(file_exists[message.id]));
		strcpy(file_exists[message.id], message.text);
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

void initialise_stuff()
{
	SERIAL_BEGIN(9600);

	SERIAL_PRINTLN("Turning on!");
	pixel.begin();
	delay(6000);
	// When loading => red
	light_red_on();

	WiFi.mode(WIFI_AP_STA);

	int32_t channel = get_WiFi_channel(SSID1);
	WiFi.printDiag(Serial);
	SERIAL_PRINTLN("Channel: " + String(channel));
	esp_wifi_set_promiscuous(true);
	esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
	esp_wifi_set_promiscuous(false);
	WiFi.printDiag(Serial);

	// Connect to Wifi
	int status = 0;
	while (!status) {
		status = connect_to_wifi(SSID1, PASS1);
	}
	// Set device as a Wi-Fi Station
	SERIAL_PRINTLN(WiFi.macAddress());

	// config time
	configTime(gmt_offset_sec, daylight_offset_sec, ntp_server1, ntp_server2);

	// Init ESP-NOW
	if (esp_now_init() != ESP_OK) {
		SERIAL_PRINTLN("Error initializing ESP-NOW");
		return;
	}

	esp_now_register_send_cb(on_data_sent);
	esp_now_register_recv_cb(on_data_recv);

	// Register all peers
	for (int i = start_point; i < end_point; i++) {
		memcpy(peer_info.peer_addr, mac[i], 6);
		peer_info.channel = 0;
		peer_info.encrypt = false;

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

	light_green_on();

	delay(100);
	// Turn the light off
	light_off();
	SERIAL_PRINTLN("Initialised!");
}

long long get_current_time()
{
	// Calculate current time
	struct tm timeinfo;
	long long timestamp = 0;
	if (!getLocalTime(&timeinfo)) {
		SERIAL_PRINTLN("Failed to obtain time");
		return timestamp;
	}

	// Transform in seconds
	timestamp = mktime(&timeinfo);
	SERIAL_PRINTLN("Timestamp = " + String(timestamp));
	return timestamp;
}

void setup()
{
	initialise_stuff();

	// Check if server is alive
	// <ESP_IP>/isalive
	server.on("/isalive", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", "Yes");
	});

	// <ESP_IP>/init?file=<file_name>
	server.on("/init", HTTP_GET, [](AsyncWebServerRequest *request) {
		SERIAL_PRINTLN("Program initialised!");
		if (init_state == 1) {
			request->send(200, "text/plain", "Program already initialised!");
			return;
		}
		if (request->hasParam(FILE_INPUT)) {
			init_state = 1;
			file_name = request->getParam(FILE_INPUT)->value();
			send_message(-1, 0, file_name);
			request->send(200, "text/plain", "Program initialised!");
		}
		request->send(200, "text/plain", "No file name provided!");
	});

	// <ESP_IP>/start
	server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (init_state == 0) {
			request->send(200, "text/plain", "Program not initialised!");
			return;
		}
		if (start_state == 1) {
			request->send(200, "text/plain", "Program already started!");
			return;
		}
		start_state = 1;
		send_message(-1, 1, "");
		request->send(200, "text/plain", "Program started!");
	});

	// <ESP_IP>/stop
	server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (init_state == 1 && start_state == 0) {
			request->send(200, "text/plain",
						  "Program initialised but not started!");
			return;
		}

		if (start_state == 0) {
			request->send(200, "text/plain", "Program not started!");
			return;
		}

		start_state = 0;
		init_state = 0;

		send_message(-1, 2, "");
		request->send(200, "text/plain", "Program stopped!");
	});

	// <ESP_IP>/all_bat
	server.on("/all_bat", HTTP_GET, [](AsyncWebServerRequest *request) {
		battery_level_str = "";
		for (int i = start_point; i < end_point; i++) {
			send_message(i, 3, "");
			battery_level_str +=
				"ESP32_" + String(i) + ": " + battery_level[i] + "\n";
		}
		SERIAL_PRINTLN(battery_level_str);
		request->send(200, "text/plain", battery_level_str);
	});

	// <ESP_IP>/sync
	server.on("/sync", HTTP_GET, [](AsyncWebServerRequest *request) {
		for (int i = start_point; i < end_point; i++) {
			timestamp = get_current_time();
			send_message(i, 4, String(timestamp));
		}
		request->send(200, "text/plain", "Syncing!");
	});

	// <ESP_IP>/all_timestamp
	server.on("/all_timestamp", HTTP_GET, [](AsyncWebServerRequest *request) {
		timestamp_str = "";
		for (int i = start_point; i < end_point; i++) {
			send_message(i, 5, "");
			timestamp_str += "ESP32_" + String(i) + ": " + timestamps[i] + "\n";
		}
		SERIAL_PRINTLN(timestamp_str);
		request->send(200, "text/plain", timestamp_str);
	});

	// <ESP_IP>/file_exists?file=<file_name>
	server.on("/file_exists", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (request->hasParam(FILE_INPUT)) {
			file_exists_str = "";
			for (int i = start_point; i < end_point; i++) {
				send_message(i, 6, request->getParam(FILE_INPUT)->value());
				file_exists_str +=
					"ESP32_" + String(i) + ": " + file_exists[i] + "\n";
			}
			SERIAL_PRINTLN(file_exists_str);
			request->send(200, "text/plain", file_exists_str);
		}
		request->send(200, "text/plain", "No sensor id or file name provided!");
	});

	// <ESP_IP>/enable_download
	server.on("/enable_download", HTTP_GET, [](AsyncWebServerRequest *request) {
		send_message(-1, 7, "");
		request->send(200, "text/plain", "Download enabled!");
	});

	server.begin();
}

void loop()
{
}
