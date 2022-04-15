# daqviki Makefile
NAME = daqviki
OBJ = $(NAME).o log_file.o i2cfunc.o
LIBS = -ldaqhats -lm -lwiringPi
CFLAGS = -Wall -I/usr/local/include -I$(WSDIR)/include -pthread -pedantic -g
CC = g++
EXTENSION = .cpp

all: $(NAME)

%.o: %$(EXTENSION) $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(NAME): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	@rm -f *.o *~ core $(NAME)
