//https://www.robocore.net/tutoriais/celula-de-carga-hx711-com-arduino
#include "HX711.h"
#include "thermistor.h"
#include "HardwareSerial.h"
#include <LCDWIKI_GUI.h> //Core graphics library
#include <SSD1283A.h> //Hardware-specific library
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>

// Update these with values suitable for your network.

const char* ssid = "NPITI-IoT";
const char* password = "NPITI-IoT";
//IO Adatfruit
const char* mqttserver = "io.adafruit.com";
const int mqttPort = 1883;
const char* mqttUser = "marimelo";
const char* mqttPassword = "--";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
//LCD definições
#if (defined(TEENSYDUINO) && (TEENSYDUINO == 147))
SSD1283A_GUI mylcd(/*CS=*/ 10, /*DC=*/ 15, /*RST=*/ 14, /*LED=*/ -1); //hardware spi,cs,cd,reset,led

// for my wirings used for e-paper displays:
#elif defined (ESP8266)
SSD1283A_GUI mylcd(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*LED=D2*/ 4); //hardware spi,cs,cd,reset,led
#elif defined(ESP32)
SSD1283A_GUI mylcd(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*LED=*/ 4); //hardware spi,cs,cd,reset,led
#elif defined(_BOARD_GENERIC_STM32F103C_H_)
SSD1283A_GUI mylcd(/*CS=4*/ SS, /*DC=*/ 3, /*RST=*/ 2, /*LED=*/ 1); //hardware spi,cs,cd,reset,led
#elif defined(__AVR)
SSD1283A_GUI mylcd(/*CS=10*/ SS, /*DC=*/ 8, /*RST=*/ 9, /*LED=*/ 7); //hardware spi,cs,cd,reset,led
#else
// catch all other default
SSD1283A_GUI mylcd(/*CS=10*/ SS, /*DC=*/ 8, /*RST=*/ 9, /*LED=*/ 7); //hardware spi,cs,cd,reset,led
#endif

#define  BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

//NTC definições
//  pino 
#define NTC_PIN               34

// Thermistor object
THERMISTOR thermistor(NTC_PIN,        // Analog pin
                      100000,          // Nominal resistance at 25 ºC
                      3950,           // thermistor's beta coefficient
                      100000);         // Value of the series resistor

// Global temperature reading
uint16_t temp;


//BALANÇA definições e variaveis
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 14;

float pesoMaximo = 0;
float refZero = 0.0;

long extraZero = 0;
int pegarVar = 0;
float porcPeso = 0;
float medicao = 0;
int tarando = 0;
int resetPin = 27;
int taraPin = 33;
float variacao = 1.0;


int resetState = HIGH; // Estado anterior do botão reset
int taraState = HIGH;  // Estado anterior do botão tara

const int TEMPO_ESPERA = 1000; //declaracao da variavel de espera

HX711 escala; //declaracao do objeto escala na classe HX711 da biblioteca

float fator_calibracao = -21000; //pre-definicao da variavel de calibracao

char comando; //declaracao da variavel que ira receber os comandos para alterar o fator de calibracao

//conf para enviar dados
int qcofPass = 0;


//configurações das telas
int tela = 1;
int tliq = 0;
int qcof = 0;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      // client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("marimelo/feeds/teste");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  escala.begin (LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); //inicializacao e definicao dos pinos DT e SCK dentro do objeto ESCALA
  escala.tare(); //zera a escala
  escala.set_scale(fator_calibracao);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(taraPin, INPUT_PULLUP);
  mylcd.init();
  mylcd.Fill_Screen(BLACK);
  setup_wifi();
  client.setServer(mqttserver, 1883);
  client.setCallback(callback);
}

