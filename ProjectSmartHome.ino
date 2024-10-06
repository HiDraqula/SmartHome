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
    <title>Smart Home Monitoring System</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<style>
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
        background: rgb(191, 191, 191);
    }

    .motorControl *:focus:not(:active) {
        background: lightgreen;
    }
</style>

<body>
    <div class="container mx-auto p-5">
        <h1 class="text-4xl font-bold mb-6">Smart Home Monitoring System</h1>



        <div class="mb-6">
            <h2 class="text-2xl font-semibold">Temperature and Humidity Monitoring</h2>
            <p>Temperature: <span id="temperature">--</span> °C</p>
            <p>Humidity: <span id="humidity">--</span> %</p>
            <div class="grid grid-cols-2 gap-6">
                <div>
                    <h3 class="text-xl font-semibold mb-3">Temperature History</h3>
                    <canvas id="temperatureChart"></canvas>
                </div>
                <div>
                    <h3 class="text-xl font-semibold mb-3">Humidity History</h3>
                    <canvas id="humidityChart"></canvas>
                </div>
            </div>

        </div>
        <div class="grid grid-cols-2 gap-6">
            <div>

                <div class="mb-6">
                    <h2 class="text-2xl font-semibold">Fan Speed History</h2>
                    <canvas id="fanSpeedChart" width="400" height="200"></canvas>
                </div>


            </div>
            <div class="mb-6">

                <h2 class="text-2xl font-semibold">Motor Speed Control</h2>
                <div class="my-3 motorControl flex justify-center items-center gap-2">
                    <label for="motorSpeed">Set Speed:</label>
                    <input type="range" min="0" max="255" value="60" id="motorSpeed"
                        oninput="debouncedUpdateSpeed(this.value)">
                    <span id="speedValue">60</span>
                </div>
                <p id="status" class="text-lg mt-3"></p>
                <div class="mb-6">
                    <h2 class="text-2xl font-semibold">Fan Control</h2>
                    <div class="motorControl">
                        <button onclick="controlMotor('forward')">Forward</button>
                        <button onclick="controlMotor('backward')">Backward</button>
                        <button onclick="controlMotor('stop')">Stop</button>
                        <button onclick="controlMotor('check')">Check Status</button>
                    </div>
                    <p id="lastOperation" class="text-xl font-semibold mt-4">Last Motor Operation: --</p>
                </div>
            </div>

        </div>

        <div class="mb-6">
            <h2 class="text-2xl font-semibold">WiFi LED Control</h2>
            <p>Click to turn <a href="ledOn" target="myIframe" class="text-blue-600">LED ON</a></p>
            <p>Click to turn <a href="ledOff" target="myIframe" class="text-blue-600">LED OFF</a></p>
            <p>LED State: <iframe name="myIframe" width="100" height="25" frameborder="0"></iframe></p>
        </div>


    </div>

    <script>
        const baseUrl = "";
        const motorSpeed = document.getElementById('motorSpeed');
        const speedValue = document.getElementById('speedValue');
        const statusEl = document.getElementById('status');
        const lastOperationEl = document.getElementById('lastOperation');
        let timeoutId;
        let tempData = JSON.parse(localStorage.getItem('tempData')) || [];
        let humidityData = JSON.parse(localStorage.getItem('humidityData')) || [];
        let fanSpeedData = JSON.parse(localStorage.getItem('fanSpeedData')) || [];
        let labels = JSON.parse(localStorage.getItem('labels')) || [];

        let referenceTemperature = null; // To hold the reference temperature
        let referenceSpeed = null; // To hold the reference fan speed

        const MAX_POINTS = 150; // 5 minutes worth of data, with one point every 2 seconds

        // Initialize the temperature and humidity charts
        const tempChartCtx = document.getElementById('temperatureChart').getContext('2d');
        const humidityChartCtx = document.getElementById('humidityChart').getContext('2d');
        const fanSpeedChartCtx = document.getElementById('fanSpeedChart').getContext('2d');


        const temperatureChart = new Chart(tempChartCtx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Temperature (°C)',
                    data: tempData,
                    borderColor: 'rgba(255, 99, 132, 1)',
                    borderWidth: 2,
                    fill: false
                }]
            },
            options: {
                scales: {
                    x: {
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        beginAtZero: false
                    }
                }
            }
        });

        const humidityChart = new Chart(humidityChartCtx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Humidity (%)',
                    data: humidityData,
                    borderColor: 'rgba(54, 162, 235, 1)',
                    borderWidth: 2,
                    fill: false
                }]
            },
            options: {
                scales: {
                    x: {
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        beginAtZero: false
                    }
                }
            }
        });

        const fanSpeedChart = new Chart(fanSpeedChartCtx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [{
                    label: 'Fan Speed',
                    data: fanSpeedData,
                    borderColor: 'rgba(255, 206, 86, 1)',
                    borderWidth: 2,
                    fill: false
                }]
            },
            options: {
                scales: {
                    x: {
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        beginAtZero: true,
                        max: 255 // Max fan speed
                    }
                }
            }
        });

        // Debounced function to limit API calls for speed updates
        function debounce(func, delay) {
            return function (...args) {
                clearTimeout(timeoutId);
                timeoutId = setTimeout(() => func.apply(this, args), delay);
            };
        }

        // Function to update motor speed
        function updateSpeed() {
            const speed = motorSpeed.value;
            speedValue.textContent = speed;

            fetch(`${baseUrl}/setSpeed?value=${speed}`)
                .then(response => response.text())
                .then(data => {
                    statusEl.textContent = `Speed set to ${speed}`;
                })
                .catch(error => console.error('Error:', error));
        }

        // const debouncedUpdateSpeed = debounce(updateSpeed, 300);

        // Function to control motor direction and pass the current speed value
        function controlMotor(action) {
            const speed = motorSpeed.value;

            fetch(`${baseUrl}/${action}?speed=${speed}`)
                .then(response => response.text())
                .then(data => {
                    statusEl.textContent = data;
                    lastOperationEl.textContent = `Last Motor Operation: ${action}`;
                    localStorage.setItem('lastOperation', action); // Store last motor operation
                })
                .catch(error => console.error('Error:', error));
        }

        // Function to store and keep only the last 5 minutes (150 data points) in localStorage
        function storeData(currentTime, temperature, humidity, fanSpeed) {
            if (labels.length >= MAX_POINTS) {
                labels.shift(); // Remove the oldest time point
                tempData.shift(); // Remove the oldest temperature data point
                humidityData.shift(); // Remove the oldest humidity data point
                fanSpeedData.shift(); // Remove the oldest fan speed data point
            }

            labels.push(currentTime); // Add new time point
            tempData.push(temperature); // Add new temperature data point
            humidityData.push(humidity); // Add new humidity data point
            fanSpeedData.push(fanSpeed); // Add new fan speed data point

            // Save data to localStorage
            localStorage.setItem('labels', JSON.stringify(labels));
            localStorage.setItem('tempData', JSON.stringify(tempData));
            localStorage.setItem('humidityData', JSON.stringify(humidityData));
            localStorage.setItem('fanSpeedData', JSON.stringify(fanSpeedData));

            // Update the charts with new data
            temperatureChart.update();
            humidityChart.update();
            fanSpeedChart.update();
        }

        // Function to adjust the fan speed based on temperature
        function adjustFanSpeed(currentTemperature) {
            const temperatureDifference = currentTemperature - referenceTemperature;

            // Calculate new fan speed based on temperature change
            let newFanSpeed = referenceSpeed;
            let speedFactor = ((255 - 60) / 10) / 100; // => ((max-min)/factor)/100

            // Adjust fan speed based on temperature difference
            if (temperatureDifference > 0) {
                // Increase speed by a smaller factor (e.g., 2% of reference speed per degree)
                newFanSpeed = Math.min(referenceSpeed + Math.floor(temperatureDifference * (speedFactor * referenceSpeed)), 255);
            } else if (temperatureDifference < 0) {
                // Decrease speed by a smaller factor (e.g., 2% of reference speed per degree)
                newFanSpeed = Math.max(referenceSpeed + Math.ceil(temperatureDifference * (speedFactor * referenceSpeed)), 0);
            }

            // Update the motor speed only if there's a significant change
            if (newFanSpeed !== motorSpeed.value) {
                motorSpeed.value = newFanSpeed;
                speedValue.textContent = newFanSpeed;
                updateSpeed(); // Call update to send new speed to the server
            }
        }

        // Fetch temperature and humidity data every 2 seconds and update the charts
        function updateTemperatureHumidity() {
            fetch(`${baseUrl}/readDHT`)
                .then(response => response.json())
                .then(data => {
                    const { temperature, humidity } = data;
                    const currentTime = new Date().toLocaleTimeString();

                    document.getElementById('temperature').textContent = temperature;
                    document.getElementById('humidity').textContent = humidity;

                    // Check if we have a reference to adjust the fan speed
                    if (referenceTemperature !== null && referenceSpeed !== null) {
                        // Calculate the new fan speed based on the current temperature
                        adjustFanSpeed(temperature);
                    }

                    // Update the fan speed based on the current reading
                    //if (temperature > 25) { // Adjust this threshold as needed
                    //    referenceTemperature = temperature;
                    //    referenceSpeed = motorSpeed.value; // Set the current fan speed as reference
                    //}
                    // Fetch the fan speed and update the fan speed chart
                    //fetch(`${baseUrl}/fanSpeed`)
                    //    .then(response => response.json())
                    //    .then(fanData => {
                    //        const currentFanSpeed = fanData.speed;
                    //        motorSpeed.value = currentFanSpeed;
                    //        speedValue.textContent = currentFanSpeed;
                    //    });

                    storeData(currentTime, temperature, humidity, motorSpeed.value); // Store the last 5 minutes of data
                })
                .catch(error => console.error('Error:', error))

        }

        // Restore last motor operation
        const lastOperation = localStorage.getItem('lastOperation');
        if (lastOperation) {
            lastOperationEl.textContent = `Last Motor Operation: ${lastOperation}`;
        }

        // Function to handle updating the fan reference when the user changes the speed
        function updateFanReference(newSpeed) {
            referenceSpeed = newSpeed;
            referenceTemperature = parseFloat(document.getElementById('temperature').textContent); // Use current temperature as reference
            updateSpeed();
        }
        const debouncedUpdateSpeed = debounce(updateFanReference, 300);

        // Fetch sensor data every 2 seconds
        setInterval(updateTemperatureHumidity, 2000);
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

void loop(void) {
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
