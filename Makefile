OSTYPE := $(shell uname -s)

INC_DIRS = -Iinclude/OpenNI/
RELEASE_DIR = bin/
SRC = src/
LIB_DIR = lib/

CC = g++

CCFLAGS = -c
CCFLAGS2 = 
SO_LINK = 
DYNAMIC_LINK = 

USED_LIBS += -lOpenNI

# Se alterar PATH aqui deve se alterar no Defintions.h
NAME_OF_LIBRARY =
SHARES_LIBRARY_PATH = /usr/lib/
BIN_PATH = /usr/bin/
FILES_PATH = /usr/share/true

# Se alterar o NOME aqui deve se alterar no Defintions.h
EXE_TRACKER = tracker
EXE_REC = recognizer
EXE_REG = register

ifeq ("$(OSTYPE)","Darwin")
	NAME_OF_LIBRARY = libTrackerRunnable.dylib
	CCFLAGS += -arch x86_64
	LDFLAGS += -framework OpenGL -framework GLUT `pkg-config --cflags opencv` `pkg-config --libs opencv`
	CCFLAGS2 += -arch x86_64
	SO_LINK += -I/System/Library/Java/JavaVirtualMachines/1.6.0.jdk/Contents/Home/include 
	EXE_LIB = $(LIB_DIR)$(NAME_OF_LIBRARY)
	DYNAMIC_LINK += -dynamiclib -Wl,-dylib_install_name,TrackerRunnable
else
	NAME_OF_LIBRARY = libTrackerRunnable.so
	USED_LIBS += -lglut -lglib-2.0 -lcv -lcxcore 
	INC_DIRS += -I/usr/local/include/opencv 
	LDFLAGS += `pkg-config --cflags opencv` `pkg-config --libs opencv`
	SO_LINK += -I/usr/lib/jvm/java-6-openjdk/include -I/usr/lib/jvm/java-6-openjdk/include/linux
	EXE_LIB = $(LIB_DIR)$(NAME_OF_LIBRARY)
	DYNAMIC_LINK += -shared -Wl,-soname,TrackerRunnable
endif

OBJS: 	$(RELEASE_DIR)Recognizer.o \
 		$(RELEASE_DIR)ImageUtil.o \
 		$(RELEASE_DIR)KeyboardUtil.o \
 		$(RELEASE_DIR)KinectUtil.o \
 		$(RELEASE_DIR)MessageQueue.o \
 		$(RELEASE_DIR)SceneDrawer.o \
 		$(RELEASE_DIR)StatisticsUtil.o \
 		$(RELEASE_DIR)Tracker.o \
 		$(RELEASE_DIR)Register.o \
 		$(RELEASE_DIR)TrackerRunnable.o \
 		$(RELEASE_DIR)StringUtil.o

all: $(OBJS)
	$(CC) -o $(EXE_TRACKER) $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)StatisticsUtil.o $(RELEASE_DIR)StringUtil.o $(RELEASE_DIR)Tracker.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS) 
	$(CC) -o $(EXE_REC) $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)StringUtil.o $(RELEASE_DIR)Recognizer.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)
	$(CC) -o $(EXE_REG) $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)StringUtil.o $(RELEASE_DIR)Register.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)
	$(CC) $(DYNAMIC_LINK) -o $(EXE_LIB) $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)ImageUtil.o $(RELEASE_DIR)StringUtil.o $(RELEASE_DIR)TrackerRunnable.o $(CCFLAGS2) $(USED_LIBS) $(LDFLAGS)

$(RELEASE_DIR)ImageUtil.o: 
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)ImageUtil.o $(SRC)ImageUtil.cpp
$(RELEASE_DIR)KeyboardUtil.o:
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)KeyboardUtil.o $(SRC)KeyboardUtil.cpp
$(RELEASE_DIR)KinectUtil.o: 
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)KinectUtil.o $(SRC)KinectUtil.cpp
$(RELEASE_DIR)MessageQueue.o: $(RELEASE_DIR)KinectUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)MessageQueue.o $(SRC)MessageQueue.cpp
$(RELEASE_DIR)SceneDrawer.o:
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)SceneDrawer.o $(SRC)SceneDrawer.cpp
$(RELEASE_DIR)StatisticsUtil.o: $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)StatisticsUtil.o $(SRC)StatisticsUtil.cpp
$(RELEASE_DIR)StringUtil.o: 
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)StringUtil.o $(SRC)StringUtil.cpp	
$(RELEASE_DIR)Recognizer.o: $(RELEASE_DIR)ImageUtil.o  $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)Recognizer.o $(SRC)Recognizer.cpp
$(RELEASE_DIR)Tracker.o: $(RELEASE_DIR)KeyboardUtil.o $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)SceneDrawer.o $(RELEASE_DIR)StatisticsUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)Tracker.o $(SRC)Tracker.cpp
$(RELEASE_DIR)Register.o: $(RELEASE_DIR)ImageUtil.o  $(RELEASE_DIR)KinectUtil.o $(RELEASE_DIR)MessageQueue.o
	$(CC) $(CCFLAGS) $(INC_DIRS) -o $(RELEASE_DIR)Register.o $(SRC)Register.cpp
$(RELEASE_DIR)TrackerRunnable.o: $(RELEASE_DIR)MessageQueue.o $(RELEASE_DIR)ImageUtil.o
	$(CC) $(CCFLAGS) $(INC_DIRS) $(SO_LINK) -o $(RELEASE_DIR)TrackerRunnable.o $(SRC)TrackerRunnable.cpp

clean: 
	rm -rf $(RELEASE_DIR)*.o $(EXE_TRACKER) $(EXE_REC) $(EXE_REG) $(EXE_LIB)

install: $(all)
	sudo cp $(EXE_TRACKER) $(BIN_PATH)$(EXE_TRACKER)
	sudo cp recognizer $(BIN_PATH)$(EXE_REC)
	sudo cp register $(BIN_PATH)$(EXE_REG)
	
	sudo mkdir -pv $(FILES_PATH)
	sudo mkdir -pv $(FILES_PATH)/LOG/
	sudo cp -r Config/ $(FILES_PATH)/Config/
	sudo cp -r Eigenfaces/ $(FILES_PATH)/Eigenfaces/
	sudo cp -r Eigenfaces/facedata.xml $(FILES_PATH)/Eigenfaces/facedata.xml
	sudo cp -r Eigenfaces/haarcascade_frontalface_alt.xml $(FILES_PATH)/Eigenfaces/haarcascade_frontalface_alt.xml
	
	# TODO : Verificar qual a premissão correta nesse caso
	sudo chmod 777 $(FILES_PATH)/Eigenfaces/ -R
	
	sudo cp lib/$(NAME_OF_LIBRARY) $(SHARES_LIBRARY_PATH)$(NAME_OF_LIBRARY).1
	ldconfig -n $(FILES_PATH)/lib/
	
uninstall: 
	sudo rm -f $(BIN_PATH)$(EXE_TRACKER)
	sudo rm -f $(BIN_PATH)$(EXE_REC)
	sudo rm -f $(BIN_PATH)$(EXE_REG)
	
	sudo rm -r -f $(FILES_PATH)
	sudo rm -f $(SHARES_LIBRARY_PATH)$(NAME_OF_LIBRARY).1
