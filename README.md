# IrrigationMonitor

This code will serve to control an ESP32 and SIM-7000G modem in order to monitor the location of a center pivot irrigation sprinkler.

The IrrigationMonitor folder contains the code for the ESP32. The IrrigationMonitorWebServer contains the Javascript and HTML for the NodeJS web server.

UPDATE 6-21-22

Hello anybody visiting! This code is no longer the current code being used in this project. I am in the process of writing my own library to use the ESP32 with a SIM7000A modem. It functions the same as the code listed here, but gives me more control over the timing and arguments of the AT commands sent to the modem.

The JavaScript is mostly still the same. I have implemented my new method (Patent pending) of center calculation as a new function. Once this next revision of the project is finished, I will update this repository or post a link to the new repository. My new center calculation method is described in the 'Sphere Method - Part 1.pdf' document in this repository.


Made by Dane Thompson
