#include "SwarmNode.h"
#include <esp_wifi.h>
#include <DHT.h>

SwarmNode::SwarmNode() : currentState(SEARCH_AP_INFO), server(80) {}

void SwarmNode::begin() {
    WiFi.mode(WIFI_MODE_APSTA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSent);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = channel;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    strcpy(ssid, "none");
}

void SwarmNode::handleRoot() {
  server.send(200, "text/html", handle_root_html);
}

void SwarmNode::handleRedirect(String message, String color) {
  server.send(200, "text/html", handle_redirect_html(message, color));
}

void SwarmNode::handleSubmit() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    char aux_ssid[32];
    char aux_password[32];
    strcpy(aux_ssid, server.arg("ssid").c_str());
    strcpy(aux_password, server.arg("password").c_str());

    WiFi.begin(aux_ssid, aux_password);
    Serial.print("Connecting to WiFi");
    for (int i = 0; i < 20; i++) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi");
        handleRedirect("Connected to WiFi successfully!", "green");
        currentState = ROOT_NODE;

        wifi_second_chan_t second;
        uint8_t channel;
        esp_wifi_get_channel(&channel, &second);
        Serial.printf("Channel: %d\n", channel);

        strcpy(ssid, aux_ssid);
        strcpy(password, aux_password);
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
    if (strcmp(ssid, "none") != 0 && WiFi.status() != WL_CONNECTED) {
        // print the ssid and the password
        Serial.printf("SSID: %s, Password: %s\n", ssid, password);
        WiFi.begin(ssid, password);
        Serial.print("Connecting to WiFi");

        for (int i = 0; i < 20; i++) {
            Serial.print(".");
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("Connected to WiFi");
                currentState = ROOT_NODE;
                break;
            }
            delay(500);
        }
    }
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

            if (forward_is_available) {
                currentState = FORWARD_NODE;
                break;
            }

			if (search_forward_node_retry > 1) {
				currentState = AP_HTTP_SERVER;
				search_forward_node_retry = 0;

				Serial.println("State: AP_HTTP_SERVER");
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
            if (data_to_send != "") {
                sendToServer(data_to_send);
                strcpy(data_to_send, "");
            }
			break;
		default:
			break;
	}
}


void SwarmNode::onReceive(const uint8_t *macAddr, const uint8_t *incomingData, int len) {
    Message message;
    memcpy(&message, incomingData, sizeof(Message));

    Serial.printf("Received message from: %02X:%02X:%02X:%02X:%02X:%02X\n", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    SwarmNode& instance = SwarmNode::getInstance();
    switch (message.type) {
        case DATA:
            Serial.printf("Received data: %s\n", message.payload.data);
            strcpy(data_to_send, message.payload.data);
            break;
        case AP_INFO:
            Serial.printf("Received AP info: %s, %s\n", message.payload.apInfo.ssid, message.payload.apInfo.password);

            strcpy(ssid, message.payload.apInfo.ssid);
            strcpy(password, message.payload.apInfo.password);
            break;
        case REQUEST_TYPE:
            Serial.printf("Received request type: %d\n", message.payload.requestType);

            if (message.payload.requestType == V1) {
                if (ssid != "none") {
                    Serial.println("Sending AP info");
                    Message response;
                    response.type = AP_INFO;
                    strcpy(response.payload.apInfo.ssid, ssid);
                    strcpy(response.payload.apInfo.password, password);
                    esp_now_send(instance.broadcastAddress, (uint8_t*)&response, sizeof(Message));

                    Serial.printf("Sent AP info: %s, %s\n", response.payload.apInfo.ssid, response.payload.apInfo.password);
                } else {
                    Serial.println("No AP info to send");
                }
            }

            if (message.payload.requestType == V2) {
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.println("Sending forward node");
                    Message response;
                    response.type = REQUEST_TYPE;
                    response.payload.requestType = V3;
                    esp_now_send(instance.broadcastAddress, (uint8_t*)&response, sizeof(Message));

                    Serial.println("Sent forward node");
                } else {
                    Serial.println("Not connected to WiFi, cannot send forward node");
                }
            }

            if (message.payload.requestType == V3) {
                forward_is_available = true;
            }
            
            break;
    }
}

void SwarmNode::onSent(const uint8_t *macAddr, esp_now_send_status_t status) {
    Serial.printf("Message sent: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void SwarmNode::sendToServer(const char *data) {
    if (WiFi.status() == WL_CONNECTED) {

        Serial.print("Sending data to server: ");

        WiFiClientSecure client;
        client.setCACert(serverCert); // Setăm certificatul serverului
        client.setInsecure(); // Dezactivează verificarea certificatului

        Serial.print("Connecting to server... ");
        
        HTTPClient https;
        https.begin(client, "https://34.91.131.136:8000/sensor-data");
        https.addHeader("Content-Type", "application/json");

        int httpResponseCode = https.POST(data);

        Serial.print("Server response: ");

        if (httpResponseCode == 200) {
        Serial.println("Data sent successfully");
        } else {
        Serial.printf("Failed to send data. HTTP response code: %d\n", httpResponseCode);
        }

        Serial.println("Disconnecting from server");

        https.end();
    } else {
        Serial.println("Not connected to WiFi, cannot send data.");
    }
}

SwarmNode& SwarmNode::getInstance() {
    static SwarmNode instance;
    return instance;
}
