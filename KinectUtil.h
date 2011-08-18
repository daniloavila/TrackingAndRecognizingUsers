#ifndef KINECT_UTIL_H_
#define KINECT_UTIL_H_

#include "XnCppWrapper.h"
#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include "MessageQueue.h"

// Impacta tb na fila de mensagens
#define KINECT_WIDTH_CAPTURE 480
#define KINECT_HEIGHT_CAPTURE 640
#define KINECT_FPS_CAPTURE 30
#define KINECT_NUMBER_OF_CHANNELS 3

/**
 * Retorna um frame do Kinect e inicia o mesmo caso ainda não o tenha sido feito.
 */
IplImage* getKinectFrame();

char* transformToCharAray(const XnRGB24Pixel* source);

void transformAreaVision(short unsigned int* source, int id);

char *getSharedMemory(int memory_id, bool create);

unsigned int getMemoryKey();

#endif 
