DEFINES = --std=c++11 -pthread -Werror -DVERSION=\"1.0.$(shell [[ ! -d .git ]] && echo -n 1; [[ -d .git ]] && git log --pretty=format:\"%h\" | head -n 1 | tr -d '"' | tr -d '\n')\" -Wall -fPIC -O3 -DRELEASE=1

ifeq '$(findstring ;,$(PATH))' ';'
    detected_OS := Windows
else
    detected_OS := $(shell uname 2>/dev/null || echo Unknown)
    detected_OS := $(patsubst CYGWIN%,Cygwin,$(detected_OS))
    detected_OS := $(patsubst MSYS%,MSYS,$(detected_OS))
    detected_OS := $(patsubst MINGW%,MSYS,$(detected_OS))
endif

ifeq ($(detected_OS),Darwin)
	LIB_EXT = dylib
	INSTALL_INC_ROOT = /usr/local/include/pe
	INSTALL_LIB_ROOT = /usr/local/lib
	CC = clang++
	EX_DEFINES = -I/usr/local/opt/openssl/include/
	EX_FLAGS = -L/usr/local/opt/openssl/lib
else
	LIB_EXT = so
	INSTALL_INC_ROOT = /usr/include/pe
	INSTALL_LIB_ROOT = /usr/lib64
	CC = g++
	EX_DEFINES = 
	EX_FLAGS =
endif

PECO_AUTO_STAGE_DEFINES = $(EX_DEFINES) -I$(INSTALL_INC_ROOT)/utils -I$(INSTALL_INC_ROOT)/cotask -I$(INSTALL_INC_ROOT)/conet -I./
PECO_AUTO_STAGE_CFLAGS = $(EX_FLAGS) -lcotask -lssl -lresolv -lpeutils -lconet

PECO_AUTO_STAGE_CPP_FILES = ./main.cpp
PECO_AUTO_STAGE_OBJ_FILES = $(PECO_AUTO_STAGE_CPP_FILES:.cpp=.o)

all : 
	@mkdir -p bin
	$(MAKE) coas

%.o : %.cpp
	$(CC) $(DEFINES) $(PECO_AUTO_STAGE_DEFINES) -c $< -o $@

coas : $(PECO_AUTO_STAGE_OBJ_FILES)
	$(CC) -o bin/coas $^ $(PECO_AUTO_STAGE_CFLAGS)

loadpac : $(PECO_LOAD_PAC_OBJ_FILES)
	$(CC) -o bin/loadpac $^ $(PECO_AUTO_STAGE_CFLAGS)

install : 
	@cp -vrf ./bin/coas /usr/local/bin/

clean :
	@rm -vrf *.o
	@rm -vrf bin/coas
