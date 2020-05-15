# Drag Rake
## Project Overview

Drag Rake is an integrating drag measuring rake with a universal attachment mechanism for all wings and a modern, wireless solution that records and displays drag data to the cockpit in real-time created by [Akaflieg SLO](akslo.com). A drag rake is composed of a series of pneumatic tubes that sample the air pressure of the wing wake. By knowing the wake air pressure and the undisturbed air pressure, a good estimate of the wing drag can be determined. Our drag rake is unique in that all the electronics are contained on the rake itself, as opposed to existing drag rake systems where the electronics are inside of the aircraft away from the drag rake. A self-contained unit significantly reduces the set-up time since the system is portable and only needs to be mounted to the wing; unlike traditional systems, pneumatic tubing does not need to be run from the drag rake to the interior of the aircraft. Our drag rake relays data to the pilot in real-time via an IoT WiFi equipped microcontroller that can send the data to any smart device with a web browser.

This repository contains all the drawings, CAD files, PCB files, code, and guides for how to build our particular drag rake. In the spirit of the open-source community, every part of this project is open-source and free to use. All feedback and improvements are appreciated!

### Drag Rake Pneumatic System Overview
Like other drag measuring rakes, Drag Rake measures and compares the head pressure in the freestream and wing wake. A series of tubes parallel to the approximate local flow vertically oriented at the trailing edge of the wing collect the in-wake head pressure and a common plenum chamber connected to these tubes provides an average of the head pressure. Another tube also parallel to the flow, but sticking out from the wing, as to avoid its influence, provides freestream head pressure. The wake head and freestream head are compared by a highly accurate digital differential pressure transducer. Since the upper surface and lower surface of the wing have different localized pressures, there are two sets of wake head and freestream head tubes and differential pressure sensors, one set for each side of the wing.  

### Electrical System Overview
Pressure values from the pressure sensors are read by a microcontroller via the SPI digital protocol. Measurement commands are sent to each sensor and are then read back by the microcontroller. These pressure sensors have multiple kinds of measurement commands (single read and average of 2, 4, 8 or 16 reads), but testing will need to be conducted to see if one kind of read is better (more accurate, less noise, etc.). Also on the SPI bus are the SD card for data logging and the integrated WiFi module. 

Power to the system is provided by a 400 mAh battery that is kept underneath the microcontroller. Power to the SD card and pressure sensors comes from the battery via the microcontroller and 3.3V line.

### Wireless System Overview
In order to ensure that a wide range of devices could wirelessly connect to Drag Rake, an agnotistic wireless connection scheme was developed by using a WiFi-equipped microcontroller. The microcontroller creates a wireless network and hosts a webpage that displays the data from Drag Rake in real time. An agnostic WiFi-equipped device can then connect to the network, find the webpage's IP address, and display the data in real time (updates automatically, no refreshing necessary). 

## Current Tasks
- [ ] Implement SD card data logging
- [ ] Convert from I2C data bus to SPI data bus
- [ ] Software testing
- [ ] Publish polished code to master! :shipit: 
- [ ] PCB redesign for SPI
- [ ] Create drawings from CAD for manufacturing
- [ ] Create BOM for DIYers
- [ ] Create build guide for DIYers
- [ ] Whitepaper on wireless interface