# Mutteruhr
a pulse of alternating polarity every minute.

Motherclock, or german "Mutteruhr" is a arduino based Master Clock or german "Hauptuhr". 
It generates a short pulse of alternating polarity to drive so called slave clocks or german "Nebenuhr".

https://en.wikipedia.org/wiki/Master_clock

With a DC/DC converter it uses minimal power, the highly accurate DS3231 RTC to keep time, and a Arduino Nano Every as a Brain.

![grafik](https://user-images.githubusercontent.com/1591573/145025681-a7b4bdb3-0974-4b92-9626-26711a841b9c.png)

![grafik](https://user-images.githubusercontent.com/1591573/145027173-5980cd03-52ee-4388-b84f-cefec5592010.png)

## Circuit

Schematic: https://github.com/designer2k2/Mutteruhr/blob/main/MotherClock/Motherclock%20Schematic.pdf

The actual clock drive is a transistor h-bridge, parts can be either SMD or THT. 

2 Buttons and a DIL switch is on it to make settings while the clock operates.

A small mosfet can be used to switch some LED´s on/off
