#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#ifndef STASSID
#define STASSID "OGMv2"
#define STAPSK  "amenoera"
#endif

// //first dev
#define SENSORS_SIZE 1
#define ACTIVE_SIZE 2
#define TOGGLES_SIZE 0
String introductionJson = "identifier {\"mac\":\""+ String(WiFi.macAddress()) +"\",\"devices\":[{\"name\":\"sonar\",\"type\":\"passive\",\"units\":\"cm\"},{\"name\":\"motorR\",\"type\":\"active\",\"units\":null},{\"name\":\"motorL\",\"type\":\"active\",\"units\":null}]}";

//second dev
// #define SENSORS_SIZE 1
// #define ACTIVE_SIZE 2
// #define TOGGLES_SIZE 2
// OneWire oneWire(D2); 
// DallasTemperature temp(&oneWire);
// String introductionJson = "identifier {\"mac\":\""+ String(WiFi.macAddress()) +"\",\"devices\":[{\"name\":\"temperatureSensor\",\"type\":\"passive\",\"units\":\"C\"},{\"name\":\"buttonL\",\"type\":\"passive\",\"units\":\"pressed\"},{\"name\":\"buttonR\",\"type\":\"passive\",\"units\":\"pressed\"},{\"name\":\"LedRed\",\"type\":\"active\",\"units\":null},{\"name\":\"LedGreen\",\"type\":\"active\",\"units\":null}]}";

const char* ssid     = STASSID;
const char* password = STAPSK;

const char* host = "192.168.55.112";
const uint16_t port = 3000;

WiFiClient client;

char buff[100];

struct sensor
{
  String name;
  float (*func)();
  unsigned long last;
  unsigned long interval;
  
};

struct toggle
{
  String name;
  int pin;  
  int state;
};

struct active{
  String name;
  int state;
  int* pins; 
  void (*func)(struct active*,int);
};

struct sensor sensors[SENSORS_SIZE];
struct active actives[ACTIVE_SIZE];
struct toggle toggles[TOGGLES_SIZE];

float getReadOut(String name){
  for(int i =0; i < SENSORS_SIZE; ++i){
    if(name == sensors[i].name){
      return sensors[i].func();;
    }
  }
  return 0;
}

void analyseMsg(){
  for(int i =0; i < ACTIVE_SIZE; ++i){
    if(String(buff).startsWith(actives[i].name)){
      Serial.println(actives[i].name);
      Serial.println(*(buff+actives[i].name.length()+1));
      if (*(buff+actives[i].name.length()+1) == '0'){
        actives[i].func(actives+i,0);
      }else{
        actives[i].func(actives+i,1);
      }
      return;
    }
  }
}