void loop() {
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  temp = thermistor.read()/10;   // Read temperature
  int currentStateReset = digitalRead(resetPin);
  int currentStateTara = digitalRead(taraPin);
  //verifica se o modulo esta pronto para realizar leituras
  if (escala.is_ready()){
    medicao = - escala.get_units()  ;
      // delay(1000);
  } else {
      // Serial.print("HX-711 ocupado");
  }

  if (currentStateReset == LOW && resetState == HIGH) {
    Serial.println("Reset pressionado");
    tela = 1;
    refZero = 0;
    pesoMaximo = 0;
    delay(3000);
    // Coloque aqui o código que você deseja executar apenas uma vez quando o botão for pressionado
  }

  if (currentStateTara == LOW && taraState == HIGH) {
    Serial.println("Tara pressionado");
    if(tarando == 0){
      refZero = medicao;
      tarando = 1;
      tela=2;
      Serial.print("Nova referência do zero:");
      Serial.println(refZero);
      // delay(1000);
    }else if(tarando == 1){

      pesoMaximo = medicao;

      variacao = pesoMaximo - refZero;

      Serial.print("Peso máximo:");
      Serial.println(pesoMaximo);
      // delay(1000);
      tarando = 2;
      tela = 3;
    }
    resetState = currentStateReset; // Atualiza o estado anterior do botão reset
    taraState = currentStateTara;   // Atualiza o estado anterior do botão tara
  }

  if((resetState == LOW || taraState == LOW)&& (currentStateReset == HIGH && currentStateTara == HIGH)){
    delay(500);
    resetState = HIGH;
    taraState = HIGH;
  }


  //Calculo de porcentagem do café
  porcPeso = ( (medicao - refZero)  / -variacao)*-100 ;
  char s_porc[8];
  dtostrf(porcPeso,1,2,s_porc);
  client.publish("marimelo/feeds/nivel", s_porc);
  // delay(
  char s_temp[8];
  dtostrf(temp,1,2,s_temp);
  client.publish("marimelo/feeds/temp", s_temp);
  delay(2000);
  if(temp<45.0){
    tliq = 0;
  } else if (temp>=45.0 && temp<65.0){
    tliq = 1;
  } else {
    tliq = 2;
  }

  if(porcPeso<10.0){
    qcof = 0;
  } else if (porcPeso<25.0){
    qcof = 1;
  } else if (porcPeso>=25.0 && porcPeso<85.0){
    qcof = 2;
  } else {
    qcof = 3;
  }






  //telas
  mylcd.Set_Text_Mode(0);
  switch (tela) {
  case 1:
    // mylcd.Set_Text_Mode(0);
    mylcd.Fill_Screen(0x0000);
    mylcd.Set_Text_colour(RED);
    mylcd.Set_Text_Back_colour(BLACK);
    mylcd.Set_Text_Size(2);
    mylcd.Print_String("Aperte ", 0, 0);
    mylcd.Print_String("botao para", 0, 14);
    mylcd.Print_String("nova tara", 0, 28);
    break;
  case 2:
    // mylcd.Set_Text_Mode(0);
    mylcd.Fill_Screen(0x0000);
    mylcd.Set_Text_colour(RED);
    mylcd.Set_Text_Back_colour(BLACK);
    mylcd.Set_Text_Size(2);
    mylcd.Print_String("Aperte ", 0, 0);
    mylcd.Print_String("botao para ", 0, 14);
    mylcd.Print_String("novo max", 0, 28);
    break;
  case 3:
    // mylcd.Set_Text_Mode(0);
    mylcd.Fill_Screen(0x0000);
    mylcd.Set_Text_colour(RED);
    mylcd.Set_Text_Back_colour(BLACK);
    mylcd.Set_Text_Size(2);
    if(porcPeso>10.0){
      mylcd.Print_String("Temperatura", 0, 0);
      mylcd.Print_String("do cafe:", 0, 14);
      switch (tliq){
        case 0:
          mylcd.Print_String("Frio", 0, 28);
          break;
        case 1:
          mylcd.Print_String("Morno", 0, 28);
          break;
        case 2:
          mylcd.Print_String("Quente", 0, 28);
          break;
      }
    }
    mylcd.Print_String("Quantidade", 0, 42);
    mylcd.Print_String("do cafe:", 0, 56);
    switch (qcof){
      case 0:
        mylcd.Print_String("Nao tem", 0, 70);
        break;
      case 1:
        mylcd.Print_String("Pouco cafe", 0, 70);
        break;
      case 2:
        mylcd.Print_String("Media", 0, 70);
        break;
      case 3:
        mylcd.Print_String("Muito cafe", 0, 70);
        break;
    }
    break;
  }
}
