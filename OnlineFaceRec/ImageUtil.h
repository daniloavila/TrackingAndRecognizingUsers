#ifndef IMAGE_UTIL_H_
#define IMAGE_UTIL_H_

#include <OpenCV/cv.h>
#include <OpenCV/cvaux.h>
#include <OpenCV/highgui.h>

IplImage* convertImageToGreyscale(const IplImage *imageSrc);
IplImage* cropImage(const IplImage *img, const CvRect region);
IplImage* resizeImage(const IplImage *origImg, int newWidth, int newHeight);
IplImage* convertFloatImageToUcharImage(const IplImage *srcImg);
void saveFloatImage(const char *filename, const IplImage *srcImg);
CvRect detectFaceInImage(const IplImage *inputImg, const CvHaarClassifierCascade* cascade );


#endif