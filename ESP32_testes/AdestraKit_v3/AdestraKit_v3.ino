//importando as bibliotecas necessárias
#include "time.h"
#include "EEPROM.h"
#include <WiFi.h>
#include <BLE2902.h>
#include <BLEUtils.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <Ultrasonic.h>
#include <FirebaseESP32.h>

//Definindo os pinos na esp32 e variáveis.
#define pino_echo 27
#define pino_buzzer 5
#define pino_trigger 2
#define pino_led_azul 12 //bluetooth
#define pino_led_branco 4 //sinal sonoro
#define pino_led_verde 18 //WiFi

#define EEPROM_SIZE 128
#define SERVICE_UUID        "87b34f52-4765-4d3a-b902-547751632d72"
#define CHARACTERISTIC_UUID "a97d209a-b1d6-4edf-b67f-6a0c25fa42c9"
#define TARGET_DEVICE_NAME  "AdestraKit"

#define Host "https://adestrakit.firebaseio.com/"
#define Senha_Fire "8ZMdyCFrQ9KRMFPJOuVkRrNRtdAcCB9BKDy6UIRx"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

const int modeAddr = 0;
const int wifiAddr = 10;

int modeIdx;
int channel = 0;
int resolution = 10;
int frequence = 2000;

int qtdEntradaLocal;
float distancia = 50.0; //virá do app o valor.

//chama uma função no servidor de horário.
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;

//regula o fuso horário.
const int   daylightOffset_sec = -3600*3;

//Define FirebaseESP32 data object
FirebaseData firebaseData;
const String bd = "configEsp/";

//Inicializa o sensor nos pinos definidos acima
Ultrasonic ultrasonic(pino_trigger, pino_echo);

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();

      if (value.length() > 0) {
        Serial.print("Value Callback: ");
        //se quiser ver o que está chegando
        Serial.println(value.c_str());
        writeString(wifiAddr, value.c_str());
      }
    }
    void writeString(int add, String data) {
      int _size = data.length();
      for (int i = 0 ; i < _size; i++) {
        EEPROM.write(add+i, data[i]);
      }
      EEPROM.write(add+_size, '\0');
      EEPROM.commit();
    }
};

//imprime o horário do servidor.
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Falha ao obter a hora");
    return;
  }
 Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

void setup(){  
  Serial.begin(115200); 
     
  pinMode(pino_echo, INPUT);
  pinMode(pino_trigger, OUTPUT);  
  pinMode(pino_led_azul, OUTPUT);//bluetooth
  pinMode(pino_led_verde, OUTPUT); //WiFi
  pinMode(pino_led_branco, OUTPUT);//sinal sonoro
  qtdEntradaLocal = 0;

  ledcSetup(channel, frequence, resolution);
  ledcAttachPin(pino_buzzer, channel); 

  /*/connect to WiFi
  Serial.printf("Conectando em %s ", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }
  Serial.println(" Feito");*/
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  if (!EEPROM.begin(EEPROM_SIZE)) {
    delay(1000);
  }
  modeIdx = EEPROM.read(modeAddr);
  Serial.print("modeIdx: ");
  Serial.println(modeIdx);

  EEPROM.write(modeAddr, modeIdx != 0 ? 0 : 1 );
  EEPROM.commit();

  if (modeIdx != 0) {
    //BLE Mode, azul
    digitalWrite(pino_led_azul, false);//liga no false
    Serial.println("BLE MODE");
    bleTask();
  } else {
    //Wifi mode, verde
    digitalWrite(pino_led_verde, true);
    Serial.println("WIFI MODE");
    wifiTask();
  }
}

void bleTask() {
  // Create the BLE Device
  BLEDevice::init("ESP32 AdestraKit");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void wifiTask() {
  String receivedData;
  receivedData = read_string(wifiAddr);

  if (receivedData.length() > 0 ) {
    String wifiName = getValue(receivedData, ',' , 0);
    String  wifiPassword = getValue(receivedData, ' ' , 1);

    if (wifiName.length() > 0 && wifiPassword.length() > 0) {
     
      WiFi.begin(wifiName.c_str(), wifiPassword.c_str());
      Serial.print("Conectando na rede Wifi: ");
      Serial.println(wifiName);
      Serial.println("Senha da Rede Wifi: Oculta");

      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      
      Serial.println();
      Serial.print("WiFi conectada com IP:");
      Serial.println(WiFi.localIP());
      Serial.println();
    }
    //conecta no firebase
    Firebase.begin(Host, Senha_Fire); 

    //só teste 
    sendFirebase();
  }
}

//ok
String read_string(int add) {
  char data[100];
  int len = 0;
  unsigned char k;

  k = EEPROM.read(add);
  while (k != '\0' && len < 110) {
    k = EEPROM.read(add + len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}

String getValue(String data, char separa, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) { 
    if (data.charAt(i) == separa || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  Serial.print(found);
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void sendFirebase(){
  //temperatura
  String macAdress = WiFi.macAddress()+"/";
  Firebase.setString(firebaseData,bd + macAdress + "nome", WiFi.macAddress());
  Firebase.setFloat(firebaseData,bd + macAdress + "distancia_alarme", 15 );
  Firebase.setFloat(firebaseData,bd + macAdress + "distancia_alerta", 30 );
  Firebase.setBool(firebaseData,bd + macAdress + "ligado", true);
  Firebase.setString(firebaseData,bd + macAdress + "local", "local novo" );
  Firebase.setString(firebaseData,bd + macAdress + "user_id", "user_id novo" );
}

float medeDistancia(){  
  float cmMsec;
  long microsec = ultrasonic.timing();
  cmMsec = ultrasonic.convert(microsec, Ultrasonic::CM);

  return cmMsec;  
}

void loop(){
  
  if (medeDistancia() <= distancia){    
    qtdEntradaLocal++;
    Serial.printf("Entrou no local:%d" , qtdEntradaLocal);
    printLocalTime();               
    digitalWrite(pino_led_branco, HIGH);   
    delay(3000);
    ledcWriteTone(channel,650);//frequencia  
  }else{  
    digitalWrite(pino_led_branco, LOW);
    ledcWriteTone(channel,0);
  }
  delay(1000);
}
