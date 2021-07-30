// TODO: Make the email text nicer. Lighten up the serial debug printing too. Get timestamps to work right.

#include <NTPClient.h>
#include <AlertMe.h>
#include <WiFiUdp.h>
#include <TimeLib.h>


// Pin definitions
#define RESET_PIN D7 // Pin that reset switch is connected to
#define ALARM_PIN D6 // Pin that reads alarm state from the Thermo Locator Plus
#define STATUS_LED D4 // Power status LED

// Create alert and udp comms objects
AlertMe alert;
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionally you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "us.pool.ntp.org", 0, 60000); // UTC Time

// Initialize globals
String subject_line = "ALARM: Low N2 Level";
String message = "";
bool already_alarming = false;
bool alarm_pins = false;
bool connection_success = true;

// Device initialization
void setup() {

  // Start listening to the reset button pins
  pinMode(RESET_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RESET_PIN), isr_handler, FALLING);

  // Set up other pins
  pinMode(ALARM_PIN,INPUT_PULLUP);
  pinMode(STATUS_LED, OUTPUT);
  
  // Status LED on to indicate we've got power
  digitalWrite(STATUS_LED,LOW);

  // Start serial comms
  Serial.begin(250000);
  //Serial.setDebugOutput(true);
  delay(100);
  
  //alert.reset(false);
  //alert.reset(true);
  
  // Print startup messages
  Serial.println("\nLNAlert Cryogenic Liquid Level Remote Alarm"); 
  Serial.println("created by Keiran Cantilina \n\nSoftware version 1.0, hardware revision A \nChip ID: "+String(ESP.getChipId()));
  Serial.print("\nConnecting to WiFi/SMTP...");
  alert.debug(true);                 
  connection_success = alert.connect(true); // Connect to WiFi, then try to connect to SMTP server.
                                            // If we fail with either, or they aren't configured,
                                            // host an AP portal for configuration.
                                            // alert.connect(true) enables printing WiFi debug info.
                                            // If the config AP sits for 3 minutes with no activity, we return FALSE.
  // If the portal has timed out, reboot
  if (!connection_success){
    Serial.println("Config Portal Timeout: Rebooting");
    ESP.restart();
  }

  Serial.println("Connected!");

  // Connect to NTP time servers
  Serial.print("Connecting to time server...");
  timeClient.begin();
  if(timeClient.update()){
    Serial.println("Connected!");
  }
  else{
    Serial.print("Failed! :(");
  }
  Serial.println("Current timestamp is: " + current_timestamp() + "\n");
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
    already_alarming = true;
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
    message = "Low LN2 level alarm sounded at " + current_timestamp() + 
    ".<br>This email was sent from an LNAlert device with Chip ID "+ String(ESP.getChipId()) + 
    ". <br>(Software ver. 1.0, hardware rev. A)";
    Serial.println("Alarm detected! Sending Email...");
    success = alert.send(subject_line, message); // alert.send() returns "SENT" on success, or a specific error message on failure
    Serial.print(success);
  }

  // If sending message fails three times in a row, reboot 
  // (don't worry, message sending will get triggered again if Thermo Locator Plus is still alarming)
  if (success != "SENT"){
    Serial.println("Failed to send alarm message 3 times. Rebooting to hopefully reconnect.");
    ESP.restart();
  }
}


// Get NTP time and convert to timestamp
String current_timestamp(){
  time_t utcCalc = timeClient.getEpochTime();
  return String(hour(utcCalc)) + ":" + String(minute(utcCalc)) + ":" + String(second(utcCalc)) + " UTC on " + String(day(utcCalc)) + " " + monthString(month(utcCalc)) + " " + String(year(utcCalc));
}


// Convert integer month to word month
String monthString(int monthInt){
  String monthStringArray[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
  for (int i = 1; i<13; i++){
    if(monthInt == i){
      return monthStringArray[i-1];
    }
  }
  return "MONTH ERROR";
}


// Interrupt routine that runs when the reset button is pressed
ICACHE_RAM_ATTR void isr_handler() {
  alert.reset(false);
  ESP.restart();
}
