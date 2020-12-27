#include <FS.h>          // this needs to be first, or it all crashes and burns...
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include "DHT.h"
#include <FirebaseArduino.h>

#ifdef ESP32
  #include <SPIFFS.h>
#endif

//define your default values here, if there are different values in config.json, they are overwritten.
char firebase_host[100] = "FIREBASE_HOST";
char firebase_auth[45] = "DATABASE_SECRET";
char schedule[7] = "20000";

// SET RELAY PIN
#define D1 5   // เนื่องจากขาของ ESP8266 คือขาที่16 แต่ขาที่โชว์บนบอร์ด NodeMCU คือขา D0
#define relay_1 D1  // ขา D0 ของบอร์ด NodeMCU เป็นขาที่ต่อกับ LED 
//#define relay_1 0
//#define relay_1 0
//#define relay_1 0
//#define relay_1 0
//#define relay_1 0

// flag for saving data
bool shouldSaveConfig = false;

// Config DHT
#define DHTPIN 2
#define DHTTYPE DHT22

// Config time
int timezone = 7;

char ntp_server1[20] = "ntp.ku.ac.th";
char ntp_server2[20] = "fw.eng.ku.ac.th";
char ntp_server3[20] = "time.uni.net.th";
int dst = 0;

// SET LIB
DHT dht(DHTPIN, DHTTYPE);

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupSpiffs(){
  //clean FS, for testing
//   SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(firebase_host, json["firebase_host"]);
          strcpy(firebase_auth, json["firebase_auth"]);
          strcpy(schedule, json["schedule"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setup() {
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP  
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  pinMode(relay_1, OUTPUT);
  digitalWrite(relay_1, HIGH);

  setupSpiffs();

  // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  // setup custom parameters
  // 
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_firebase_host("fbhost", "firebase host", firebase_host, 100);
  WiFiManagerParameter custom_firebase_auth("fbauth", "firebase authost", firebase_auth, 45);
  WiFiManagerParameter custom_schedule("HM", "schedule push dht", schedule, 7);

  //add all your parameters here
  wm.addParameter(&custom_firebase_host);
  wm.addParameter(&custom_firebase_auth);
  wm.addParameter(&custom_schedule);
  
  // reset settings - wipe credentials for testing
//   wm.resetSettings();

  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  //here  "AutoConnectAP" if empty will auto generate basedcon chipid, if password is blank it will be anonymous
  //and goes into a blocking loop awaiting configuration
  if (!wm.autoConnect("MeeiIoT", "#qazwsx.edc")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // if we still have not connected restart and try all over again
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(firebase_host, custom_firebase_host.getValue());
  strcpy(firebase_auth, custom_firebase_auth.getValue());
  strcpy(schedule, custom_schedule.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["firebase_host"] = firebase_host;
    json["firebase_auth"] = firebase_auth;
    json["schedule"] = schedule;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());

  Serial.println(firebase_host);
  Serial.println(firebase_auth);
  Serial.println(schedule);

  configTime(timezone * 3600, dst, ntp_server1, ntp_server2, ntp_server3);
  Serial.println("Waiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Now: " + NowString());

  Firebase.begin(firebase_host, firebase_auth);
  Firebase.stream("/configs");

  dht.begin();
}

void loop() {
//  delay(2000);
  StreamAll();
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    delay(atoi(schedule));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["temperature"] = t;
  root["humidity"] = h;
  root["time"] = NowString();

  // append a new value to /logDHT
  String name = Firebase.push("logDHT", root);
  
  // handle error
  if (Firebase.failed()) {
      Serial.print("pushing /logDHT failed:");
      Serial.println(Firebase.error());  
      return;
  }

  Serial.print("pushed: /logDHT/");
  Serial.println(name);
  delay(atoi(schedule));
}

void StreamAll() {
    // handle error
  if (Firebase.failed()) {
      Serial.print("stream all failed:");
      Serial.println(Firebase.error());  
      return;
  }
    if (Firebase.available()) {
     FirebaseObject event = Firebase.readEvent();
     String eventType = event.getString("type");
     eventType.toLowerCase();
     
     Serial.print("event: ");
     Serial.println(eventType);
     if (eventType == "put") {
       Serial.print("data: ");
       Serial.println(event.getString("data"));
       String path = event.getString("path");
       String data = event.getString("data");

       int dataInt = event.getInt("data");
       Serial.print("dataInt: ");
       Serial.println(dataInt);

       Serial.print("path: ");
       Serial.println(path);

       if (path == "/relay1") {
          Serial.print("Open Relay 1:");
          // digitalWrite(relay_1, dataInt);
       }

       if (path == "/schedules") {
          char dataTimeStr[7];
          data.toCharArray(dataTimeStr, 7);
         strcpy(schedule, dataTimeStr);
       }
     }
  }   

}

String NowString() {
  time_t now = time(nullptr);
  struct tm* newtime = localtime(&now);

  String tmpNow = "";
  tmpNow += String(newtime->tm_year + 1900);
  tmpNow += "-";
  tmpNow += String(newtime->tm_mon + 1);
  tmpNow += "-";
  tmpNow += String(newtime->tm_mday);
  tmpNow += " ";
  tmpNow += String(newtime->tm_hour);
  tmpNow += ":";
  tmpNow += String(newtime->tm_min);
  tmpNow += ":";
  tmpNow += String(newtime->tm_sec);
  return tmpNow;
}
