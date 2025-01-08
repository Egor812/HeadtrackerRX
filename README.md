# BT-PPM Module
Bluetooth приемник для Headtracker-а на протоколк FRSky Para 
  
Based on BTWifiModule by dlktdr (https://github.com/dlktdr/BTWifiModule)

## Описание
Получаем по Bluetooth данные от HeadTracker по протоколу FRSky Para. В моем случае используется https://github.com/ysoldak/HeadTracker на Seed Studio nRF52840 Sense.
Отправляем на тренерский разъем PPM данные.  

Нужно тем, кто не хочет впаивать BT приемник в аппаратуру.

## Железо
Seed Studio XIAO-ESP32C3

Jack 3.5 4pin  
|= = = >  
 A B C D  
D - PPM - D0  
B и С - GND  
A - не используется  

## Настройки аппаратуры
Model-Setup-Trainer mode Master/Jack  
Sys-Trainer - каналы и тримеры.

## Настройка приемника
Подключить по USB. В serial monitor:  
Ввести команду AT+DISC?  
В списке найти адрес передатчика. Ввести AT+CONадрес (напимер AT+CONDCB116851608)  
Теперь этот приемник будет автоматически подключатся к этому передатчику  

## Примечание
Работоспособность проверена только на ESP32C3
