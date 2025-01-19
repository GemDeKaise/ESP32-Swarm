#include "SwarmNode.h"
#include <DHT.h>

#define DHTPIN 22 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

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
    dht.begin();
}

void loop() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    }

    SensorData sensorData = {
        (uint32_t)ESP.getEfuseMac(),
        temperature,
        humidity
    };

    String data = serializeSensorData(sensorData);

    swarmNode.send(data.c_str());

    Serial.println("Sending data: " + data);

    delay(5000);
}
