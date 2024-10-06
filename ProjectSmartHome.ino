#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>  // Include the WebServer library
// #include <ESPAsyncWebServer.h>  // Use the AsyncWebServer library
#include "DHT.h"

// Uncomment one of the lines below for whatever DHT sensor type you're using!
//#define DHTTYPE DHT11   // DHT 11
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
#define DHTTYPE DHT22  // DHT 22  (AM2302), AM2321

//---------------------------------------------------------------
//Our HTML webpage contents in program memory
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>

<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>My Home!</title>
    <!-- <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bulma@1.0.2/css/bulma.min.css"> -->
    <script src="https://cdn.tailwindcss.com"></script>
</head>
<style>
    *,
    *::after,
    *::before {
        box-sizing: border-box;
    }

    body {
        font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, 'Open Sans', 'Helvetica Neue', sans-serif;
        font-size: medium;
        text-align: center;
    }

    .motorControl * {
        padding: 1rem;
        background-color: lightgray;
        border: 0;
    }

    .motorControl *:active {
        /*background: lightblue;*/
        background: rgb(191, 191, 191);
    }

    .motorControl *:focus:not(:active) {
        background: lightgreen;
    }
</style>

<body>
    <div class="led">
        <h2 class="text-3xl font-bold">WiFi LED</h2><br>
        Click to turn <a href="ledOn" target="myIframe">LED ON</a><br>
        Click to turn <a href="ledOff" target="myIframe">LED OFF</a><br>
        LED State:<iframe name="myIframe" width="100" height="25" frameBorder="0"></iframe><br>
        <br>
        </center>
        <h2>Temperature and Humidity</h2>
        <p>Temperature: <span id="temperature"></span></p>
        <p>Humidity: <span id="humidity"></span></p>
        <h2 class="text-2xl font-bold my-2">Motor Control</h2>
        <div class="motorControl">
            <button onclick="controlMotor('forward')">Forward</button>
            <button onclick="controlMotor('backward')">Backward</button>
            <button onclick="controlMotor('stop')">Stop</button>
            <button onclick="controlMotor('check')">Check</button>
        </div>
        <p id="status"></p>

        <script>
            const baseUrl = "";
            function controlMotor(action) {
                fetch(`${baseUrl}/${action}`)
                    .then(response => response.text())
                    .then(data => {
                        document.getElementById('status').textContent = data;
                    })
                    .catch(error => console.error('Error:', error));
            }

            // Read temperature and humidity
            let readDHTPromise = null;
            function readDHT() {
                if (readDHTPromise) return;
                readDHTPromise = fetch(`${baseUrl}/readDHT`)
                    .then(response => response.json())
                    .then(data => {
                        let { temperature, humidity } = data;
                        document.getElementById('temperature').textContent = temperature;
                        document.getElementById('humidity').textContent = humidity;
                    })
                    .catch(error => console.error('Error:', error))
                    .finally(() => {
                        readDHTPromise = null;
                        setTimeout(readDHT, 2000);
                    });
            }
            readDHT();
        </script>
</body>

</html>
)=====";


// String SendHTML(float Temperaturestat, float Humiditystat) {
//   String ptr = "<!DOCTYPE html> <html>\n";
//   ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
//   // ptr += "<meta http-equiv=\"refresh\" content=\"2\" >\n";
//   ptr += "<title>ESP8266 Weather Report</title>\n";
//   ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
//   ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
//   ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
//   ptr += "</style>\n";
//   ptr += "<script>\n";
//   ptr += "setInterval(loadDoc,2000);\n";
//   ptr += "function loadDoc() {\n";
//   ptr += "var xhttp = new XMLHttpRequest();\n";
//   ptr += "xhttp.onreadystatechange = function() {\n";
//   ptr += "if (this.readyState == 4 && this.status == 200) {\n";
//   ptr += "document.getElementById(\"webpage\").innerHTML =this.responseText}\n";
//   ptr += "};\n";
//   ptr += "xhttp.open(\"GET\", \"/\", true);\n";
//   ptr += "xhttp.send();\n";
//   ptr += "}\n";
//   ptr += "</script>\n";
//   ptr += "</head>\n";
//   ptr += "<body>\n";
//   ptr += "<div id=\"webpage\">\n";
//   ptr += "<h1>ESP8266 NodeMCU Weather Report</h1>\n";

//   ptr += "<p>Temperature: ";
//   // ptr += (int)Temperaturestat;
//   // ptr += (float)Temperaturestat;
//   ptr += String(Temperaturestat, 2);  // Format temperature with 2 decimal places
//   // ptr += "°C</p>";
//   ptr += "&deg;C</p>";
//   ptr += "<p>Humidity: ";
//   // ptr += (int)Humiditystat;
//   // ptr += (float)Humiditystat;
//   ptr += String(Humiditystat, 2);  // Format humidity with 2 decimal places
//   ptr += "%</p>";

