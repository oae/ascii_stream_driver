obj-m += asciistreamer.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc writer.c -o writer
	gcc reader.c -o reader
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm writer
	rm reader