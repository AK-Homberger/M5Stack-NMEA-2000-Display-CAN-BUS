/*
  M5Stack NMEA 2000 display and NMEA 0183 WiFi Gateway
  Reads Data from CAN bus and displays is on the M5Stack module
  Converts NMEA2000 to NMEA0183 and send it to TCP clients via WiFi (port 2222)

  Version 0.3 / 02.03.2021
*/

#define ENABLE_WIFI 0  // Set to 1 to enable M5Stack acts also as NMEA0183 WiFi Gateway. Set to 0 to disable.
#define ENABLE_DEBUG_LOG 0 // Debug log, set to 1 to enable

#define ESP32_CAN_TX_PIN GPIO_NUM_17 // Set CAN TX port to 17 for M5 Stack
#define ESP32_CAN_RX_PIN GPIO_NUM_16  // Set CAN RX port to 16 for M5 Stack

#include <Arduino.h>
#include <Time.h>
#include <sys/time.h>
#include <NMEA2000_CAN.h>  // This will automatically choose right CAN library and create suitable NMEA2000 object
#include <Seasmart.h>
#include <N2kMessages.h>
#include <WiFi.h>
#include <M5Stack.h>
#include <Preferences.h>

#include "N2kDataToNMEA0183.h"
#include "List.h"
#include "BoatData.h"

int NodeAddress;            // To store last Node Address
Preferences preferences;    // Nonvolatile storage on ESP32 - To store LastDeviceAddress

tBoatData BoatData;  // Struct to store Boat Data (from BoatData.h)

// Wifi AP
const char *ssid = "MyM5Stack";
const char *password = "password";

// Put IP address details here
IPAddress local_ip(192, 168, 15, 1);
IPAddress gateway(192, 168, 15, 1);
IPAddress subnet(255, 255, 255, 0);

const uint16_t ServerPort = 2222; // Define the port, where server sends data. Use this e.g. on OpenCPN or Navionics

const size_t MaxClients = 10;
bool SendNMEA0183Conversion = true; // Do we send NMEA2000 -> NMEA0183 conversion
bool SendSeaSmart = false; // Do we send NMEA2000 messages in SeaSmart format

WiFiServer server(ServerPort, MaxClients);

using tWiFiClientPtr = std::shared_ptr<WiFiClient>;
LinkedList<tWiFiClientPtr> clients;

tN2kDataToNMEA0183 tN2kDataToNMEA0183(&NMEA2000, 0);


const unsigned long ReceiveMessages[] PROGMEM = {
  127250L, // Heading
  127258L, // Magnetic variation
  128259UL,// Boat speed
  128267UL,// Depth
  129025UL,// Position
  129026L, // COG and SOG
  129029L, // GNSS
  130306L, // Wind
  128275UL,// Log
  127245UL,// Rudder
  0
};


// Forward declarations
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg);
void SendNMEA0183Message(const tNMEA0183Msg &NMEA0183Msg);

double t = 0;  // Time
int page = 0;  // Initial page to show
int pages = 4; // Number of pages -1
int LCD_Brightness = 250;

long MyTime = 0;  // System Time from NMEA2000
bool TimeSet = false;


void debug_log(char* str) {
#if ENABLE_DEBUG_LOG == 1
  Serial.println(str);
#endif
}

