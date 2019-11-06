#include "Arduino.h"
#include "EEPROM.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Ultrasonic.h>
#include "time.h"
#include "FirebaseJson.h"


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

//pinos em uso
#define PIN_TRIG_CENTRO 32  //amarelo - envia
#define PIN_ECHO_CENTRO 35  //verde - recebe
#define ledBle 2 //azul
#define ledWifi 4 //verde
#define ledInvasao 16 //vermelho ou branco no dispositivo final
#define BUZZER_PIN 33//pino do buzzer

//variáveis globais
const int modeAddr = 0;
int modeIdx;
const int wifiAddr = 10;
String receivedData;
unsigned int pingSpeed = 1500;//substirui o delay para a busca de dados e mantem as outras atividades funcionando
unsigned long pingTimer;
Ultrasonic sensorCentro(PIN_TRIG_CENTRO, PIN_ECHO_CENTRO);
unsigned int distanciaLimite;
unsigned int distanciaAviso;

//configurando o buzzer
#define CHANELL    0
#define FREQUENCE  25000
#define RESOLUTION 8

//ajustando o horário
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -3600 * 3;
const int   daylightOffset_sec = 3600;

//Define FirebaseESP32 data object
FirebaseData firebaseData;
const String bdConfigEsp = "configEsp/";
const String bdDispositivosUser = "dispositivosUser/";
const String bdAlarmes = "alarme/";

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
        Serial.print("Value Callback: ");

      //recebimento do ble
      //AroldoGisele,Ar01d0&Gi5373,12345678,Aroldo sala,Sala,45,Bl9gZlathLdzZajuCXKFAPqWjm03
      //var wifidata = rede+","+senhaenviada+","+dispositivoid+","+nome+","+local+","+distancia;
      //wifidata = redeWifi,senhaWifi,dispositivoid,nomeAdestra,localAdestra,distanciaLimite,userIdFirebase
      //wifidata[0] = redeWifi,
      //wifidata[1] = senhaWifi
      //wifidata[2] = senhaCompartilhamento,
      //wifidata[3] = nomeAdestra,
      //wifidata[4] = localAdestra,
      //wifidata[5] = distanciaLimite,
      //wifidata[6] = userIdFirebase
      
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
  pinMode(ledInvasao, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIN_TRIG_CENTRO, OUTPUT);
  pinMode(PIN_ECHO_CENTRO, INPUT);

  //colocando o sensor - sonar
  ledcSetup(CHANELL, FREQUENCE, RESOLUTION);
  ledcAttachPin(BUZZER_PIN, CHANELL);

  //definindo para deixar não usar com o delay
  pingTimer = millis();

  if (!EEPROM.begin(EEPROM_SIZE)) {
    delay(1000);
    Serial.println("Erro Epron");
  }
  modeIdx = EEPROM.read(modeAddr);
  Serial.print("modeIdx: ");
  Serial.println(modeIdx);

  EEPROM.write(modeAddr, modeIdx != 0 ? 0 : 1 );
  EEPROM.commit();

  if (modeIdx != 0) {
    bleTask();
  } else {
    wifiTask();
  }
}


void bleTask() {
    //BLE Mode, azul
    digitalWrite(ledBle, false);//liga o ledo do bluetooth
    digitalWrite(ledWifi, true);//desliga o led do wifi
    Serial.println("BLE MODE");
  
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
  digitalWrite(ledBle, true);//desliga o ledBle
  Serial.println("WIFI MODE");
  
  receivedData = read_string(wifiAddr);

  if (receivedData.length() > 0 ) {
      if (getValue(receivedData, ',' , 0).length() > 0 && getValue(receivedData, ',' , 1).length() > 0) {

      WiFi.begin(getValue(receivedData, ',' , 0).c_str(),getValue(receivedData, ',' , 1).c_str());

      //para usar e emitir um aviso;
      //wifidata[5] = distanciaLimite,
      distanciaLimite = atoi(getValue(receivedData, ',' , 5).c_str());
      distanciaAviso = distanciaLimite * 1.25;
      Serial.print("distanciaLimite: ");
      Serial.println(distanciaLimite);
      Serial.print("distanciaAviso: ");
      Serial.println(distanciaAviso);
      
      Serial.print("Conectando na rede Wifi: ");
      Serial.println(getValue(receivedData, ',' , 0).c_str());
      Serial.println("Senha da Rede Wifi: Oculta");

      while (WiFi.status() != WL_CONNECTED) {
        
        digitalWrite(ledWifi, true);
        delay(300);        
        digitalWrite(ledWifi, false);//liga no false
        delay(300);
        Serial.print(".");
      }
      
      Serial.println();
      Serial.print("WiFi conectada com IP:");
      Serial.println(WiFi.localIP());
      Serial.println();
    }
    
    //ajusta a data e hora atual
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    //conecta no firebase
    Firebase.begin(Host, Senha_Fire); 

    //configuração inicial do firebase
    inicializaFirebase();
  }
}

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
  Serial.println(data);
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

