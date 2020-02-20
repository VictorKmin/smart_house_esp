#include "DHTesp.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>
#include "PWM.hpp"

// Replace with your network credentials
const char* ssid     = "EleksPublic";
const char* password = "8353eWg2";
const String host    = "http://172.19.16.72:5000";
const String url     = "/";
WiFiServer server(80);
HTTPClient http;

//#define roomId 3
#define roomId 4

//Server setup
const long SERVER_CHECKOUT_INTERVAL = 1000 * 60 * 2;  //milliseconds
boolean backendAvailable;

// Client setup
unsigned long previousSrvMillis = 0;
unsigned long currentSrvMillis = 0;
String header;

//temperature / humidity / cO2 sensors setup
#define DHTPIN 5
//#define DHTTYPE DHT11   // DHT 11
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHTesp dht;
float temperatureCFloat;
float humidityHFloat;
const long DHTxx_CHECKOUT_INTERVAL = 2000;  //milliseconds
unsigned long previousDHTxxMillis = 0;

// pin for pwm reading
#define MH_Z19_RX D4
#define MH_Z19_TX D0
#define MH_Z19_PWM D6
int prevVal = LOW;
long th, tl, h, l, ppm, co2PPM = 0;

//Heater setup
float roomTemp = 0;
#define HEATER_RELLAY_PIN 4

//Simple Moving Average algorithm
#define WINDOW_SIZE 10
float simpleMovingAverage[WINDOW_SIZE];
int arrayIndex = 0;

//System setup
#define SERVER_STATUS_LED_PIN 0 //led - server is not available

void resetServerWatchdog(unsigned long millisec = -1){
  if(millisec < 0){
    previousSrvMillis = currentSrvMillis;
  }else{
    previousSrvMillis = currentSrvMillis = millisec;
  }
}

void setRoomTemp(float temp)
{
  roomTemp = temp;
  Serial.println("server setup the Temp valuse = '" + String(roomTemp) + "'");
  heaterHandler();
}

void checkDTHSensor()
{
  // Check if any reads failed and exit early (to try again).
  if (isnan(humidityHFloat) || isnan(temperatureCFloat)) {
    Serial.println("Failed to read from DHT sensor!");
    humidityHFloat = 0;
    temperatureCFloat = 0;
  }
}

void setup() {
  Serial.begin(115200);

//  my_pwm.begin(true);
//  co2Serial.begin(9600);

  pinMode(SERVER_STATUS_LED_PIN, OUTPUT);
  pinMode(HEATER_RELLAY_PIN, OUTPUT);
  digitalWrite(HEATER_RELLAY_PIN, LOW); // set heater to LOW state

  dht.setup(DHTPIN, DHTesp::DHTTYPE);
  delay(1000);
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  humidityHFloat = dht.getHumidity();
  temperatureCFloat = dht.getTemperature();
  checkDTHSensor();

  // Connect to Wi-Fi network with SSID and password
  //Serial.print("Connecting to ");
  //Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  // Print local IP address and start web server
  //Serial.println("WiFi connected.");
  //Serial.println("IP address: ");
  //Serial.println(WiFi.localIP());
  server.begin();
  backendAvailable = false;
}

void loop(){
  //heaterHandler(); //no need to ping it all the time

//    Serial.print("Value: ");
//    Serial.println(my_pwm.getValue());
//    delay(1000);

    long tt = millis();
    int val = digitalRead(MH_Z19_PWM);
    if (val == HIGH) {
      if (val != prevVal) {
        h = tt;
        tl = h - 1;
        prevVal = val;
      }
    }  else {
      if (val != prevVal) {
        l = tt;
        th = 1 - h;
        prevVal = val;
        co2PPM = 5000 * (th - 2) / (th + tl - 4);
      }
    }
    Serial.println(co2PPM);
    delay(1000);
//  co2Serial.write(cmd, 9);
//  co2Serial.readBytes(response, 9);
//  unsigned int responseHigh = (unsigned int) response[2];
//  unsigned int responseLow = (unsigned int) response[3];
//  co2PPM = (256 * responseHigh) + responseLow;
//  Serial.println("ppm = " + String(co2PPM));
//  delay(10000);

//  co2Serial.write(cmd, 9);
//  memset(response, 0, 9);
//  co2Serial.readBytes(response, 9);
//  int i;
//  byte crc = 0;
//  for (i = 1; i < 8; i++) crc+=response[i];
//  crc = 255 - crc;
//  crc++;
//
//    Serial.println(response[0]);
//    Serial.println(response[1]);
//    Serial.println(response[2]);
//    Serial.println(response[3]);
//    Serial.println(response[8]);

//  if ( !(response[0] == 0xFF && response[1] == 0x86 && response[8] == crc) ) {
//    Serial.println("CRC error: " + String(crc) + " / "+ String(response[8]));
//  } else {
//    unsigned int responseHigh = (unsigned int) response[2];
//    unsigned int responseLow = (unsigned int) response[3];
//    unsigned int co2PPM = (256 * responseHigh) + responseLow;
//    Serial.println(co2PPM);
//  }
//  delay(1000);

  //----

  WiFiClient client = server.available();

  digitalWrite(SERVER_STATUS_LED_PIN, backendAvailable ? LOW : HIGH);

  if(!backendAvailable)
  {
    HTTPClient http;
    http.begin(host);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(getJSON());
    if(httpCode > 0) {
      Serial.printf("[HTTP] POST... code: %d\n", httpCode);
      if(httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        //Serial.println(payload);
        setRoomTemp(getLookUpValue(payload.substring(1, payload.length()-1), "\"message\":").toFloat());
        backendAvailable = true;
      }
    } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    delay(5000);
    resetServerWatchdog(millis());
  }else{
    // checking the interval whether server is alive and still pinging us.
    currentSrvMillis = millis();
    if (currentSrvMillis - previousSrvMillis >= SERVER_CHECKOUT_INTERVAL) {
      resetServerWatchdog();
      backendAvailable = false;
    }
    if (client) {
      Serial.println("New Client.");
      String currentLine = "";
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          Serial.write(c);
          header += c;

          if (c == '\n') {
            if (currentLine.length() == 0) {
              setRoomTemp(getLookUpValue(header, "room_temp=").toFloat());

              //now output HTML data header
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:application/json");
              client.println("Connection: close");
              client.println();
              client.println(getJSON());

              resetServerWatchdog(millis()); //only when all data is sent we need to reset watchdog timer and understand that the server is alive
              break;
            } else {
              currentLine = "";
            }
          } else if (c != '\r') {  // if you got anything else but a carriage return character,
            currentLine += c;      // add it to the end of the currentLine
          }
        }
      }
      // Clear the header variable
      header = "";
      // Close the connection
      client.stop();
      Serial.println("");
      Serial.println("Client disconnected.");
    }
  }

  if(DHTxxInterval(millis())){
    humidityHFloat = dht.getHumidity();
    temperatureCFloat = dht.getTemperature();
  }
  checkDTHSensor();
}

