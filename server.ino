// Import required libraries
#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncTCP.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncWebServer.h>

// Replace with your network credentials
const char* ssid = "Kolya";
const char* password = "00001234";

#define DHTPIN 5     // Digital pin connected to the DHT sensor
#define LED_GET_JSON_INFO 14
#define LED_GET_INFO 12

// Uncomment the type of sensor in use:
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

#define roomId 1

#define SERVER_IP "192.168.0.102:5000"

DHT dht(DHTPIN, DHTTYPE);

// current temperature & humidity, updated in loop()
float t = 0.0;
float h = 0.0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
HTTPClient http;
WiFiClient client;


// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;    // will store last time DHT was updated

// Updates DHT readings every 10 seconds
const long interval = 10000;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h2>ESP8266 DHT Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i>
    <span class="dht-labels">Temperature</span>
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i>
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">%</sup>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";

// Replaces placeholder with DHT values
String processor(const String& var){
  if(var == "TEMPERATURE"){
    return String(t);
  }
  else if(var == "HUMIDITY"){
    return String(h);
  }
  return String();
}

String getJSON(){
  String jsonString = String("{") +
                        "\"error_code\": 0," +
                        "\"ip\": \"" + WiFi.localIP().toString() + "\"," +
                        "\"room_id\": " + String(roomId) + "," +
                        "\"room_temp\": " + "null" + "," +
                        "\"interface\": {" +
                          "\"room_heater\": " + String(digitalRead(DHTPIN) == HIGH ? "1" : "0") + "," +
                          "\"sensor_temp\": " + String(t) + "," +
                          "\"sensor_humidity\": " + String(h) + "," +
                          "\"sensor_co2\": " + String(22) +
                          "}" +
                        "}";
  return jsonString;
}

void led_info(int pin) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
}

void setup(){
  pinMode(LED_GET_JSON_INFO, OUTPUT);
  pinMode(LED_GET_INFO, OUTPUT);
  // Serial port for debugging purposes
  Serial.begin(115200);
  dht.begin();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
    led_info(LED_GET_INFO);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(t).c_str());
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", String(h).c_str());
  });

  server.on("/get-data", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "applicatiion/json", String(getJSON()).c_str());
    led_info(LED_GET_INFO);
  });
// Start server
  server.begin();
  http.begin(client, "http://" SERVER_IP "/module");
}

void loop(){
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    http.addHeader("Content-Type", "application/json");
    http.POST(getJSON());
    led_info(LED_GET_JSON_INFO);

    // save the last time you updated the DHT values
    previousMillis = currentMillis;
    float newT = dht.readTemperature();
    float newH = dht.readHumidity();

    if (isnan(newT)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      t = newT;
      Serial.print(t);
      Serial.println(": temperature");
    }

    if (isnan(newH)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      h = newH;
      Serial.print(h);
      Serial.println(": humidity");
    }
  }
}
