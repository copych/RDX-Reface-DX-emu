# RDX -- Yamaha Reface DX emulator
This is an ESP32S3 / ESP32P4 approximation of a known FM synth. I don't have the real one, so it's a kind of a self-set challenge. It has a lot of 'AS IS', and mostly consists of compromises.
Yet, by default it's a USB-MIDI device that one can use with some midi host. It understands almost every documented midi feature of a Reface DX and can connect to Soundmondo(tm), though I don't know how the original behaves exactly, I've never had my hands on a real Reface synth.
On an ESP32-S3 RDX has 7-8 notes (ESP32-P4 gives 11 notes) polyphony depending on the effects chosen, 4 FM-operators and 2 effects in a chain. Sample rate is same as original's 44100Hz.
The code is written from scratch, but has a lot of AI assisted pieces. Otherwise it'd take me forever to get to the current state.

<a href=https://youtube.com/shorts/Vu2NzcPyU-I>Youtube short video to have some idea of what it is</a>

##  TODO 
*  GUI
*  Looper
*  Hardware controls
*  Ability of saving patches
*  Effects improving

##  BUILD
RDX folder contains the Arduino IDE project.

**!IMPORTANT!** Upload `/data` folder with patches to the ESP board, otherwise you won't hear anything worth.

Read the instructions here [https://github.com/earlephilhower/arduino-littlefs-upload]

Refer to `config.h` to see pins and edit settings

<img src=https://github.com/copych/RDX/blob/main/rdx.jpg>


##  COMPILE OPTIONS
Please, refer to the `config.h` for pins etc. The project is mutating, so keeping docs in sync is a hard task for me alone. 
