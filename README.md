ascii_stream_driver
===================

Simple char device driver for streaming


To Install
-------------

```
make
sudo insmod asciistreamer.ko
sudo chown $USER /dev/asciistreamer
```

To Run
-------------

First Run `./writer starwars.txt` then open a new terminal and run `./reader`. You can run `./reader` as much as you want.

To Stop
-------------

Just press Ctrl + c in `./writer` terminal.


To Remove
-------------

```
sudo rmmod asciistreamer
make clean
```
