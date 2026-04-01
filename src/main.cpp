#define BLYNK_TEMPLATE_ID "TMPL61hu1C9SK"
#define BLYNK_TEMPLATE_NAME "gg"
#define BLYNK_AUTH_TOKEN "8sCjg4sM3q4sRP8qYmE-nENkma0QvkIA"

#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ld2410.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time.h"

char ssid[] = "";    // Will be set by WiFiManager
char pass[] = "";  // Will be set by WiFiManager

#define DHTPIN 4
#define DHTTYPE DHT22

#define LED_ROOM_LIGHT 21
#define LED_LAMP 18

#define LDR_PIN 34

// Các chân điều khiển quạt
#define FAN_ENA 19
#define FAN_IN1 23

// Cấu hình PWM cho quạt
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

#define FAN_STRONG 255
#define FAN_MEDIUM 180
#define FAN_LOW 120

ld2410 radar;
HardwareSerial RadarSerial(2);

DHT dht(DHTPIN, DHTTYPE);

float temperature = 0;
int lightValue = 0;
String lightState = "";

int hourNow = 0;

// Biến để theo dõi thời gian phát hiện chuyển động và đứng yên
unsigned long moveStart = 0;
unsigned long stillStart = 0;
unsigned long lastPresence = 0;

bool manualMode =
    false;  // Biến để theo dõi chế độ điều khiển (manual hoặc auto)
int fanSpeed = 0;

// Hàm điều chỉnh tốc độ quạt
void setFanSpeed(int speed) {
  fanSpeed = speed;
  ledcWrite(PWM_CHANNEL, speed);
}

// Điều chỉnh tốc độ quạt dựa trên nhiệt độ
void controlFanByTemp() {
  if (temperature > 28)
    setFanSpeed(FAN_STRONG);

  else if (temperature < 25)
    setFanSpeed(FAN_MEDIUM);

  else
    setFanSpeed(FAN_LOW);
}

// Cập nhật trạng thái của các thiết bị lên Blynk
void updateBlynkUI() {
  Blynk.virtualWrite(V0, digitalRead(LED_ROOM_LIGHT));
  Blynk.virtualWrite(V1, digitalRead(LED_LAMP));
  Blynk.virtualWrite(V2, fanSpeed);
  Blynk.virtualWrite(V4, temperature);
}
// dieu khien che do manual hay auto
BLYNK_WRITE(V3) { manualMode = param.asInt(); }

// dieu khien den chinh chi co tac dung khi o che do manual,
BLYNK_WRITE(V0) {
  if (manualMode) {
    int state = param.asInt();
    digitalWrite(LED_ROOM_LIGHT, state);
  }
}

// dieu khien den ban chi co tac dung khi o che do manual,
BLYNK_WRITE(V1) {
  if (manualMode) {
    int state = param.asInt();
    digitalWrite(LED_LAMP, state);
  }
}
// dieu khien quat chi co tac dung khi o che do manual,
BLYNK_WRITE(V2) {
  if (manualMode) {
    int speed = param.asInt();
    setFanSpeed(speed);
  }
}

