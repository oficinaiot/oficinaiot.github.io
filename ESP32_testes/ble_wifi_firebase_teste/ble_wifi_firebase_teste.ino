#include "EEPROM.h"
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <FirebaseESP32.h>

#define EEPROM_SIZE 128
#define SERVICE_UUID        "87b34f52-4765-4d3a-b902-547751632d72"
#define CHARACTERISTIC_UUID "a97d209a-b1d6-4edf-b67f-6a0c25fa42c9"
#define TARGET_DEVICE_NAME "AdestraKit"

#define Host "https://adestrakit.firebaseio.com/"
#define Senha_Fire "8ZMdyCFrQ9KRMFPJOuVkRrNRtdAcCB9BKDy6UIRx"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define ledBle 2 //azul
#define ledWifi 4 //verde
const int modeAddr = 0;
const int wifiAddr = 10;
String receivedData;

int modeIdx;

//Define FirebaseESP32 data object
FirebaseData firebaseData;
const String bd = "configEsp/";

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
        Serial.print("Value Callback: ");
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

void setup() {
  Serial.begin(115200);
  pinMode(ledBle, OUTPUT);
  pinMode(ledWifi, OUTPUT);

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
    digitalWrite(ledBle, false);//liga no false
    digitalWrite(ledWifi, true);
    Serial.println("BLE MODE");
    bleTask();
  } else {
    //Wifi mode, verde
    digitalWrite(ledWifi, false);//liga no false
    digitalWrite(ledBle, true);
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
  Serial.println("Aguardando uma conexão Ble...");
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
      
      Serial.print("Conectando na rede Wifi: ");
      //Serial.println(wifiName);
      //Serial.println(WiFi.begin(wifiName.c_str());
      Serial.println(getValue(receivedData, ',' , 0).c_str());
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



void loop() {
  // put your main code here, to run repeatedly:
























}
