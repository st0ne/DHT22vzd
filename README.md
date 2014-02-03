DHT22vzd
========

DHT22 linux daemon for raspberry pi and volkszaehler.org

This project is a merge between 1wirezv (https://github.com/w3llschmidt/1wirevz) and lol_dht22 (https://github.com/technion/lol_dht22).


Installing
==========

Precondition: Raspian Linux (http://www.raspberrypi.org/downloads) 

Binding libraries: libcurl & libconfig -> 'sudo apt-get install libcurl4-gnutls-dev libconfig-dev'

Install Wiring Pi: http://wiringpi.com/download-and-install/

Download: 'git clone https://github.com/st0ne/DHT22vzd'

Build: './build.sh'

Copy DHT22vzd.cfg to /etc.

Config
======

Edit /etc/DHT22bzd.cfg.
See comments for help.


License
=======

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see http://www.gnu.org/licenses/.
