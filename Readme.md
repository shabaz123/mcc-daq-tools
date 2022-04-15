mcc-daq-tools
=============

This repository contains various tools/applications that can be used for working with the MCC 172 DAQ board from Measurement Computing.

This repository also contains sample captures for those interesting in exploring such data.

daqproj
=======
The **daqproj** folder contains tools for running on the Pi that has the DAQ board physically attached. 
Create a folder in **/home/pi** that is called **development**.
Place the daqproj folder in the **/home/pi/development** folder.
The daqproj folder contains the following tools:

daqstream
---------
A prerequisite for the **daqstream** code is to install the following code first: https://github.com/Theldus/wsServer
Edit in **/home/pi/development/daqproj/daqstream/Makefile** the line that contains the **WSDIR** entry, so that it can find the wsServer library and include files.
Build the daqstream code by typing the following in the **/home/pi/development/daqproj/daqstream** folder:

    make

Run the code by typing:

    ./daqstream

When run, the code prompt you to press Enter, and then the code will continuously obtain samples from the MCC 172 DAQ board. The samples will be continuously thrown away whilst it waits for a remote connection. Once the remote connection is made, the samples will be streamed to the remote destination. 
The supported protocol for streaming is WebSocket, and the data is streamed in sets of 16 bytes (8 bytes for channel 0, and 8 bytes for channel 1, both are in IEEE 8-byte floating point format).
If desired, the remote side can temporarily pause the streaming by issuing a **pause** ASCII string over the WebSocket. The stream can be resumed by issuing the **resume** ASCII string.
A suitable WebSocket client to use daqstream is the Matlab program called WSStream.m (see further below).

daqviki
-------
Build the daqviki code by typing the following in the **/home/pi/development/daqproj/daqviki** folder:

    make

The daqviki program is ordinarily installed as a service. To do that, type the following:

    sh ./install_service.sh

If you ever need to remove the service, type:

    sh ./remove_service.sh

The daqviki program currently only runs on a Pi 4. It will be the Pi 4 which has the MCC 172 DAQ board plugged on top. If you wish to use it with an earlier Pi model (e.g. Pi 3), then you'll need to modify the i2cfunc.h file and change the line that contains **#define PERIPHERAL_BASE**. You'll need to consult the raspberrypi.org documentation to figure out what to change the definition to.
The daqviki program controls an I2C LCD screen and accepts input from a couple of buttons. You can refer to the **daqviki.c** source code, and especially the **#define** lines in it, to see what GPIO pins the LCD and buttons need to be wired to.
When installed as a service, when the Pi is started up, the LCD screen will display a menu which will allow the user to press the two buttons to initiate and control MCC 172 data logging. It offers standalone operation (no need for HDMI monitor or keyboard or WiFi or Ethernet connection), ideal when using the MCC 172 on site.

dspgen
======
The dspgen folder contains tools for controlling a Digital Signal Processor (DSP) board connected to the Raspberry Pi. The DSP board is not really related to the MCC 172 DAQ board, but it can be handy for providing stimulus signals.
The DSP board consists primarily of an ADAU1401 module available from AliExpress. The ADAU1401 module contains an Analog Devices ADAU1401 chip.

The dspgen code runs on the Pi which has the DSP board plugged to it. It can be the same Pi as the one that has the MCC 172 DAQ board, because both boards can be plugged at the same time. Alternatively, a separate Pi can be used for the DSP board if desired. Only the Pi 4 has been tested.

Create a folder in **/home/pi** that is called **development**.
Place the dspgen folder in the **/home/pi/development**  folder.

To build the code, type the following in the **/home/pi/development/dspgen** folder:

    make

The dspgen code can be run in two ways; either directly from the command line, or via remote commands in a server mode.
To run directly from the command line, you can type the following to install the **Tone Generator** application into the DSP board:

    ./dspgen -ptone_app.bin

Power-cycle the DSP board for the application to take effect. After you do that, the tone generator application will run and a tone will be generated (you can check it with headphones for instance).

You can type the following sorts of commands to control the tone. To set the frequency to (say) 1000 Hz:

    ./dspgen -f1000

To adjust the amplitude (the range is 0 to 1):

    ./dspgen -a0.5

To control the DSP remotely, type the following to run the server mode:

    chmod 755 http_server.py
    ./http_server.py

The supported protocol is REST. The server will listen for the following typs of commands; this example is used to set the frequency to 1000 Hz:

    http://192.168.1.88/dspgen/f1000

The http_server.py code will respond back with 200 OK, and will issue the appropriate dspgen command line to control the DSP.

To set the amplitude, issue the following sort of command:

    http://192.168.1.88/dspgen/a0.5

daqmatlab Matlab Tools
======================

The **daqmatlab** folder contains programs that can be run on a PC with Matlab installed.

