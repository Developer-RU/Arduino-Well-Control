//#include <TimerOne.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>
#include <string.h>

#define SSerialRX        11  //Serial Receive pin
#define SSerialTX        12  //Serial Transmit pin

#define SSerialTxControl 2   //RS485 Direction control
#define RS485Transmit    HIGH
#define RS485Receive     LOW

#define Pin13LED         13

SoftwareSerial RS485Serial(SSerialRX, SSerialTX); // RX, TX

int byteReceived;
int byteSend;

#define ONE_WIRE_BUS 9 //датчик темпиратуры скважины
OneWire oneWire(ONE_WIRE_BUS);

DallasTemperature sensors(&oneWire);

DeviceAddress Thermometer1 = {0x28, 0xFF, 0x60, 0x2A, 0x15, 0x15, 0x02, 0xE2 };
  
DeviceAddress Thermometer2 = {0x28, 0x9E, 0x59, 0x5B, 0x15, 0x15, 0x02, 0xB2 };
  
// адреса памяти EEPROM

// 0; //первый запуск автономной работы
// 1; //минимальная темпиратура
// 2; //максимальная темпиратура
// 3; //минимальное давление
// 4; //максимальное давление
// 5; //режим работы насоса
// 6; //режим управления нагревом

int pinPressureSensor = 2; //пин датчика давления
int pinAvailabilityWater = 6; //пин наличие воды (уровень воды)
int pinRelayPump = 4; //пин реле насоса
int pinRelayHeating = 5; //пин реле обогрева трубы

int minTemperature = -2; //минимальная темпиратура
int maxTemperature = 34; //максимальная темпиратура

int minPressure = 250; //минимальное давление
int maxPressure = 650; //максимальное давление

int temperaturePipe = 0;  //текущая темпиратура воды в трубе
int temperatureWater = 0; //текущая темпиратура воды в скважине
int pressure = 0; //текущее давление
bool isWater = false; // воды нет по умолчанию

int manualModePipe = 0; //ручное управление насосом
int manualModeTan = 0; //ручное управление подогревом

int dataSending = 0;

char UnitID[10];
char Parameters[200];
char Command[10];
char buffer[100];

int currentTime = 0;

void setup(void) 
{
    //устанавливаем пины на входы|выходы
    pinMode(Pin13LED, OUTPUT);     
    pinMode(pinPressureSensor, INPUT);
    pinMode(pinAvailabilityWater, INPUT);
    pinMode(pinRelayPump, OUTPUT);
    pinMode(pinRelayHeating, OUTPUT);

    //устанавливаем начальные значения пинам
    digitalWrite(pinRelayPump, LOW);
    digitalWrite(pinRelayHeating, LOW);
         
    //Скачиваем настройки из епром, если они есть и применяем
    if(int(EEPROM.read(0)) == 0)
    {
         EEPROM.write(0, 1); 
         
         EEPROM.write(1, minTemperature); 
         EEPROM.write(2, maxTemperature); 
         EEPROM.write(3, minPressure); 
         EEPROM.write(4, maxPressure); 

         EEPROM.write(5, manualModePipe); 
         EEPROM.write(6, manualModeTan); 
    }
    else
    {
        minTemperature = int(EEPROM.read(1));
        maxTemperature = int(EEPROM.read(2));
        minPressure = int(EEPROM.read(3));
        maxPressure = int(EEPROM.read(4));
        manualModePipe = int(EEPROM.read(5));
        manualModeTan = int(EEPROM.read(6));
    }

        
    Serial.begin(57600);
    
    pinMode(SSerialTxControl, OUTPUT);  
    digitalWrite(SSerialTxControl, RS485Receive); //Включение на прием RS485

    RS485Serial.begin(57600);   // set the data rate 
    
    sensors.begin();
    sensors.setResolution(Thermometer1, 10); //(темпиратура трубы)
    sensors.setResolution(Thermometer2, 10); //(темпиратура воды)

    //Timer1.initialize();
    //Timer1.attachInterrupt(Timer_action);
    //currentTime = 1;

    //Timer1.start();
}


