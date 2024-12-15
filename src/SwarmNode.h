#ifndef SWARMNODE_H
#define SWARMNODE_H

#include <WiFi.h>
#include <esp_now.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Arduino.h>

class SwarmNode {
public:
    enum MessageType {
        DATA,
        AP_INFO,
        REQUEST_TYPE
    };

    enum RequestType {
        V1,
        V2,
        V3,
        V4,
        V5
    };

    typedef struct {
        char ssid[32];
        char password[32];
    } APInfo;

    typedef union {
        char data[128];
        APInfo apInfo;
        RequestType requestType;
    } MessagePayload;

    typedef struct {
        MessageType type;
        MessagePayload payload;
    } Message;

    enum State {
        ROOT_NODE,
        FORWARD_NODE,
        SEARCH_AP_INFO,
        SEARCH_FORWARD_NODE,
        AP_HTTP_SERVER
    };

    SwarmNode();
    void begin();
    void send(const char *data);

private:
    State currentState;
    WebServer server;
    uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	uint search_ap_info_retry = 0;
	uint search_forward_node_retry = 0;
	char ssid[32];
	char password[32];

    static void onReceive(const uint8_t *macAddr, const uint8_t *incomingData, int len);
    static void onSent(const uint8_t *macAddr, esp_now_send_status_t status);
	void handleRoot();
	void handleRedirect(String message, String color);
	void setupHTTPServer();
	void handleSubmit();
    void sendToServer(const char *data);
    static SwarmNode& getInstance();
};

#endif // SWARMNODE_H
