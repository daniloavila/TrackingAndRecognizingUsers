OSTYPE := $(shell uname -s)

BIN_DIR = Bin

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
# TRACKER_OBJS: KeyboardUtil.o KinectUtil.o MessageQueue.o SceneDrawer.o UserUtil.o Tracker.o
# REC_OBJS: ImageUtil.o KeyboardUtil.o KinectUtil.o MessageQueue.o UserUtil.o FaceRecognition.o

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
Release/Tracker.o: KeyboardUtil.o Release/KinectUtil.o Release/MessageQueue.o Release/SceneDrawer.o Release/UserUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o Release/Tracker.o Tracker.cpp

clean: 
	rm -rf Release/*.o $(EXE_TRACKER) $(EXE_REC)

# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/FaceRecognition.o FaceRecognition.cpp	
# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/ImageUtil.o ImageUtil.cpp	
# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/KeyboardUtil.o KeyboardUtil.cpp	
# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/KinectUtil.o KinectUtil.cpp	
# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/MessageQueue.o MessageQueue.cpp	
# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/SceneDrawer.o SceneDrawer.cpp	
# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/UserUtil.o UserUtil.cpp	
# g++ -c -arch x86_64 -O2 -DNDEBUG -msse3 -IOpenNI/  -o Release/main.o main.cpp	
# g++ -o Bin/Release/kinectTrack ./Release/FaceRecognition.o ./Release/ImageUtil.o ./Release/KeyboardUtil.o ./Release/KinectUtil.o ./Release/MessageQueue.o ./Release/SceneDrawer.o ./Release/UserUtil.o ./Release/main.o -framework OpenGL -framework GLUT -framework OpenCV `pkg-config --cflags opencv` `pkg-config --libs opencv` -arch x86_64   -LBin/Release -lOpenNI
# ld: warning: in /Library/Frameworks//OpenCV.framework/OpenCV, missing required architecture x86_64 in file

#g++ -o tracker -arch x86_64  -IOpenNI/ -framework OpenGL -framework GLUT -framework OpenCV `pkg-config --cflags opencv` `pkg-config --libs opencv`  