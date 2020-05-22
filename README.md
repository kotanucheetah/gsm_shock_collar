# gsm_shock_collar
A project to control a cheap Petronics shock collar through a GSM board using an Arduino

# Introduction
This was my first "real" Arduino project that wasn't just blinking an LED. I started this in 2018 and I'm finally getting around to posting it on here in hopes that it'll be useful for someone else in the future.

This project is intended to demonstrate a proof of concept for remotely controlling
a Petronics 433mhz shock collar using an Arduino, a GSM board, and a 433mhz transmitter.

## Goals:
* Use a cheap GSM board to receive text (SMS) commands
* Output those commands to a cheap, commercially available shock collar

## Items used in this proof of concept:
Collar: https://www.amazon.com/gp/product/B018I5XQ2M

Arduino: https://www.amazon.com/gp/product/B00D9NA4CY

(I used a Mega2560 specifically to allow for future improvements, such as reporting GPS location from the GSM board)

GSM board: https://www.amazon.com/gp/product/B072K5JLZD

433MHz transmitter: https://www.amazon.com/gp/product/B01AL5Q7SC

**IMPORTANT NOTE**: This project REQUIRES a modification to the Arduino's default serial buffer. 

By default, this is only 64 bytes long, which will truncate SMS messages received from the GSM board. I used these instructions: http://www.hobbytronics.co.uk/arduino-serial-buffer-size

Example code for this specific board is from here: https://drive.google.com/drive/folders/0ByLlBkJVmxooRDVnSEtJU3hWNlk

Any non-English comments in the .ino file are directly from these samples. 

# Decoding the Petronics remote
In order to capture the signals coming from the remote included in the box, I used an oscilloscope connected to the data pins on the small receiver board soldered to the main board inside the collar receiver unit. 

The manual does mention that you can re-pair collars to a new remote. I'm assuming the remote's unique ID is factory set, so one could theoretically "pair" their existing collar to this code. At this time, I don't have a way to capture the code emitted from a remote "off the air". (Feel free to PR one in if you want!)

This collar uses simple on-off keying, so the waveform was pretty simple. You can clearly see a pattern of 0s and 1s.

By transmitting several signals, I was able to determine the pattern of bits. 

This collar has 4 modes - light the LED (L), sound the beeper (B), vibrate (V), and shock (S). 

The remote can control two collars, and can range in intensity from 00-100.

From my remote, I captured the following bit streams. (S 05 1 = shock, 05 intensity, selected first collar).

Note that every command is preceeded with a specific start (ST) pattern:
```
               0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1
L 00 1      ST 0 1 0 0 0 1 0 0 0 1 1 0 1 1 1 0 1 0 1 0 1 1 0 1 1 0 0 0 0 0 0 0 1 1 1 1 0 1 1 1 0 0
B 00 1      ST 0 1 0 0 0 0 1 0 0 1 1 0 1 1 1 0 1 0 1 0 1 1 0 1 1 0 0 0 0 0 0 0 1 1 1 0 1 1 1 1 0 0
V 10 1      ST 0 1 0 0 0 0 0 1 0 1 1 0 1 1 1 0 1 0 1 0 1 1 0 1 1 0 0 0 0 1 0 1 0 1 0 1 1 1 1 1 0 0
S 05 1      ST 0 1 0 0 0 0 0 0 1 1 1 0 1 1 1 0 1 0 1 0 1 1 0 1 1 0 0 0 0 0 1 0 1 0 1 1 1 1 1 1 0 0
S 63 1      ST 0 1 0 0 0 0 0 0 1 1 1 0 1 1 1 0 1 0 1 0 1 1 0 1 1 0 0 1 1 1 1 1 1 0 1 1 1 1 1 1 0 0
S 99 1      ST 0 1 0 0 0 0 0 0 1 1 1 0 1 1 1 0 1 0 1 0 1 1 0 1 1 0 1 1 0 0 0 1 1 0 1 1 1 1 1 1 0 0
S 99 2      ST 0 1 1 1 1 0 0 0 1 1 1 0 1 1 1 0 1 0 1 0 1 1 0 1 1 0 1 1 0 0 0 1 1 0 1 1 1 0 0 0 0 0
```
Based on this data, I interpreted the data as follows:
*Intensity is stored in bits 26-32
*Mode is stored in bits 6-9 and 34-37

We can guess that:
*Collar select (1 or 2) is in bits 3-5 and 38-40
*Remote's unique ID is in bits 10-26
*Bits 1,2, 41, and 42 are constant (framing?)

I only have one collar to test from - if anyone knows better than me, please let me know!

Data is transmitted as follows:
```
ST -> On for 1.6ms, off for 800us
1 -> On for 800us, off for 200us
0 -> On for 400us, off for 600us
```

# FAQs
*Why text message control?

At the time I was working on this, I had concurrent projects going on involving sending SMSes, so it was easy to reuse that.

# Comments
I probably won't do any additional work on this, but I wanted to share in case it helps anyone else in the future. 