void inicializaFirebase(){
  Firebase.setBool(firebaseData,bdConfigEsp   + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress() + "/" + "ligado", true);
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress() + "/" + "redeWifi", getValue(receivedData, ',' , 0).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress() + "/" + "senhaCompartilhamento", getValue(receivedData, ',' , 2).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress() + "/" + "nomeAdestra", getValue(receivedData, ',' , 3).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress() + "/" + "localAdestra", getValue(receivedData, ',' , 4).c_str() );
  Firebase.setFloat(firebaseData,bdConfigEsp  + getValue(receivedData, ',' , 6).c_str()  +  "/" + WiFi.macAddress() + "/" + "distanciaLimite", getValue(receivedData, ',' , 5).toFloat());
      //wifidata[0] = redeWifi,
      //wifidata[1] = senhaWifi
      //wifidata[2] = senhaCompartilhamento,
      //wifidata[3] = nomeAdestra,
      //wifidata[4] = localAdestra,
      //wifidata[5] = distanciaLimite,
      //wifidata[6] = userIdFirebase
}
//MPhZCXF8mnUlUvcUHp30Msupnd53 - Elaine id

//StaticJsonBuffer<200> jsonBuffer;
//JsonObject &root = jsonBuffer.createObject();

String dataAtualFormatada()
{
  struct tm timeinfo;
  char data_formatada[64];
  if(!getLocalTime(&timeinfo)){
    return String("Erro ao obter a o horário");
  }
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //Serial.println(&timeinfo, "%d/%m/%Y %H:%M:%S");
  //strftime(data_formatada, 64, "%d/%m/%Y %H:%M:%S", &timeinfo);//Cria uma String formatada da estrutura "data"
  strftime(data_formatada, 64, "%Y/%m/%d %H:%M:%S", &timeinfo);//Cria uma String formatada da estrutura "data"
  strftime(data_formatada, 64, "%Y/%m/%d %H:%M:%S", &timeinfo);//Cria uma String formatada da estrutura "data"
  Serial.println();
  Serial.println(data_formatada);
  return data_formatada;
}


