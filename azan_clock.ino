#include <UTFT.h>
#include <UTFT_SdRaw.h>
#include <SPI.h>
#include <SdFat.h>
#include "WiFiEsp.h"
#include "WiFiEspUdp.h"
#include <Wire.h>
#include "RTClib.h"
#include "Timer.h"
#include <stdio.h>
#include "PrayerTimes.h"
#include <SmoothADC.h>
#include "MapFloat.h"
#include <TrueRandom.h>

SmoothADC    ADC_14;       // SmoothADC instance for Pin 0
Timer t;
UTFT myGLCD(R61581, 38, 39, 40, 41); // 480x320 pixels
UTFT_SdRaw myFiles(&myGLCD);
RTC_DS3231 rtc;
SdFat sd;
SdFile file;
WiFiEspUDP Udp;
DateTime now;

unsigned int  ADC0Value;    // ADC0 final value
float battvolt;
float battpercent;
extern unsigned int side[16000];
extern uint8_t Sinclair_M[];
extern uint8_t Sinclair_S[];
extern uint8_t Grotesk32x64[];
//extern uint8_t segment18_XXL[];
//extern uint8_t SevenSegmentFull[];
//extern uint8_t mykefont2[];
//extern uint8_t Retro8x16[];
//extern uint8_t Ubuntu[];
char daysOfTheWeek[7][12] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
char ssid[] = "HK SG Note 9";    // your network SSID (name)
char pass[] = "07081989";        // your network password
int status = WL_IDLE_STATUS;     // the Wifi radio's status
char timeServer[] = "pool.ntp.org";  // NTP server
unsigned int localPort = 2390;       // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;  // NTP timestamp is in the first 48 bytes of the message
const int UDP_TIMEOUT = 2000;    // timeout in miliseconds to wait for an UDP packet to arrive
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets
unsigned long epoch;
int recievedpacketsize;
int n = 0;
String dateday;
String datemonth;
float salatLatitude = 30.0242081; //New Cairo
float salatLongitude = 31.4381218; //New Cairo
int salatTimezone = 2;  //New Cairo
double salatTimes[sizeof(TimeName) / sizeof(char*)];
int salatHours;
int salatMinutes;
int currentSalatID;
int previousSalatID;
int line = 100;
int hoursIn12hr;

#define button 13

