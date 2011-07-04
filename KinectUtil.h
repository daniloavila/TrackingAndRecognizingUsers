#ifndef KINECT_UTIL_H_
#define KINECT_UTIL_H_

#include "XnCppWrapper.h"
#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

/**
 * Retorna um frame do Kinect e inicia o mesmo caso ainda não o tenha sido feito.
 */
IplImage* getKinectFrame();

#define KINECT_WIDTH_CAPTURE 480
#define KINECT_HEIGHT_CAPTURE 640
#define KINECT_FPS_CAPTURE 30
#define KINECT_NUMBER_OF_CHANNELS 3

#endif 
