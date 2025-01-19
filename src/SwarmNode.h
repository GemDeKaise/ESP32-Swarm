#ifndef SWARMNODE_H
#define SWARMNODE_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_now.h>
#include <WebServer.h>
#include <Arduino.h>

const char *serverCert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDmzCCAoOgAwIBAgIURE6LJ06Rb+shJuS5p3Vvo6/s7+4wDQYJKoZIhvcNAQEL
BQAwXTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM
GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDEWMBQGA1UEAwwNMzQuOTEuMTMxLjEz
NjAeFw0yNTAxMTYxNDM4NDFaFw0yNjAxMTYxNDM4NDFaMF0xCzAJBgNVBAYTAkFV
MRMwEQYDVQQIDApTb21lLVN0YXRlMSEwHwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRz
IFB0eSBMdGQxFjAUBgNVBAMMDTM0LjkxLjEzMS4xMzYwggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQDNYYat8koeyXCo+KCBGMq0yE+7LIuDD6bbtnM3+RSL
aKCzq8YMYQ5RyMiuyRdbKsmS4xJ0S9nX2YaGStmJdJcqYjd4Zwtt2/oQwzyVXFbN
UPqWcFqk3bg7ELezsUJNfCxP6xf6IQ4pyGyGCV0eoy4x5FBYoAJdZxHATdfTW1nN
D6uNWmR39VO0Se/XC7B21+CsDPi+sl7eFx9HW75Wq0hQCJFYoQ3yl8zsq8yjxmru
0sKHyJbOfbYk9tjnCi8JlPiy63DDCB+IDs0fiKiRvwbio0/q1ZqoZazKHCsqrAsp
b7BS1zc16KOaqKdmaD0xsql4XyZptxQhikHpRV2jTVLxAgMBAAGjUzBRMB0GA1Ud
DgQWBBRWMGUJaGfxajFcY8m3nrCK3GfL4jAfBgNVHSMEGDAWgBRWMGUJaGfxajFc
Y8m3nrCK3GfL4jAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQAz
Gq8CIRx9FMl2c5nLLwQulTaFwr4bcg4rmyYy31v65JkGHmbBeVYp+D8EyxVhdVLL
o3kttW4bihtHSCbVy3xpii/+TTtza0yssCpDsjzVgMVb7onEjKTzZxT/AbsWmNE4
0BelUeQ8EVdWZm3pJ6hrXjLbVIQFYHY5HhdzTDg/VcYJxP1c7nn50PDW4kLMaoBb
suNVOhpkt9+lG/7GLavV6VH/e70Z2Pxf4sjcYRWhoz+7nGiaO8qiuL1bGwwfGknV
lbHEIPykCXCFfQ+vQJkQS12/GnSxLjLNIjCFs67OMKrg2TH0Jh3oLO4pU4tIs3Ve
KNH5FMDp3LNRWu7KlfsM
-----END CERTIFICATE-----
)EOF";

int channel;
char ssid[32];
char password[32];

bool forward_is_available = false;
char data_to_send[128];

int32_t getWiFiChannel(const char *ssid) {
    if (int32_t n = WiFi.scanNetworks()) {
        for (uint8_t i=0; i<n; i++) {
            if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
                return WiFi.channel(i);
            }
        }
    }
    return 0;
}

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

    String handle_root_html = "<!DOCTYPE html>"
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

    String handle_redirect_html(const String& message, const String& color) {
        return String(
            "<!DOCTYPE html>"
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
            "</html>"
        );
    }   

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