void setup()
{
  set_calc_method(Egypt);
  set_asr_method(Shafii);
  set_high_lats_adjust_method(AngleBased);
  set_fajr_angle(19.5);
  set_isha_angle(17.5);

  ADC_14.init(A14, TB_MS, 500);  // Init ADC0 attached to A0 with a 50ms acquisition period
  if (ADC_14.isDisabled())  {
    ADC_14.enable();
  }

  pinMode(button, INPUT);
  Serial.begin(115200);

  myGLCD.InitLCD();
  myGLCD.fillScr(0, 0, 0);
  myGLCD.setBackColor(0, 0, 0);
  myGLCD.setColor(255, 255, 255);
  myGLCD.setFont(Sinclair_M);
  myGLCD.print("BEDSIDE NIGHT CLOCK", CENTER, 5);
  myGLCD.setFont(Sinclair_S);
  myGLCD.print("with PRAYER TIMES", CENTER, 24);
  myGLCD.drawLine(68, 35, 420, 35);
  myGLCD.print("Adruino Mega 2560 + ESP8266 WiFi + DS3231 RTC +", CENTER, 40);
  myGLCD.print("R61581 3.5inch LCD + LM2590 DC/DC Converter + MicroSD Slot", CENTER, 51);
  myGLCD.print("Design & Execution: HK89", CENTER, 70);
  myGLCD.print("Build Date: OCT 15, 2018", CENTER, 81);
  if (! rtc.begin()) {
    Serial.println("RTC initialization failed! System can not continue!");
    line = line + 11;
    myGLCD.setColor(255, 0, 0);
    myGLCD.print("> RTC initialization failed! System can not continue!", LEFT, line);
    while (1);
  }
  else {
    Serial.println("RTC initialized!");
    myGLCD.print("> RTC initialized!", LEFT, line);
  }
  if (rtc.lostPower()) {
    line = line + 11;
    myGLCD.setColor(255, 0, 0);
    myGLCD.print("> RTC lost time/date! Replace CR2032 battery.", LEFT, line);
    line = line + 11;
    myGLCD.setColor(255, 255, 255);
    myGLCD.print("> Setting RTC module time from WiFi now!", LEFT, line);
    Serial.println("RTC lost power! Replace CR2032 battery. Setting RTC from WiFi now!");
    SETtimeWiFi();
  }
  else {
    line = line + 11;
    myGLCD.print("> Hold button to set time using WiFi ...", LEFT, line);
    line = line + 11;
    myGLCD.print(">  3", LEFT, line);
    delay(1000);
    myGLCD.print("> . 2", LEFT, line);
    delay(1000);
    myGLCD.print("> .. 1", LEFT, line);
    delay(1000);
    if (digitalRead(button)) {
      myGLCD.print("> Button held! Proceeding ...", LEFT, line);
      delay(1500);
      SETtimeWiFi();
    }
    else {
      myGLCD.print("> Proceeding to slideshow ...", LEFT, line);
      delay(500);
    }
  }
  line = line + 11;
  myGLCD.print("> Initializing SD card", LEFT, line);
  line = line + 11;
  myGLCD.print("  Please convert pictures to .RAW format and place in root", LEFT, line);
  line = line + 11;
  myGLCD.print("  directory of the SD card with their sequential filenames", LEFT, line);
  line = line + 11;
  myGLCD.print("  as IMG_<picture number>.RAW", LEFT, line);
  line = line + 11;
  //sd.begin(53, SPI_FULL_SPEED);
  if (sd.begin(53, SD_SCK_MHZ(50))) {
    myGLCD.print("> SD card initialized at 50MHz clock speed", LEFT, line);
    Serial.println("SD card initialized at 50MHz clock speed");
    while (file.openNext(sd.vwd(), O_READ)) {
      n++;
      file.close();
    }
    line = line + 11;
    n--;  // to discount the System Volume Information file
    if (n > 0) {
      Serial.print("Found ");
      Serial.print(n);
      Serial.println(" image file(s) on SD card");
      myGLCD.print("> Found " + String(n) + " image file(s) on SD card. Starting slideshow ...", LEFT, line);
      delay(3500);
      myGLCD.fillScr(0, 0, 0);
      drawrandompic();
      t.every(15000, drawrandompic, 0);
    }
    if (n <= 0) {
      Serial.println("ERROR! No files present!");
      myGLCD.setColor(255, 0, 0);
      myGLCD.print("> ERROR! No files present!", LEFT, line);
      myGLCD.setColor(255, 255, 255);
      delay(3500);
      myGLCD.fillScr(0, 0, 0);
    }
  }
  else {
    Serial.println("SD card initialization failed! No slideshow!");
    myGLCD.setColor(255, 0, 0);
    myGLCD.print("> SD card initialization failed! No slideshow!", LEFT, line);
    myGLCD.setColor(255, 255, 255);
    delay(3500);
    myGLCD.fillScr(0, 0, 0);
    myGLCD.drawBitmap (0, 0, 50, 320, side);
    myGLCD.drawBitmap (430, 0, 50, 320, side);
  }
  t.every(500, lcdupdatetime, 0);
}


void loop()
{
  t.update();
  now = rtc.now();
  ADC_14.serviceADCPin();
  ADC0Value = ADC_14.getADCVal();
  battvolt = (ADC0Value / 1024.0) * 5.00 * 12 / 3.837;
  battpercent = mapFloat(battvolt, 9.0, 12.6, 0.0, 99.0);
  if (battpercent < 0) {
    battpercent = 0;
  }
  //Serial.print(battvolt);
  //Serial.print('\t');
  //Serial.print(battpercent);
  //Serial.print('\t');
  //Serial.println(rtc.getTemperature());
}

