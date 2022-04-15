LIB_PATH = /usr/local/lib

all: libi2cfunc.a dspgen

libi2cfunc.a: i2cfunc.c
	gcc -c i2cfunc.c
	ar -rs libi2cfunc.a i2cfunc.o

dspgen: main.c
	gcc -o dspgen main.c -I. -L. -L ${LIB_PATH} -lwiringPi -li2cfunc

clean:
	rm -rf *.o *.a dspgen


