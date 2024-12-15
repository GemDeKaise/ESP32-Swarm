#include "SwarmNode.h"

SwarmNode::SwarmNode() : currentState(SEARCH_AP_INFO), server(80) {}

void SwarmNode::begin() {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSent);

    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
}

void SwarmNode::handleRoot() {
  String html = "<!DOCTYPE html>"
                "<html lang='en'>"
                "<head>"
                "<meta charset='UTF-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>ESP32 Configuration</title>"
                "<style>"
                "body { font-family: Arial, sans-serif; background-color: #f3f4f6; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }"
                ".container { background: #ffffff; padding: 30px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); width: 100%; max-width: 400px; text-align: center; }"
                "h1 { font-size: 24px; margin-bottom: 20px; color: #333; text-align: center; }"
                "label { font-size: 14px; color: #555; display: block; margin-bottom: 8px; }"
                "input[type=text], input[type=password] { width: 100%; padding: 12px; margin-bottom: 20px; border: 1px solid #ddd; border-radius: 5px; font-size: 16px; }"
                "input[type=submit] { width: 100%; padding: 12px; background-color: #007BFF; color: #fff; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; }"
                "input[type=submit]:hover { background-color: #0056b3; }"
                "</style>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h1>Configure ESP32 WiFi</h1>"
                "<form action='/submit' method='POST'>"
                "<label for='ssid'>WiFi SSID</label>"
                "<input type='text' id='ssid' name='ssid' placeholder='Enter WiFi SSID'>"
                "<label for='password'>WiFi Password</label>"
                "<input type='password' id='password' name='password' placeholder='Enter WiFi Password'>"
                "<input type='submit' value='Save Configuration'>"
                "</form>"
                "</div>"
                "</body>"
                "</html>";
  server.send(200, "text/html", html);
}

void SwarmNode::handleRedirect(String message, String color) {
  String html = "<!DOCTYPE html>"
                "<html lang='en'>"
                "<head>"
                "<meta charset='UTF-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                "<title>ESP32 Configuration</title>"
                "<style>"
                "body { font-family: Arial, sans-serif; background-color: #f3f4f6; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; height: 100vh; }"
                ".container { background: #ffffff; padding: 30px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1); width: 100%; max-width: 400px; text-align: center; }"
                "h1 { font-size: 24px; margin-bottom: 20px; color: #333; text-align: center; }"
                "p { font-size: 18px; color: " + color + "; }"
                "</style>"
                "</head>"
                "<body>"
                "<div class='container'>"
                "<h1>WiFi Connection</h1>"
                "<p>" + message + "</p>"
                "<script>setTimeout(function(){ location.href='/'; }, 5000);</script>"
                "</div>"
                "</body>"
                "</html>";
  server.send(200, "text/html", html);
}

void SwarmNode::handleSubmit() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    strcpy(ssid, server.arg("ssid").c_str());
    strcpy(password, server.arg("password").c_str());

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    for (int i = 0; i < 20; i++) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
        handleRedirect("Connected to WiFi successfully!", "green");
        currentState = ROOT_NODE;
        return;
      }
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nFailed to connect to WiFi");
    handleRedirect("Failed to connect to WiFi. Please try again.", "red");
  } else {
    handleRedirect("Invalid Request. Please provide SSID and Password.", "red");
  }
}

void SwarmNode::setupHTTPServer() {
  server.on("/", [this]() { handleRoot(); });
  server.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
  server.begin();
  Serial.println("HTTP server started");
}

void SwarmNode::send(const char *data) {
	Message message;
	switch (currentState) {
		case SEARCH_AP_INFO: 
			Serial.println("Searching for AP info");
			search_ap_info_retry++;

			if (search_ap_info_retry > 2) {
				currentState = SEARCH_FORWARD_NODE;
				search_ap_info_retry = 0;

				break;
			}

			message.type = REQUEST_TYPE;
			message.payload.requestType = V1;
			esp_now_send(broadcastAddress, (uint8_t*)&message, sizeof(Message));
			break;
		case SEARCH_FORWARD_NODE:
			Serial.println("Searching for forward node");
			search_forward_node_retry++;

			if (search_forward_node_retry > 2) {
				currentState = AP_HTTP_SERVER;
				search_forward_node_retry = 0;

				Serial.println("State: AP_HTTP_SERVER");
				WiFi.mode(WIFI_AP);
				WiFi.softAP("ESP32_Config", "12345678");
				setupHTTPServer();

				Serial.println(WiFi.softAPIP());
				while (currentState == AP_HTTP_SERVER) {
					server.handleClient();
					delay(100);
				}

				break;
			}
			message.type = REQUEST_TYPE;
			message.payload.requestType = V2;
			esp_now_send(broadcastAddress, (uint8_t*)&message, sizeof(Message));
			break;
		case FORWARD_NODE:
			Serial.printf("Forwarding data: %s\n", data);
			
			message.type = DATA;
			strcpy(message.payload.data, data);
			esp_now_send(broadcastAddress, (uint8_t*)&message, sizeof(Message));
			break;
		case ROOT_NODE:
			Serial.printf("Sending data to server: %s\n", data);

			sendToServer(data);
			break;
		default:
			break;
	}
}

void SwarmNode::onReceive(const uint8_t *macAddr, const uint8_t *incomingData, int len) {
    Message message;
    memcpy(&message, incomingData, sizeof(Message));

    switch (message.type) {
        case DATA:
            Serial.printf("Received data: %s\n", message.payload.data);
            break;
        case AP_INFO:
            Serial.printf("Received AP info: %s, %s\n", message.payload.apInfo.ssid, message.payload.apInfo.password);
            break;
        case REQUEST_TYPE:
            Serial.printf("Received request type: %d\n", message.payload.requestType);
            break;
    }
}

void SwarmNode::onSent(const uint8_t *macAddr, esp_now_send_status_t status) {
    Serial.printf("Message sent: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void SwarmNode::sendToServer(const char *data) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin("http://192.168.0.132:8000/sensor-data");
      http.addHeader("Content-Type", "application/json");

      int httpResponseCode = http.POST(data);

    if (httpResponseCode == 200) {
      Serial.println("Data sent successfully");
    } else {
      Serial.printf("Failed to send data. HTTP response code: %d\n", httpResponseCode);
    }

        http.end();
    } else {
        Serial.println("Not connected to WiFi, cannot send data.");
    }
}

SwarmNode& SwarmNode::getInstance() {
    static SwarmNode instance;
    return instance;
}
