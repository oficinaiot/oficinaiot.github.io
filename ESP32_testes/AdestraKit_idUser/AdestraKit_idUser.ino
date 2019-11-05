#include "Arduino.h"
#include "EEPROM.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Ultrasonic.h>

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
unsigned int distanciaLimite = 50;

//configurando o buzzer
#define CHANELL    0
#define FREQUENCE  200
#define RESOLUTION 10


//criando o sonar
//#define SONAR_NUM 1      // Number of sensors.
//#define MAX_DISTANCE 500 // Maximum distance (in cm) to ping.
//usando este para os testes unitários
//#define PIN_TRIG_DIR   //amarelo
//#define PIN_ECHO_DIR   //verde
//#define PIN_TRIG_ESQ   //amarelo
//#define PIN_ECHO_ESQ   //verde
//Ultrasonic sensorDireito(PIN_TRIG_DIR,PIN_ECHO_DIR);
//Ultrasonic sendorEsquerdo(PIN_TRIG_ESQ,PIN_ECHO_ESQ);
/*
NewPing sonar[SONAR_NUM] = {   // Sensor object array.
  //NewPing(PIN_TRIG_DIR, PIN_ECHO_DIR, MAX_DISTANCE), // Each sensor's trigger pin, echo pin, and max distance to ping. 
  NewPing(PIN_TRIG_CENTRO, PIN_ECHO_CENTRO, MAX_DISTANCE), //
  //NewPing(PIN_TRIG_ESQ, PIN_ECHO_ESQ, MAX_DISTANCE)
};
*/


//Define FirebaseESP32 data object
FirebaseData firebaseData;
const String bdConfigEsp = "configEsp/";
const String bdDispositivosUser = "dispositivosUser/";
const String bdAlarmes = "alarmes/";

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
  //comentando para fazer testes com as novas variáveis até que o código esteja recebendo informações do app
  //String receivedData;
  receivedData = read_string(wifiAddr);

  //receivedData = "rede,senha,dispositivoid,nome,local,15";
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
        //Wifi mode, verde
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

void sendFirebase(){
  //temperatura
  //Firebase.setString(firebaseData,bdConfigEsp + WiFi.macAddress()+"/" + "macAddress", WiFi.macAddress());
  //Firebase.setFloat(firebaseData,bdConfigEsp + WiFi.macAddress()+"/" + "distancia_alerta", 30 );

 //wifidata[0] = redeWifi,
      //wifidata[1] = senhaWifi
      //wifidata[2] = senhaCompartilhamento,
      //wifidata[3] = nomeAdestra,
      //wifidata[4] = localAdestra,
      //wifidata[5] = distanciaLimite,
      //wifidata[6] = userIdFirebase
for (int i = 0 ; i < 20 ; i++){
  String val = ""+i;
  Firebase.setBool(firebaseData,bdConfigEsp + WiFi.macAddress()+val+"/" + "ligado", true);
  Firebase.setString(firebaseData,bdConfigEsp + WiFi.macAddress()+val+"/" + "redeWifi", getValue(receivedData, ',' , 0).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + WiFi.macAddress()+val+"/" + "senhaCompartilhamento", getValue(receivedData, ',' , 2).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + WiFi.macAddress()+val+"/" + "nomeAdestra", getValue(receivedData, ',' , 3).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + WiFi.macAddress()+val+"/" + "localAdestra", getValue(receivedData, ',' , 4).c_str() );
  Firebase.setFloat(firebaseData,bdConfigEsp + WiFi.macAddress()+val+"/" + "distanciaLimite", getValue(receivedData, ',' , 5).toFloat());
  Firebase.setString(firebaseData,bdConfigEsp + WiFi.macAddress()+val+"/" + "userIdFirebase", getValue(receivedData, ',' , 6).c_str() );

  //bdDispositivosUser
  Firebase.setString(firebaseData,bdDispositivosUser + getValue(receivedData, ',' , 6).c_str() + "/" + "dispositivoId", WiFi.macAddress()+i);

  //Firebase.setString(firebaseData,bdDispositivosUser + WiFi.macAddress()+"/" + "userIdFirebase", getValue(receivedData, ',' , 6).c_str() );
  
  //jeito2
  Firebase.setBool(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress()+val+ "/" + "ligado", true);
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress()+val+ "/" + "redeWifi", getValue(receivedData, ',' , 0).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress()+val+ "/" + "senhaCompartilhamento", getValue(receivedData, ',' , 2).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress()+val+ "/" + "nomeAdestra", getValue(receivedData, ',' , 3).c_str());
  Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress()+val+ "/" + "localAdestra", getValue(receivedData, ',' , 4).c_str() );
  Firebase.setFloat(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() +  "/" + WiFi.macAddress()+val+ "/" + "distanciaLimite", getValue(receivedData, ',' , 5).toFloat());
  //Firebase.setString(firebaseData,bdConfigEsp + getValue(receivedData, ',' , 6).c_str() + WiFi.macAddress()+ "/" + "uderId", getValue(receivedData, ',' , 6).c_str() );
}
}
//MPhZCXF8mnUlUvcUHp30Msupnd53 - Elaine id

