# Intro
This is a driver for Victron's Venus OS to read data from SMA inverters using the SMANet (RS485) protocol. It uses the YASDI C library originally made available by SMA to communicate with the inverter using an attached USB<>RS485 interface and makes the information available on DBus according to the Victron specification for a "PV Inverter" (as com.victronenergy.pvinverter.smanet). Other Victron services will then take this data and show it in the GUI/VRM etc. When I was developing this project I could not find a nice simple example for making a Venus OS DBus compatible C driver, so the ve_dbus module is also useful as a basic reference implementation using the low level libdbus libraries.

# Supported hardware
In theory any device supporting the SMANet protocol should work. This is most of the older SMA inverters. By default they usually don't have a communication card in them (and these communication cards are now very expensive or unobtainium), however it is possible to use a generic *ISOLATED* TTL<>RS485 board (I used https://www.aliexpress.com/item/1005009093575093.html) connected to the right pins on the SMA motherboard. Note that there are very high and dangerous voltages inside the inverter when powered on or when just switched off, so do this at your own risk!! See this excellent write-up https://mensi.ch/blog/articles/data-interface-of-an-sma-sunnyboy-inverter for the details. On my inverter, I connected only V+,TX,RX,GND to the "hot" side and it worked straight away.

# SMANet over IP
It should be possible to make this driver work with SMANet over IP as well by changing the yasdi.ini file, but I don't have one to test with.

# How to configure
I wrote this for my installation with one SMA-3800 inverter, so it is customised to this. However, it should be quite easy to modify if the fields reported by your inverter model are different. 

- Using the software yasdishell (by SMA, but built in this project too) you can connect to your inverter and see the available fields with a CLI
- In main.c -> bridge_keymap[] is the table for translating SMA fields to paths on the DBus
- If you need to add more dbus fields or modify some defaults, in ve_dbus.c -> dbus_paths is the place to do it
- At the moment this driver only supports 1 device, although some other parts are built with multiple devices in mind
- Configure yasdi.ini to tell the driver which tty devices to use for the inverter (default is ttyUSB0)

If you need help or find a bug please open an issue and I will take a look

# How to build
The project uses Cmake so the easiest way to build is to download the cross-compile toolchain for Venus OS. After that it's as simple as (in the project root)
```
mkdir build
cd build
cmake ..
make
```
Copy all of the resultant binaries (libyasdi***.so, yasdishell, venus-sma-net) to the /data partition of your Venus OS device. You also need yasdi.ini, configured to the USB<>RS485 adapter you're using. Run it with the local directory as the library path
```
LD_LIBRARY_PATH=. ./venus-sma-net
```
 To set it up to auto-run on boot, see https://www.victronenergy.com/live/ccgx:root_access 

# How it works
The project uses the YASDI library to scan and connect to the available SMA device. It discovers the channel list (parameters) and then maps these to the defined Victron Venus OS DBus paths. The parameters are updated from the inverter at 5 second intervals (to not overload the CPU on either device, since the protocol is quite slow). When these values change, the changes are sent to DBus using the ItemsChanged event (so only notifying of the values which changed). All the core DBus functions for Venus OS services to identify this devices have been implemented, but it may not be 100% complete.

# License
- The YASDI library is under its own license (LGPL) and is included here because a couple of small modifications have been made to make it compile properly
- The project itself is made available for you under the MIT license to do with it what you like, but there is no warranty and you use it all at your own risk!

# Thanks
- Victron for good documentation on Venus OS and DBus making it possible to write a simple C driver at all
- SMA for making a good quality C library available for third party integrations!
- The Ardexa SMA-RS485 project for giving a practical usage example for the YASDI library https://github.com/ardexa/sma-rs485-inverters
