#include "EEPROM.h"
#include <WiFi.h>
#include <BLEDevice.h>
//#include <BLEServer.h>
//#include <BLEUtils.h>
#include <BLE2902.h>
#include <FirebaseESP32.h>

//erivelton
//#include "time.h"
//#include <Ultrasonic.h>

#define EEPROM_SIZE 128

//conexão com BLE
const PROGMEM char SERVICE_UUID[] = "87b34f52-4765-4d3a-b902-547751632d72";
const PROGMEM char CHARACTERISTIC_UUID[] = "a97d209a-b1d6-4edf-b67f-6a0c25fa42c9";
const PROGMEM char TARGET_DEVICE_NAME[] = "AdestraKit";
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

//conexão com firebase
const PROGMEM char Host[] = "https://adestrakit.firebaseio.com/";
const PROGMEM char Senha_Fire[] = "8ZMdyCFrQ9KRMFPJOuVkRrNRtdAcCB9BKDy6UIRx";
FirebaseData firebaseData;
const PROGMEM char bd[] = "configEsp/";
const PROGMEM char avancos[] = "avancos/";

//definindo pinos e variáveis esp32 - 38 pinos
#define ledBle 2 //azul
#define ledWifi 4 //verde
#define ledAlerta 12 //led de alerta para o animal

#define pino_echo 17
#define pino_buzzer 13
#define pino_trigger 16

//erivelton
//Definindo os pinos na esp32 e variáveis.

//#define pino_led_azul 12 //bluetooth
//#define pino_led_branco 4 //sinal sonoro
//#define pino_led_verde 18 //WiFi


//contantes
const PROGMEM int modeAddr = 0;
const PROGMEM int wifiAddr = 10;

//variáveis globais
String receivedData;
int modeIdx;

