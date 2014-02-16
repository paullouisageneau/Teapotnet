prefix=/usr

DESTDIR=
TPROOT=/var/lib/teapotnet

CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-O2
LDFLAGS=-O2
LDLIBS=-lpthread -ldl

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
        LDLIBS += -framework CoreFoundation
endif

SRCS=$(shell printf "%s " tpn/*.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all: teapotnet

include/sqlite3.o: include/sqlite3.c
	$(CC) -c $(CPPFLAGS) -I. -DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS $*.c -o $*.o
	
%.o: %.cpp
	$(CXX) $(CPPFLAGS) -I. -MMD -MP -o $@ -c $<
	
-include $(subst .o,.d,$(OBJS))
	
teapotnet: $(OBJS) include/sqlite3.o
	$(CXX) $(LDFLAGS) -o teapotnet $(OBJS) include/sqlite3.o $(LDLIBS) 
	
clean:
	$(RM) include/*.o tpn/*.o tpn/*.d

dist-clean: clean
	$(RM) teapotnet
	$(RM) tpn/*~

install: teapotnet teapotnet.service
	install -d $(DESTDIR)$(prefix)/bin
	install -d $(DESTDIR)$(prefix)/share/teapotnet
	install -d $(DESTDIR)/etc/teapotnet
	install -m 0755 teapotnet $(DESTDIR)$(prefix)/bin
	cp -r static $(DESTDIR)$(prefix)/share/teapotnet
	echo "static_dir=$(prefix)/share/teapotnet/static" > $(DESTDIR)/etc/teapotnet/config.conf
	@if [ -z "$(DESTDIR)" ]; then bash -c "./daemon.sh install $(prefix) $(TPROOT)"; fi

uninstall:
	rm -f $(DESTDIR)$(prefix)/bin/teaponet
	rm -rf $(DESTDIR)$(prefix)/share/teapotnet
	rm -f $(DESTDIR)/etc/teapotnet/config.conf
	@if [ -z "$(DESTDIR)" ]; then bash -c "./daemon.sh uninstall $(prefix) $(TPROOT)"; fi

bundle: teapotnet
ifeq ($(UNAME_S),Darwin)
	mkdir -p Teapotnet.app/Contents
	cp Info.plist Teapotnet.app/Contents/Info.plist
	mkdir -p Teapotnet.app/Contents/MacOS
	cp teapotnet Teapotnet.app/Contents/MacOS/Teapotnet
	mkdir -p Teapotnet.app/Contents/Resources
	cp teapotnet.icns Teapotnet.app/Contents/Resources/TeapotnetIcon.icns
	cp -r static Teapotnet.app/Contents/Resources/static
	cd ..
	zip -r Teapotnet.zip Teapotnet.app
	rm -r Teapotnet.app
else
	@echo "This target is only available on Mac OS"
endif
