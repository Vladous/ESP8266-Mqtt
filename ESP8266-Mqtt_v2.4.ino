// MQTT Client IoT
// Připojení k WiFi a MQTT, ovládání LED, detekce tlesknutí
// v2.4 14.05.2023

// Připojení :
//

// Mikrofon (KY-038) VCC → 3.3V , GNG → GND , A0 → A0

// Položky s komentářem !! CHANGE !! upravte podle Vašich potřeb

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

#define WIFI_NAME "Změňte na název Vaší sítě"              /// !! CHANGE !!
#define WIFI_PASSWORD "Změnte na Vaše helso"               /// !! CHANGE !!
#define WIFI_HOSTNAME "ZASUVKA_OBYVAK"                     /// !! CHANGE !!
#define MQTT_SERVER "Změnte na adresu vašeho MQTT serveru" /// !! CHANGE !!
#define MQTT_PORT 1883                                     /// !! CHANGE !!

WiFiClient espClient;
PubSubClient client(espClient);

int ledType = 1;                    // Typ LED světel: 1 - bílá, 2 - RGB, 3 - relé     /// !! CHANGE !!
String deviceName = "device_2";     // MQTT topic název zařízení                       /// !! CHANGE !!
const bool isClapEnabled = true;    // Použití mikrofonu                               /// !! CHANGE !!
const int clapThresholdValue = 500; // Nastavitelná hladina detekce tlesknutí          /// Můžete upravit
const int buttonPin = D4;           // Tlačítko
const int powerSwitchPin = D3;      // Ledka / transistor zapnuti
const int wifiLedPin = D2;          // Ledka připojení k WiFi
const int relayPin = D1;            // Relé
const int powerLedPin = D0;         // Ledka power
const int redLedPin = D5;           // Ledka power (red)
const int greenLedPin = D6;         // Ledka power (green)
const int blueLedPin = D7;          // Ledka power (blue)
const int clapSensorPin = A0;       // Zvukový senzor připojený na analogový vstup A0
char deviceCharArray[50];           // Přetypování názvu proměnné pro MQTT
boolean repeatFlag;                 // Kontrola opakování stisku tlačítka
boolean isDeviceOn;                 // Proměnná pro stav zapnuto / vypnuto
boolean previousDeviceState;        // Dočasná proměnná pro předchozí stav zapnuto / vypnuto
int redIntensity = 254;             //
int greenIntensity = 254;           //
int blueIntensity = 254;            //
int ledBrightness = 254;            //
int indicatorLedBrightness = 254;   //
bool mqttConnected = false;         //