void Timer_action()
{ 
    if(currentTime >= 30000)
    {
        sensors.requestTemperatures();
    
        //получаем темпиратуру трубы 
        temperaturePipe = int(sensors.getTempC(Thermometer1) + 0.5); //округляем до целых чисел
        delay(1000);
        
        //получаем темпиратуру воды
        temperatureWater = int(sensors.getTempC(Thermometer2) + 0.5); //округляем до целых чисел
        delay(1000);
  
        currentTime = 1;
    }
    
    //проверяем уровень воды
    if(digitalRead(pinAvailabilityWater) > 0) //возможно надо проверять HIGH или LOW
    {
        isWater = true;
    }
    else
    {
        isWater = false;
    }

    //считываем давление 
    pressure = analogRead(0);
    
    //pressure = digitalRead(pinPressureSensor);

    currentTime++;
}

//прием данных с сервера
void getData485()
{
    //пока не будем принимать - так как придется дописать свитч на разбор данных и адресацию устройств
    
    if (RS485Serial.available()) 
    {        
        digitalWrite(Pin13LED, HIGH);  // включаем светодиод пока данные принимаются

        int i = 0; 
        
        if(RS485Serial.available())
        {
            //delay(100);
            while( RS485Serial.available() && i < 99) 
            { 
                buffer[i++] = RS485Serial.read();
            } 
            
            buffer[i++] = '\0';
        }
        
        if(i > 0)
        {
            Serial.print(buffer); // Выводим что приняли с других устройств
            Serial.print("\n"); // Выводим что приняли с других устройств
            //разбираем принятую строку
            sscanf(buffer, "%s%s%s", &UnitID, &Command, &Parameters);
        }
        
       //--------------данные уже получили - теперь разбор------------------------//
        
        // если пришла команда S - то в параметрах строки настроек
        if((String)UnitID == "ID01" && (String)Command == "S")
        {

            //разбираем строку по знаку ":"
            String Str = String((String)Parameters);
            Str.toCharArray(buffer, 18);

            //применяем параметры
            minTemperature = atoi(strtok(buffer,":"));
            Serial.print("Set min temperature: " + String(minTemperature) + "\n");
            maxTemperature = atoi(strtok(NULL,":"));
            Serial.print("Set max temperature: " + String(maxTemperature) + "\n");
            minPressure = atoi(strtok(NULL,":"));
            Serial.print("Set min pressure: " + String(minPressure) + "\n");
            maxPressure = atoi(strtok(NULL,":"));
            Serial.print("Set max pressure: " + String(maxPressure) + "\n");
            manualModePipe = atoi(strtok(NULL,":"));
            Serial.print("Set manual mode pumpe: " + String(manualModePipe) + "\n");
            manualModeTan = atoi(strtok(NULL,":"));
            Serial.print("Set manual mode pipe: " + String(manualModeTan) + "\n");
              
            //сохраняем параметры в епром
            EEPROM.write(1, minTemperature); 
            EEPROM.write(2, maxTemperature); 
            EEPROM.write(3, minPressure); 
            EEPROM.write(4, maxPressure); 
            EEPROM.write(5, manualModePipe); 
            EEPROM.write(6, manualModeTan); 

            //обнуляем буферы данных
            UnitID [0] = '\0';
            Command [0] = '\0';
            Parameters [0] = '\0';
        }
        
        else if((String)UnitID == "ID01" && (String)Command == "T")
        {
            // ТЕСТ СВЯЗИ С КЛИЕНТОМ
            digitalWrite(SSerialTxControl, RS485Transmit); 
            
            //передаем данные
            RS485Serial.print("ID01");
            digitalWrite(SSerialTxControl, RS485Receive); 
        }
        
        //если в параметрах пришла команда М - то команда ручного управления
        else if((String)UnitID == "ID01" && (String)Command == "M")
        {                    
            if((String)Parameters == "11") //включаем/выключаем насос
            {
                digitalWrite(pinRelayPump, HIGH); //включили насос
            }

            if((String)Parameters == "22") //включаем/выключаем насос
            {
                digitalWrite(pinRelayPump, LOW); //выключили насос
            }

            if((String)Parameters == "33") //включаем/выключаем подогрев трубы
            {
                digitalWrite(pinRelayHeating, HIGH); //включили подогрев
            }

            if((String)Parameters == "44") //включаем/выключаем подогрев трубы
            {
                digitalWrite(pinRelayHeating, LOW); //выключили подогрев              
            }

            if((String)Parameters == "55") //включаем/выключаем подогрев трубы
            {
                manualModePipe = int(EEPROM.read(5));
                if(manualModePipe == 0)
                    EEPROM.write(5, 1);                
                else
                    EEPROM.write(5, 0);                
                manualModePipe = int(EEPROM.read(5));                
            }
            
            if((String)Parameters == "66") //включаем/выключаем подогрев трубы
            {
                manualModeTan = int(EEPROM.read(6));
                if(manualModeTan == 0)
                    EEPROM.write(6, 1);                
                else
                    EEPROM.write(6, 0);                
                manualModeTan = int(EEPROM.read(6));              
            }

            //обнуляем буферы данных
            UnitID [0] = '\0';
            Command [0] = '\0';
            Parameters [0] = '\0';
        }
        
        //если D - отдаем строкой все значения датчиков и реле
        else if((String)UnitID == "ID01" && (String)Command == "D")
        {
            //вклчаем передачу
            digitalWrite(SSerialTxControl, RS485Transmit); 

            //узнаем состояние помпы(насоса)
            String p = "00";
            if(digitalRead(pinRelayPump) == HIGH) p = "11";

            //Узнаем состояние нагрева
            String h = "00";
            if(digitalRead(pinRelayHeating) == HIGH) h = "11";              

            //Проверяем наличие воды
            String w = "00";
            if(isWater) w = "11";
            
            //формируем строку с данными для ответа
            String im = "ID01 D "                       
                      + String(temperaturePipe) + ":" 
                      + String(temperatureWater) + ":"
                      + String(pressure) + ":"
                      + p + ":"
                      + h + ":"
                      + w + ":"
                      + String(manualModePipe) + ":"
                      + String(manualModeTan) + ":"
                      + String(minTemperature) + ":"
                      + String(maxTemperature) + ":"
                      + String(minPressure) + ":"
                      + String(maxPressure);
                                      
            
            //передаем данные
            RS485Serial.print(im);
            Serial.print("\n"); Serial.print(im); Serial.print("\n"); // Выводим что приняли с других устройств           
            digitalWrite(SSerialTxControl, RS485Receive);   
            
            //обнуляем буферы данных
            UnitID [0] = '\0';
            Command [0] = '\0';
            Parameters [0] = '\0';
        }
        else
        {
            //обнуляем буферы данных
            UnitID [0] = '\0';
            Command [0] = '\0';
            Parameters [0] = '\0';
            
            //это не ко мне обращаются идем дальше
            Serial.print(buffer);
            dataSending = 0; //восстанавливаем все процессы
        }

        //если вдруг произошол обрыв связи
        dataSending = 0; //восстанавливаем все процессы
        digitalWrite(Pin13LED, LOW);  // отключаем светодиод - данные не принимаются  
    }
}