void connect(){
  Serial.print("(Re)Connecting\n");
  while (!client.connect(host, port)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nSuccess\n");
}

float sonarfunc(){
  digitalWrite(D6, LOW);
  delayMicroseconds(2);
  digitalWrite(D6, HIGH);
  delayMicroseconds(10);
  digitalWrite(D6, LOW);
  return (pulseIn(D5,HIGH)*0.034)/2;
}

void motorfunc(struct active *motor, int state){
  //Serial.println("Curr motor state: " + String(motor->state)+ " Desired state: " + String(state));
  //Serial.println(String(motor->pins[0])+":"+String(motor->pins[1])+":"+String(motor->pins[2]));
  if(motor->state != state){
    if(motor->state == 0){
      //Serial.println("Setting pin 1 to HIGH");
      digitalWrite(motor->pins[0],HIGH);
      digitalWrite(motor->pins[1],HIGH);
      digitalWrite(motor->pins[2],LOW);
      motor->state = 1;
    }else{
      //Serial.println("Setting pin 1 to LOW");
      digitalWrite(motor->pins[0],HIGH);
      digitalWrite(motor->pins[1],LOW);
      digitalWrite(motor->pins[2],LOW);
      motor->state = 0;
    }
  }
}

void ledfunc(struct active *led, int state){
  // Serial.println("Curr led state: " + String(led->state)+ " Desired state: " + String(state));
  // Serial.println(String(led->pins[0]));
  if(led->state != state){
    if(led->state == 0){
      // Serial.println("Setting pin 1 to HIGH");
      digitalWrite(*(led->pins),HIGH);
      led->state = 1;
    }else{
      // Serial.println("Setting pin 1 to LOW");
      digitalWrite(*(led->pins),LOW);
      led->state = 0;
    }
  }
}

void initDev1(){
  sensors[0].name = "sonar";
  sensors[0].last = millis();
  sensors[0].interval = 500UL;
  sensors[0].func = sonarfunc;

  pinMode(D6,OUTPUT);//trig
  pinMode(D5,INPUT);//echo
  digitalWrite(D6,LOW);

  actives[0].name = "motorR";
  actives[0].state = 1;
  int * pins1 = new int[3];
  int pin1[] ={D2,D0,D1};
  memcpy(pins1,pin1,sizeof(pin1));
  actives[0].pins = pins1;
  actives[0].func = motorfunc;

  pinMode(D2,OUTPUT);
  pinMode(D0,OUTPUT);
  pinMode(D1,OUTPUT);

  actives[0].func(actives,0);

  actives[1].name = "motorL";
  actives[1].state = 1;
  int * pins2 = new int[3];
  int pin2[] = {D8,D3,D4};
  memcpy(pins2,pin2,sizeof(pin2));
  actives[1].pins = pins2;
  actives[1].func = motorfunc;

  pinMode(D8,OUTPUT);
  pinMode(D3,OUTPUT);
  pinMode(D4,OUTPUT);

  actives[1].func(actives+1,0);
}


// float tempfunc(){
//   temp.requestTemperatures();
//   return temp.getTempCByIndex(0);
// }
// void initDev2(){
//   sensors[0].name = "temperatureSensor";
//   sensors[0].func = tempfunc;
//   sensors[0].last = millis();
//   sensors[0].interval = 5000UL;

//   pinMode(D1, INPUT);
//   toggles[0].name = "buttonL";
//   toggles[0].state = 0;
//   toggles[0].pin = D1;
//   pinMode(D0, INPUT);
//   toggles[1].name = "buttonR";
//   toggles[1].state = 0;
//   toggles[1].pin = D0;

//   pinMode(D5, OUTPUT); //TODO
//   actives[0].name = "LedRed";
//   actives[0].state = 0;
//   int * pin = new int;
//   *pin = D5;//TODO
//   actives[0].pins = pin;
//   actives[0].func = ledfunc;
//   ledfunc(actives  , 0);

//   pinMode(D6, OUTPUT); //TODO
//   actives[1].name = "LedGreen";
//   actives[1].state = 0;
//   int * pin2 = new int;
//   *pin2 = D6;//TODO
//   actives[1].pins = pin2;
//   actives[1].func = ledfunc;
//   ledfunc(actives +1 , 0);
//  }

unsigned long heartbeat, pulse = 1500UL;

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");

  Serial.print("connecting to ");
  Serial.print(host);
  Serial.print(':');
  Serial.println(port);

  Serial.println(WiFi.localIP());
  connect();
  client.write(introductionJson.c_str(),introductionJson.length()+1);
  heartbeat = millis();
  initDev1();
  // initDev2();
}

void loop() {
  if(!client.connected()){
    connect();
    client.write(introductionJson.c_str(),introductionJson.length()+1);
    for(int i = 0; i <SENSORS_SIZE;++i){
      sensors[i].last = millis();
    }
  }
  if(client.available()) {
    memset(buff,0, sizeof(buff));
    for(int i = 0; i< 100&&client.available(); ++i) {
      buff[i] = client.read();
      if(buff[i] =='\0') break;
    }
    Serial.print(buff);
    Serial.print('\n');
    for (int i = 0;buff[i] != '\0'; ++i)
    {
      Serial.print(buff[i],DEC);
      Serial.print(' ');
    }
    Serial.print('\n');
    analyseMsg();
  }
  //Checking if should send sensor update
  for(int i =0; i < SENSORS_SIZE ; ++i){
    if(sensors[i].last + sensors[i].interval < millis()){
      float readout = getReadOut(sensors[i].name);
      String msg = sensors[i].name + " " +  String(readout); 
      if(client.connected()){
        client.write(msg.c_str(),msg.length()+1);
      }else return;
      // Serial.println("Sent measurement of " + sensors[i].name);
      sensors[i].last = millis();
    }
  }
  //Checking if should send toggles update
  for(int i =0; i < TOGGLES_SIZE ; ++i){
    int curr = digitalRead(toggles[i].pin);
    if(curr!=toggles[i].state){
      toggles[i].state = curr;
      String msg = toggles[i].name + " " +String(curr);
      if(client.connected()){
        client.write(msg.c_str(),msg.length()+1);
      }else return;
      // Serial.println("Sent measurement of " + toggles[i].name);
      sensors[i].last = millis();
    }
  }
  if(heartbeat + pulse < millis()){
    client.write("_alive",7);
    heartbeat = millis();
  }
}
