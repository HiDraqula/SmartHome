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
<!doctypehtml><meta charset="utf-8"><meta content="width=device-width,initial-scale=1" name="viewport"><title>Smart Home Monitoring System</title><script src="https://cdn.tailwindcss.com"></script><script src="https://cdn.jsdelivr.net/npm/chart.js"></script><style>body{font-family:system-ui,-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen,Ubuntu,Cantarell,'Open Sans','Helvetica Neue',sans-serif;font-size:medium;text-align:center}.motorControl *{padding:1rem;background-color:#d3d3d3;border:0}.motorControl :active{background:#bfbfbf}.motorControl :focus:not(:active){background:#90ee90}</style><div class="flex flex-col px-7 py-7"><h1 class="mb-6 font-bold text-4xl">Smart Home Monitoring System</h1><div class="mb-6"><h2 class="font-semibold mb-2 text-2xl text-slate-800 text-start">Temperature and Humidity Monitoring</h2><div class="gap-6 grid grid-cols-2"><div class="flex"><div class="flex-1"><canvas id="temperatureChart"></canvas></div><div class="flex flex-col items-center justify-center"><p class="font-semibold mb-2 text-slate-700">Temperature</p><p class="text-2xl"><span id="temperature">--</span>°C</p></div></div><div class="flex"><div class="flex-1"><canvas id="humidityChart"></canvas></div><div class="flex flex-col items-center justify-center"><p class="font-semibold mb-2 text-slate-700">Humidity</p><p class="text-2xl"><span id="humidity">--</span>%</p></div></div></div></div><h2 class="font-semibold mb-2 text-2xl text-slate-800 text-start">Fan Speed History</h2><div class="gap-6 grid grid-cols-2"><div class="flex"><div class="mb-6 flex-1"><canvas id="fanSpeedChart" height="200" width="400"></canvas></div><div class="flex"><div class="flex flex-col items-center justify-center"><p class="font-semibold mb-2 text-slate-700">Fan Speed</p><p class="text-2xl"><span id="fanspeed">--</span></p></div></div></div><div class="flex flex-col items-center justify-center mb-6"><h2 class="font-semibold text-2xl">Fan Speed Control</h2><div class="flex items-center justify-center gap-2 motorControl my-3"><label for="motorSpeed">Set Speed:</label><input id="motorSpeed" max="255" min="0" oninput="debouncedUpdateSpeed(this.value)" type="range" value="60"> <input id="speedValue" max="255" min="0" oninput="debouncedUpdateSpeed(this.value)" type="number" value="60"></div><p class="mt-3 text-lg" id="status"></p></div></div><div class="mb-6"><h2 class="font-semibold text-2xl">WiFi LED Control</h2><p>Click to turn<a class="text-blue-600" href="ledOn" target="myIframe">LED ON</a></p><p>Click to turn<a class="text-blue-600" href="ledOff" target="myIframe">LED OFF</a></p><p>LED State:<iframe frameborder="0" height="25" name="myIframe" width="100"></iframe></p></div><div class="mb-6"><h2 class="font-semibold text-2xl">Motor Control</h2><div class="motorControl"><button onclick='controlMotor("forward")'>Forward</button><button onclick='controlMotor("backward")'>Backward</button><button onclick='controlMotor("stop")'>Stop</button><button onclick='controlMotor("check")'>Check Status</button></div><p class="font-semibold mt-4 text-xl" id="lastOperation">Last Motor Operation: --</p></div></div><script>let baseUrl="",motorSpeed=document.getElementById("motorSpeed"),speedValue=document.getElementById("speedValue"),statusEl=document.getElementById("status"),fanspeedEl=document.getElementById("fanspeed"),lastOperationEl=document.getElementById("lastOperation"),timeoutId,tempData=JSON.parse(localStorage.getItem("tempData"))||[],humidityData=JSON.parse(localStorage.getItem("humidityData"))||[],fanSpeedData=JSON.parse(localStorage.getItem("fanSpeedData"))||[],labels=JSON.parse(localStorage.getItem("labels"))||[],referenceTemperature=null,referenceSpeed=null,MAX_POINTS=150,tempChartCtx=document.getElementById("temperatureChart").getContext("2d"),humidityChartCtx=document.getElementById("humidityChart").getContext("2d"),fanSpeedChartCtx=document.getElementById("fanSpeedChart").getContext("2d"),temperatureChart=new Chart(tempChartCtx,{type:"line",data:{labels:labels,datasets:[{label:"Temperature (°C)",data:tempData,borderColor:"rgba(255, 99, 132, 1)",borderWidth:2,fill:!1}]},options:{scales:{x:{title:{display:!0,text:"Time"}},y:{beginAtZero:!1}}}}),humidityChart=new Chart(humidityChartCtx,{type:"line",data:{labels:labels,datasets:[{label:"Humidity (%)",data:humidityData,borderColor:"rgba(54, 162, 235, 1)",borderWidth:2,fill:!1}]},options:{scales:{x:{title:{display:!0,text:"Time"}},y:{beginAtZero:!1}}}}),fanSpeedChart=new Chart(fanSpeedChartCtx,{type:"line",data:{labels:labels,datasets:[{label:"Fan Speed",data:fanSpeedData,borderColor:"rgba(255, 206, 86, 1)",borderWidth:2,fill:!1}]},options:{scales:{x:{title:{display:!0,text:"Time"}},y:{beginAtZero:!0,max:255}}}});function debounce(t,a){return function(...e){clearTimeout(timeoutId),timeoutId=setTimeout(()=>t.apply(this,e),a)}}function updateSpeed(){let t=motorSpeed.value;speedValue.value=t,fetch(baseUrl+"/setSpeed?value="+t).then(e=>e.text()).then(e=>{statusEl.textContent="Speed set to "+t,fanspeedEl.textContent=t}).catch(e=>console.error("Error:",e))}function controlMotor(t){var e=motorSpeed.value;fetch(baseUrl+`/${t}?speed=`+e).then(e=>e.text()).then(e=>{statusEl.textContent=e,lastOperationEl.textContent="Last Motor Operation: "+t,localStorage.setItem("lastOperation",t)}).catch(e=>console.error("Error:",e))}function storeData(e,t,a,r){labels.length>=MAX_POINTS&&(labels.shift(),tempData.shift(),humidityData.shift(),fanSpeedData.shift()),labels.push(e),tempData.push(t),humidityData.push(a),fanSpeedData.push(r),localStorage.setItem("labels",JSON.stringify(labels)),localStorage.setItem("tempData",JSON.stringify(tempData)),localStorage.setItem("humidityData",JSON.stringify(humidityData)),localStorage.setItem("fanSpeedData",JSON.stringify(fanSpeedData)),temperatureChart.update(),humidityChart.update(),fanSpeedChart.update()}function adjustFanSpeed(e){console.log({currentTemperature:e,referenceTemperature:referenceTemperature,referenceSpeed:referenceSpeed});e-=referenceTemperature;let t=referenceSpeed;0<e?t=Math.min(referenceSpeed+Math.floor(e*(.195*referenceSpeed)),255):e<0&&(t=Math.max(referenceSpeed+Math.ceil(e*(.195*referenceSpeed)),0)),console.log({newFanSpeed:t},motorSpeed.value),t!==+motorSpeed.value&&(motorSpeed.value=t,updateSpeed())}function updateTemperatureHumidity(){fetch(baseUrl+"/readDHT").then(e=>e.json()).then(e=>{var{temperature:e,humidity:t}=e,a=(new Date).toLocaleTimeString([],{hour:"2-digit",minute:"2-digit",second:"2-digit",hour12:!1});document.getElementById("temperature").textContent=e,document.getElementById("humidity").textContent=t,null!==referenceTemperature&&null!==referenceSpeed?adjustFanSpeed(e):e&&(referenceTemperature=e,referenceSpeed=100,adjustFanSpeed(e)),storeData(a,e,t,motorSpeed.value)}).catch(e=>console.error("Error:",e))}let lastOperation=localStorage.getItem("lastOperation");function updateFanReference(e){referenceSpeed=parseFloat(e)||100,referenceTemperature=parseFloat(document.getElementById("temperature").textContent)||25,updateSpeed()}lastOperation&&(lastOperationEl.textContent="Last Motor Operation: "+lastOperation);let debouncedUpdateSpeed=debounce(updateFanReference,300);setInterval(updateTemperatureHumidity,2e3)</script>
)=====";

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

// SERVER FUNCTIONS
// void handleRoot();              // function prototypes for HTTP handlers
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
    // String s = MAIN_page;              // Read HTML contents
    server.send(200, "text/html", MAIN_page);  // Send web page
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

  server.on("/getUpdates", []() {
    float Temperature = dht.readTemperature();
    float Humidity = dht.readHumidity();
    int speed = analogRead(enable1Pin);
    bool motorState = digitalRead(motor1Pin1) && digitalRead(motor1Pin2);
    // Serial.printf("Temperature: %.2f°C, Humidity: %.2f%%\n", Temperature, Humidity);
    String json = "{\"status\":\"OK\", \"temperature\": " + String(Temperature) + ", \"humidity\": " + String(Humidity) + ", \"speed\": " + String(speed) + ", \"motorStatus\": " + (motorState ? "\"ON\"" : "\"OFF\"") + "}";
    // Serial.printf(json);
    // Serial.printf("%s", json.c_str());
    Serial.print(json);
    server.send(200, "application/json", json);
  });

  server.onNotFound(handleNotFound);  // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"

  server.enableCORS(true);
  // DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  server.begin();  // Actually start the server
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();  // Listen for HTTP requests from clients
}
