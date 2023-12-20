#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

boolean connected = false;

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
        static String serializeRequestData(const char* key1, const char* val1, const char* key2, const char* val2);
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

String JSON::serializer::serializeRequestData(const char* key1, const char* val1, const char* key2 = nullptr, const char* val2 = nullptr) {
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
    constexpr int relayPin = 26;
    constexpr int defaultLed = 2;
}



using namespace pins;
using namespace JSON;


void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS] Disconnected!\n");
            break;
        case WStype_CONNECTED:
            Serial.printf("[WS] Connected to url: %s\n", payload);
            connected = true;
            break;
        case WStype_TEXT: {
//            Serial.printf("[WS] Incoming data %s\n", payload);
            const char* Payload = (char *) payload;
            DynamicJsonDocument payloadData = JSON::deserializer::deserializeData(Payload);
            Serial.println(payloadData["type"].as<const char*>());
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



void setup() {
    Serial.begin(9600);

    pinMode(relayPin, OUTPUT);
    pinMode(defaultLed, OUTPUT);
    digitalWrite(defaultLed, HIGH);

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
    if(connected) {
        static unsigned long lastSend = 0;
        if(millis() - lastSend >= 500) {
            Serial.println("Sending");
            String payloadReady = serializer::serializeRequestData("type", "RECEIVE_DATA", "data", generateUID());
            webSocket.sendTXT(payloadReady);
            digitalWrite(defaultLed, HIGH);
            digitalWrite(defaultLed, LOW);
            lastSend = millis();
        }
    }
    webSocket.loop();
}