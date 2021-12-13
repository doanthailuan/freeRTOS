#include <Arduino.h>
#include <WiFi.h>
#include <firebaseESP32.h>
FirebaseData firebaseData;
FirebaseJson json;
#include <ArduinoJson.h>
#include <HTTPClient.h>
WiFiClient client;
HTTPClient http;

const char* ssid     = "THAILUAN";
const char* password = "1234512345";
const char* serverName = "http://doantotnghieplinhluan.cf/esp32_data.php";
String apiKeyValue = "doantotnghiep";
unsigned long t_led=0, mi_led=0, t1=0;
byte x=0;
int i=0, onoff=0, ok=0, left, right, option=1, onoff_old, ok_old, option_old;
String ID_patient, Full_name, DOB, Address, Right_eye, Left_eye;

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
TaskHandle_t XCore0;
TaskHandle_t XCore1;
QueueHandle_t structQueue;
struct onoff_ok{
  int onoff_struct;
  int ok_struct;
};

#define FIREBASE_HOST "https://datn-e7a2f-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "RbfdmRGWscNqzFhaNisXzWipDxOaqjXhSi4wm0fb"

void connect_wifi(){
  WiFi.mode(WIFI_AP_STA);
  WiFi.beginSmartConfig();
  while (!WiFi.smartConfigDone()) {
    delay(500);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
}

void check_on_or_off(){
  if((left==5)&&(right==5)){
    t1=millis();
    while (millis()-t1<3000){
      left=analogRead(34);
      left=map(left, 0, 4095, 0, 5);
      right=analogRead(35);
      right=map(right, 0, 4095, 0, 5);
      if((left<5)&&(right<5)){
        onoff=0;
        break;
      }
      onoff=1;
    }
  }
}

void check_off_or_confirm()
{
  if((left==5)&&(right==5)){
    t1=millis();
    while (millis()-t1<1000){
      left=analogRead(34);
      left=map(left, 0, 4095, 0, 5);
      right=analogRead(35);
      right=map(right, 0, 4095, 0, 5);
      if((left<5)&&(right<5)){
        ok=0;
        break;
      }
      ok=1;
    }
    if(ok==1){
      t1=millis();
      while (millis()-t1<2000)
      {
        left=analogRead(34);
        left=map(left, 0, 4095, 0, 5);
        right=analogRead(35);
        right=map(right, 0, 4095, 0, 5);
        if((left<5)&&(right<5)){
          onoff=0;
          break;
        }
        onoff=1;
        ok=0;
      }
    }
  }
}

void check_option_value(){
  if((left==5)&&(right<5)){
    t1=millis();
    while(millis()-t1<1000){
      left=analogRead(34);
      left=map(left, 0, 4095, 0, 5);
      if(left<5){
        i=0;
        break;
      }
      i=1;
    }
    option-=i;
    if(option==0) option=4;
  }
  if((right==5)&&(left<5)){
    t1=millis();
    while(millis()-t1<1000){
      right=analogRead(35);
      right=map(right, 0, 4095, 0, 5);
      if(right<5){
        i=0;
        break;
      }
      i=1;
    }
    option+=i;
    if(option==5) option=1;
  }
}

void send_data_to_database(){
  Full_name = Firebase.getString(firebaseData, "/ByMIT/ten");
  DOB = Firebase.getString(firebaseData, "/ByMIT/DOB");
  Address = Firebase.getString(firebaseData, "/ByMIT/Address");
  http.begin(client, serverName);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String httpRequestData = "api_key=" + apiKeyValue + "&Full_Name=" + Full_name + "&DOB=" + DOB
                           + "&Address=" + Address + "&Right_eye" + Right_eye + "&Left_eye" + Left_eye;

  http.POST(httpRequestData);
  http.end();
}

void Task_core0_code (void * pvParameters){
  (void) pvParameters;
  for(;;){
    struct onoff_ok data_queue;
    data_queue.onoff_struct = 0;
    data_queue.ok_struct = 0;
    left=analogRead(34);
    left=map(left, 0, 4095, 0, 5);
    right=analogRead(35);
    right=map(right, 0, 4095, 0, 5);
    if(onoff==0){
      onoff_old=onoff;
      check_on_or_off();
      if(onoff != onoff_old){
        Firebase.setInt(firebaseData, "/ByESP32/onoff", onoff);
        data_queue.onoff_struct = onoff;
        xQueueSend(structQueue, &data_queue, portMAX_DELAY);
      }
    } else if(onoff==1){
      Right_eye = Right_eye + ", " + right;
      Left_eye = Left_eye + ", " + left;
      onoff_old=onoff;
      option_old=option;
      ok_old=ok;
      check_option_value();
      check_off_or_confirm();
      if(onoff != onoff_old){
        Firebase.setInt(firebaseData, "/ByESP32/onoff", onoff);
        data_queue.onoff_struct = onoff;
        xQueueSend(structQueue, &data_queue, portMAX_DELAY);
      }
      if(option != option_old){
        Firebase.setInt(firebaseData, "/ByESP32/option", option);
      }
      if(ok != ok_old){
        Firebase.setInt(firebaseData, "/ByESP32/ok", ok);
        data_queue.ok_struct = ok;
        xQueueSend(structQueue, &data_queue, portMAX_DELAY);
      }
    }
  }
  vTaskDelete ( NULL );
}

void Task_core1_code (void * pvParameters){
  (void) pvParameters;
  int onoff_tam=0;
  int ok_tam=0;
  for(;;){
    struct onoff_ok data_queue;
    Serial.print("start!");
    Serial.println(millis());
    t_led=millis();
    if(t_led-mi_led>2000){
      mi_led=millis();
      if(digitalRead(2)==LOW){
        digitalWrite(2, HIGH);
      } else {
        digitalWrite(2, LOW);
      }
    }
    if (xQueueReceive(structQueue, &data_queue, portMAX_DELAY) == pdPASS){
      onoff_tam = data_queue.onoff_struct;
      ok_tam = data_queue.ok_struct;
    }
    if(ok_tam == 1){
      send_data_to_database();
    }
    if(onoff_tam == 1){
      Serial.println(millis());
      Right_eye = Right_eye + ", " + right;
      Left_eye = Left_eye + ", " + left;
    } else {
      Right_eye = "";
      Left_eye = "";
    }
    Serial.println(onoff);
    Serial.print("finish!");
    Serial.println(millis());
    delay(1000);
  }
  vTaskDelete ( NULL );
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  pinMode(2,OUTPUT);
  pinMode(34, INPUT);
  pinMode(35, INPUT);
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  Firebase.setInt(firebaseData, "/ByESP32/onoff", onoff);
  Firebase.setInt(firebaseData, "/ByESP32/ok", ok);
  Firebase.setInt(firebaseData, "/ByESP32/option", option);

  xTaskCreatePinnedToCore(
                    Task_core0_code,   /* Task function. */
                    "Task_core0",     /* name of task. */
                    30000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &XCore0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500);

  xTaskCreatePinnedToCore(Task_core1_code, "Task_core1", 30000, NULL, 1, &XCore1, 1);
  delay(500);

  structQueue = xQueueCreate(1, // Queue length
                              sizeof(onoff_ok) // Queue item size
                              );
}

void loop() {
}
