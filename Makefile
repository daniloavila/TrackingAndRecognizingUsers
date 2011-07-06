OSTYPE := $(shell uname -s)

INC_DIRS = -IOpenNI/

CC = g++

CCFLAGS = -c
CCFLAGS2 = 

USED_LIBS += -lOpenNI

EXE_TRACKER = tracker
EXE_REC = recognition
RELEASE_DIR = Release/

ifeq ("$(OSTYPE)","Darwin")
	CCFLAGS += -arch x86_64
	LDFLAGS += -framework OpenGL -framework GLUT -framework OpenCV `pkg-config --cflags opencv` `pkg-config --libs opencv`
	CCFLAGS2 += -arch x86_64
else
	USED_LIBS += -lglut -lglib-2.0 -lcv -lcv -lcxcore 
	INC_DIRS += -IOpenNI/ -I/usr/local/include/opencv 
	LDFLAGS += `pkg-config --cflags opencv` `pkg-config --libs opencv`
endif

OBJS: $(RELEASE_DIR)FaceRecognition.o $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)UserUtil.o $(RELEASE_DIR)Tracker.o

all: $(OBJS)
	$(CC) -o $(EXE_TRACKER) $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)UserUtil.o $(RELEASE_DIR)Tracker.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)
	$(CC) -o $(EXE_REC) $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)UserUtil.o $(RELEASE_DIR)FaceRecognition.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)

$(RELEASE_DIR)ImageUtil.o: 
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)ImageUtil.o ImageUtil.cpp
$(RELEASE_DIR)KeyboardUtil.o:
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)KeyboardUtil.o KeyboardUtil.cpp
$(RELEASE_DIR)KinectUtil.o: 
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)KinectUtil.o KinectUtil.cpp
$(RELEASE_DIR)MessageQueue.o: $(RELEASE_DIR)KinectUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)MessageQueue.o MessageQueue.cpp
$(RELEASE_DIR)SceneDrawer.o:
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)SceneDrawer.o SceneDrawer.cpp
$(RELEASE_DIR)UserUtil.o: $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)UserUtil.o UserUtil.cpp
$(RELEASE_DIR)FaceRecognition.o: $(RELEASE_DIR)ImageUtil.o  $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)UserUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)FaceRecognition.o FaceRecognition.cpp
$(RELEASE_DIR)Tracker.o: $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)UserUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)Tracker.o Tracker.cpp

clean: 
	rm -rf $(RELEASE_DIR)*.o $(EXE_TRACKER) $(EXE_REC)