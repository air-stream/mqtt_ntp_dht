#include <ESP8266WiFi.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <PubSubClient.h>
#include <DHTesp.h>

#define TZ              -5      // (utc+) TZ in hours
#define DST_MN           0      // use 60mn for summer time in some countries

#define RTC_TEST     1510592825 // 1510592825 = Monday 13 November 2017 17:07:05 UTC

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

#define DHT_PORT        2

// Update these with values suitable for your network.
const char* ssid = "";
const char* password = "";

// MQTT settings
const char* mqtt_server = "";
const int   mqtt_port = 12051;
const char* mqtt_user = "";
const char* mqtt_password = "";
const char* mqtt_in_topic = "in";
const char* mqtt_out_topic = "out";

WiFiClient espClient;
PubSubClient client(espClient);

DHTesp* dht;

// Time NTP variables
timeval cbtime;           // time set in callback
bool cbtime_set = false;

void time_is_set(void) {
  gettimeofday(&cbtime, NULL);
  cbtime_set = true;
  Serial.println("------------------ settimeofday() was called ------------------");
}

void setup() {
  // Config the NTP
  configTime(TZ_SEC, DST_SEC, "pool.ntp.org");

  // Initialize the BUILTIN_LED pin as an output
  //pinMode(BUILTIN_LED, OUTPUT);

  Serial.begin(115200);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  settimeofday_cb(time_is_set);

  dht = new DHTesp();

  dht->setup(DHT_PORT, DHTesp::DHT22);
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_out_topic, "hello world");
      // ... and resubscribe
      client.subscribe(mqtt_in_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();


  TempAndHumidity newValues = dht->getTempAndHumidity();
  // Check if any reads failed and exit early (to try again).
  if (dht->getStatus() != 0) {
    Serial.println("DHT22 estado: " + String(dht->getStatusString()));
    delay(2000);
    return;
  }

  float heatIndex = dht->computeHeatIndex(newValues.temperature, newValues.humidity);

  std::string msg = getDate() + ',' + getTime();

  char str[6];

  snprintf(str, sizeof(str), "%02.2f", newValues.temperature);
  msg += ',' + std::string(str);

  snprintf(str, sizeof(str), "%02.2f", newValues.humidity);
  msg += ',' + std::string(str);

  snprintf(str, sizeof(str), "%02.2f", heatIndex);
  msg += ',' + std::string(str);

  Serial.print("Publish message: ");
  Serial.println(msg.c_str());
  client.publish(mqtt_out_topic, msg.c_str());


  delay(10000);
}

std::string getDate() {
  time_t now;
  now = time(nullptr);

  // 2019-04-02
  char buffer[11];
  struct tm* curr_tm;

  curr_tm = localtime(&now);
  strftime(buffer, sizeof(buffer), "%F", curr_tm);

  return std::string(buffer);
}

std::string getTime() {
  time_t now;
  now = time(nullptr);

  // 20:55:29
  char buffer[9];
  struct tm* curr_tm;

  curr_tm = localtime(&now);
  strftime(buffer, sizeof(buffer), "%T", curr_tm);

  return std::string(buffer);
}

std::string getTimestamp() {


  return getDate() + " " + getTime();
}

