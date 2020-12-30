# esp8266
Example project for debugging


## Building

In a first step, run the script `setup.sh`. If it completed successfully, source
the generated environment file with

   . setup\_env

Then you can enter the directory `firmware` and call `make menuconfig` there. Enter
your SSID and password - and set the IP of your workstation.


## Flashing

With `make flash` the built binary can be copied to the target.


## Running

Before running, run `make monitor` to see the console output.

If you keep the port as it is, you can catch the UDP messages with netcat:

    nc -kul 16983

