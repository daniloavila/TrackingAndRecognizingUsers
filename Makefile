OSTYPE := $(shell uname -s)

INC_DIRS = -IOpenNI/

CC = g++

CCFLAGS = -c
CCFLAGS2 = 

USED_LIBS += -lOpenNI

EXE_TRACKER = tracker
EXE_REC = recognizer
EXE_REG = register
EXE_LIB = libTrackerRunnable.so
RELEASE_DIR = Release/

ifeq ("$(OSTYPE)","Darwin")
	CCFLAGS += -arch x86_64
	LDFLAGS += -framework OpenGL -framework GLUT `pkg-config --cflags opencv` `pkg-config --libs opencv`
	CCFLAGS2 += -arch x86_64
else
	USED_LIBS += -lglut -lglib-2.0 -lcv -lcxcore 
	INC_DIRS += -I/usr/local/include/opencv 
	LDFLAGS += `pkg-config --cflags opencv` `pkg-config --libs opencv`
endif

OBJS: $(RELEASE_DIR)Recognizer.o $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)StatisticsUtil.o $(RELEASE_DIR)Tracker.o $(RELEASE_DIR)Register.o $(RELEASE_DIR)TrackerRunnable.o

all: $(OBJS)
	$(CC) -o $(EXE_TRACKER) $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)StatisticsUtil.o $(RELEASE_DIR)Tracker.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS) 
	$(CC) -o $(EXE_REC) $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)Recognizer.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)
	$(CC) -o $(EXE_REG) $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)Register.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)
	$(CC) -shared -Wl,-soname,TrackerRunnable -o $(EXE_LIB) $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)TrackerRunnable.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)

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
$(RELEASE_DIR)StatisticsUtil.o: $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)StatisticsUtil.o StatisticsUtil.cpp	
$(RELEASE_DIR)Recognizer.o: $(RELEASE_DIR)ImageUtil.o  $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)Recognizer.o Recognizer.cpp
$(RELEASE_DIR)Tracker.o: $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)StatisticsUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)Tracker.o Tracker.cpp
$(RELEASE_DIR)Register.o: $(RELEASE_DIR)ImageUtil.o  $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)Register.o Register.cpp
$(RELEASE_DIR)TrackerRunnable.o: $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -I/usr/lib/jvm/java-6-openjdk/include -I/usr/lib/jvm/java-6-openjdk/include/linux -o $(RELEASE_DIR)TrackerRunnable.o TrackerRunnable.cpp

clean: 
	rm -rf $(RELEASE_DIR)*.o $(EXE_TRACKER) $(EXE_REC) $(EXE_REG) $(EXE_LIB)
