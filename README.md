# ESP32_DINTimeSwitch

Source code for the ESP32 DIN Rail Module:
https://www.beyondlogic.org/esp32-din-rail-module/

<P ALIGN="CENTER"><IMG SRC="raw.githubusercontent.com/craigpeacock/ESP32_DINTimeSwitch/doc/screenshot.png" width=50% height=50%></P>

Current code switches on output based on SA Power Networks Time of Use (ToU) tarrif. 
* Solar Sponge 10am to 3pm
* Off-Peak 1am to 6pm

Next stage is to switch output off if the spot price exceeds a certain threshold. 