void lcdupdatetime() {
  if (n != 0) {
    if (now.hour() > 0 && now.hour() <= 12) {
      hoursIn12hr = now.hour();
    }
    if (now.hour() > 12 && now.hour() <= 23) {
      hoursIn12hr = now.hour() - 12;
    }
    if (now.hour() == 00) {
      hoursIn12hr = 12;
    }
    myGLCD.setBackColor(0, 0, 0);
    myGLCD.setColor(255, 255, 255);
    myGLCD.drawLine(0, 300, 478, 300);
    myGLCD.setFont(Sinclair_M);
    myGLCD.print(daysOfTheWeek[now.dayOfTheWeek()], LEFT, 302);
    myGLCD.printNumI(now.day(), 60, 302, 2, '0');
    myGLCD.print("/", 92, 302);
    myGLCD.printNumI(now.month(), 110, 302, 2, '0');
    //myGLCD.print("/", 142, 302);
    //myGLCD.printNumI(now.year() - 2000, 160, 302, 2, '0');
    myGLCD.printNumI(hoursIn12hr, 359, 302, 2, '0');
    myGLCD.print(":", 389, 302);
    myGLCD.printNumI(now.minute(), 408, 302, 2, '0');
    //myGLCD.print(":", 390, 302);
    //myGLCD.printNumI(now.second(), 405, 302, 2, '0');
    if (now.hour() >= 0 && now.hour() < 12) {
      myGLCD.print("AM", RIGHT, 302);
    }
    else {
      myGLCD.print("PM", RIGHT, 302);
    }
    myGLCD.setFont(Sinclair_S);
    myGLCD.print("AMBIENT TEMP.:", 155, 302);
    myGLCD.print("BATTERY LEVEL:", 170, 311);
    myGLCD.printNumF(rtc.getTemperature(), 1, 270, 302);
    myGLCD.printNumI(battpercent, 285, 311, 2, '0');
    myGLCD.print("degC", 305, 302);
    myGLCD.print("%", 305, 311);
  }
  else {
    myGLCD.setBackColor(0, 0, 0);
    myGLCD.setColor(255, 255, 255);
    myGLCD.setFont(Sinclair_S);
    myGLCD.print("AMBIENT TEMPERATURE: ", 123, 260);
    myGLCD.printNumF(rtc.getTemperature(), 1, 293, 260);
    myGLCD.print("degC", 328, 260);
    myGLCD.print("LONG 30.0242081, LAT 31.4381218, SHAAFI", CENTER, 285);
    myGLCD.print("EGYPTIAN GENERAL AUTHORITY OF SURVEY", CENTER, 296);
    myGLCD.print("FAJR ANGLE 19.5, ISHA ANGLE 17.5", CENTER, 307);

    //---------------------------Battery------------------------------
    myGLCD.drawRect(400, 5, 425, 18);
    myGLCD.fillRect(425, 8, 427, 15);
    myGLCD.fillRect(403, 8, map(battpercent, 0, 99, 404, 422), 15);
    myGLCD.setColor(0, 0, 0);
    myGLCD.fillRect(map(battpercent, 0, 99, 404, 422), 8, 422, 15);
    myGLCD.setColor(255, 255, 255);
    myGLCD.printNumI(battpercent, 402, 23, 2, '0');
    myGLCD.print("%", 417, 23);

    //--------------------------Time/Date-----------------------------

    if (now.hour() > 0 && now.hour() <= 12) {
      hoursIn12hr = now.hour();
    }
    if (now.hour() > 12 && now.hour() <= 23) {
      hoursIn12hr = now.hour() - 12;
    }
    if (now.hour() == 00) {
      hoursIn12hr = 12;
    }
    myGLCD.setFont(Sinclair_M);
    myGLCD.print(daysOfTheWeek[now.dayOfTheWeek()], 125, 10);
    myGLCD.printNumI(now.day(), 193, 10, 2, '0');
    myGLCD.print("/", 225, 10);
    myGLCD.printNumI(now.month(), 243, 10, 2, '0');
    myGLCD.print("/", 275, 10);
    myGLCD.printNumI(now.year(), 293, 10, 2, '0');
    myGLCD.setFont(Grotesk32x64);
    myGLCD.printNumI(hoursIn12hr, 70, 35, 2, '0');
    myGLCD.print(":", 135, 35);
    myGLCD.printNumI(now.minute(), 170, 35, 2, '0');
    myGLCD.print(":", 235, 35);
    myGLCD.printNumI(now.second(), 270, 35, 2, '0');
    if (now.hour() >= 0 && now.hour() < 12) {
      myGLCD.print("AM", 347, 35);
    }
    else {
      myGLCD.print("PM", 347, 35);
    }
    myGLCD.setFont(Sinclair_S);
    myGLCD.print("TIMEZONE: CAIRO (UTC+02:00)", CENTER, 102);

    //---------------------------Prayer Times----------------------------

    myGLCD.setFont(Sinclair_M);
    previousSalatID = currentSalatID;
    get_prayer_times(now.year(), now.month(), now.day(), salatLatitude, salatLongitude, salatTimezone, salatTimes);
    for (int i = 0; i < sizeof(salatTimes); i++) {
      if (i == 4) {
        continue;
      }
      if ((now.hour() + now.minute() / 60.00) > salatTimes[i] && (now.hour() + now.minute() / 60.00) <= salatTimes[i + 1]) {
        currentSalatID = i + 1;
        if (currentSalatID == 4) {
          currentSalatID = 5;
        }
        break;
      }
    }

    //Serial.println(currentSalatID);

    if (currentSalatID != previousSalatID) {
      myGLCD.setColor(0, 0, 0);
      myGLCD.fillRect(80, 126, 400, 250); // blank screen for prayer times
    }
    myGLCD.setColor(192, 173, 124);
    switch (currentSalatID) {
      case 0:
        myGLCD.drawRoundRect(80, 128, 400, 148);
        delay(400);
        myGLCD.setColor(10, 10, 10);
        myGLCD.drawRoundRect(80, 128, 400, 148);
        break;
      case 1:
        myGLCD.drawRoundRect(80, 148, 400, 168);
        delay(400);
        myGLCD.setColor(10, 10, 10);
        myGLCD.drawRoundRect(80, 148, 400, 168);
        break;
      case 2:
        myGLCD.drawRoundRect(80, 168, 400, 188);
        delay(400);
        myGLCD.setColor(10, 10, 10);
        myGLCD.drawRoundRect(80, 168, 400, 188);
        break;
      case 3:
        myGLCD.drawRoundRect(80, 188, 400, 208);
        delay(400);
        myGLCD.setColor(10, 10, 10);
        myGLCD.drawRoundRect(80, 188, 400, 208);
        break;
      case 5:
        myGLCD.drawRoundRect(80, 208, 400, 228);
        delay(400);
        myGLCD.setColor(10, 10, 10);
        myGLCD.drawRoundRect(80, 208, 400, 228);
        break;
      case 6:
        myGLCD.drawRoundRect(80, 228, 400, 248);
        delay(400);
        myGLCD.setColor(10, 10, 10);
        myGLCD.drawRoundRect(80, 228, 400, 248);
        break;
      default:
        myGLCD.drawRoundRect(80, 128, 400, 148);
        delay(400);
        myGLCD.setColor(10, 10, 10);
        myGLCD.drawRoundRect(80, 128, 400, 148);
        break;
    }

    myGLCD.setColor(255, 255, 255);
    if ((now.hour() + now.minute() / 60.00) > salatTimes[6]) {
      get_prayer_times(now.year(), now.month(), now.day() + 1, salatLatitude, salatLongitude, salatTimezone, salatTimes);
    }

    //---------------FAJR----------------
    get_float_time_parts(salatTimes[0], salatHours, salatMinutes);
    myGLCD.print(TimeName[0], 87, 130);
    if (salatHours > 12) {
      salatHours = salatHours - 12;
      myGLCD.print("PM", 365, 130);
    }
    else {
      if (salatHours == 12) {
        myGLCD.print("PM", 365, 130);
      }
      else {
        myGLCD.print("AM", 365, 130);
      }
    }
    myGLCD.printNumI(salatHours, 275, 130, 2, '0');
    myGLCD.print(":", 305, 130);
    myGLCD.printNumI(salatMinutes, 325, 130, 2, '0');

    //---------------SUNRISE----------------
    get_float_time_parts(salatTimes[1], salatHours, salatMinutes);
    myGLCD.print(TimeName[1], 87, 150);
    if (salatHours > 12) {
      salatHours = salatHours - 12;
      myGLCD.print("PM", 365, 150);
    }
    else {
      if (salatHours == 12) {
        myGLCD.print("PM", 365, 150);
      }
      else {
        myGLCD.print("AM", 365, 150);
      }
    }
    myGLCD.printNumI(salatHours, 275, 150, 2, '0');
    myGLCD.print(":", 305, 150);
    myGLCD.printNumI(salatMinutes, 325, 150, 2, '0');

    //---------------DHUHR----------------
    get_float_time_parts(salatTimes[2], salatHours, salatMinutes);
    myGLCD.print(TimeName[2], 87, 170);
    if (salatHours > 12) {
      salatHours = salatHours - 12;
      myGLCD.print("PM", 365, 170);
    }
    else {
      if (salatHours == 12) {
        myGLCD.print("PM", 365, 170);
      }
      else {
        myGLCD.print("AM", 365, 170);
      }
    }
    myGLCD.printNumI(salatHours, 275, 170, 2, '0');
    myGLCD.print(":", 305, 170);
    myGLCD.printNumI(salatMinutes, 325, 170, 2, '0');

    //---------------ASR----------------
    get_float_time_parts(salatTimes[3], salatHours, salatMinutes);
    myGLCD.print(TimeName[3], 87, 190);
    if (salatHours > 12) {
      salatHours = salatHours - 12;
      myGLCD.print("PM", 365, 190);
    }
    else {
      if (salatHours == 12) {
        myGLCD.print("PM", 365, 190);
      }
      else {
        myGLCD.print("AM", 365, 190);
      }
    }
    myGLCD.printNumI(salatHours, 275, 190, 2, '0');
    myGLCD.print(":", 305, 190);
    myGLCD.printNumI(salatMinutes, 325, 190, 2, '0');

    //---------------MAGHRIB----------------
    get_float_time_parts(salatTimes[5], salatHours, salatMinutes);
    myGLCD.print(TimeName[5], 87, 210);
    if (salatHours > 12) {
      salatHours = salatHours - 12;
      myGLCD.print("PM", 365, 210);
    }
    else {
      if (salatHours == 12) {
        myGLCD.print("PM", 365, 210);
      }
      else {
        myGLCD.print("AM", 365, 210);
      }
    }
    myGLCD.printNumI(salatHours, 275, 210, 2, '0');
    myGLCD.print(":", 305, 210);
    myGLCD.printNumI(salatMinutes, 325, 210, 2, '0');

    //---------------ISHA----------------
    get_float_time_parts(salatTimes[6], salatHours, salatMinutes);
    myGLCD.print(TimeName[6], 87, 230);
    if (salatHours > 12) {
      salatHours = salatHours - 12;
      myGLCD.print("PM", 365, 230);
    }
    else {
      if (salatHours == 12) {
        myGLCD.print("PM", 365, 230);
      }
      else {
        myGLCD.print("AM", 365, 230);
      }
    }
    myGLCD.printNumI(salatHours, 275, 230, 2, '0');
    myGLCD.print(":", 305, 230);
    myGLCD.printNumI(salatMinutes, 325, 230, 2, '0');
  }
}