//   ptr += "</div>\n";
//   ptr += "</body>\n";
//   ptr += "</html>\n";
//   return ptr;
// }
// //---------------------------------------------------------------

ESP8266WiFiMulti wifiMulti;   // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);  // Create a webserver object that listens for HTTP request on port 80
// AsyncWebServer server(80);  // Create a webserver object that listens for HTTP request on port 80

// #define LED 2  //On board LED Connected to GPIO2
// uint8_t led = BUILTIN_LED;
// uint8_t led = D0;
const int led = 2;  // D4 - GPIO2
// const int led = 4;  // D2 - GPIO4

// DHT Sensor
uint8_t DHTPin = D2;
// uint8_t DHTPin = D7;
// Initialize DHT sensor.
DHT dht(DHTPin, DHTTYPE);

float Temperature;
float Humidity;

// Motor A
int motor1Pin1 = 12;
int motor1Pin2 = 14;
int enable1Pin = 13;

// Setting minimum duty cycle
int dutyCycle = 60;
int motorspeed = 60;

//SSID and Password of your WiFi router
// char ssid[] = "AniFi";  // type your wifi name
// char pass[] = "12345678A";  // type your wifi password
// const char* ssid = "AniFy";
// const char* password = "Ani@4321";

// void handleRoot();              // function prototypes for HTTP handlers
// SERVER FUNCTIONS
void handleRoot() {
  // server.send(200, "text/plain", "Hello world!");   // Send HTTP status 200 (Ok) and send some text to the browser/client
  // server.send(200, "text/html", "<form action=\"/LED\" method=\"POST\"><input type=\"submit\" value=\"Toggle LED\"></form>");

  Serial.println("You called root page");
  String s = MAIN_page;              //Read HTML contents
  server.send(200, "text/html", s);  //Send web page

  // Temperature = dht.readTemperature();  // Gets the values of the temperature
  // Humidity = dht.readHumidity();        // Gets the values of the humidity
  // // Check if any reads failed and exit the loop early
  // if (isnan(Humidity) || isnan(Temperature)) {
  //   Serial.println("Failed to read from DHT sensor!");
  // } else {
  //   // Print the sensor readings to the Serial Monitor
  //   Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", Temperature, Humidity);
  // }
  // server.send(200, "text/html", SendHTML(Temperature, Humidity));
}
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");  // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