void setup() {
  uint8_t chipid[6];
  uint32_t id = 0;
  int i = 0;

  M5.begin();
  M5.Power.begin();
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  M5.Lcd.setCursor(0, 0, 1);

  ledcDetachPin(SPEAKER_PIN);
  pinMode(SPEAKER_PIN, INPUT);

  Serial.begin(115200); delay(500);

#if ENABLE_WIFI == 1

  // Init wifi connection
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  delay(100);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Start TCP server
  server.begin();

#endif

  // Reserve enough buffer for sending all messages. This does not work on small memory devices like Uno or Mega

  NMEA2000.SetN2kCANMsgBufSize(8);
  NMEA2000.SetN2kCANReceiveFrameBufSize(250);

  esp_efuse_mac_get_default(chipid);
  for (i = 0; i < 6; i++) id += (chipid[i] << (7 * i));

  // Set product information
  NMEA2000.SetProductInformation("1", // Manufacturer's Model serial code
                                 100, // Manufacturer's product code
                                 "M5 Display",  // Manufacturer's Model ID
                                 "1.0.2.25 (2019-07-07)",  // Manufacturer's Software version code
                                 "1.0.2.0 (2019-07-07)" // Manufacturer's Model version
                                );
  // Set device information
  NMEA2000.SetDeviceInformation(id, // Unique number. Use e.g. Serial number. Id is generated from MAC-Address
                                132, // Device function=Analog to NMEA 2000 Gateway. See codes on http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                25, // Device class=Inter/Intranetwork Device. See codes on  http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20&%20function%20codes%20v%202.00.pdf
                                2046 // Just choosen free from code list on http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
                               );

  // If you also want to see all traffic on the bus use N2km_ListenAndNode instead of N2km_NodeOnly below

  NMEA2000.SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text. Leave uncommented for default Actisense format.
  NMEA2000.SetMode(tNMEA2000::N2km_NodeOnly, 32);

  NMEA2000.ExtendReceiveMessages(ReceiveMessages);
  NMEA2000.AttachMsgHandler(&tN2kDataToNMEA0183); // NMEA 2000 -> NMEA 0183 conversion
  NMEA2000.SetMsgHandler(HandleNMEA2000Msg); // Also send all NMEA2000 messages in SeaSmart format

  tN2kDataToNMEA0183.SetSendNMEA0183MessageCallback(SendNMEA0183Message);

  NMEA2000.Open();

  Display_Main();

}


//*****************************************************************************
void SendBufToClients(const char *buf) {
  for (auto it = clients.begin() ; it != clients.end(); it++) {
    if ( (*it) != NULL && (*it)->connected() ) {
      (*it)->println(buf);
    }
  }
}

#define MAX_NMEA2000_MESSAGE_SEASMART_SIZE 500
//*****************************************************************************
//NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &N2kMsg) {

  if ( !SendSeaSmart ) return;

  char buf[MAX_NMEA2000_MESSAGE_SEASMART_SIZE];
  if ( N2kToSeasmart(N2kMsg, millis(), buf, MAX_NMEA2000_MESSAGE_SEASMART_SIZE) == 0 ) return;
  SendBufToClients(buf);
}


#define MAX_NMEA0183_MESSAGE_SIZE 150
//*****************************************************************************
void SendNMEA0183Message(const tNMEA0183Msg &NMEA0183Msg) {
  if ( !SendNMEA0183Conversion ) return;

  char buf[MAX_NMEA0183_MESSAGE_SIZE];
  if ( !NMEA0183Msg.GetMessage(buf, MAX_NMEA0183_MESSAGE_SIZE) ) return;
  SendBufToClients(buf);
}


//*****************************************************************************
void AddClient(WiFiClient &client) {
  Serial.println("New Client.");
  clients.push_back(tWiFiClientPtr(new WiFiClient(client)));
}

//*****************************************************************************
void StopClient(LinkedList<tWiFiClientPtr>::iterator &it) {
  Serial.println("Client Disconnected.");
  (*it)->stop();
  it = clients.erase(it);
}

//*****************************************************************************
void CheckConnections() {
  WiFiClient client = server.available();   // listen for incoming clients

  if ( client ) AddClient(client);

  for (auto it = clients.begin(); it != clients.end(); it++) {
    if ( (*it) != NULL ) {
      if ( !(*it)->connected() ) {
        StopClient(it);
      } else {
        if ( (*it)->available() ) {
          char c = (*it)->read();
          if ( c == 0x03 ) StopClient(it); // Close connection by ctrl-c
        }
      }
    } else {
      it = clients.erase(it); // Should have been erased by StopClient
    }
  }
}





void set_system_time(void) {
  MyTime = (BoatData.DaysSince1970 * 3600 * 24) + BoatData.GPSTime;

  if (MyTime != 0 && !TimeSet) { // Set system time from valid NMEA2000 time (only once)
    struct timeval tv;
    tv.tv_sec = MyTime;
    settimeofday(&tv, NULL);
    TimeSet = true;
  }
}