WSStream.m
----------
A prerequisite to using the WSStream.m code is to first install the code here: https://github.com/jebej/MatlabWebSocket 
The WSStream.m code defines a class called WSStream that can be used to open a WebSocket stream and acquire data. It is suitable for using with the **daqstream** program described earlier.

Together with daqstream, the WSStream.m code will allow Matlab to obtain data from the MCC 172 DAQ board in near-real-time.

To prevent overloading Matlab with excessive data, the WSStream.m code also contains **pause()** and **resume()** methods, to temporarily pause and resume the WebSocket streamed data.

To use the WSStream.m program, first run the daqstream program on the Pi (as described earlier), and press Enter to kick off the data acquisition. The Pi 4 will continuously acquire and throw away the data, because no WebSocket connection has been established so far.
To establish the connection, type the following from the Matlab command line (change the IP address to suit your Pi 4):

    adc = WSStream('ws://192.168.1.88:8080');

As data arrives, it will be automatically stored in a variable called adc.Data and it will grow in size as more and more data is acquired.
You can erase the received data at any time by typing:

    adc.Data = [];

The variable adc.Data has two rows, one for each channel. If you wish to (say) plot the data for the first channel, type:

    plot(adc.Data(1,:));

Once you have enough data for analysis, you can pause the streaming by typing:

    adc.pause();

You could then do the processing, and then erase the data as described above, and then resume the data by typing:

    adc.resume();

do_thd.m
--------
The **do_thd.m** code can be used to view total harmonic distortion (THD) in near-real-time, plotted on a chart.

To run the code, first examine the do_thd.m file and edit it, because it contains some hard-coded values (in particular, the IP address of the Pi 4 that has the MCC 172 DAQ board attached).

Prepare the Pi 4 by running the **daqstream** program as described earlier, and press Enter as described earlier to start the data acquisition.

Run the Matlab code by typing:
    do_thd

The code will make a WebSocket connection (the do_thd.m code uses the earlier WSStream.m code to do that) and acquire data and create a plot that updates every second or so. After a while, the do_thd.m code will exit, it doesn't run forever. You can tweak the code to determine how long it runs for, or edit it to run forever.

freq_chart.m
------------
The **freq_chart.m** code can be used to create a frequency response chart for a device-under-test (DUT). In brief, the code instructs a DSP board to generate a tone, and then the code will acquire samples and work out the amplitude. Then, the code will instruct the DSP board to increase its frequency, and will repeat. This occurs over the range of 10 Hz to 20 kHz (you can edit the freq_chart.m code to specify the spot frequencies).

You will need to run the **dspgen** program in the remote command server mode as described earlier.

Edit the code, and run it in a similar way as described above for the **do_thd.m** code.

daqgrab.m
---------
The **daqgrab.m** code simply acquires samples from the MCC 172 DAQ board, for a set period of time. To use the code, type the following:

    [c1 c2] = daqgrab(1)

where c1 and c2 are the variables where you wish to store the samples for the first and second channels and 1 is the period of time in seconds.

The code relies on the WSStream.m code described earlier, and the Pi 4 must be running the **daqstream** code as before. You will need to edit the daqgrab.m code since it has a hard-coded IP address for the Pi 4 that has the MCC 172 DAQ board plugged on.

manual_freq_response.m
----------------------
The **manual_freq_response.m** code can be used to plot the frequency response of a device-under-test (DUT). The code basically prompts the user to manually set the stimulus frequency, and then press a key, and then the samples are captured. This repeats until all the data is captured to build up the frequency response chart.

You will need to edit the code because it contains a hard-coded IP address for the Pi 4. The Pi 4 needs to run the **daqstream** code as before.

To run the manual_freq_response code, type:

    manual_freq_response


dsp_chart.m
-----------
The **dsp_chart.m** code is used to establish the performance of the DSP board. It plots the SINAD and THD charts for the DSP board, across a range of frequencies.

You will need to edit the code because it contains hard-coded IP addresses. The Pi 4 needs to be running the **daqstream** code as before. The Pi 4 that has the DSP board attached needs to be running the **dspgen** code in the remote command server mode.

To run the dsp_chart.m code, simply type:

    dsp_chart


Sample Captures
===============
The **sample-captures** folder contains various samples. Some of them are:

**watch5**, **watch9**: Samples taken using IEPE vibration sensor in contact with a 2-year-old Seiko watch (with Seiko 5 mechanism)

**clock3**: Sample taken using IEPE vibration sensor in contact with an approx. 60-year-old WestClox Travalarm clock.

**firstcoffee**, **secondcoffee**: Samples taken using IEPE vibration sensor attached to a Nespresso coffee machine. A descaling cycle was run between the first coffee and second coffee.

**tuning2**: Samples taken using IEPE vibration sensor attached to a low-cost tuning fork that has been struck.

**mot3**: Samples taken using IEPE vibration sensor attached to a DC motor and gearbox taken from a cheap toy.

**fan**: Samples taken using IEPE vibration sensor attached to a new 5V DC fan.

