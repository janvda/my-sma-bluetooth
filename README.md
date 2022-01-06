# sma-bluetooth
A C program to connect to SMA inverters via Bluetooth and export data (e.g. into MySQL database.

Forked to implement own adaptations, and for Raspberry Pi (Buster)

Needs:
* xml2 (libxml2-dev)
* bluetooth (libbluetooth-dev)
* mysql client (was libmysqlclient-dev, apparently now default-libmysqlclient-dev)

On RPi:
sudo apt-get libxml2-dev install libbluetooth-dev default-libmysqlclient-dev

To compile run make smatool

To install run make install

Make sure you edit the /etc/smatool.conf to update the SMA converter bluetooth MAC address
