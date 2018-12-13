//кнопки слева направо
#define BTNPREW   0
#define BTNNEXT   1
#define BTNDOWN   2
#define BTNUP     3
#define BTNENTER  4
#define BTNESC    5
#define BTNSTTME  6
#define BTNSTDEV  7
//----------------//
#define LED      13
#define ONE_WIRE_BUS 2
//меню
#define MNLIGHT   0
#define MNAERO    1
#define MNFILTR   2
#define MNHEATER  3
#define MNCOLD    4
#define MNFOOD    5

#define RELE1     6
#define RELE2     7
#define KEY       10

//type termo
#define heater 0
#define colder 1
#include <string.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "DS1307.h"
#include <TM1638lite.h>
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DS1307 clock;
TM1638lite tm(5, 4, 3);
double temperature = 0.0;
unsigned int systick = 0;
uint8_t ledTrig = 0;
uint8_t pointTrig = 1;
uint8_t segLight = 0;
struct timeStrct {
  uint8_t hr;
  uint8_t mn;
  uint8_t sec;
} globalTime;

struct tempStrct{
  uint8_t tempOn;
  uint8_t tempOff;
};
class powerKey{//класс описывающий ключи(реле, транзистор) управления
  
  uint8_t bitOn, bitOff, pin;
  char * nmDev;
  bool atached;
  public:

  void deatach(){//отвязка ключа от устройства
    atached = false;
  }

  void atach(char *val){//привязка ключа к устройству
    atached = true;
    nmDev = val;
  }
  char *getAtachDevName(){//возвращает имя устройства к которому привязана
    if(atached)
      return nmDev;
    else
      return " ";
  }
  bool isFree(){//проверка свободно ли устройство
    return !atached;
  }
  void keyOn(){//включение ключа
    digitalWrite(pin, bitOn);
    if(pin == RELE1)tm.setLED(6, 1);
    if(pin == RELE2)tm.setLED(7, 1);
  }
  void keyOff(){//отключение ключа
    digitalWrite(pin, bitOff);
    if(pin == RELE1)tm.setLED(6, 0);
    if(pin == RELE2)tm.setLED(7, 0);
  }
  powerKey(uint8_t bOn, uint8_t bOff, uint8_t _pin){
    bitOn = bOn;
    bitOff = bOff;
    pin = _pin;
    atached = false;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, bitOff);
  }
};

powerKey transistor(HIGH,LOW,KEY);
powerKey rele1(LOW,HIGH,RELE1);
powerKey rele2(LOW,HIGH,RELE2);

class Buttons { //класс описывающий кнопку, проверяет нажата ли она и сохраняет это значение до первой проверки о нажатии
    uint8_t nButt, stad;
    bool res;
  public:
    bool check(uint8_t btnState) {
      if ((btnState & (1 << nButt)) && stad == 0) {//если кнопка была нажата и перед этим не нажималась
        stad = 1;
        res = true;
        return true;
      } else if (!(btnState & (1 << nButt)) && stad == 1) {//если кнопка была отпущенна и перед этим была нажата
        stad = 0;
        return false;
      } else {//если кнопка не была нажата и не нажималась(во всех остальных случаях)
        return false;
      }
    }
    bool ifPress() {//проверяем 1 нажатие кнопки, после проверки снимаем состояние нажатия если таково было
      if (res) {
        res = false;
        return true;
      }
      return false;
    }
    Buttons() {}
    Buttons(uint8_t _nButt) {
      nButt = _nButt;
      stad = 0;
      res = false;
    }
};

Buttons btn[8];//массив кнопок


void procBtn() { //считываем показания всех кнопок и сохраняем их
  uint8_t buttonState = tm.readButtons();
  uint8_t CurButt;
  for (uint8_t i = 0; i <= 7; i++) {//заносим значение контактов кнопок в текущий момент времени в каждую "кнопку"
    btn[i].check(buttonState);
  }
}

bool isKeyPress() {//проверка была ли вобще нажата какая нибудь кнопка
  return (tm.readButtons() > 0) ? true : false;
}

int8_t getIndex(int8_t _start, int8_t _max, int8_t _cur) { //инкрементируем или декрементируем значении переменной в указанных пределах по нажатию соответствующих кнопок
  if (btn[BTNUP].ifPress()) {
    if (_cur < _max) {
      _cur++;
    } else {
      _cur = _start;
    }
  } else if (btn[BTNDOWN].ifPress()) {
    if (_cur > _start) {
      _cur--;
    } else {
      _cur = _max;
    }
  }
  return _cur;
}

