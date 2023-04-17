# 433MHz Auriol temperature sensor data decoder
 
This is a source code for the Arduino Nano application that decodes Auriol temperature sensor (AURIOL RC TEMPERATURE STATION 4-LD5832-2) data from attached 433MHz receiver. Receiver output is connected to D8 input pin. Serial data is delivered at D0 pin (UART Tx pin) at 115.2kbps.

Delivered data has 24 bits:
* bits 0-7   - random value (never 0x00) set when inserting batteries
* bit 8      - bit used for asigning sensor to the receiving station ('1' when sensor button is pressed)
* bit 9      - low battery
* bits 10-11 - channel
* bits 12-23 - temperature in 'degC x 10' at two's complement format
