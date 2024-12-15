#include "SwarmNode.h"

SwarmNode swarmNode;

typedef struct {
    uint32_t chipID;
    float temperature;
    float humidity;
} SensorData;

String serializeSensorData(const SensorData& sensorData) {
    return String("{\"chipID\":") + String(sensorData.chipID) +
           ",\"temperature\":" + String(sensorData.temperature, 2) +
           ",\"humidity\":" + String(sensorData.humidity, 2) + "}";
}

void setup() {
    Serial.begin(115200);

    uint32_t chipID = (uint32_t)ESP.getEfuseMac();
    Serial.printf("Chip ID: %u\n", chipID);

    swarmNode.begin();
}

void loop() {
    SensorData sensorData = {
        (uint32_t)ESP.getEfuseMac(),
        random(200, 300) / 10.0, 
        random(400, 600) / 10.0  
    };

    String data = serializeSensorData(sensorData);

    swarmNode.send(data.c_str());

    Serial.println("Sending data: " + data);

    delay(5000);
}
