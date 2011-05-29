OSTYPE := $(shell uname -s)

BIN_DIR = ../Bin

# INC_DIRS = ../../Include /usr/include/ni

SRC_FILES = *.cpp

EXE_NAME = onlineFaceRec

ifeq ("$(OSTYPE)","Darwin")
	LDFLAGS +=  -framework OpenCV `pkg-config --cflags opencv` `pkg-config --libs opencv`
else
	# USED_LIBS += glut
endif

# USED_LIBS += OpenNI

# LIB_DIRS += ../../Lib

#colocar isso no .profile export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig/

include ../CommonMakefile