void drawrandompic() {
  if (n != 0) {
    //myGLCD.setBackColor(0, 0, 0);
    //myGLCD.setColor(255, 255, 255);
    myGLCD.setFont(Sinclair_S);
    uint16_t random_filenumber = TrueRandom.random(1, n + 1);
    String filename = "IMG_" + String(random_filenumber) + ".RAW";
    int strLen = filename.length(); // Get the length
    char *img = (char *)malloc(strLen + 1); //Allocate space to hold the data
    filename.toCharArray(img, strLen + 1);
    myFiles.load(0, 0, 480, 300, img, 1, 0);
    String imagenumberstring = " " + String(random_filenumber) + "/" + String(n) + " ";
    //myGLCD.setBackColor(0, 0, 0);
    //myGLCD.setColor(255, 255, 255);
    myGLCD.print(String(imagenumberstring), CENTER, 288);
  }
}

void GETtimeWiFi(char *ntpSrv) // send an NTP request to the time server at the given address
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)

  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(ntpSrv, 123); //NTP requests are to port 123

  Udp.write(packetBuffer, NTP_PACKET_SIZE);

  Udp.endPacket();

  // wait for a reply for UDP_TIMEOUT miliseconds
  unsigned long startMs = millis();
  while (!Udp.available() && (millis() - startMs) < UDP_TIMEOUT) {}
  recievedpacketsize = Udp.parsePacket();
  Serial.print("Recieved Packet Size: ");
  Serial.println(recievedpacketsize);
  if (Udp.parsePacket()) {
    // We've received a packet, read the data from it into the buffer
    Udp.read(packetBuffer, NTP_PACKET_SIZE);

    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800
    // subtract seventy years:
    epoch = secsSince1900 - 2208988800UL;
    Serial.print("Unix time: ");
    Serial.print(epoch);
    // print the hour, minute and second:
    Serial.print(", UTC time: ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if (((epoch % 3600) / 60) < 10) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ((epoch % 60) < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print(epoch % 60); // print the second

    Serial.print(", Region time: ");       // Region time
    epoch = epoch + 7200UL;  // adding two hours for timezone
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if (((epoch % 3600) / 60) < 10) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ((epoch % 60) < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second  }
  }
}