//основной цикл программы
void loop(void)
{
 
    //проверяем сеть есть ли данные по сети
    getData485();
    
    //Если идет передача данных - тормозим все замеры отключения и включения
    if(!dataSending)
    { 
        Timer_action();
        
        //если темпиратура ниже заданной, включаем обогрев
        if(manualModeTan != 0  && temperaturePipe < minTemperature)
        {
            digitalWrite(pinRelayHeating, HIGH);
            //Serial1.print ("RelayHeating is ON -");
        }
        
        //если темпиратура выше заданной выключаем обогрев
        if(manualModeTan != 0  && temperaturePipe > maxTemperature)
        {
            digitalWrite(pinRelayHeating, LOW);
            //Serial1.print ("RelayHeating is OFF -");
        }

        //Serial.println ("Tempirature Pire: " + temperaturePipe); //пишем в сериал температуру датчика обогрева трубы для отладки
        
        //если давление ниже заданного и вода присутствует включаем насос
        if(manualModePipe != 0 && pressure < minPressure)
        {
            //здесь автомат - но тоже оповещаем сервер
            digitalWrite(pinRelayPump, HIGH);
            //Serial.print ("RelayPump is ON -");
        }
        
        //если давление выше заданного выключаем насос
        if(manualModePipe != 0 && pressure > maxPressure)
        {
            digitalWrite(pinRelayPump, LOW); 
            //Serial.print ("RelayPump is OFF -");
        }

        //если давление воды низкое
        if(!isWater == true)
        {
            //Serial.print ("WARNING Water - RelayPump is OFF");
        }
            
        //Serial.println ("Pressure Water: " + String(pressure)); //пишем в сериал температуру датчика темпиратуры воды    
        //Serial.println ("------------------");  

    }
    
}
