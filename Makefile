OSTYPE := $(shell uname -s)

INC_DIRS = -IOpenNI/

CC = g++

CCFLAGS = -c
CCFLAGS2 = 

USED_LIBS += -lOpenNI

EXE_TRACKER = tracker
EXE_REC = recognition

ifeq ("$(OSTYPE)","Darwin")
	CCFLAGS += -arch x86_64
	LDFLAGS += -framework OpenGL -framework GLUT -framework OpenCV `pkg-config --cflags opencv` `pkg-config --libs opencv`
	CCFLAGS2 += -arch x86_64
else
	USED_LIBS += -lglut -lglib-2.0 -lcv -lcv -lcxcore 
	INC_DIRS += -IOpenNI/ -I/usr/local/include/opencv 
	LDFLAGS += `pkg-config --cflags opencv` `pkg-config --libs opencv`
endif

OBJS: Release/FaceRecognition.o Release/ImageUtil.o Release/KeyboardUtil.o Release/KinectUtil.o Release/MessageQueue.o Release/SceneDrawer.o Release/UserUtil.o Release/Tracker.o

all: $(OBJS)
	$(CC) -o $(EXE_TRACKER) Release/KeyboardUtil.o Release/KinectUtil.o Release/MessageQueue.o Release/SceneDrawer.o Release/UserUtil.o Release/Tracker.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)
	$(CC) -o $(EXE_REC) Release/ImageUtil.o Release/KeyboardUtil.o Release/KinectUtil.o Release/MessageQueue.o Release/UserUtil.o Release/FaceRecognition.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)

Release/ImageUtil.o: 
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/ImageUtil.o ImageUtil.cpp
Release/KeyboardUtil.o:
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/KeyboardUtil.o KeyboardUtil.cpp
Release/KinectUtil.o: 
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/KinectUtil.o KinectUtil.cpp
Release/MessageQueue.o: Release/KinectUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/MessageQueue.o MessageQueue.cpp
Release/SceneDrawer.o:
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/SceneDrawer.o SceneDrawer.cpp
Release/UserUtil.o: Release/MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/UserUtil.o UserUtil.cpp
Release/FaceRecognition.o: Release/ImageUtil.o  Release/KinectUtil.o Release/MessageQueue.o Release/UserUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/FaceRecognition.o FaceRecognition.cpp
Release/Tracker.o: Release/KeyboardUtil.o Release/KinectUtil.o Release/MessageQueue.o Release/SceneDrawer.o Release/UserUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/Tracker.o Tracker.cpp

clean: 
	rm -rf Release/*.o $(EXE_TRACKER) $(EXE_REC)