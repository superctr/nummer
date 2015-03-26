# nummer

Nummer is a number station voice generator written in C using PortAudio.

### Building:

If you don't like codeblocks, you can use [cbp2make](http://sourceforge.net/projects/cbp2make/) to generate a makefile.

Or you could just open a console and compile it manually.

```
gcc -Wall -O2  -c ./main.c -o ./main.o
```

```
g++ -o ./nummer ./main.o -s -lportaudio
```

### Usage:

```
nummer 01234 56789
```

for a simple count.

To play something similar to what [E11a](http://priyom.org/number-stations/english/e11a) would transmit:

```
nummer -c 884/04 195 "* 43689 43689 31781 31781 01397 01397 79735 79735 * 43689 31781 01397 79735 ,_@"
```

This includes a call-up section that runs for a little over 3 minutes, followed by the message.

For more information, run the application without any options.

If you're on Linux and don't hear anything, you might have to manually select the output device. Do that like this:

```
nummer -o
```

to get a list of devices, then

```
nummer -o "<your device>" (rest of options...)
```

to select the device.

## Copyright
### 2015 Ian Karlsson.

Released under the MIT license.
(I do not own the voice samples, they could be removed eventually.)