void loop() {
  M5.update();
  CheckSourceAddressChange();
  
  if (millis() > t + 1000) {
    t = millis();

    if (page == 0) Page_1();
    if (page == 1) Page_2();
    if (page == 2) Page_3();
    if (page == 3) Page_4();
    if (page == 4) Page_5();

    set_system_time();
    DiplayDateTime();
  }


  if (M5.BtnB.wasPressed() == true) {
    page++;                        // Button B pressed -> Next page
    if (page > pages) page = 0;
    t -= 1000;
  }


  if (M5.BtnA.wasPressed() == true) {
    page--;
    if (page < 0) page = pages;
    t -= 1000;
  }



  if (M5.BtnC.wasPressed() == true)                         /* Button C pressed ? --> Change brightness */
  {
    //      M5.Speaker.tone(NOTE_DH2, 1);
    if (LCD_Brightness < 250)                               /* Maximum brightness not reached ? */
    {
      LCD_Brightness = LCD_Brightness + 10;                 /* Increase brightness */
    }
    else                                                    /* Maximum brightness reached ? */
    {
      LCD_Brightness = 10;                                  /* Set brightness to lowest value */
    }
    M5.Lcd.setBrightness(LCD_Brightness);                   /* Change brightness value */
  }

#if ENABLE_WIFI == 1
  CheckConnections();
#endif

  NMEA2000.ParseMessages();
  tN2kDataToNMEA0183.Update(&BoatData);

}




void Page_5(void) {
  char buffer[40];

  M5.Lcd.fillRect(0, 31, 320, 178, 0x439);
  M5.Lcd.setCursor(0, 45, 2);
  M5.Lcd.setTextSize(3);
  sprintf(buffer, " Water %3.0f", BoatData.WaterTemperature);
  M5.Lcd.print(buffer);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print(" o ");
  M5.Lcd.setTextSize(3);
  M5.Lcd.println("C  ");
  sprintf(buffer, " Trip  %4.1f nm", BoatData.TripLog);
  M5.Lcd.print(buffer);
  M5.Lcd.println("  ");
  sprintf(buffer, " Log %5.0f nm", BoatData.Log);
  M5.Lcd.println(buffer);
}



void Page_4(void) {
  char buffer[40];

  M5.Lcd.fillRect(0, 31, 320, 178, 0x439);
  M5.Lcd.setCursor(0, 45, 2);
  M5.Lcd.setTextSize(3);
  sprintf(buffer, " STW   %3.1f kn", BoatData.STW);
  M5.Lcd.print(buffer);
  M5.Lcd.println("  ");
  sprintf(buffer, " Depth %4.1f m", BoatData.WaterDepth);
  M5.Lcd.print(buffer);
  M5.Lcd.println("  ");
  sprintf(buffer, " Rudder %2.1f", BoatData.RudderPosition);
  M5.Lcd.print(buffer);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print(" o ");
  M5.Lcd.setTextSize(3);
}



void Page_3(void) {
  char buffer[40];

  M5.Lcd.fillRect(0, 31, 320, 178, 0x439);
  M5.Lcd.setCursor(0, 45, 2);
  M5.Lcd.setTextSize(3);
  sprintf(buffer, " HDG %03.0f", BoatData.Heading);
  M5.Lcd.print(buffer);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print(" o ");
  M5.Lcd.setTextSize(3);
  M5.Lcd.println("  ");
  sprintf(buffer, " COG %03.0f", BoatData.COG);
  M5.Lcd.print(buffer);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print(" o ");
  M5.Lcd.setTextSize(3);
  M5.Lcd.println("  ");
  sprintf(buffer, " SOG %2.1f kn", BoatData.SOG);
  M5.Lcd.println(buffer);
}

void Page_2(void) {
  char buffer[40];
  double minutes;

  M5.Lcd.fillRect(0, 31, 320, 178, 0x439);
  M5.Lcd.setCursor(0, 40, 2);
  M5.Lcd.setTextSize(2);

  minutes = BoatData.Latitude - trunc(BoatData.Latitude);
  minutes = minutes / 100 * 60;

  M5.Lcd.println("LAT");
  M5.Lcd.setTextSize(3);
  sprintf(buffer, "   %02.0f", trunc(BoatData.Latitude));
  M5.Lcd.print(buffer);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print("o ");
  M5.Lcd.setTextSize(3);
  sprintf(buffer, "%06.3f", minutes * 100);
  M5.Lcd.print(buffer);
  if (BoatData.Latitude > 0 ) M5.Lcd.println("'N"); else M5.Lcd.println("'S");

  minutes = BoatData.Longitude - trunc(BoatData.Longitude);
  minutes = minutes / 100 * 60;

  M5.Lcd.setTextSize(2);
  M5.Lcd.println("LON");
  M5.Lcd.setTextSize(3);
  sprintf(buffer, "  %03.0f", trunc(BoatData.Longitude));
  M5.Lcd.print(buffer);
  M5.Lcd.setTextSize(1);
  M5.Lcd.print("o ");
  M5.Lcd.setTextSize(3);
  sprintf(buffer, "%06.3f", minutes * 100);
  M5.Lcd.print(buffer);
  if (BoatData.Longitude > 0 ) M5.Lcd.println("'E"); else M5.Lcd.println("'W");
}

