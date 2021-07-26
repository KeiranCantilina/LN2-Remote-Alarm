#include <NTPClient.h>
#include <AlertMe.h>
#include <WiFiUdp.h>

// Pin definitions
#define RESET_PIN D7 // Pin that reset switch is connected to
#define ALARM_PIN D6 // Pin that reads alarm state from the Thermo Locator Plus
#define STATUS_LED D0 // Power status LED

// Create alert and udp comms objects
AlertMe alert;
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionally you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", -3600, 60000); // EST timezone

// Initialize globals
String subject_line = "ALARM: Low N2 Level";
String message = "";
bool already_alarming = false;
bool alarm_pins = false;


// Device initialization
void setup() {

  // Start listening to the reset button pins
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), isr_handler, RISING);

  // Set up other pins
  pinMode(ALARM_PIN,INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  
  // Status LED on to indicate we've got power
  digitalWrite(STATUS_LED,LOW);

  // Start serial comms
  Serial.begin(250000);

  // Print startup messages
  Serial.println("LNAlert Cryogenic Liquid Level Remote Alarm" +
                  "\ncreated by Keiran Cantilina" +
                  
                  "\n\nSoftware version 1.0, hardware revision A" + 
                  "\nChip ID: "+String(ESP.getChipId() + 

                   "\n\nConnecting to WIFI/SMTP...");
                   
  alert.connect(); // Connect to WiFi, then try to connect to SMTP server.
                   // If we fail with either, or they aren't configured,
                   // host an AP portal for configuration.
                   // alert.connect(true) enables printing WiFi debug info.
                   // If the config AP sits for 5 minutes with no activity, we
                   // try to connect again.

  Serial.print("Connected!");

  // Connect to NTP time servers
  timeClient.begin();
  Serial.println("Current timestamp is: " + timeClient.getFormattedTime()+ "\n");
}


// Main program loop
void loop() {
  
  // Check Thermo Locator Plus alarm interface
  if (digitalRead(ALARM_PIN) == LOW){
    alarm_pins = true;
  }

  // If we were already alarming and there's now no alarm, clear alarm status
  if(already_alarming == true && alarm_pins == false){
    already_alarming = false;
  }

  // If we weren't already alarming and now we are, update alarm status and send alarm.
  if (already_alarming == false && alarm_pins == true){
    send_alarm();
    already_alarming = true
  }

  delay(100); // Dunno exactly what the signal from the Thermo Locator Plus looks like, so 
              // this serves as debouncing just in case.
}


// Alarm messaging function
void send_alarm(){

  // Initialize flags
  int attempts = 0;
  String success = "";

  // If we still have attempts left and messaging success has yet to be achieved, try to send message
  while (attempts <3 && success!="SENT"){
    message = "Low LN2 level alarm sounded at " + timeClient.getFormattedTime() + 
    "\nThis was email sent from a LNAlert device with Chip ID "+String(ESP.getChipId()) + 
    ". \n(Software ver. 1.0, hardware rev. A)";
    Serial.println("Alarm detected! Sending Email...");
    success = alert.send(subject_line, message); // alert.send() returns "SENT" on success, or a specific error message on failure
    Serial.print(success);
  }

  // If sending message fails three times in a row, reboot
  if (success != "SENT"){
    Serial.println("Failed to send alarm message 3 times. Rebooting to hopefully reconnect.")
    ESP.restart();
  }
}


// Interrupt routine that runs when the reset button is pressed
ICACHE_RAM_ATTR void isr_handler() {
  alert.reset(bool format == false);
  ESP.restart();
}
