#include <HTTPClient.h>
#include "config.h"
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <WiFi.h>
#include "DHT.h"

#define RAIN_SENSOR_PIN 34
#define DHTPIN 32
#define LED_PIN1 25
#define PUMP 26
#define LED_PIN3 18
#define ldr_pin 12
#define SOIL_MOISTURE_PIN 35
#define DHTTYPE DHT11 

// set up the 'digital' feed
AdafruitIO_Feed *digital1 = io.feed("digital1");
AdafruitIO_Feed *pump = io.feed("pump");
AdafruitIO_Feed *soil = io.feed("soil");
AdafruitIO_Feed *light = io.feed("light");
AdafruitIO_Feed *temperature = io.feed("temperature");
AdafruitIO_Feed *humidity = io.feed("humidity");
AdafruitIO_Feed *rain = io.feed("rain");

String GOOGLE_SCRIPT_ID = "AKfycbzsDWxfvOXVNJcZg1qdsc7aYgJ_lI4_hfMIIGAcGQxBNmMbISmZDsWY75Xvvlt-AFhFNQ";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800;
const int   daylightOffset_sec = 0;

unsigned long previousMillis = 0;
const long interval = 3600000;
int count = 0;

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  
  // start the serial connection
  Serial.begin(115200);
  delay(1000);


  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(LED_PIN3, OUTPUT);
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(ldr_pin, INPUT);
  pinMode(DHTPIN, INPUT);
  
  // wait for serial monitor to open
  while(!Serial);

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  dht.begin();

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // set up a message handler for the 'digital' feed.
  // received from adafruit io.
  digital1->onMessage(handleMessage1);
  pump->onMessage(handleMessage2);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  // we are connected
  Serial.println();
  Serial.println(io.statusText());
  digital1->get();
  pump->get();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Read sensor data
    int ldrSensorValue = digitalRead(ldr_pin);
    float humidityValue = dht.readHumidity();
    float temperatureValue = dht.readTemperature();
    float soilMoistureValue = getSoilPercentage();
    float rainValue = getRainPercentage();

    // Save data to Adafruit IO feeds
    light->save(ldrSensorValue);  // Update the digital feed based on LDR status
    temperature->save(temperatureValue);
    humidity->save(humidityValue);
    soil->save(soilMoistureValue);
    rain->save(rainValue);

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidityValue) || isnan(temperatureValue)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    static bool flag = false;
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    char timeStringBuff[50]; //50 chars should be enough
    strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
    String asString(timeStringBuff);
    asString.replace(" ", "-");
    Serial.print("Time:");
    Serial.println(asString);
    // Add sensor values to the URL
      String urlFinal = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?" + 
                        "date=" + asString + 
                        "&sensor=" + String(count) +
                        "&ldrSensor=" + String(ldrSensorValue) +
                        "&temperature=" + String(temperatureValue) +
                        "&humidity=" + String(humidityValue) +
                        "&soilMoisture=" + String(soilMoistureValue) +
                        "&rain=" + String(rainValue);
    Serial.print("POST data to spreadsheet:");
    Serial.println(urlFinal);

    HTTPClient http;
    http.begin(urlFinal.c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET(); 
    Serial.print("HTTP Status Code: ");
    Serial.println(httpCode);
    //---------------------------------------------------------------------
    //getting response from google sheet
    String payload;
    if (httpCode > 0) {
        payload = http.getString();
        Serial.println("Payload: "+payload);    
    }
    //---------------------------------------------------------------------
    http.end();
    count++;
  }
  // No delay needed here
  io.run();
}

float getSoilPercentage(void)
{
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  float soilpercentage = (100 - ((soilMoistureValue / 4095.00) * 100));
  return soilpercentage;
  //return soilMoistureValue;
}

float getRainPercentage(void)
{
  int rainValue = analogRead(RAIN_SENSOR_PIN);
  float rainpercentage = (100 - ((rainValue / 4095.00) * 100));
  return rainpercentage;
  //return soilMoistureValue;
}

// this function is called whenever a 'digital' feed message
// is received from Adafruit IO. it was attached to
// the 'digital' feed in the setup() function above.
void handleMessage1(AdafruitIO_Data *data) {
  
  Serial.print("led 1 received <- ");
  
  if(data->toPinLevel() == HIGH)
    Serial.println("HIGH");
  else
    Serial.println("LOW");1

  digitalWrite(LED_PIN1, data->toPinLevel());
}

void handleMessage2(AdafruitIO_Data *data) {

  Serial.print("Pump received <- ");
  
  if(data->toPinLevel() == HIGH){
    Serial.println("HIGH");
  }
    
  else{ 
    Serial.println("LOW");
    delay(5000);
    }
   
  digitalWrite(PUMP, data->toPinLevel());
}