void Page_1(void) {
  char buffer[40];

  double AWS = 0;

  if (BoatData.AWS > 0) AWS = BoatData.AWS;

  M5.Lcd.fillRect(0, 31, 320, 178, 0x439);
  M5.Lcd.setCursor(0, 50, 2);
  M5.Lcd.setTextSize(4);
  sprintf(buffer, " AWS %3.0f kn", AWS);
  M5.Lcd.println(buffer);
  sprintf(buffer, " MAX %3.0f kn", BoatData.MaxAws);
  M5.Lcd.println(buffer);
}

void DiplayDateTime(void) {
  char buffer[50];

  M5.Lcd.setTextFont(1);
  M5.Lcd.setTextSize(2);                                      /* Set text size to 2 */
  M5.Lcd.setTextColor(WHITE);                                 /* Set text color to white */

  M5.Lcd.fillRect(265, 0, 320, 30, 0x1E9F);

  M5.Lcd.setCursor(265, 7);
  sprintf(buffer, "%3.0d", M5.Power.getBatteryLevel());
  M5.Lcd.print(buffer);
  M5.Lcd.print("%");


  M5.Lcd.fillRect(0, 210, 320, 30, 0x1E9F);
  M5.Lcd.setCursor(10, 218);

  if (MyTime != 0) {

    time_t rawtime = MyTime; // Create time from NMEA 2000 time (UTC)
    struct tm  ts;
    ts = *localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%d.%m.%Y - %H:%M:%S UTC", &ts); // Create directory name from date
    M5.Lcd.print(buffer);
  }

}



void Display_Main (void)
{
  M5.Lcd.setTextFont(1);
  M5.Lcd.fillRect(0, 0, 320, 30, 0x1E9F);                      /* Upper dark blue area */
  M5.Lcd.fillRect(0, 31, 320, 178, 0x439);                   /* Main light blue area */
  M5.Lcd.fillRect(0, 210, 320, 30, 0x1E9F);                    /* Lower dark blue area */
  M5.Lcd.fillRect(0, 30, 320, 4, 0xffff);                     /* Upper white line */
  M5.Lcd.fillRect(0, 208, 320, 4, 0xffff);                    /* Lower white line */
  //  M5.Lcd.fillRect(0, 92, 320, 4, 0xffff);                     /* First vertical white line */
  //  M5.Lcd.fillRect(0, 155, 320, 4, 0xffff);                    /* Second vertical white line */
  //  M5.Lcd.fillRect(158, 30, 4, 178, 0xffff);                   /* First horizontal white line */
  M5.Lcd.setTextSize(2);                                      /* Set text size to 2 */
  M5.Lcd.setTextColor(WHITE);                                 /* Set text color to white */
  M5.Lcd.setCursor(10, 7);                                    /* Display header info */
  M5.Lcd.print("NMEA Display");
  M5.Lcd.setCursor(210, 7);
  M5.Lcd.print("Batt");
}


//*****************************************************************************
// Function to check if SourceAddress has changed (due to address conflict on bus)

void CheckSourceAddressChange() {
  int SourceAddress = NMEA2000.GetN2kSource();

  if (SourceAddress != NodeAddress) { // Save potentially changed Source Address to NVS memory
    NodeAddress = SourceAddress;      // Set new Node Address (to save only once)
    preferences.begin("nvs", false);
    preferences.putInt("LastNodeAddress", SourceAddress);
    preferences.end();
    Serial.printf("Address Change: New Address=%d\n", SourceAddress);
  }
}
