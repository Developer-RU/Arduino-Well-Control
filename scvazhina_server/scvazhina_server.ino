//#include <TimerOne.h>
#include <SPI.h>
#include <Ethernet.h>
#include <SD.h>
#include <OneWire.h>
#include <EEPROM.h>
#include <string.h>

#define SSerialTxControl 9   // RS485 Direction control
#define RS485Transmit    HIGH
#define RS485Receive     LOW
#define Pin13LED         13
#define REQ_BUF_SZ 40

char buffer[100];

File webFile;
char HTTP_req[REQ_BUF_SZ] = {0};
char req_index = 0;
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

IPAddress ip(192, 168, 200, 100);
EthernetServer server(80);

bool pin1, pin2, pin3, pin4; // переключатели положения пинов - не задействованы

int numCommand = 0; // Номер команды клиента при следующем вызове
String paramsCommand[] = {}; // Параметры принятые с браузера для запроса клиенту RS485
int cntErrors = 0; // Колличество ошибок получения данных от клиента RS485

// Обновляемые данные с контроллера насоса - по умолчанию 0, не изменять
char UnitID[10];
char Parameters[200];
char Command[10];

int minTemperature = 0; //минимальная темпиратура
int maxTemperature = 0; //максимальная темпиратура
int minPressure = 0; //минимальное давление
int maxPressure = 0; //максимальное давление
int temperaturePipe = 0;
int temperatureWater = 0;
int pressure = 0;
int relayPump = 0; 
int relayHeating = 0;
int isWater = 0;
int manualModePipe = 0;
int manualModeTan = 0;

int currentTime = 0;
int commandNum = 3;

int repeat = 0;