// SETUP
void setup(void) {
  Serial.begin(115200);  // Start the Serial communication to send messages to the computer
  Serial.println("Hii 👋\n");
  delay(1000);
  Serial.println("Setting Up\n");

  // Initialize LED pin
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);  // Turn off LED initially

  // Initialize DHT sensor
  // pinMode(DHTPin, INPUT);
  dht.begin();

  // sets the pins as outputs:
  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

  // Start WiFi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // Ensure no previous connection
  delay(100);

  // Scan for available networks
  Serial.println("Scanning for available Wi-Fi networks...");
  int n = WiFi.scanNetworks();

  if (n == 0) {
    Serial.println("No networks found.");
  } else {
    Serial.printf("%d networks found:\n", n);
    for (int i = 0; i < n; ++i) {
      // Print SSID, RSSI (signal strength), and encryption type
      Serial.printf("%d: %s, Signal Strength (RSSI): %d, Encryption Type: %s\n",
                    i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                    (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "Open" : "Encrypted");
    }
  }

  // WiFi.begin(ssid, password);     //Connect to your WiFi router
  wifiMulti.addAP("AniFy", "Ani@4321");  // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("AniFi", "12345678A");
  // wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  // Serial.println("Connecting ...");
  Serial.println("\nConnecting to known Wi-Fi networks...");

  int i = 0;

  // // Wait for connection
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  // }

  while (wifiMulti.run() != WL_CONNECTED) {  // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    Serial.print('.');
    delay(250);
  }

  Serial.println("\nWiFi connected successfully.");
  // Serial.println(ssid);
  // Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.printf("Connected to: %s\n", WiFi.SSID().c_str());
  // Serial.print("IP address:\t");
  // Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());

  if (MDNS.begin("esp8266")) {  // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  server.on("/", []() {
    Serial.println("You called root page");
    String s = MAIN_page;              // Read HTML contents
    server.send(200, "text/html", s);  // Send web page
  });

  server.on("/LED", HTTP_POST, []() {
    digitalWrite(led, !digitalRead(led));  // Change the state of the LED
    Serial.println(digitalRead(led));
    server.send(200, "text/html", digitalRead(led) ? "ON" : "OFF");  // Send status of the led
  });

  server.on("/ledOn", []() {
    digitalWrite(led, HIGH);
    server.send(200, "text/html", "ON");
  });

  server.on("/ledOff", []() {
    digitalWrite(led, LOW);
    server.send(200, "text/html", "OFF");
  });

  server.on("/motorControl", HTTP_GET, []() {
    String action = server.uri();  // Get the action (e.g., /forward, /backward, /stop)

    if (server.hasArg("speed")) {
      String speedParam = server.arg("speed");
      int speed = speedParam.toInt();

      if (speed >= 0 && speed <= 255) {
        analogWrite(enable1Pin, speed);
        if (action == "/forward") {
          digitalWrite(motor1Pin1, HIGH);
          digitalWrite(motor1Pin2, LOW);
          server.send(200, "text/html", "Motor running forward at speed " + String(speed));
        } else if (action == "/backward") {
          digitalWrite(motor1Pin1, LOW);
          digitalWrite(motor1Pin2, HIGH);
          server.send(200, "text/html", "Motor running backward at speed " + String(speed));
        } else if (action == "/stop") {
          digitalWrite(motor1Pin1, LOW);
          digitalWrite(motor1Pin2, LOW);
          server.send(200, "text/html", "Motor stopped");
        } else {
          server.send(400, "text/html", "Invalid action. Use /forward, /backward, or /stop");
        }
      } else {
        server.send(400, "text/html", "Invalid speed value. Speed must be between 0 and 255.");
      }
    } else {
      server.send(400, "text/html", "Missing speed parameter.");
    }
  });

  server.on("/forward", []() {
    analogWrite(enable1Pin, motorspeed);
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, HIGH);
    server.send(200, "text/html", "Moving Forward");
    Serial.println("Moving Forward");
  });

  server.on("/backward", []() {
    analogWrite(enable1Pin, motorspeed);
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW);
    server.send(200, "text/html", "Moving Backward");
    Serial.println("Moving Backward");
  });

  server.on("/stop", []() {
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
    digitalWrite(enable1Pin, LOW);
    server.send(200, "text/html", "Motor Stopped");
    Serial.println("Motor Stopped");
  });

  server.on("/check", []() {
    digitalWrite(motor1Pin2, HIGH);
    digitalWrite(motor1Pin1, LOW);
    while (dutyCycle <= 255) {
      analogWrite(enable1Pin, dutyCycle);
      Serial.print("Forward with duty cycle: ");
      Serial.println(dutyCycle);
      dutyCycle += 5;
      delay(500);
    }
    dutyCycle = 60;
    server.send(200, "text/html", "Motor Status: Check");
    Serial.println("Motor Check Requested");
  });

  server.on("/readDHT", []() {
    float Temperature = dht.readTemperature();
    float Humidity = dht.readHumidity();

    if (isnan(Humidity) || isnan(Temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      server.send(500, "application/json", "{\"status\":\"Error\", \"message\":\"Failed to read from DHT sensor!\"}");
    } else {
      Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", Temperature, Humidity);
      String json = "{\"status\":\"OK\", \"temperature\": " + String(Temperature) + ", \"humidity\": " + String(Humidity) + "}";
      server.send(200, "application/json", json);
    }
  });

  server.on("/setSpeed", []() {
    if (server.hasArg("value")) {
      int speed = server.arg("value").toInt();
      if (speed >= 0 && speed <= 255) {
        analogWrite(enable1Pin, speed);
        motorspeed = speed;
        String message = "Motor speed set to: " + String(speed);
        server.send(200, "text/plain", message);
        Serial.println(message);
      } else {
        server.send(400, "text/plain", "Invalid speed value");
        Serial.println("Invalid speed value");
      }
    } else {
      server.send(400, "text/plain", "Speed value not provided");
      Serial.println("Speed value not provided");
    }
  });

  server.onNotFound(handleNotFound);  // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  server.enableCORS(true);
  // DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.begin();  // Actually start the server
  Serial.println("HTTP server started");
}

void loop(vṭoid) {
  server.handleClient();  // Listen for HTTP requests from clients
  // Read humidity and temperature from the DHT22 sensor
  // float humidity = dht.readHumidity();
  // float temperature = dht.readTemperature();

  // // Check if any reads failed and exit the loop early
  // if (isnan(humidity) || isnan(temperature)) {
  //   Serial.println("Failed to read from DHT sensor!");
  // } else {
  //   // Print the sensor readings to the Serial Monitor
  //   Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", temperature, humidity);
  // }

  // delay(2000);  // Wait 2 seconds between readings
}
