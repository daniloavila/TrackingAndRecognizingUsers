OSTYPE := $(shell uname -s)

BIN_DIR = Bin

INC_DIRS = /home/tales/kinect/OpenNI/Include

SRC_FILES = *.cpp	

EXE_NAME = onlineFaceRec

ifneq "$(GLES)" "1"
ifeq ("$(OSTYPE)","Darwin")
	LDFLAGS += -framework OpenGL -framework GLUT
else
	USED_LIBS += glut glib-2.0 cv
#	USED_LIBS += cv
	LDFLAGS += -lcv -lcxcore -I/usr/local/include/opencv `pkg-config --cflags opencv` `pkg-config --libs opencv`
endif
else
	DEFINES += USE_GLES
	USED_LIBS += GLES_CM IMGegl srv_um
	SRC_FILES += ../../../../../Samples/NiUserTracker/opengles.cpp
endif

USED_LIBS += OpenNI

include CommonMakefile

