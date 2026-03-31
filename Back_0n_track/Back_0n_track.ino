#include <Wire.h>
#include <MPU6050.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_sleep.h>

MPU6050 mpu;

const char* ssid = "12345678";
const char* password = "12345678";

String postureURL = "https://back-on-track-af999-default-rtdb.firebaseio.com/posture_focus.json";
String eyeURL = "https://back-on-track-af999-default-rtdb.firebaseio.com/eye_focus.json";

int buzzerPin = 2;

/* MPU VALUES */

float AccX,AccY,AccZ;
float GyroX,GyroY,GyroZ;

float roll=0;
float pitch=0;

float dt;
unsigned long previousTime;

const float alpha=0.90;

/* POSTURE */

bool postureFocused=false;

/* ALERT TIMER */

unsigned long notFocusStart=0;
bool counting=false;
unsigned long lastBeepTime=0;

/* SLEEP */

bool sleepPosition=false;
unsigned long sleepStartTime = 0;


/* ===== BUZZER ===== */

void beepAlert(){

digitalWrite(buzzerPin,HIGH);
vTaskDelay(pdMS_TO_TICKS(120));

digitalWrite(buzzerPin,LOW);
vTaskDelay(pdMS_TO_TICKS(120));

digitalWrite(buzzerPin,HIGH);
vTaskDelay(pdMS_TO_TICKS(120));

digitalWrite(buzzerPin,LOW);

}


/* ===== SLEEP ===== */

void enterSleep(){

Serial.println("Entering Sleep");

digitalWrite(buzzerPin,HIGH);
delay(1500);
digitalWrite(buzzerPin,LOW);

WiFi.disconnect(true);

delay(500);

esp_deep_sleep_start();

}


/* ===== SENSOR TASK ===== */

void sensorTask(void *pvParameters){

TickType_t xLastWakeTime=xTaskGetTickCount();

while(true){

int16_t ax,ay,az;
int16_t gx,gy,gz;

mpu.getMotion6(&ax,&ay,&az,&gx,&gy,&gz);

/* Convert */

AccX=ax/16384.0;
AccY=ay/16384.0;
AccZ=az/16384.0;

GyroX=gx/131.0;
GyroY=gy/131.0;
GyroZ=gz/131.0;

/* Time */

unsigned long currentTime=millis();

dt=(currentTime-previousTime)/1000.0;

if(dt>0.02) dt=0.02;

previousTime=currentTime;

/* Angles */

float accRoll=atan2(AccY,AccZ)*180/PI;

float accPitch=
atan(-AccX/sqrt(AccY*AccY+AccZ*AccZ))*180/PI;

/* Complementary filter */

roll=alpha*(roll+GyroX*dt)+(1-alpha)*accRoll;

pitch=alpha*(pitch+GyroY*dt)+(1-alpha)*accPitch;

/* Debug */

Serial.print("Roll: ");
Serial.print(roll);

Serial.print(" Pitch: ");
Serial.println(pitch);


/* ===== SLEEP DETECTION ===== */

if(pitch < -60 && abs(roll)<30){

if(!sleepPosition){

sleepStartTime=millis();

sleepPosition=true;

}

if(millis()-sleepStartTime>5000){

enterSleep();

}

}

else{

sleepPosition=false;

sleepStartTime=0;

}


/* ===== POSTURE CHECK ===== */

/* Focus only if BOTH angles are good */

postureFocused =
(abs(pitch)<20 && abs(roll)<15);


/* small stability margin */

if(abs(pitch)<20 && abs(roll)<15){

postureFocused=true;

}

vTaskDelayUntil(&xLastWakeTime,
pdMS_TO_TICKS(10));

}
}


/* ===== FIREBASE TASK ===== */

void firebaseTask(void *pvParameters){

while(true){

if(!sleepPosition){

if(WiFi.status()==WL_CONNECTED){

HTTPClient http;

/* POSTURE */

http.begin(postureURL);

if(postureFocused)

http.PUT("1");

else

http.PUT("0");

http.end();

/* EYE */

http.begin(eyeURL);

int httpCode=http.GET();

if(httpCode>0){

String payload=http.getString();

payload.trim();

unsigned long now=millis();

/* ALERT CONDITION */

if(payload=="0" || !postureFocused){

if(!counting){

notFocusStart=now;

counting=true;

}

if(now-notFocusStart>20000){

if(now-lastBeepTime>10000){

beepAlert();

lastBeepTime=now;

}

}

}

else{

digitalWrite(buzzerPin,LOW);

counting=false;

}

}

http.end();

}

}

vTaskDelay(pdMS_TO_TICKS(300));

}
}


/* ===== SETUP ===== */

void setup(){

Serial.begin(115200);

Wire.begin(6,7);

Wire.setClock(400000);

mpu.initialize();

if(!mpu.testConnection()){

Serial.println("MPU FAIL");

while(1);

}

pinMode(buzzerPin,OUTPUT);

/* WIFI */

WiFi.begin(ssid,password);

while(WiFi.status()!=WL_CONNECTED){

delay(500);

Serial.print(".");

}

WiFi.setSleep(false);

Serial.println("WiFi Connected");

previousTime=millis();

/* TASKS */

xTaskCreate(sensorTask,"Sensor",4096,NULL,2,NULL);
xTaskCreate(firebaseTask,"Firebase",20000,NULL,1,NULL);

}


/* ===== LOOP ===== */

void loop(){

}