void sendAlarme(int distancia){
  //Firebase.setString(firebaseData,bdConfigEsp +"/" + WiFi.macAddress()+"/" + "macAddress", WiFi.macAddress().c_str());
  //Firebase.setFloat(firebaseData,bdConfigEsp +"/" + WiFi.macAddress()+"/" + "distancia_alerta", 30 );
  
  Firebase.setBool(firebaseData,bdAlarmes +"/" + WiFi.macAddress()+"/" + "ligado", true);
  Firebase.setString(firebaseData,bdAlarmes +"/" + WiFi.macAddress()+"/" + "rede", getValue(receivedData, ',' , 0).c_str());
  Firebase.setString(firebaseData,bdAlarmes + "/" +WiFi.macAddress()+"/" + "dispositivoid", getValue(receivedData, ',' , 2).c_str());
  Firebase.setString(firebaseData,bdAlarmes +"/" + WiFi.macAddress()+"/" + "nome", getValue(receivedData, ',' , 3).c_str());
  Firebase.setString(firebaseData,bdAlarmes + "/" + WiFi.macAddress()+"/" + "local", getValue(receivedData, ',' , 4).c_str() );
  Firebase.setFloat(firebaseData,bdAlarmes + "/" +WiFi.macAddress()+"/" + "distancia", getValue(receivedData, ',' , 5).toFloat());
}


void loop() {
  //Value Callback: AroldoGisele,Ar01d0&Gi5373,12345678,Aroldo sala,Sala,45,Bl9gZlathLdzZajuCXKFAPqWjm03
  
  
  //assim o processador fica livre para outras tarefas substitui o delay mais eficiente

  //assim o processador fica livre para outras tarefas substitui o delay mais eficiente

  if (millis() >= pingTimer){
    pingTimer += pingSpeed;
    int distMedida = sensorCentro.read(CM);
    Serial.print(F("Sensor centro: "));
    Serial.print(distMedida); // a distância default é em cm
    Serial.println(F("cm"));

    if (distMedida <= distanciaLimite && distMedida > 0 ) {    
        //qtdEntradaLocal++;
        //Serial.printf("Entrou no local:%d" , qtdEntradaLocal);
        //printLocalTime();               
        digitalWrite(ledInvasao, LOW);   
        ledcWriteTone(CHANELL, FREQUENCE);
        //ledcWrite(CHANELL, 1024);
    }else{  
        digitalWrite(ledInvasao, HIGH);
        //desligando o buzzer
        ledcWriteTone(CHANELL,0);
        //ledcWrite(CHANELL, 0);
    }
    
    }
  /*
   * 
   * 
   * 

ledcWriteTone(channel, freq);
     delay(tempoDelay);
     dutyCycle = freq;
     Serial.print("dutyCycle: ");
     Serial.println(dutyCycle);
     ledcWrite(channel, dutyCycle);
     delay(tempoDelay);
     Serial.print("channel: ");
     Serial.print(channel);
     Serial.print("freq: ");
     Serial.print(freq);
     Serial.print("dutyCycle: ");
     Serial.println(dutyCycle);
     ledcSetup(channel, freq, dutyCycle);

  
  // put your main code here, to run repeatedly:
  for (uint8_t i = 0; i < SONAR_NUM; i++) { // Loop through each sensor and display results.
    delay(50); // Wait 50ms between pings (about 20 pings/sec). 29ms should be the shortest delay between pings.
    Serial.print(i);
    Serial.print("=");
    Serial.print(sonar[i].ping_cm());
    Serial.print("cm ");
      if (sonar[i].ping_cm() != 0 && sonar[i].ping_cm() < 50){
        ledcWriteTone(CHANELL,FREQUENCE);
        }else{
          //ledcWriteTone(CHANELL, 0);
        }
    delay(500);
  }
  Serial.println();
*/




















}
