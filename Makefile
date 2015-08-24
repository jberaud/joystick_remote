# Makefile for joystick_remote

PREFIX  ?=/
INSTALL = install
CC	= gcc
CFLAGS	= -g -Wall -Wextra -O3
HEADERS = joystick_remote.h remote.h joystick.h
LIBS	= -lpthread
PROGRAM = joystick_remote

OBJS	= joystick_remote.o remote.o joystick.o

all: $(PROGRAM)

%.o : %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROGRAM) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	-rm -f $(OBJS) $(PROGRAM) *~

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	$(INSTALL) -D $(PROGRAM) $(DESTDIR)$(PREFIX)/bin