void setup()
{
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(powerSwitchPin, OUTPUT);
  pinMode(wifiLedPin, OUTPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(powerLedPin, OUTPUT);  /// Ledka power
  pinMode(redLedPin, OUTPUT);    /// Ledka power (red)
  pinMode(greenLedPin, OUTPUT);  /// Ledka power (green)
  pinMode(blueLedPin, OUTPUT);   /// Ledka power (blue)
  pinMode(clapSensorPin, INPUT); /// Mikrofon
  analogWrite(powerLedPin, ledBrightness);
  Serial.begin(19200);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  connectToNetwork(); // Volání nové funkce pro připojení k WiFi a MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  deviceName.toCharArray(deviceCharArray, deviceName.length() + 1);
  Serial.println("Moje IP adresa je:");
  Serial.println(WiFi.localIP());
}

bool detectClap()
{
  int clapValue = analogRead(clapSensorPin);
  if (clapValue > clapThresholdValue)
  {
    delay(50); // Zpoždění pro zabránění falešných detekcí
    return true;
  }
  return false;
}

void loop()
{
  previousDeviceState = isDeviceOn;
  checkConnection();     // Kontrola připojení
  handleClapDetection(); // Detekce tlesknutí
  processMQTTMessages(); // Zpracování MQTT zpráv
  updateLEDStatus();     // Aktualizace stavu led
}

void updateLEDStatus()
{
  switch (ledType)
  {
  case 1:
    if (isDeviceOn == 1)
    {
      analogWrite(relayPin, indicatorLedBrightness);
      analogWrite(powerSwitchPin, ledBrightness);
    }
    else
    {
      analogWrite(relayPin, 0);
      analogWrite(powerSwitchPin, 0);
    }
    break;
  case 2:
    if (isDeviceOn == 1)
    {
      analogWrite(redLedPin, redIntensity);
      analogWrite(greenLedPin, greenIntensity);
      analogWrite(blueLedPin, blueIntensity);
      analogWrite(powerSwitchPin, ledBrightness);
    }
    else
    {
      analogWrite(redLedPin, 0);
      analogWrite(greenLedPin, 0);
      analogWrite(blueLedPin, 0);
      analogWrite(powerSwitchPin, 0);
    }
    break;
  case 3:
    if (isDeviceOn == 1)
    {
      digitalWrite(relayPin, HIGH);
      analogWrite(powerSwitchPin, ledBrightness);
    }
    else
    {
      digitalWrite(relayPin, LOW);
      analogWrite(powerSwitchPin, 0);
    }
    break;
  }
  analogWrite(powerLedPin, ledBrightness);
  analogWrite(wifiLedPin, ledBrightness);
}

void processMQTTMessages()
{
  if (previousDeviceState == isDeviceOn)
  {
    processButtonInput();
  }
  if (previousDeviceState != isDeviceOn)
  {
    publishMqttData();
    delay(300);
  }
}

void handleClapDetection()
{
  // Kód pro detekci tlesknutí
  if (isClapEnabled)
  {
    if (detectClap())
    {
      isDeviceOn = !isDeviceOn;
    }
  }
}

void checkConnection()
{
  // Kontrola připojení
  if (!mqttConnected)
  {
    connectToNetwork();
  }
  client.loop();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  char str[length + 1];
  for (int i = 0; i < length; i++)
  {
    str[i] = (char)payload[i];
  }
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.print("deserializeJson() selhalo: ");
    Serial.println(error.c_str());
    return;
  }
  if (doc["on"] != nullptr)
  {
    isDeviceOn = doc["on"];
  }
  if (doc["spectrumRGB"][0] != nullptr)
  {
    redIntensity = doc["spectrumRGB"][0];
  }
  if (doc["spectrumRGB"][1] != nullptr)
  {
    greenIntensity = doc["spectrumRGB"][1];
  }
  if (doc["spectrumRGB"][2] != nullptr)
  {
    blueIntensity = doc["spectrumRGB"][2];
  }
  if (doc["brightArd"] != nullptr)
  {
    ledBrightness = doc["brightArd"];
  }
  if (doc["brightness"] != nullptr)
  {
    indicatorLedBrightness = doc["brightness"];
    indicatorLedBrightness = round(indicatorLedBrightness * 2.54);
  }

  bool Stav;
  if (doc["stav"] != nullptr)
  {
    publishMqttData();
  }
}

void reconnect()
{
  if (!client.connected())
  {
    mqttConnected = false;
  }
}

void publishMqttData()
{

  DynamicJsonDocument doc(1024);
  doc["on"] = isDeviceOn;
  doc["ip"] = WiFi.localIP().toString();
  doc["host"] = WIFI_HOSTNAME;
  doc["signal"] = WiFi.RSSI();
  doc["brightArd"] = ledBrightness;
  if (ledType == 1)
  {
    doc["brightness"] = round(indicatorLedBrightness / 2.54);
  }

  if (ledType == 2)
  {
    JsonArray data = doc.createNestedArray("spectrumRGB");
    data.add(redIntensity);
    data.add(greenIntensity);
    data.add(blueIntensity);
  }
  char out[256];
  serializeJson(doc, out);
  client.publish(deviceCharArray, out);
}

void processButtonInput()
{
  boolean buttonState;                  // Stav stisku tlačítka
  buttonState = digitalRead(buttonPin); // Stisk tlačítka
  if (buttonState == false)
  {
    if (repeatFlag == false)
    {
      isDeviceOn = !isDeviceOn; // Změna stavu zapnutí
      repeatFlag = true;
    }
  }
  else
  {
    repeatFlag = false;
  }
}

void connectToNetwork()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < MAX_WIFI_RECONNECT_ATTEMPTS)
    {
      delay(500);
      wifiAttempts++;
    }
    if (WiFi.status() != WL_CONNECTED)
    {
      ESP.restart(); // Restartuje ESP
    }
  }
  if (!client.connected())
  {
    int mqttAttempts = 0;
    while (!client.connected() && mqttAttempts < MAX_MQTT_RECONNECT_ATTEMPTS)
    {
      if (client.connect(WIFI_HOSTNAME))
      {
        client.subscribe(deviceChr);
      }
      else
      {
        delay(500);
        mqttAttempts++;
      }
    }
    if (!client.connected())
    {
      ESP.restart(); // Restartuje ESP
    }
  }
}