void SETtimeWiFi() {
  Serial3.begin(115200);
  line = line + 11;
  myGLCD.print("> Initializing ESP WiFi module", LEFT, line);
  line = line + 11;
  WiFi.init(&Serial3);
  myGLCD.print("> ESP WiFi module initialized! - v" + String(WiFi.firmwareVersion()), LEFT, line);
  int connectcounter = 0;
  // attempt to connect to WiFi network
  line = line + 11;
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    myGLCD.print("> Connecting to: " + String(ssid) + " ... " + String(connectcounter) + " retrial(s)" , LEFT, line);
    status = WiFi.begin(ssid, pass);
    connectcounter++;
    if (connectcounter == 5) {
      line = line + 11;
      myGLCD.setColor(255, 0, 0);
      myGLCD.print("> Failed connecting to: " + String(ssid) + " after 5 retrials!", LEFT, line);
      myGLCD.setColor(255, 255, 255);
      line = line + 11;
      if (rtc.lostPower()) {
        myGLCD.setColor(255, 0, 0);
        myGLCD.print("> System time undefined! System can not continue!", LEFT, line);
        myGLCD.setColor(255, 255, 255);
        while (1);
      }
      else {
        myGLCD.setColor(255, 0, 0);
        myGLCD.print("> Could not set time for RTC module!", LEFT, line);
        myGLCD.setColor(255, 255, 255);
      }
      delay(5000);
      WiFioff();
      goto WiFiout;
    }
  }
  //uint32_t ip;
  //ip = (uint32_t) WiFi.localIP();
  //char *buf;
  //sprintf(buf, "%u.%u.%u.%u", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
  //Serial.println(buf);
  line = line + 11;
  myGLCD.print("> WiFi connected!", LEFT, line);

  Udp.begin(localPort);

  line = line + 11;
  for (int i = 0; i < 10; i++) {
    myGLCD.print("> Getting time from WiFi ... " + String(i) + " retrial(s)", LEFT, line);
    if (recievedpacketsize != 0) {
      line = line + 11;
      myGLCD.print("> Got time from WiFi - Epoch: " + String(epoch), LEFT, line);
      rtc.adjust(DateTime(epoch));
      now = rtc.now();
      delay(1000);
      line = line + 11;
      myGLCD.print("> RTC module time set from WiFi", LEFT, line);
      delay(1000);
      WiFioff();
      goto WiFiout;
    }
    else {
      GETtimeWiFi(timeServer); // send an NTP packet to a time server
    }
  }
  line = line + 11;
  myGLCD.setColor(255, 0, 0);
  myGLCD.print("> Could not get time from WiFi after 10 retrials!", LEFT, line);
  myGLCD.setColor(255, 255, 255);
  line = line + 11;
  if (rtc.lostPower()) {
    myGLCD.setColor(255, 0, 0);
    myGLCD.print("> System time undefined! System can not continue!", LEFT, line);
    myGLCD.setColor(255, 255, 255);
    while (1);
  }
  else {
    myGLCD.setColor(255, 0, 0);
    myGLCD.print("> Could not set time for RTC module!", LEFT, line);
    myGLCD.setColor(255, 255, 255);
  }
  WiFioff();
WiFiout:
  delay(2);
}

void WiFioff() {
  Serial.println("Shutting down WiFi ...");
  line = line + 11;
  myGLCD.print("> Shutting down WiFi ...", LEFT, line);
  status = WiFi.disconnect();
  Serial3.print("AT+GSLP = 0");
  Serial3.end();
}