double getTemp() {//считываем показание температуры
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

void show_Temp(int8_t temp, uint8_t pos) { //выводим температуру
  if (temp < 0) {
    tm.displayASCII(pos, '-');
    temp *= -1;
  }else{
    tm.displayASCII(pos, ' ');
  }
  pos++;
  if (temp > 9) {
    tm.displayInt(pos, temp);
  } else {
    tm.displayInt(pos, 0);
    tm.displayInt(pos + 1, temp);
  }
  tm.displayASCII(pos + 2, 'C');

}
void show_Time(uint8_t hr, uint8_t mn, uint8_t pos, uint8_t point) { //выводим время
  if (hr > 9) {
    tm.displayInt(pos + 0, hr, point);
  } else {
    tm.displayInt(pos + 0, 0);
    tm.displayInt(pos + 1, hr, point);
  }
  if (mn > 9) {
    tm.displayInt(pos + 2, mn);
  } else {
    tm.displayInt(pos + 2, 0);
    tm.displayInt(pos + 3, mn);
  }
}

bool isTimeInterval(timeStrct onTime, timeStrct offTime){//проверяем находится ли текущее время в заданном диапазоне
  int8_t sdvigDay = 0; 
  int8_t sdvigDay2 = 0;
  if(onTime.hr > offTime.hr){//время включения больше чем время віключения
    if(globalTime.hr >= onTime.hr && globalTime.hr <= 23){//до полуночи
      sdvigDay2 = 24;
    }else if(globalTime.hr <= offTime.hr && globalTime.hr >= 0){//после полуночи
      sdvigDay = sdvigDay2 = 24;
    }
  }
  //if(globalTime.hr >= onTime.hr && globalTime.hr <= offTime.hr){
    int a = (offTime.hr+sdvigDay2)*60+offTime.mn;
    int b = onTime.hr*60+onTime.mn;
    int c = (globalTime.hr+sdvigDay)*60+globalTime.mn;
    if(a>c && c>=b){
      return true;
    }else{
      return false;
    }
  //}else{
  //   return false;
  //}

}

class menu { //класс описывающий меню настроек(других меню нету)
    uint8_t hr, mn;
    uint8_t tempOn, tempOff;
    uint8_t curBlink, blnk;
    
  public:
    void set_Time(timeStrct &val, uint8_t pos) {
      hr = val.hr;
      mn = val.mn;
      while (!btn[BTNENTER].ifPress()) { //до тех пор пока пользователь не нажмет enter
        if (curBlink == 0) { //часы
          hr = getIndex(0, 23, hr);
          if (btn[BTNNEXT].ifPress() || btn[BTNPREW].ifPress()) {
            curBlink = 1;
          }
        } else { //минуты
          mn = getIndex(0, 59, mn);
          if (btn[BTNNEXT].ifPress() || btn[BTNPREW].ifPress()) {
            curBlink = 0;
          }
        }
        if (btn[BTNESC].ifPress()) { //если пользователь хочет выйти
          return;
        }
        show_Time(hr, mn, pos, 1);
        procBtn();
      }
      val.hr = hr;
      val.mn = mn;
    }
    
    void set_Temp(uint8_t &val) {
      uint8_t temp = val;
      while (!btn[BTNENTER].ifPress()) { //до тех пор пока пользователь не нажмет enter
        temp = getIndex(16, 32, temp);
        show_Temp(temp, 4);
        if (btn[BTNESC].ifPress()) { //если пользователь хочет выйти
          return;
        }
        procBtn();
      }
    val = temp;
    }
} mnst;

class dev {
    
  public:
    powerKey *key;
    bool devEnable;
    uint8_t pinKey;
    char *devName;
    virtual void check();
    virtual void setDev(powerKey*);
};

class timer: public dev {

    struct {
      timeStrct on;
      timeStrct off;
    } times[8];
    int8_t timesCnt;
  public:
    void setDev(powerKey *relay) {
      key = relay;
      key->atach(devName);
      for (uint8_t i = 0; i < 8; i++) {
        procBtn();
        if (!btn[BTNESC].ifPress()) { //если не нажата кнопка выхода
          tm.setLED(i, 1);
          tm.displayText("On");
          mnst.set_Time(times[i].on, 4);
          tm.displayText("Off");
          mnst.set_Time(times[i].off, 4);
          tm.setLED(i, 0);
          do {
            procBtn();
            if (btn[BTNENTER].ifPress()) {
              timesCnt = i;
              break;
            } else if (btn[BTNESC].ifPress()) {
              timesCnt = i - 1;
              return;
            }
          } while (true);
        } else {
          return;
        }
      }
    }
    void check() {
      bool trig = true;
      if (devEnable) {
        for (uint8_t i = 0; i <= timesCnt; i++) {
          if (isTimeInterval(times[i].on, times[i].off)){
            key->keyOn();
            trig = false;
          }
        }
        if(trig){
          key->keyOff();
        }
      }
    }
    timer(char *_devName) {
      devName = _devName;
      devEnable = false;
      timesCnt = 0;
      key = 0;
    }
};


class led:  public dev {
    timeStrct on;
    timeStrct off;
  public:
    void setDev(powerKey *relay) {
      key = relay;
      key->atach(devName);
      procBtn();
      if (!btn[BTNESC].ifPress()) { //если не нажата кнопка выхода
        tm.displayText("On");
        mnst.set_Time(on, 4);
        tm.displayText("Off");
        mnst.set_Time(off, 4);
     //   procBtn();
     //   if (btn[BTNENTER].ifPress())return;
     // } else {
     //   return;
      }
    }

    void check() {
      if (devEnable) {
        if (isTimeInterval(on, off)){
          key->keyOn();
        }else{
          key->keyOff();
        }
      } 
    }

    led(char *_devName) {
      devName = _devName;
      devEnable = false;
      key = 0;
    }
};




class termo:  public dev {
  tempStrct tempConf;
  public:
    void setDev(powerKey *relay) {
      key = relay;
      key->atach(devName);
      procBtn();
      if (!btn[BTNESC].ifPress()) { //если не нажата кнопка выхода
        tm.displayText("On");
        mnst.set_Temp(tempConf.tempOn);
        tm.displayText("Off");
        mnst.set_Temp(tempConf.tempOff);
      //  procBtn();
      //  if (btn[BTNENTER].ifPress())return;
      //} else {
      //  return;
      }
    }

    void check() {
      if (devEnable) {
        for (uint8_t i = 0; i < 8; i++) {
          if (tempConf.tempOn <= tempConf.tempOff) {//если у нас нагреватель (температура включения выше температуры отключения, для охладителя все наоборот)
            if(tempConf.tempOn >= temperature)
              key->keyOn();
              
            if(tempConf.tempOff <= temperature)
              key->keyOff();
            
          }else{//если у нас охладитель
            if(tempConf.tempOn <= temperature)
              key->keyOn();
              
            if(tempConf.tempOff >= temperature)
              key->keyOff();
           
          }
        }
      }
    }

    termo(char *_devName) {
      tempConf.tempOn = 22;
      tempConf.tempOff = 23;
      devName = _devName;
      devEnable = false;
      key = 0;
    }
};
//class termo: public dev{

//};
void sysCountTik() { //примитивный счетчик, всегда вызывается в конце главного цикла
  if (systick < 30000) {
    systick += 1;
  } else {
    systick = 0;
  }
  delay(1);
}
led lamp("Lamp");
timer aero("Aero");
timer filtr("Fltr");
termo heat("Heat");
termo cold("Cold");
void showInfo() {
  pointTrig = pointTrig ? 0 : 1;
  show_Time(globalTime.hr, globalTime.mn, 0, pointTrig);
  show_Temp(temperature = getTemp(), 4);
}
#define NUMOFDEVS  5
dev *devs[] = {
  &lamp,
  &aero,
  &filtr,
  &heat,
  &cold
};



void setup() {
  clock.begin();
  for (uint8_t i = 0; i <= 7; i++) {
    btn[i] = Buttons(i);
  }
  uint8_t hr, mn, sc;
  hr = mn = sc = 0;
  pinMode(LED, OUTPUT);
  tm.reset();
  tm.setLight(segLight);
  mnst.set_Time(globalTime, 4);
  clock.fillByYMD(2018, 9, 3); //Jan 19,2013
  clock.fillByHMS(globalTime.hr, globalTime.mn, 0);
  clock.setTime();
  // put your setup code here, to run once:

}

uint8_t buffChoise;
bool trig;
char stroka[9];
void loop() {

  procBtn();//считываем показания кнопок
  clock.getTime();
  globalTime.hr = clock.hour;
  globalTime.mn = clock.minute;
  //---отображения тика на плате, просто индикация того что программа крутится в цикле
  if (systick % 100 == 0) {
    ledTrig = ledTrig ? 0 : 1;
    digitalWrite(LED, ledTrig);
  }
  //---отображения времени и температуры
  if (systick % 300 == 0) {
    showInfo();
  }
  if (btn[BTNDOWN].ifPress()) { //настройка яркости 7сег модуля -
    if (segLight > 0) {
      segLight -= 1;
      tm.setLight(segLight);
    }
  }
  if (btn[BTNUP].ifPress()) { ////настройка яркости 7сег модуля +
    if (segLight < 1) {
      segLight += 1;
      tm.setLight(segLight);
    }
  }
  if (btn[BTNSTTME].ifPress()) { //настройка времени
    mnst.set_Time(globalTime, 4);
    clock.fillByHMS(globalTime.hr, globalTime.mn, 0);
    clock.setTime();
  }
  if (btn[BTNSTDEV].ifPress()) { //настройка устройств
    uint8_t menuDevCnt = 0;
    while (true) {
      procBtn();//считываем показания кнопок
      menuDevCnt = getIndex(0, NUMOFDEVS-1, menuDevCnt);
      if(menuDevCnt == 0){//для транзисторного ключа
        stroka[0] = 0;
        strcat(stroka, devs[menuDevCnt]->devName);
        if (devs[menuDevCnt]->devEnable){
          strcat(stroka, "  on");
        }else{
          strcat(stroka, " off");
        }
        tm.displayText(stroka);
        if (btn[BTNENTER].ifPress()) {
          if (devs[menuDevCnt]->devEnable) {
            devs[menuDevCnt]->devEnable = false;
          } else {
            devs[menuDevCnt]->devEnable = true;
            devs[menuDevCnt]->setDev(&transistor);
            devs[menuDevCnt]->key->atach(devs[menuDevCnt]->devName);
          }
        }
      }else{//для релейных ключей
        
        stroka[0] = 0;
        strcat(stroka, devs[menuDevCnt]->devName);
        if (devs[menuDevCnt]->devEnable){
          strcat(stroka, "  on");
        }else{
          strcat(stroka, " off");
        }
        tm.displayText(stroka);
        if (btn[BTNENTER].ifPress()) {
          if (devs[menuDevCnt]->devEnable) {
            devs[menuDevCnt]->devEnable = false;
            if(devs[menuDevCnt]->key){
              devs[menuDevCnt]->key->keyOff();
              devs[menuDevCnt]->key->deatach();
              devs[menuDevCnt]->key = 0;
            }
          } else {
            devs[menuDevCnt]->devEnable = true;     //включаем устройство
            trig = true;
            buffChoise = 0;
            do {                                    //выбираем к какому реле подключится и конфигурируем устройство
              procBtn();                            //считываем показания кнопок
              switch (buffChoise = getIndex(0, 2, buffChoise)) {  //
                case 0:
                  tm.displayText("chs rele");
                  break;
                case 1:
                  if(rele1.isFree()){//если ключ свободен
                    tm.displayText("to rele1");
                    procBtn();
                    if (btn[BTNENTER].ifPress()) {
                      devs[menuDevCnt]->setDev(&rele1);
                      trig = false;
                    }
                  }else{//если ключ занят
                    stroka[0]=0;
                    strcat(stroka, rele1.getAtachDevName());
                    strcat(stroka, " bL1");
                    tm.displayText(stroka);//выводим имя устройства которое заняло ключ
                  }
                  break;
                case 2:
                  if(rele2.isFree()){
                    tm.displayText("to rele2");
                    procBtn();
                    if (btn[BTNENTER].ifPress()) {
                      devs[menuDevCnt]->setDev(&rele2);
                      trig = false;
                    }
                  }else{
                    stroka[0]=0;
                    strcat(stroka, rele1.getAtachDevName());
                    strcat(stroka, " bL2");
                    tm.displayText(stroka);//выводим имя устройства которое заняло ключ
                  }
                  break;
              }
              procBtn(); 
                  if (btn[BTNESC].ifPress()) {
                    trig = false;
                  }
            } while (trig);
            //
          }
        }
      }
      if (btn[BTNESC].ifPress()) {
        break;
      }
    }
  }
  for(uint8_t x = 0; x < NUMOFDEVS; x++){
    devs[x]->check();
  }
  sysCountTik();
}
