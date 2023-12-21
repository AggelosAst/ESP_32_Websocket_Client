#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <dht.h>

WiFiMulti WiFiMulti;
WebSocketsClient webSocket;


const char * generateUID(){
    const char possible[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static char uid[8 + 1];
    for(int p = 0, i = 0; i < 8; i++){
        int r = random(0, strlen(possible));
        uid[p++] = possible[r];
    }
    uid[8] = '\0';
    return uid;
}

namespace JSON {
    class serializer {
    public:
        static String serializeRequestData(const char* key1, const char* val1, const char* key2, int val2);
    };
    class deserializer {
    public:
        static DynamicJsonDocument deserializeData(const char* input);
    };
}

DynamicJsonDocument JSON::deserializer::deserializeData(const char* input) {
    DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, input);
        if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
        }
        return doc;
};

String JSON::serializer::serializeRequestData(const char* key1, const char* val1, const char* key2, int val2) {
    DynamicJsonDocument doc(256);
    String JSONData;
    if(key1 && val1) {
        doc[key1] = val1;
    }

    if(key2 && val2) {
        doc[key2] = val2;
    }
    serializeJson(doc, JSONData);
    return JSONData;
}

namespace pins {
    constexpr int sensorPin = 26;
    constexpr int defaultLed = 2;
}

namespace Messages {
    constexpr const char *LowTemp = "LowTemp";
    constexpr const char *HighTemp = "HighTemp";
    constexpr const char *LowHum = "LowHum";
    constexpr const char *HighHum = "HighHum";
    namespace Warnings {
        const char* humWarning{};
        const char* tempWarning{};
    }
}

namespace BackupVariables {
    class variables {
    public:
        int humidity = 0;
        int temperature = 0;
    };
}



using namespace pins;
using namespace JSON;
BackupVariables::variables backupVars;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Disconnected!\n");
            break;
        case WStype_CONNECTED:
            Serial.printf("[WS] Connected to url: %s\n", payload);
            break;
        case WStype_TEXT: {
            const char* Payload = (char *) payload;
            DynamicJsonDocument payloadData = JSON::deserializer::deserializeData(Payload);
            if (payloadData["type"]) {
                if (strcmp(payloadData["type"], "PONG") == 0) {
                    Serial.printf("[WS]: Ping/Pong Frame Event: %s\n", payloadData["type"].as<const char*>());
                }
            }
        }
            break;
        case WStype_BIN:
            break;
        case WStype_ERROR:
        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT_BIN_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            break;
    }

}


DHT dht(sensorPin, DHT11);

void setup() {
    Serial.begin(9600);
    pinMode(sensorPin, OUTPUT);
    pinMode(defaultLed, OUTPUT);
    digitalWrite(defaultLed, HIGH);
    dht.begin();
    WiFiMulti.addAP("ssid", "passcode");

    while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);
    }

    Serial.println("Attempting connect");

    webSocket.begin("host", 4040, "/"); //Backend at https://github.com/AggelosAst/ESP_32_Websocket

    webSocket.onEvent(webSocketEvent);

    webSocket.setReconnectInterval(5000);
}

void loop() {
    webSocket.loop();
    if(webSocket.isConnected()) {
        static unsigned long lastSend = 0;
        static unsigned long lastPing = 0;
        if(millis() - lastSend >= 1000) {
            Serial.println("Sending");
            int temperature = dht.readTemperature();
            Serial.println(temperature);
            if (temperature > 50) {
                if (backupVars.temperature > 0) {
                    temperature = backupVars.temperature;
                    Serial.println("Adjusted temp due to error.");
                } else {
                    Serial.println("Unset value");
                }
            }
            if (temperature > 30) {
                Messages::Warnings::tempWarning = Messages::HighTemp;
            } else if (temperature < 20) {
                Messages::Warnings::tempWarning = Messages::LowTemp;
            } else {
                Messages::Warnings::tempWarning = "Normal";
            }
            backupVars.temperature = temperature;
            String payloadReady = serializer::serializeRequestData("type", "SEND_DATA", "data", temperature);
            webSocket.sendTXT(payloadReady);
            digitalWrite(defaultLed, HIGH);
            yield();
            delay(50);
            digitalWrite(defaultLed, LOW);
            lastSend = millis();
        }
        if (millis() - lastPing >= 5000) {
            Serial.println("[WS]: Pinging...");
            if (webSocket.isConnected()) {
                String pingPacket = serializer::serializeRequestData("type", "PING", "data", 0);
                webSocket.sendTXT(pingPacket);
                lastPing = millis();
            } else {
                Serial.println("[WS]: Ping Frame not sent: Ws closed.");
            }
        }
    }
}