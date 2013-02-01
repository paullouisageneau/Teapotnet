CC=$(CROSS)gcc
CXX=$(CROSS)g++
RES=$(CROSS)windres
STRIP=$(CROSS)strip
RM=rm -f
CPPFLAGS=-O
LDFLAGS=-O -static -static-libgcc -mwindows -Wl,-subsystem,windows
LDLIBS=-lwininet -lcrypt32

SRCS=$(shell printf "%s " *.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all: winupdater.exe

winupdater.exe: $(OBJS) res.o
	$(CXX) $(LDFLAGS) -o winupdater.exe $(OBJS) res.o $(LDLIBS) 
	$(STRIP) winupdater.exe

clean:
	$(RM) $(OBJS) res.o
	$(RM) res.rc
	
dist-clean: clean
	$(RM) *~
	$(RM) winupdater.exe

res.o: winupdater.exe.manifest
	echo 1 24 "winupdater.exe.manifest" > res.rc
	$(RES) res.rc res.o

