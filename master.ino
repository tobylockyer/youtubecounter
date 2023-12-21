#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <WiFiManager.h> // Include the WiFiManager library

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET     -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char* apiKey = "AIzaSyC5Mvij0Kzv3Vgo_1CE6WMZ5O4rKSiQ3Ps"; // Replace with your actual API Key

Preferences preferences;




// Define a custom parameter for channel name
WiFiManagerParameter* custom_channel_name;

//WiFiManagerParameter custom_channel_name("channel", "YouTube Channel Name", "", 40);
#define RESET_PIN 13 // Change this to the GPIO pin connected to your reset button


String channelId;
String channelName; // Declare channelName as a global variable

// Define a custom parameter for channel name


void setup() {
  Serial.begin(115200);
  WiFiManager wifiManager;
  // Inside the setup function, before initializing the WiFiManager
  preferences.begin("yt-app", true); // Read-only
  String storedChannelName = preferences.getString("channelName", "");
  if (storedChannelName != "") {
      channelName = storedChannelName;
  }
  custom_channel_name = new WiFiManagerParameter("channel", "YouTube Channel Name", channelName.c_str(), 40);
  wifiManager.addParameter(custom_channel_name);

  preferences.end();



  pinMode(RESET_PIN, INPUT_PULLUP); // Initialize the reset pin with internal pull-up
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  // Uncomment the next line if you want to reset WiFi settings every time

  // Update display to show WiFi setup message
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("WiFi-Setup");
  display.display();

  //wifiManager.resetSettings();

  wifiManager.autoConnect("YouTubeCounter");

  // Correctly assign the value to the global variable
  channelName = custom_channel_name->getValue();
  preferences.begin("yt-app", false);
  preferences.putString("channelName", channelName);
  preferences.end();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Fetching Data...");
  display.display();
}


String getChannelId(String channelName) {
  HTTPClient http;
  String searchURL = "https://www.googleapis.com/youtube/v3/search?part=snippet&q=" + channelName + "&type=channel&key=" + apiKey;
  Serial.println(searchURL);
  http.begin(searchURL);
  int httpCode = http.GET();

  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Search Payload: " + payload); // Debug
    Serial.print("Payload length: ");
    Serial.println(payload.length()); // Print the length of the payload

    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);

    if(doc.containsKey("items") && doc["items"].as<JsonArray>().size() > 0) {
      String channelId = doc["items"][0]["id"]["channelId"].as<String>();
      return channelId;
    } else {
      Serial.println("No items found in search response"); // Debug
    }
  } else {
    Serial.print("Search API Error: ");
    Serial.println(httpCode); // Debug
  }
  http.end();
  return String("");
  delete custom_channel_name;

}

void loop() {

  bool currentButtonState = digitalRead(RESET_PIN);
  if (currentButtonState == LOW) {
    Serial.println("Button pressed - Resetting WiFi settings");
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    preferences.begin("yt-app", false);
    preferences.remove("channelName");
    preferences.end();
    delete custom_channel_name;
    ESP.restart(); // Restart the ESP32
    delay(1000); // Short delay to allow serial print before reset
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (channelId.length() == 0) {
      channelId = getChannelId(channelName);
      Serial.println("Channel ID: " + channelId);
    }

    if(channelId.length() > 0) {
      HTTPClient http;
      String apiURL = "https://www.googleapis.com/youtube/v3/channels?part=statistics,snippet&id=" + channelId + "&key=" + apiKey;
      http.begin(apiURL);
      int httpCode = http.GET();

      if (httpCode > 0) {
        String payload = http.getString();
        Serial.println("Channel Info Payload: " + payload); 

        // Filter to reduce the memory footprint
        StaticJsonDocument<200> filter;
        filter["items"][0]["statistics"]["subscriberCount"] = true;

        // Smaller DynamicJsonDocument due to filtering
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        if (doc.containsKey("items")) {
          long subscriberCount = doc["items"][0]["statistics"]["subscriberCount"].as<long>();
          Serial.println("Subscriber Count: " + String(subscriberCount));

          display.clearDisplay();
          display.setTextSize(3);
          display.setCursor(0,0);
          display.println(subscriberCount);
          display.display();
        } else {
          Serial.println("No items found in channel response");
        }
      } else {
        Serial.print("Channel API Error: ");
        Serial.println(httpCode);
      }
      http.end();
    } else {
      Serial.println("Failed to get Channel ID");
    }
  } else {
    Serial.println("WiFi Not Connected");
  }
  delete custom_channel_name;
  delay(300000); // Check every 5 minutes
}
