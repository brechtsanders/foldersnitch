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
ifeq ($(OS),Windows_NT)
PROCESSFOLDERSLIBS += -ldirtravw
else
PROCESSFOLDERSLIBS += -ldirtrav
endif
FINDDUPLICATESLIBS := -lsqlite3 -lmhash
GENERATEREPORTSLIBS := -lsqlite3 -lxlsxio_write
GENERATEUSERREPORTSLIBS := -lsqlite3 -lxlsxio_write -ldirtrav
TOOLSBIN = processfolders$(BINEXT) findduplicates$(BINEXT) findduplicates$(BINEXT) generatereports$(BINEXT) generateuserreports$(BINEXT)

tools: $(TOOLSBIN)

default: all

all: tools

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) 

processfolders$(BINEXT): src/processfolders.o src/sqlitefunctions.o
	$(CC) -o $@ $^ $(PROCESSFOLDERSLIBS)
	
findduplicates$(BINEXT): src/findduplicates.o src/sqlitefunctions.o
	$(CXX) -o $@ $^ $(FINDDUPLICATESLIBS)

generatereports$(BINEXT): src/generatereports.o src/sqlitefunctions.o src/dataoutput.o
	$(CXX) -o $@ $^ $(GENERATEREPORTSLIBS)

generateuserreports$(BINEXT): src/generateuserreports.o src/sqlitefunctions.o src/dataoutput.o
	$(CXX) -o $@ $^ $(GENERATEUSERREPORTSLIBS)

clean:
	$(RM) src/*.o $(TOOLSBIN)