void TaskRadar(void* pvParameters) {
  while (1) {
    radar.read();

    if (radar.presenceDetected()) {
      Serial.println(" detected");
    } else {
      Serial.println("No detected ");
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Nhiệm vụ đọc cảm biến nhiệt độ và ánh sáng, cập nhật thời gian hiện tại
void TaskSensor(void* pvParameters) {
  while (1) {
    temperature = dht.readTemperature();

    lightValue = analogRead(LDR_PIN);

    if (lightValue > 2000)
      lightState = "TOI";
    else
      lightState = "SANG";

    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) hourNow = timeinfo.tm_hour;

    Serial.print("Temperature: ");
    Serial.print(temperature);

    Serial.print("Light Value: ");
    Serial.println(lightValue);

    Serial.print("Light State: ");
    Serial.println(lightState);

    Serial.print("Hour: ");
    Serial.println(hourNow);

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// Nhiệm vụ điều khiển đèn và quạt dựa trên dữ liệu từ radar và cảm biến, cũng
// như cập nhật giao diện Blynk
void TaskControl(void* pvParameters) {
  while (1) {
    if (!manualMode)  //
    {
      bool moving = radar.movingTargetDetected();
      bool still = radar.stationaryTargetDetected();
      bool presence = moving || still;  //

      if (millis() - lastPresence < 3000) {
        Serial.println("khong co nguoi");

        moveStart = 0;
        stillStart = 0;

        digitalWrite(LED_ROOM_LIGHT, LOW);
        digitalWrite(LED_LAMP, LOW);

        setFanSpeed(0);
      }

      else {
        if (lightState.equals("SANG")) {
          digitalWrite(LED_ROOM_LIGHT, LOW);
          digitalWrite(LED_LAMP, LOW);

          setFanSpeed(FAN_STRONG);
        }

        else {
          if (!(hourNow >= 22 || hourNow < 7)) {
            Serial.println("chua phai gio ngu");

            digitalWrite(LED_ROOM_LIGHT, HIGH);
            digitalWrite(LED_LAMP, LOW);
          }

          else {
            Serial.println("da den gio ngu");

            if (moving) {
              if (moveStart == 0) moveStart = millis();

              if (millis() - moveStart > 3000) {
                digitalWrite(LED_ROOM_LIGHT, HIGH);
                digitalWrite(LED_LAMP, LOW);
              }

              stillStart = 0;
            }

            else if (still) {
              if (stillStart == 0) stillStart = millis();

              if (millis() - stillStart > 3000) {
                digitalWrite(LED_ROOM_LIGHT, LOW);
                digitalWrite(LED_LAMP, HIGH);
              }

              moveStart = 0;
            }
          }

          controlFanByTemp();
        }
      }
    }

    updateBlynkUI();

    vTaskDelay(500 / portTICK_PERIOD_MS);

    Serial.println("----- DEVICE STATE -----");

    Serial.print("Room Light: ");
    Serial.println(digitalRead(LED_ROOM_LIGHT));

    Serial.print("Desk Lamp: ");
    Serial.println(digitalRead(LED_LAMP));

    Serial.print("Fan Speed: ");
    Serial.println(fanSpeed);

    Serial.print("Manual Mode: ");
    Serial.println(manualMode);

    Serial.println("------------------------");
    delay(1000);
  }
}

// Nhiệm vụ chạy Blynk để đảm bảo giao diện luôn phản hồi
void TaskBlynk(void* pvParameters) {
  while (1) {
    Blynk.run();  // Chạy Blynk trong một nhiệm vụ riêng biệt để đảm bảo giao
                  // diện luôn phản hồi
    vTaskDelay(10);
  }
}

// Hàm setup khởi tạo các thiết bị, kết nối WiFi, Blynk và tạo các nhiệm vụ
void setup() {
  Serial.begin(115200);

  // WiFi configuration using WiFiManager
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  
  // Thử kết nối với WiFi đã lưu
  if (!wm.autoConnect("SmartRoom_AP")) {
    Serial.println("WiFi failed to connect. Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("");
  Serial.println("WiFi Connected!");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print(" RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Kết nối Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
  Blynk.syncAll();

  dht.begin();

  Serial.print("Connecting");

  pinMode(LED_ROOM_LIGHT, OUTPUT);

  pinMode(LED_LAMP, OUTPUT);
  pinMode(FAN_IN1, OUTPUT);

  digitalWrite(FAN_IN1, HIGH);

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(FAN_ENA, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 180);  // Bắt đầu với tốc độ quạt bằng 0

  analogReadResolution(12);  // Đặt độ phân giải ADC là 12 bit

  RadarSerial.begin(256000, SERIAL_8N1, 16, 17);

  radar.begin(RadarSerial);

  configTime(
      7 * 3600, 0,
      "pool.ntp.org");  // Cấu hình múi giờ và máy chủ NTP để đồng bộ thời gian

  xTaskCreatePinnedToCore(TaskRadar, "Radar", 2048, NULL, 2, NULL,
                          1);  // Tạo nhiệm vụ đọc radar với độ ưu tiên cao hơn
                               // để đảm bảo phản hồi nhanh chóng

  xTaskCreatePinnedToCore(TaskSensor, "Sensor", 4096, NULL, 1, NULL,
                          1);  // Tạo nhiệm vụ đọc cảm biến với độ ưu tiên thấp
                               // hơn, vì nó không cần phản hồi nhanh như radar

  xTaskCreatePinnedToCore(
      TaskControl, "Control", 4096, NULL, 1, NULL,
      1);  // Tạo nhiệm vụ điều khiển với độ ưu tiên thấp nhất, vì nó chỉ cần
           // cập nhật sau khi có dữ liệu mới từ radar và cảm biến

  xTaskCreatePinnedToCore(TaskBlynk, "Blynk", 4096, NULL, 1, NULL,
                          1);  // Tạo nhiệm vụ chạy Blynk với độ ưu tiên thấp,
                               // vì nó chỉ cần đảm bảo giao diện luôn phản hồi
}
void loop() {}