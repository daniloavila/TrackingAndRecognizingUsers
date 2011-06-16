#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>
#include "XnCppWrapper.h"

#define KINECT_WIDTH_CAPTURE 480
#define KINECT_HEIGHT_CAPTURE 640
#define KINECT_FPS_CAPTURE 30
#define KINECT_NUMBER_OF_CHANNELS 3

int kinectMounted = 0;
xn::Context context;
xn::ImageGenerator image;

/**
 * Transforma um array de XnRGB24Pixel para um array de char representando a mesma infromação.
 */
char* transformToCharAray(const XnRGB24Pixel* source) {
        if(source == NULL) {
                return NULL;
        }
        
        char* result = (char *) malloc(KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS * sizeof(char));
        for(int i = 0; i < KINECT_HEIGHT_CAPTURE; i++) {
                for(int j = 0; j < KINECT_WIDTH_CAPTURE; j++) {
                        XnRGB24Pixel *aux = (XnRGB24Pixel *) &source[(i*KINECT_WIDTH_CAPTURE)+j];
                        result[(i*KINECT_WIDTH_CAPTURE*KINECT_NUMBER_OF_CHANNELS)+(j*KINECT_NUMBER_OF_CHANNELS)+2] = aux->nRed;
                        result[(i*KINECT_WIDTH_CAPTURE*KINECT_NUMBER_OF_CHANNELS)+(j*KINECT_NUMBER_OF_CHANNELS)+1] = aux->nGreen;
                        result[(i*KINECT_WIDTH_CAPTURE*KINECT_NUMBER_OF_CHANNELS)+(j*KINECT_NUMBER_OF_CHANNELS)+0] = aux->nBlue;
                }
        }

        return result;
}

/**
 * Inicia o Kinect
 */
void mountKinect() {
        printf("\nInitializing Kinect...\n");
        XnStatus nRetVal = XN_STATUS_OK; 

        // Initialize context object 
        nRetVal = context.Init(); 
        // Create a ImageGenerator node 
        nRetVal = image.Create(context);
        // Make it start generating data 
        nRetVal = context.StartGeneratingAll(); 

        XnMapOutputMode mapMode;
        mapMode.nXRes = KINECT_HEIGHT_CAPTURE;
        mapMode.nYRes = KINECT_WIDTH_CAPTURE;
        mapMode.nFPS = KINECT_FPS_CAPTURE;

        nRetVal = image.SetMapOutputMode( mapMode );
        printf("Kinect Ready\n");
}

/**
 * Retorna um frame do Kinect e inicia o mesmo caso ainda não o tenha sido feito.
 */
IplImage* getKinectFrame() {
        XnStatus nRetVal = XN_STATUS_OK; 

        if(!kinectMounted) {
                mountKinect();
                kinectMounted = 1;
        }

        // Wait for new data to be available 
        nRetVal = context.WaitOneUpdateAll(image); 
        if (nRetVal != XN_STATUS_OK) { 
                printf("Failed updating data: %s\n", 
                xnGetStatusString(nRetVal)); 
                return NULL; 
        } 
        IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
        char* result = transformToCharAray(image.GetRGB24ImageMap());
        frame->imageData = result; 
        return frame;
}