#
# qw scores makefile
#
# johnnycz
#

CC = "g++"
LD = "g++"

LDFLAGS = -L/usr/lib -Llibircclient/src -lpthread -lc -lircclient -lm
CCFLAGS = -Ilibircclient/include

OFILES = \
	main.o	\
	http.o	\
	utils.o	\
	irc.o	\
	qw.o \
	conf.o \
	sockets.o \
	calendar.o \
	events.o

build: clean dep all

clean:
	rm *.o scoreacc 2> nul &
	
dep:
	$(CC) $(CCFLAGS) -MM *.cpp > .depend
	
.cpp.o:
	$(CC) $(CCFLAGS) -c $<

-include .depend

all: scoreacc

scoreacc: $(OFILES)
	$(LD) -o scoreacc $(OFILES) $(LDFLAGS) 
