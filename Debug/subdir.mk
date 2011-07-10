################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../FaceRecognition.cpp \
../ImageUtil.cpp \
../KeyboardUtil.cpp \
../KinectUtil.cpp \
../MessageQueue.cpp \
../SceneDrawer.cpp \
../Tracker.cpp \
../UserUtil.cpp 

OBJS += \
./FaceRecognition.o \
./ImageUtil.o \
./KeyboardUtil.o \
./KinectUtil.o \
./MessageQueue.o \
./SceneDrawer.o \
./Tracker.o \
./UserUtil.o 

CPP_DEPS += \
./FaceRecognition.d \
./ImageUtil.d \
./KeyboardUtil.d \
./KinectUtil.d \
./MessageQueue.d \
./SceneDrawer.d \
./Tracker.d \
./UserUtil.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -I/usr/local/include/opencv -I/home/tales/workspace-eclipse-c/TrackerRecognitionUser/OpenNI -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


