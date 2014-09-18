Rhome V3
=========

Rhome V3 is a stand-alone ARM based room automation system. It features:

  - (Wireless) Light control
  - Window blind control
  - Local LCD based GUI
  - Wireless remote control (for now 433MHz remotes only)
  - Wifi connectivity
  - Easy to use web control interface (integrated web server)

It runs on a standard STM32F4 Cortex M4 microcontroller. This project includes all the necessary source code (EMBLOCKS PROJECT folder) and eagle board files with schematics (SCHEMATIC folder). I will also add .stl files for a 3D printed case later.

Changelog
----

0.1:

This is an alpha release and needs a lot of code cleanup. More details on how to set eveything up are comming later.

Requirements
-----------

You will need the following things to get this running:

  - A built and ready Rhome V3 PCB (or an STM32F4 discovery board with necessary components connected)
  - A sailwinch servo for each window blind that you want to control (more details are comming later)
  - A wireless 433MHz switch for each light that you want to control (I am using ones made by Intertechno, but others should work)
  - An STM32F4 Discovery board for programming (anything with STLINK should work though)
  - 5V power supply (computer USB port should work though some have trouble powering the PCB with servo motors attached)
  - A running installation of em::blocks IDE (open source, very easy to use) for compiling and uploading the project to the board

After that you just need is to connect your STLINK programmer to the programming interface on the board, select release in the em::blocks IDE and upload the code to the board. All the configuration is done through the integrated GUI.

License
----

GNU GPL v3 - see LICENSE for more details
