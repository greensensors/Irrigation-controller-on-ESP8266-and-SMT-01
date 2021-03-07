# Irrigation-controller-on-ESP8266-and-SMT-01
Irrigation controller measures soil moisture and turns on irrigating in a timely manner in sufficient quantity for plant growth.
RainON analyzes soil parameters by Soil Moisture and Temperature sensor [SMT-01](https://github.com/greensensors/SMT-Soil-Moisture-Sensor-for-Arduino). If the soil moisture is below the set limit, the controller turns on irrigating the plants. Plants are irrigating in portions, which ensures an even and economical distribution of moisture in the soil. During irrigating, the controller monitors the state of soil moisture. As soon as the soil moisture reaches the set value, irrigating stops.

![image](https://user-images.githubusercontent.com/77538035/110234558-c891f380-7f33-11eb-8668-9c807c7b3e10.png)

The design of RainON is made on the ESP8266-12F and soil moisture and temperature sensor SMT-01 (see
[Electrical circuit diagram](https://github.com/greensensors/Irrigation-controller-on-ESP8266-and-SMT-01/blob/main/RainON-ESP8266-circuit.JPG) and
[Open source software](https://github.com/greensensors/Irrigation-controller-on-ESP8266-and-SMT-01/blob/main/RainON-210302.ino)).

## Main technical characteristics
* Supply voltage: DC 5V
* Current consumption: < 300 mA
* Relay current: up to 5A
* Temperature range: -20 + 55 ° C
* Measurement error: < 3%
* Soil moisture and temperature sensor: SMT-01

