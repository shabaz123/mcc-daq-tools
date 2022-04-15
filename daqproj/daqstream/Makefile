# daqstream Makefile
NAME = daqstream
OBJ = $(NAME).o
# change WSDIR path as appropriate
WSDIR = /home/pi/development/wsServer
LIBS = -ldaqhats -lm $(WSDIR)/libws.a
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