void sendAlarme(int distMedida){
  String dataAtual = dataAtualFormatada();

  FirebaseJson json2;
  json2.set("distanciaAferida", distMedida);
  json2.set("data", getValue(dataAtual, ' ' , 0).c_str());
  json2.set("hora", getValue(dataAtual, ' ' , 1).c_str());  
  
  Firebase.pushJSON(firebaseData, bdAlarmes + WiFi.macAddress()+"", json2);

  String idFirebase = firebaseData.pushName();
  Firebase.setTimestamp(firebaseData,bdAlarmes + WiFi.macAddress() + "/" + idFirebase +"/timestamp" );
  Serial. println (idFirebase);
  Serial. println (firebaseData.dataPath()); 

  //assim 
  Firebase.setInt(      firebaseData,bdAlarmes + WiFi.macAddress() + "/" + getValue(dataAtual, ' ' , 0).c_str() + "/" + getValue(dataAtual, ' ' , 1).c_str() + "/" + "distanciaAferida", distMedida);
  Firebase.setString(   firebaseData,bdAlarmes + WiFi.macAddress() + "/" + getValue(dataAtual, ' ' , 0).c_str() + "/" + getValue(dataAtual, ' ' , 1).c_str() + "/" + "data", getValue(dataAtual, ' ' , 0).c_str());
  Firebase.setString(   firebaseData,bdAlarmes + WiFi.macAddress() + "/" + getValue(dataAtual, ' ' , 0).c_str() + "/" + getValue(dataAtual, ' ' , 1).c_str() + "/" + "hora", getValue(dataAtual, ' ' , 1).c_str());

//assim 
  Firebase.pushInt(      firebaseData,bdAlarmes + WiFi.macAddress() + "/" + getValue(dataAtual, ' ' , 0).c_str() + "/" + "distanciaAferida", distMedida);
  Firebase.pushString(   firebaseData,bdAlarmes + WiFi.macAddress() + "/" + getValue(dataAtual, ' ' , 0).c_str() + "/" + "data", getValue(dataAtual, ' ' , 0).c_str());
  Firebase.pushString(   firebaseData,bdAlarmes + WiFi.macAddress() + "/" + getValue(dataAtual, ' ' , 0).c_str() + "/" + "hora", getValue(dataAtual, ' ' , 1).c_str());

  
  
 //assim se usar o timestamp neste ponto apaga os valores acima
  //Firebase.setTimestamp(firebaseData,bdAlarmes + WiFi.macAddress());
  //Firebase.pushTimestamp(firebaseData,bdAlarmes + WiFi.macAddress());
  //Firebase.pushInt( firebaseData,bdAlarmes + WiFi.macAddress() + "/" + dataAtual + "/" + "distanciaAferida", distMedida);
  //Firebase.pushString(   firebaseData,bdAlarmes + WiFi.macAddress() + "/" + dataAtual + "/" + "data", getValue(dataAtual, ' ' , 0).c_str());
  //Firebase.pushString(   firebaseData,bdAlarmes + WiFi.macAddress() + "/" + dataAtual + "/" + "hora", getValue(dataAtual, ' ' , 1).c_str());
 
  //assim 
  //Firebase.pushString( firebaseData,bdAlarmes + WiFi.macAddress()+"/",jsonStr);
  //Firebase.pushInt( firebaseData,bdAlarmes + "/" + WiFi.macAddress() + "/" + Firebase.pushTimestamp + "/" + "distanciaAferida", distMedida);
  //Firebase.pushString(   firebaseData,bdAlarmes + "/" + WiFi.macAddress() + "/" + dataAtual + "/" + "data", getValue(dataAtual, ' ' , 0).c_str());
  //Firebase.pushString(   firebaseData,bdAlarmes + "/" + WiFi.macAddress() + "/" + dataAtual + "/" + "hora", getValue(dataAtual, ' ' , 1).c_str());

  //assim
  /*
  Firebase.pushtInt( firebaseData,bdAlarmes + "/" + WiFi.macAddress() + "/" + getValue(dataAtual, ' ' , 0).c_str() + "/" + "distanciaAferida", distMedida);
  Firebase.pushString(   firebaseData,bdAlarmes + "/" + WiFi.macAddress() + "/" + dataAtual + "/" + "data", getValue(dataAtual, ' ' , 0).c_str());
  Firebase.pushString(   firebaseData,bdAlarmes + "/" + WiFi.macAddress() + "/" + dataAtual + "/" + "hora", getValue(dataAtual, ' ' , 1).c_str());
  */
 // Firebase.setString( firebaseData,bdAlarmes + "/" + WiFi.macAddress()+"/" + "nome", getValue(receivedData, ',' , 3).c_str());
 // Firebase.setString( firebaseData,bdAlarmes + "/" + WiFi.macAddress()+"/" + "local", getValue(receivedData, ',' , 4).c_str() );
 // Firebase.setFloat ( firebaseData,bdAlarmes + "/" + WiFi.macAddress()+"/" + "distancia", getValue(receivedData, ',' , 5).toFloat());
}





void loop() {

  if (millis() >= pingTimer){
    //bool invasao = false;
    pingTimer += pingSpeed;
    int distMedida = sensorCentro.read(CM);
    Serial.print(F("Sensor centro: "));
    Serial.print(distMedida); // a distância default é em cm
    Serial.println(F("cm"));

    if (distMedida <= distanciaLimite && distMedida > 0 ) {
        //ligando o led branco    
        digitalWrite(ledInvasao, LOW);   
        //ligando o Buzzer
        ledcWriteTone(CHANELL, FREQUENCE);
        //enviando os dados de alarme
        sendAlarme(distMedida);
    }else{  
        //desligando o led branco
        digitalWrite(ledInvasao, HIGH);
        //desligando o buzzer
        ledcWriteTone(CHANELL,0);
        //ledcWrite(CHANELL, 0);
    }
    
    }
}
