#include <NTPClient.h>
#include <FirebaseESP8266.h>
#include <FS.h> // this needs to be first, or it all crashes and burns...
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include "DHT.h"
//#include <FirebaseArduino.h>
#include <time.h> 
#ifdef ESP32
  #include <SPIFFS.h>
#endif


//define your default values here, if there are different values in config.json, they are overwritten.
char firebase_host[100] = "humidity-50ec7-default-rtdb.firebaseio.com";
char firebase_auth[45] = "X3MGAedyY3SdKMqGvZW57afZ4GsWtixUxXorP6at";

#define FIREBASE_HOST "humidity-50ec7-default-rtdb.firebaseio.com"
#define FIREBASE_KEY "X3MGAedyY3SdKMqGvZW57afZ4GsWtixUxXorP6at"
#define API_KEY "AIzaSyD45H0aR3l-gd0XlP2d7o83jHmWnDy1VSw"
/* Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "w.wuthikun@gmail.com"
#define USER_PASSWORD "y@er2021"

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;
bool wifiState = false;
bool humidityState = false;
char schedule[20] = "1800000";

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
int timezoneOffset = 25200;




char ntp_server1[20] = "pool.ntp.org";
char ntp_server2[20] = "time.nist.gov";
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

//  pinMode(relay_1, OUTPUT);
//  digitalWrite(relay_1, HIGH);

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
  WiFiManagerParameter custom_schedule("HM", "schedule push dht", schedule, 20);

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
  if (!wm.autoConnect("dht", "y@er2021")) {
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

  


loadConfigTime();

  /* Assign the user sign in credentials */
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;
 /* Assign the project host and api key (required) */
    config.host = FIREBASE_HOST;
    config.api_key = API_KEY;
    
//  Firebase.begin(firebase_host, firebase_auth);
//Firebase.begin(FIREBASE_HOST, FIREBASE_KEY);
 /* Initialize the library with the Firebase authen and config */
    Firebase.begin(&config, &auth);


 bool wifiState = true;
if(Firebase.setBool(firebaseData, "/sensors/wifiState", wifiState)) {
    Serial.println("Added"); 
} else {
    Serial.println("Error : " + firebaseData.errorReason());
}
  dht.begin();

 


}

void loadConfigTime() {
  
  configTime(timezoneOffset, dst, ntp_server1, ntp_server2, ntp_server3);
  
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  
}



void loop() {

loadConfigTime();

  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {

     bool humidityState = false;
    if(Firebase.setBool(firebaseData, "/sensors/humidityState", humidityState)) {
        Serial.println("Added humidityState false"); 
    } else {
        Serial.println("Error humidityState false: " + firebaseData.errorReason());
    }
    Serial.println(F("Failed to read from DHT sensor!"));
    delay(atoi(schedule));
    return;
  }

     bool humidityState = true;
    if(Firebase.setBool(firebaseData, "/sensors/humidityState", humidityState)) {
        Serial.println("Added humudityState true"); 
    } else {
        Serial.println("Error humudityState true: " + firebaseData.errorReason());
    }




  

  delay(atoi(schedule));

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
  Serial.println(Date());
Serial.println(Time());


  FirebaseJson data;
data.set("temperature", t);
data.set("humidity", h);
data.set("date", Date());
data.set("time", Time());

if(Firebase.pushJSON(firebaseData, "/logDHT", data)) {
    Serial.print("pushed: /logDHT/");
} else {
    Serial.println("pushing /logDHT failed: " + firebaseData.errorReason());
    return;
}

}



String Date() {
  loadConfigTime();
  time_t timer;
  char buffer[26];
  struct tm* tm_info;

  timer = time(NULL);
  tm_info = localtime(&timer);

  strftime(buffer, 26, "%Y-%m-%d", tm_info);
  puts(buffer);
  return buffer;
}

String Time() {
  time_t rawtime = time(nullptr);
  struct tm * timeinfo = localtime(&rawtime);
  char buffer [80];
  time (&rawtime);
  timeinfo = localtime (&rawtime);
  strftime (buffer,80,"%T",timeinfo);
  return buffer;
}
