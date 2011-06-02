#ifndef KINECT_UTIL_H_
#define KINECT_UTIL_H_

#include "XnCppWrapper.h"

/**
 * Retorna um frame do Kinect e inicia o mesmo caso ainda não o tenha sido feito.
 */
IplImage* getKinectFrame();

#endif 
