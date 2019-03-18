#include <Adafruit_SSD1306.h>
#include <splash.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <OneButton.h>
#include <Wire.h>
#include "esp_camera.h"
#include "esp_wifi.h"
#include <Constellation.h>
#include "img_converters.h"

#define ENABLE_IP5306

#define WIFI_SSID   "your wifi ssid"
#define WIFI_PASSWD "you wifi password"

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

#define PWDN_GPIO_NUM 26
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 32
#define SIOD_GPIO_NUM 13
#define SIOC_GPIO_NUM 12

#define Y9_GPIO_NUM 39
#define Y8_GPIO_NUM 36
#define Y7_GPIO_NUM 23
#define Y6_GPIO_NUM 18
#define Y5_GPIO_NUM 15
#define Y4_GPIO_NUM 4
#define Y3_GPIO_NUM 14
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 27
#define HREF_GPIO_NUM 25
#define PCLK_GPIO_NUM 19

#define I2C_SDA 21
#define I2C_SCL 22

static camera_config_t camera_config = {
    .ledc_channel = LEDC_CHANNEL_0;
    .ledc_timer = LEDC_TIMER_0;
    .pin_d0 = Y2_GPIO_NUM;
    .pin_d1 = Y3_GPIO_NUM;
    .pin_d2 = Y4_GPIO_NUM;
    .pin_d3 = Y5_GPIO_NUM;
    .pin_d4 = Y6_GPIO_NUM;
    .pin_d5 = Y7_GPIO_NUM;
    .pin_d6 = Y8_GPIO_NUM;
    .pin_d7 = Y9_GPIO_NUM;
    .pin_xclk = XCLK_GPIO_NUM;
    .pin_pclk = PCLK_GPIO_NUM;
    .pin_vsync = VSYNC_GPIO_NUM;
    .pin_href = HREF_GPIO_NUM;
    .pin_sscb_sda = SIOD_GPIO_NUM;
    .pin_sscb_scl = SIOC_GPIO_NUM;
    .pin_pwdn = PWDN_GPIO_NUM;
    .pin_reset = RESET_GPIO_NUM;
    .xclk_freq_hz = 20000000;
    .pixel_format = PIXFORMAT_JPEG;//YUV422,GRAYSCALE,RGB565,JPEG
    //init with high specs to pre-allocate larger buffers
    .frame_size = FRAMESIZE_UXGA;//QQVGA-UXGA Do not use sizes above QVGA when not JPEG
    .jpeg_quality = 10;//0-63 lower number means higher quality
    .fb_count = 1;
}

#define SSD1306_ADDRESS 0x3c
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define AS312_PIN 33
#define BUTTON_1 34
String ip;

OneButton button1(BUTTON_1, true);
bool SinglePress = false;
bool LongPress = false;

#define IP5306_ADDR 0X75
#define IP5306_REG_SYS_CTL0 0x00
/* Create the Constellation client */
Constellation<WiFiClient> constellation("IP_or_DNS_CONSTELLATION_SERVER", 8088, "YOUR_SENTINEL_NAME", "YOUR_PACKAGE_NAME", "YOUR_ACCESS_KEY");

#ifdef ENABLE_IP5306
bool setPowerBoostKeepOn(int en)
{
    Wire.beginTransmission(IP5306_ADDR);
    Wire.write(IP5306_REG_SYS_CTL0);
    if (en)
        Wire.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
    else
        Wire.write(0x35); // 0x37 is default reg value
    return Wire.endTransmission() == 0;
}
#endif

void buttonClick()
{
    SinglePress = !SinglePress;
    constellation.pushStateObject("SinglePress", SinglePress);
}

void buttonLongPress()
{
    LongPress = !LongPress;
    constellation.pushStateObject("LongPress", LongPress);
}

void setup()
{
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();

    pinMode(AS312_PIN, INPUT);

    Wire.begin(I2C_SDA, I2C_SCL);
    
#ifdef ENABLE_IP5306
    bool   isOk = setPowerBoostKeepOn(1);
    String info = "IP5306 KeepOn " + String((isOk ? "PASS" : "FAIL"));
#endif

    if(!display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDRESS))
    {
        Serial.println(F("SSD1306 allocation failled"));
    }

    display.display();
    delay(50);
    display.clearDisplay();
    display.SetTextSize(2);
    display.setTextColor(WHITE);
    display.SetCursor(10, 0);
    display.println("TTGO Camera")
    display.display();

#ifdef ENABLE_IP5306
    display.clearDisplay();
    delay(1000);
    display.SetCursor(10, 0);
    display.println(info);
    delay(1000);
#endif

    camera_init();

/*
    //drop down frame size for higher initial frame rate
    sensor_t *s = esp_camera_sensor_get();
    s->set_framesize(s, FRAMESIZE_QVGA);
*/
    WiFi.begin(WIFI_SSID, WIFI_PASSWD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");

    display.clearDisplay();
    display.SetCursor(10, 0);
    display.println("WiFi Connected");
    display.display();
    delay(500);

    // Describe your custom StateObject types
    constellation.addStateObjectType("JPEG_IMG", 
    TypeDescriptor()
        .setDescription("JPEG image from TTGO ESP32CAM")
        .addProperty("Image", "System.String")
        .addProperty("Size", "System.Int32")
        .addProperty("Timestamp", "System.Int32")
        .addProperty("width", "System.Int32")
        .addProperty("height", "System.Int32")
        );

    // Declare the package descriptor
    constellation.declarePackageDescriptor();

    // (Virtual) Package started
    constellation.writeInfo("Virtual Package on '%s' is started !", constellation.getSentinelName());

    constellation.pushStateObject("SinglePress", SinglePress);
    constellation.pushStateObject("LongPress", LongPress);

    button1.attachLongPressStart(buttonLongPress);
    button1.attachClick(buttonClick);
    ip = WiFi.localIP().toString();
}

esp_err_t camera_init()
{
    //Power up camera
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, LOW);

    //initializa the camera
    esp_err_t err = esp_camera_init(&camera_config);

    if (err != ESP_OK)
        Serial.printf("Camera init Fail");

    return err;
}

esp_err_t camera_capture()
{
    esp_err_t res;
    
    //acquire a frame
    camera_fb_t *fb = esp_camera_fb_get();

    if(!fb)
    {
        constellation.writeInfo("Camera Capture Failed");
        return ESP_FAIL;
    }

    if(fb->format == PIXFORMAT_JPEG)
    {
        // Push a complex object on Constellation with JsonObject
        const int BUFFER_SIZE = JSON_OBJECT_SIZE(5);
        StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
        JsonObject& myStateObject = jsonBuffer.createObject();
        myStateObject["Image"] = (const char*)fb->buf;
        myStateObject["Size"] = fb->len;
        myStateObject["Timestamp"] = millis();
        myStateObject["width"] = fb->width);
        myStateObject["height"] = fb->height);
    }
    else
    {
        constellation.writeInfo("Image is not a JPEG");
    }

    esp_camera_fb_return(fb);

    return res;
}

void loop()
{
    button1.tick();
    display.println(ip); 
    if (digitalRead(AS312_PIN))
        display.println("AS312 Trigger");
    display.display();
    constellation.pushStateObject("PIR_Sensor", digitalRead(AS312_PIN));
    camera_capture();
    constellation.loop();
}