boolean DHTxxInterval(unsigned long millisec){
  if (millisec - previousDHTxxMillis >= DHTxx_CHECKOUT_INTERVAL) {
    previousDHTxxMillis = millisec;
    return true;
  }
  return false;
}

void heaterHandler(){
  Serial.println("t = " + String(temperatureCFloat));
  Serial.println("h = " + String(humidityHFloat));
  //float tmp = smooth(temperatureCFloat);
  //temperatureCFloat = isnan(tmp) ? 0.0 : tmp;
  Serial.println("roomTemp > temperatureCFloat = " + String(roomTemp) + " > " + String(temperatureCFloat));
  if(int(roomTemp * 100) > int(temperatureCFloat * 100)){
    // heater on
    if(digitalRead(HEATER_RELLAY_PIN) == LOW){
      Serial.print("heater on ");
      digitalWrite(HEATER_RELLAY_PIN, HIGH);
    }
  }else{
    // heater off
    if(digitalRead(HEATER_RELLAY_PIN) == HIGH){
      Serial.print("heater off ");
      digitalWrite(HEATER_RELLAY_PIN, LOW);
    }
  }
}

String getRequest(){
  return ("GET " + url + " HTTP/1.1\r\n" +
           "Host: " + host + "\r\n" +
           //"Connection: close\r\n" +
           "Content-Type: application/json\r\n" +
           "Content-Length: " + getJSON().length() + "\r\n" +
           "\r\n" + // This is the extra CR+LF pair to signify the start of a body
           getJSON() + "\n");
}

String getJSON(){
  heaterHandler();
  Serial.println("middle t = " + String(temperatureCFloat));
  String jsonString = String("{") +
                        "\"error_code\": 0," +
                        "\"ip\": \"" + WiFi.localIP().toString() + "\"," +
                        "\"room_id\": " + String(roomId) + "," +
                        "\"room_temp\": " + roomTemp + "," +
                        "\"interface\": {" +
                          "\"room_heater\": " + String(digitalRead(HEATER_RELLAY_PIN) == HIGH ? "1" : "0") + "," +
                          "\"sensor_temp\": " + String(temperatureCFloat) + "," +
                          "\"sensor_humidity\": " + String(humidityHFloat) + "," +
                          "\"sensor_co2\": " + String(co2PPM) +
                          "}" +
                        "}";
  return jsonString;
}

float smooth(float input){
  if(arrayIndex == WINDOW_SIZE){
    for (int i = 0; i < WINDOW_SIZE; i++){ // shift down the array
      if(i + 1 < WINDOW_SIZE){
        simpleMovingAverage[i] = simpleMovingAverage[i + 1];
      }else{
        simpleMovingAverage[i] = input;
      }
    }
  }else{
    simpleMovingAverage[arrayIndex] = input; // in case array is not reached WINDOW_SIZE
    arrayIndex ++;
  }

  float output = 0;
  for (int k = 0; k < arrayIndex; k++){
    output = output + simpleMovingAverage[k];
  }

  return output / float(arrayIndex);
}

String getLookUpValue(String alldata, String look_up){
  int index1 = alldata.indexOf(look_up);
  String substrval = "";
  if(index1 > -1){
    substrval = alldata.substring(index1 + look_up.length(), alldata.length());
    int index2 = substrval.indexOf("&");
    int index3 = substrval.indexOf(" ");
    if(index2 > -1){
      return substrval.substring(0, index2);
    }else if(index3 > -1){
      return substrval.substring(0, index3);
    }else{
      return substrval.substring(0, substrval.length());
    }
  }else{
    return "";
  }
}
