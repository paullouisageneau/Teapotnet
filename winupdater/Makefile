CC=i486-mingw32-gcc
CXX=i486-mingw32-g++
RES=i486-mingw32-windres
STRIP=i486-mingw32-strip
RM=rm -f
CPPFLAGS=-O
LDFLAGS=-O -static -static-libgcc
LDLIBS=-lwininet

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

res.o:
	echo 1 ICON "winupdater.ico" > res.rc
	$(RES) res.rc res.o
