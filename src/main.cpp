#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "utils.h"
#include "net.h"
#include "secrets.h"

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>

const char *WiFiSSID = "BraundData";
const char *WiFiPass = "powerboner69";

Net NetClient("airdata", "10.42.0.1", 4004);

Adafruit_BME280 bme(5, 16, 17, 18); // use I2C interface


void packetReceived(uint8_t* data, uint32_t dataLength){
    Serial.print("NetClient recieved:");
    Serial.println(String(data, dataLength));
}

void onConnected(){
    Serial.println("NetClient Connected");
}

void onDisconnected(){
    Serial.println("NetClient disconnected");
}

void setup(){
    Serial.begin(115200);
    Serial.println("Initializing...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFiPass);
    WiFi.setSleep(WIFI_PS_NONE);

    NetClient.setPacketReceivedCallback(&packetReceived);
    NetClient.setOnConnected(&onConnected);
    NetClient.setOnDisconnected(&onDisconnected);


    if (!bme.begin()){
        Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    }
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF   );
}


void loop(){
    static uint32_t lastPrintTime = 0;

    if (WiFi.status() != WL_CONNECTED){//Reconnect to WiFi
        if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status()==WL_CONNECTION_LOST || WiFi.status()==WL_DISCONNECTED || WiFi.status()==WL_IDLE_STATUS){
            WiFi.disconnect();
            WiFi.begin(WiFiSSID, WiFiPass);
            Serial.print("WiFi connection failed");
        }
        Serial.println("Trying to connect to WiFi..."+String(millis()));
        Serial.println(WiFi.status());
        delay(5000);
    }

    if (NetClient.loop()){
        if (isTimeToExecute(lastPrintTime, 1000)){
            bme.takeForcedMeasurement();
            String msg=String(millis())+","+String(bme.readTemperature())+String(",")+String(bme.readHumidity())+String(",")+String(bme.readPressure())+String(",")+String(bme.readAltitude(SENSORS_PRESSURE_SEALEVELHPA));
            NetClient.sendString(msg);
            Serial.println(msg);
        }
    }
}