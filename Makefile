# prerequisites:
# - mhash
# - sqlite3
# - libdirtrav
# - xlsxio
# runningrun under Linux may require this:
#   LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH 

ifeq ($(OS),)
OS = $(shell uname -s)
#ifneq ($(findstring Windows,$(OS))$(findstring MINGW,$(OS))$(findstring MSYS,$(OS))$(findstring CYGWIN,$(OS)),)
#OS = Windows_NT
#endif
endif
PREFIX = /usr/local
CC   = gcc
CPP  = g++
AR   = ar
LIBPREFIX = lib
LIBEXT = .a
ifeq ($(OS),Windows_NT)
BINEXT = .exe
SOLIBPREFIX =
SOEXT = .dll
else ifeq ($(OS),Darwin)
BINEXT =
SOLIBPREFIX = lib
SOEXT = .dylib
else
BINEXT =
SOLIBPREFIX = lib
SOEXT = .so
endif
DEFS = -DUSE_XLSXIO
INCS = 
CFLAGS = $(DEFS) $(INCS) -Os
CPPFLAGS = $(DEFS) $(INCS) -Os
LIBS =
LDFLAGS =
ifeq ($(OS),Darwin)
STRIPFLAG =
else
STRIPFLAG = -s
endif
MKDIR = mkdir -p
RM = rm -f
RMDIR = rm -rf
CP = cp -f
CPDIR = cp -rf
DOXYGEN = $(shell which doxygen)

OSALIAS := $(OS)
ifeq ($(OS),Windows_NT)
ifneq (,$(findstring x86_64,$(shell gcc --version)))
OSALIAS := win64
else
OSALIAS := win32
endif
endif

PROCESSFOLDERSLIBS := -lsqlite3
FINDDUPLICATESLIBS := -lsqlite3 -lmhash
GENERATEREPORTSLIBS := -lsqlite3 -lxlsxio_write
GENERATEUSERREPORTSLIBS := -lsqlite3 -lxlsxio_write -ldirtrav
ifeq ($(OS),Windows_NT)
PROCESSFOLDERSLIBS += -ldirtravw
FINDDUPLICATESLIBS += -ldirtravw
else
PROCESSFOLDERSLIBS += -ldirtrav
FINDDUPLICATESLIBS += -ldirtrav
endif
TOOLSBIN = processfolders$(BINEXT) findduplicates$(BINEXT) findduplicates$(BINEXT) generatereports$(BINEXT) generateuserreports$(BINEXT)

COMMON_PACKAGE_FILES = README.md LICENSE.txt Changelog.txt
SOURCE_PACKAGE_FILES = $(COMMON_PACKAGE_FILES) Makefile src/*.h src/*.c src/*.cpp build/*.workspace build/*.cbp build/*.depend

tools: $(TOOLSBIN)

default: all

all: tools

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) 

processfolders$(BINEXT): src/processfolders.o src/sqlitefunctions.o
	$(CC) -s -o $@ $^ $(PROCESSFOLDERSLIBS)
	
findduplicates$(BINEXT): src/findduplicates.o src/sqlitefunctions.o
	$(CXX) -s -o $@ $^ $(FINDDUPLICATESLIBS)

generatereports$(BINEXT): src/generatereports.o src/sqlitefunctions.o src/dataoutput.o
	$(CXX) -s -o $@ $^ $(GENERATEREPORTSLIBS)

generateuserreports$(BINEXT): src/generateuserreports.o src/sqlitefunctions.o src/dataoutput.o
	$(CXX) -s -o $@ $^ $(GENERATEUSERREPORTSLIBS)

install: all
	$(MKDIR) $(PREFIX)/bin
	$(CP) $(TOOLSBIN) $(PREFIX)/bin/

.PHONY: version
version:
	sed -ne "s/^#define\s*FOLDERREPORTS_VERSION_[A-Z]*\s*\([0-9]*\)\s*$$/\1./p" src/folderreportsversion.h | tr -d "\n" | sed -e "s/\.$$//" > version

.PHONY: package
package: version
	tar cfJ foldersnitch-$(shell cat version).tar.xz --transform="s?^?foldersnitch-$(shell cat version)/?" $(SOURCE_PACKAGE_FILES)

.PHONY: package
binarypackage: version
ifneq ($(OS),Windows_NT)
	$(MAKE) PREFIX=binarypackage_temp_$(OSALIAS) install
	tar cfJ foldersnitch-$(shell cat version)-$(OSALIAS).tar.xz --transform="s?^binarypackage_temp_$(OSALIAS)/??" $(COMMON_PACKAGE_FILES) binarypackage_temp_$(OSALIAS)/*
else
	$(MAKE) PREFIX=binarypackage_temp_$(OSALIAS) install
	cp -f $(COMMON_PACKAGE_FILES) binarypackage_temp_$(OSALIAS)
	rm -f foldersnitch-$(shell cat version)-$(OSALIAS).zip
	cd binarypackage_temp_$(OSALIAS) && zip -r9 ../foldersnitch-$(shell cat version)-$(OSALIAS).zip $(COMMON_PACKAGE_FILES) * && cd ..
endif
	rm -rf binarypackage_temp_$(OSALIAS)

clean:
	$(RM) src/*.o $(TOOLSBIN) version

