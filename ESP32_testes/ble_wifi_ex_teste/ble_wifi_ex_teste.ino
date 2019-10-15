#include "EEPROM.h"
#define EEPROM_SIZE 128

#define ledBle 2 //azul
const int modeAddr = 0;
int modeIdx;


void setup() {
  Serial.begin(115200);
  pinMode(ledBle, OUTPUT);

  if(!EEPROM.begin(EEPROM_SIZE)){
    delay(1000);
  }
  modeIdx = EEPROM.read(modeAddr);
  Serial.print("modeIdx: ");
  Serial.println(modeIdx);

  EEPROM.write(modeAddr, modeIdx !=0 ? 0 : 1 );
  EEPROM.commit();

  if(modeIdx != 0){
    //BLE Mode, azul
    digitalWrite(ledBle, false);//liga no false
    Serial.println("BLE MODE");
  }else{
    //Wifi mode, verde
    digitalWrite(ledBle, true);
    Serial.println("WIFI MODE");  
  }
}

void loop() {
  // put your main code here, to run repeatedly:

}
