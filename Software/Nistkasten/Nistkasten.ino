/******************************************************************************/
#include "esp_camera.h"
#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP32_FTPClient.h>
#include "soc/soc.h" // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownouz problems
#include "driver/rtc_io.h"
#include <NTPClient.h> // https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/ 
#include <WiFiUdp.h>

#define WIFI_SSID "hbvdr"
#define WIFI_PASS "babybaer"

RTC_DATA_ATTR int bootCount = 0;

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22


// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// define FTP-Server
char ftp_server[] = "FTPSERVER";
char ftp_user[] = "FTPUSER";
char ftp_pass[] = "PASSWORT";

int SleepSeconds = 900;
int SleepMultiplier = 1000000;
long SleepMicros = 1000;
int irLEDs = 12; //Pin for IR-LEDs


// you can pass a FTP timeout and debug mode on the last 2 arguments
ESP32_FTPClient ftp (ftp_server, ftp_user, ftp_pass, 5000, 2);

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
  Serial.begin( 115200 );
  Serial.setDebugOutput(true);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;


  pinMode(irLEDs, OUTPUT);
  digitalWrite(irLEDs, LOW); // turn on LEDs
  Serial.println("LEDs off");

  pinMode(4, INPUT);
  digitalWrite(4, LOW);
  rtc_gpio_hold_dis(GPIO_NUM_4);

  if (psramFound()) {

    Serial.println("Check PSRAM");
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 8;
    config.fb_count = 2;
    Serial.println("PSRAM found");
  } else {

    Serial.println("No PSRAM found");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.begin( WIFI_SSID, WIFI_PASS );

  Serial.println("Connecting Wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  Serial.println("fetching time...");
  delay(2000);
  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT 0 = 0
  timeClient.setTimeOffset(3600);

  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.print("DATE: ");
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.print("HOUR: ");
  Serial.println(timeStamp);


  digitalWrite(irLEDs, HIGH); // turn on LEDs
  Serial.println("LEDs on");
  delay(1000);

  Serial.println("Taking Picture");
  camera_fb_t * fb = NULL;

  // Take Picture
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
  // If Error generate textfile
  String path = "/error_taking_pic" + (dayStamp) + "-" + (timeStamp) + ".inf";

  // Write Picture to FTP
  ftp.OpenConnection();
  delay(500);
  ftp.InitFile("Type I");
  ftp.NewFile(path.c_str());
  ftp.WriteData( fb->buf, fb->len ); 
  ftp.CloseFile();
  ftp.CloseConnection();
    return;
  }

  digitalWrite(irLEDs, LOW); // turn on LEDs
  Serial.println("LEDs off");

  // Filename generation
  String path = "/date_" + (dayStamp) + "_time_" + (timeStamp) + ".jpg";

  ftp.OpenConnection();
  delay(500);
  ftp.InitFile("Type I");
  ftp.NewFile(path.c_str());
  ftp.WriteData( fb->buf, fb->len );
  ftp.CloseFile();
  ftp.CloseConnection();
  Serial.printf("Picture file name: %s\n", path.c_str());

  esp_camera_fb_return(fb);

  delay(1000);

  rtc_gpio_hold_en(GPIO_NUM_4);

  SleepMicros = SleepSeconds * SleepMultiplier;

  esp_sleep_enable_timer_wakeup(SleepMicros);

  Serial.print("Going to sleep for ");
  Serial.print(SleepSeconds);
  Serial.print(" seconds, good night!");

  delay(1000);
  esp_deep_sleep_start();
  Serial.println("This will never be printed, so sad");
}

void loop()
{
}