void setup()
{    
    // устанавливаем пины на входы|выходы
    pinMode(Pin13LED, OUTPUT);   
    pinMode(SSerialTxControl, OUTPUT); 
    
    // пины на выходы - индикация контроллера сервера -- не задействованы
    pinMode(2, OUTPUT);
    pinMode(3, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(6, OUTPUT);
    
    Serial.begin(57600);
    SD.begin(4); // на каком пине питание MicroCD модуля
    Serial1.begin(57600);
    Ethernet.begin(mac, ip);
    server.begin();
    
    pin1 = pin2 = pin3 = pin4 = 0; // переключатели положения пинов устанавливаем в положение выключено

    //Timer1.initialize();
    //Timer1.attachInterrupt(Timer_action);
    //currentTime = 1;
}

// Таймер - инициализирован но незадействован - срабатывает каждую секунду независимо от работы приложения
void Timer_action()
{ 
    // Таймер выполняется раз в секунду
    
    currentTime++; // Увеличиваем счетчик секунд - нужен для записи данных темпиратур на MicroCD
}

// Отправка данных
void setDataRS485(String command)
{
    digitalWrite(Pin13LED, HIGH);  // включаем светодиод пока данные принимаются
    digitalWrite(SSerialTxControl, RS485Transmit); // Включение на прием RS485
    Serial1.print(command + "\r\n");
    digitalWrite(Pin13LED, LOW);  // выключаем светодиод данные приняты
}

// Прием и применение данных
void getAndCommitDataRS485()
{
    digitalWrite(Pin13LED, HIGH);  // включаем светодиод пока данные принимаются
    digitalWrite(SSerialTxControl, RS485Receive); // Включение на прием RS485

    int i = 0; 
        
    if(Serial1.available())
    {
        //delay(100);
        while( Serial1.available() && i < 99) 
        { 
            buffer[i++] = Serial1.read();
        } 
        
        buffer[i++] = '\0';
    }
    
    if(i > 0)
    {
        Serial.print(buffer); // Выводим что приняли с других устройств в монитор порта
        Serial.print("\r\n");
        
        // Разбираем принятую строку на части (идентификатор устройства - команда - параметры)
        sscanf(buffer, "%s%s%s", &UnitID, &Command, &Parameters);
    }
    
    //--------------данные уже получили - теперь разбор------------------------//

    // если пришел ответ от "ID01 D"
    if((String)UnitID == "ID01" && (String)Command == "D")
    {        
        //разбираем строку по знаку ":"
        String Str = String((String)Parameters);
        Str.toCharArray(buffer, 24);

        // Если информация в переменных новая (состояния реле) - записываем на MicroCD


        // http://arduino.ru/forum/programmirovanie/zapis-dannykh-s-datchika-dht11-na-sd-kartu

        
        // Так же проверим время если прошло N секунд то пишем значения темпиратур на карту

        if(Str == " ") repeat = 1;

        // Заносим параметры в свойства текущего контроллера для последующего отображения в веб
        temperaturePipe = atoi(strtok(buffer,":"));
        Serial.print("Temperature pipe: " + String(temperaturePipe) + "\n");
        temperatureWater = atoi(strtok(NULL,":"));
        Serial.print("Temperature water: " + String(temperatureWater) + "\n");
        pressure = atoi(strtok(NULL,":"));
        Serial.print("Pressure: " + String(pressure) + "\n");
        relayPump = atoi(strtok(NULL,":"));
        Serial.print("Relay pump: " + String(relayPump) + "\n");
        relayHeating = atoi(strtok(NULL,":"));
        Serial.print("Relay heating: " + String(relayHeating) + "\n");
        isWater = atoi(strtok(NULL,":"));
        Serial.print("IsWater: " + String(isWater) + "\n");
        
        manualModeTan = atoi(strtok(NULL,":"));
        Serial.print("Manual mode pipe: " + String(manualModeTan) + "\n");
        
        manualModePipe = atoi(strtok(NULL,":"));
        Serial.print("Manual mode pump: " + String(manualModePipe) + "\n");


        minTemperature = atoi(strtok(NULL,":"));
        Serial.print("minTemperature: " + String(minTemperature) + "\n");
        
        maxTemperature = atoi(strtok(NULL,":"));
        Serial.print("maxTemperature: " + String(maxTemperature) + "\n");

        minPressure = atoi(strtok(NULL,":"));
        Serial.print("minPressure: " + String(minPressure) + "\n");
        
        maxPressure = atoi(strtok(NULL,":"));
        Serial.print("maxPressure: " + String(maxPressure) + "\n");
    }

    //обнуляем буферы данных
    UnitID [0] = '\0';
    Command [0] = '\0';
    Parameters [0] = '\0';
   
    digitalWrite(Pin13LED, LOW);  // выключаем светодиод данные приняты
}

void loop()
{

    if(commandNum != 3)
    {
        // код простые команды.......

         // Отправки команды клиенту RS485 - если из браузера была команда - меняется глобальная переменная 
        switch(commandNum)
        {
            case 0: setDataRS485("ID01 R"); // Перезагрузка
                    break;
            case 1: setDataRS485("ID01 G"); // Обновление данных
                    break;
            case 2: setDataRS485("ID01 T"); // Тест соединения
                    break;       
            case 3: setDataRS485("ID01 D"); // Получить текущие значения состояний и датчиков
                    break;
            case 4: setDataRS485("ID01 M 11"); // Включить насос в ручном режиме
                    break;   
            case 5: setDataRS485("ID01 M 22"); // Выключить насос в вручную
                    break;   
            case 6: setDataRS485("ID01 M 33"); // Включить подогрев трубы вручную
                    break; 
            case 7: setDataRS485("ID01 M 44"); // Включить подогрев трубы вручную
                    break;    
            case 8: setDataRS485("ID01 M 55"); // Включить ручной режим насоса
                    break;    
            case 9: setDataRS485("ID01 M 66"); // Включить ручной режим подогрева трубы
                    break;    
            case 10: // Собираем строку настроек
                    //setDataRS485("ID01 S " + ); // Сохранить и применить строку настроек
                    break;                                                               
        }
    
        // Возвращаем команду опроса датчиков и состояний реле
        commandNum = 3;
        
        //delay(10);
        
        // Смотрим ответ от устройств удаленных
        getAndCommitDataRS485();
    
        // Ожидание до ответа - возможно надо чуть меньше
        //delay(200);    

        currentTime = 1;
    }
    else if(commandNum == 3 && currentTime >= 11211)
    {
        
        setDataRS485("ID01 D"); // Получить текущие значения состояний и датчиков

        // Возвращаем команду опроса датчиков и состояний реле
        commandNum = 3;
    
        //delay(10);
        
        // Смотрим ответ от устройств удаленных
        getAndCommitDataRS485();
    
        // Ожидание до ответа - возможно надо чуть меньше
        //delay(200);
        if(repeat == 1)
        {
            setDataRS485("ID01 D"); // Получить текущие значения состояний и датчиков

            // Возвращаем команду опроса датчиков и состояний реле
            commandNum = 3;
        
            //delay(10);
            
            // Смотрим ответ от устройств удаленных
            getAndCommitDataRS485();
        }
        

        currentTime = 1;
    }
    
    currentTime++;
    
    // Смотрим надо ли отдать что в браузер (есть ли подключение к серверу по ethernet)
    EthernetClient client = server.available();

    // если подключился клиент Ethernet
    if (client)
    {
        // Запрос HTTP заканчивается пустой строкой
        boolean currentLineIsBlank = true;

        while (client.connected())
        {
            if (client.available())
            {
                // Пишем в переменную что прилетело от клиента
                char c = client.read();
                
                if (req_index < (REQ_BUF_SZ - 1))
                {
                    HTTP_req[req_index] = c; // save HTTP request character
                    req_index++;
                }

                // Пришел конец строки и следом пустая строка
                if (c == '\n' && currentLineIsBlank)
                {
                    // Если запрос к главной странице (файлу index.htm)
                    if (StrContains(HTTP_req, "GET / ") || StrContains(HTTP_req, "GET /index.htm")) 
                    {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        webFile = SD.open("index.htm"); // Открываем с MicroCD файл главной страницы
                    } 
                    
                    // Если запрос к странице статистики (файлу details.htm)
                    if (StrContains(HTTP_req, "GET /details.htm")) 
                    {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        webFile = SD.open("details.htm"); // Открываем с MicroCD файл главной страницы
                    } 


                    // Если запрос к иконке вкладки браузера favicon.ico
                    else if (StrContains(HTTP_req, "GET /favicon.ico"))
                    {
                        webFile = SD.open("favicon.ico");
                        if (webFile)
                        {
                            client.println("HTTP/1.1 200 OK");
                            client.println();
                        }
                    } 
                    
                    // Если запрос к картинке flame.png
                    else if (StrContains(HTTP_req, "GET /flame.png"))
                    {
                        webFile = SD.open("flame.png");
                        if (webFile) 
                        {
                            client.println("HTTP/1.1 200 OK");
                            client.println();
                        }
                    } 
                    
                    // Если запрос к файлу стилей my.css
                    else if (StrContains(HTTP_req, "GET /my.css")) 
                    {
                        webFile = SD.open("my.css");
                        if (webFile) 
                        {
                            client.println("HTTP/1.1 200 OK");
                            client.println();
                        }
                    }


                    // Если прилетел запрос от функции    -----    ajax_flame (функция отправляет запросы на передачу данных о состояних датчиков и т.п.)
                    else if (StrContains(HTTP_req, "ajax_flame"))
                    {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");
                        client.println();
                        
                        int smoke_gas = 0; //пин на котором подключен MQ-2
                        int sensorReading = analogRead(smoke_gas);
                        int chk;

                        // Здесь генерируется AJAX ответ - берем глобальные переменные и отправляем в виде массива
                        
                        client.print(temperaturePipe);
                        client.print(":");
                        client.print(temperatureWater);
                        client.print(":");
                        client.print(pressure);
                        client.print(":");
                        client.print(relayPump);
                        client.print(":");
                        client.print(relayHeating);
                        client.print(":");
                        client.print(isWater);
                        
                        client.print(":");
                        client.print(manualModePipe);
                        client.print(":");
                        client.print(manualModeTan);

                        client.print(":");
                        client.print(minTemperature);
                        client.print(":");
                        client.print(maxTemperature);
                        
                        client.print(":");
                        client.print(minPressure);
                        client.print(":");
                        client.print(maxPressure);
Serial.print(maxPressure);

                        /*
                        client.print(":");
                        client.print((digitalRead(2)) ? "1" : "0");
                        client.print(":");
                        client.print((digitalRead(3)) ? "1" : "0");
                        client.print(":");
                        client.print((digitalRead(5)) ? "1" : "0");
                        client.print(":");
                        client.print((digitalRead(6)) ? "1" : "0");
                        */
                    }



                    // Если прилетела команда GET -- на отключение|включение ручного управления насосом
                    else if (StrContains(HTTP_req, "command?M=1")) 
                    {
                        //commandNum = 8;
                        setDataRS485("ID01 M 66");
                        delay(500);
                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    }

                    // Если прилетела команда GET -- на отключение|включение ручного управления насосом
                    else if (StrContains(HTTP_req, "command?M=2")) 
                    {
                        //commandNum = 9;
                        setDataRS485("ID01 M 55");
                        delay(500);
                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    }
                    /*
                    // Если прилетела команда GET -- на отключение|включение ручного управления насосом
                    else if (StrContains(HTTP_req, "command?M=1")) 
                    {
                        // Инвертируем значение
                        if(manualModePipe == 1) 
                        {
                            //manualModePipe = 0; 
                            commandNum = 5;
                        }
                        else
                        {
                            //manualModePipe = 1;
                            commandNum = 4;
                        }
                        
                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    }

                    // Если прилетела команда GET -- на отключение|включение ручного управления подогревом
                    else if (StrContains(HTTP_req, "command?M=2")) 
                    {
                        // Инвертируем значение
                        if(manualModeTan == 1) 
                        {
                            manualModeTan = 0; 
                            commandNum = 7;
                        }
                        else
                        {
                            manualModeTan = 1;
                            commandNum = 6;
                        }

                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    }
                    
                    
                    // Если прилетела команда GET -- на отключение|включение пина 2
                    else if (StrContains(HTTP_req, "setpin?pin=1")) 
                    {
                        // Инвертируем значение переключателя состояния пина
                        pin1 = !pin1; 

                        // Изменяем состояние самого пина
                        digitalWrite(2, pin1);

                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    }
                    
                    // Если прилетела команда GET -- на отключение|включение пина 3
                    else if (StrContains(HTTP_req, "setpin?pin=2")) 
                    {
                        // Инвертируем значение переключателя состояния пина
                        pin2 = !pin2;

                        // Изменяем состояние самого пина
                        digitalWrite(3, pin2);

                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    } 
                    
                    // Если прилетела команда GET -- на отключение|включение пина 5
                    else if (StrContains(HTTP_req, "setpin?pin=3")) 
                    {
                        // Инвертируем значение переключателя состояния пина
                        pin3 = !pin3;

                        // Изменяем состояние самого пина
                        digitalWrite(5, pin3);

                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    }

                    // Если прилетела команда GET -- на отключение|включение пина 6
                    else if (StrContains(HTTP_req, "setpin?pin=4"))
                    {
                        // Инвертируем значение переключателя состояния пина
                        pin4 = !pin4;

                        // Изменяем состояние самого пина
                        digitalWrite(6, pin4);

                        // Пишем в ответ о проделанном действии - успешно
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-Type: text/html");
                        client.println("Connnection: close");
                        client.println();
                        
                    }
                    */


                    /*
                    // Если запрос к файлу данных
                    else if (StrContains(HTTP_req, "GET /myData.json")) 
                    {
                        webFile = SD.open("myData.json");
                        if (webFile) 
                        {
                            client.println("HTTP/1.1 200 OK");
                            client.println();

                            client.println("<h1>1111111111111111111111111111111111111111111111111111111</h1>");
                        }
                    }
                    */
                    
                    // Если файл готв
                    if (webFile) 
                    {
                        // Пытаемся отправить клиенту
                        while (webFile.available()) {
                            client.write(webFile.read()); //Отправляем файл клиенту
                        }

                        // Закрываем файл
                        webFile.close();
                    }


                    // Очищаем буферы
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    
                    // Выходим из условия (пришел конец строки и следом пустая строка)
                    break;
                }

                 // Если пришел знак конец строки, переключаем состояние для нового ожидания запроса - пустая строка
                if (c == '\n')
                {
                    currentLineIsBlank = true;
                }

                
                else if (c != '\r')
                {
                    // Мы получили символ на текущей строке
                    currentLineIsBlank = false;
                }
            }
        }
        
        
        // Даем время веб-браузеру для приема данных
        delay(1);
        
        // Закрываем соединение
        client.stop();
    }


    
   
    
}

void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++)
    {
        str[i] = 0;
    }
}

char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;
    
    len = strlen(str);
    
    if (strlen(sfind) > len) 
    {
        return 0;
    }
    
    while (index < len) 
    {
      
        if (str[index] == sfind[found])
        {
            found++;
            if (strlen(sfind) == found)
            {
              return 1;
            }
        }
        else 
        {
            found = 0;
        }
        
        index++;
    }
    
    return 0;
}