/*
//erivelton
//Inicializa o sensor nos pinos definidos acima
Ultrasonic ultrasonic(pino_trigger, pino_echo);
//regula o fuso horário.
const int   daylightOffset_sec = -3600*3;
//chama uma função no servidor de horário.
const PROGMEM char ntpServer[] = "pool.ntp.org";
long gmtOffset_sec = 0;
int qtdEntradaLocal;
float distancia = 50.0; //virá do app o valor.
int channel = 0;
int resolution = 10;
int frequence = 2000;
*/

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

      //recebimento do ble
      //var wifidata = rede+","+senhaenviada+","+dispositivoid+","+nome+","+local+","+distancia;
      //wifidata = rede,senhaenviada,dispositivoid,nome,local,distancia
      //wifidata[0] = rede
      
      if (value.length() > 0) {
        Serial.print(F("Value Callback: "));
        Serial.println(F(value.c_str()));
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
/*
//erivelton
//imprime o horário do servidor.
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Falha ao obter a hora");
    return;
  }
 Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
*/
void setup() {
  Serial.begin(115200);
  pinMode(ledBle, OUTPUT);
  pinMode(ledWifi, OUTPUT);

/*
  //erivelton
  pinMode(pino_echo, INPUT);
  pinMode(pino_trigger, OUTPUT);  
//  pinMode(pino_led_azul, OUTPUT);//bluetooth
//  pinMode(pino_led_verde, OUTPUT); //WiFi
//  pinMode(pino_led_branco, OUTPUT);//sinal sonoro
  qtdEntradaLocal = 0;
  ledcSetup(channel, frequence, resolution);
  ledcAttachPin(pino_buzzer, channel);
*/

  //resto setup esta ok
  if (!EEPROM.begin(EEPROM_SIZE)) {
    delay(1000);
  }
  modeIdx = EEPROM.read(modeAddr);
  Serial.print(F("modeIdx: "));
  Serial.println(F(modeIdx));

  EEPROM.write(modeAddr, modeIdx != 0 ? 0 : 1 );
  EEPROM.commit();

  if (modeIdx != 0) {
    //BLE Mode, azul
    digitalWrite(ledBle, false);//liga no false
    digitalWrite(ledWifi, true);//desliga o LedWifi
    Serial.println(F("BLE MODE"));
    bleTask();
  } else {
    //Wifi mode, verde
    //desligando o ledBle
    digitalWrite(ledBle, true);
    Serial.println(F("WIFI MODE"));
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
  Serial.println(F("Aguardando uma conexão Ble..."));
}

void wifiTask() {
   
  //comentando para fazer testes com as novas variáveis até que o código esteja recebendo informações do app
  //String receivedData;
  //receivedData = read_string(wifiAddr);

  receivedData = "AroldoGisele,Ar01d0&Gi5373,dispositivoid,nome,local,15";
  if (receivedData.length() > 0 ) {
    
    //tentando usar direto a variável para tentar diminuir o tamanho do sketch
    //String wifiName = getValue(receivedData, ',' , 0);
    //String  wifiPassword = getValue(receivedData, ',' , 1);

    //if (wifiName.length() > 0 && wifiPassword.length() > 0) {
      if (getValue(receivedData, ',' , 0).length() > 0 && getValue(receivedData, ',' , 1).length() > 0) {
//      atenção aqui
      //WiFi.begin(wifiName.c_str(), wifiPassword.c_str());
      WiFi.begin(getValue(receivedData, ',' , 0).c_str(),getValue(receivedData, ',' , 1).c_str());
      
      Serial.print(F("Conectando na rede Wifi: "));
      //Serial.println(wifiName);
      //Serial.println(WiFi.begin(wifiName.c_str());
      Serial.println(F(getValue(receivedData, ',' , 0).c_str()));
      Serial.println(F("Senha da Rede Wifi: Oculta"));

      while (WiFi.status() != WL_CONNECTED) {
        digitalWrite(ledWifi, true);
        delay(250);
        digitalWrite(ledWifi, false);//liga no false
        delay(250);
        Serial.print(F("."));
      }
      
      Serial.println();
      Serial.print(F("WiFi conectada com IP:"));
      Serial.println(WiFi.localIP());
      Serial.println();
    }

    //horário
   // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
   // printLocalTime();
    
    //conecta no firebase
    Firebase.begin(Host, Senha_Fire); 

    //só teste para ver se está inicializando todas as conexões
    sendFirebase();
  }
}

//ok
String read_string(int add) {
  char data[200];
  int len = 0;
  unsigned char k;

  k = EEPROM.read(add);
  while (k != '\0' && len < 210) {
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
  //Firebase.setString(firebaseData,bd + WiFi.macAddress()+"/" + "macAddress", WiFi.macAddress());
  //Firebase.setFloat(firebaseData,bd + WiFi.macAddress()+"/" + "distancia_alerta", 30 );
  Firebase.setBool(firebaseData,bd + WiFi.macAddress()+"/" + "ligado", true);
  Firebase.setString(firebaseData,bd + WiFi.macAddress()+"/" + "rede", getValue(receivedData, ',' , 0).c_str());
  Firebase.setString(firebaseData,bd + WiFi.macAddress()+"/" + "dispositivoid", getValue(receivedData, ',' , 2).c_str());
  Firebase.setString(firebaseData,bd + WiFi.macAddress()+"/" + "nome", getValue(receivedData, ',' , 3).c_str());
  Firebase.setString(firebaseData,bd + WiFi.macAddress()+"/" + "local", getValue(receivedData, ',' , 4).c_str() );
  Firebase.setFloat(firebaseData,bd + WiFi.macAddress()+"/" + "distancia", getValue(receivedData, ',' , 5).toFloat());
}



/*
//erivelton
float medeDistancia(){  
  float cmMsec = 2;
//  long microsec = ultrasonic.timing();
//  cmMsec = ultrasonic.convert(microsec, Ultrasonic::CM);
  return cmMsec;  
}
*/
void loop() {



  /*
  //loop do erivelton
  if (medeDistancia() <= distancia){    
    qtdEntradaLocal++;
    Serial.printf("Entrou no local:%d" , qtdEntradaLocal);
    //printLocalTime();               
    digitalWrite(ledAlerta, false);   
    delay(3000);
    ledcWriteTone(channel,650);//frequencia  
  }else{  
    digitalWrite(ledAlerta, true);
    ledcWriteTone(channel,0);
  }
  delay(1000);
*/





















}
