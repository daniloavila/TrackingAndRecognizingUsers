#include "KinectUtil.h"

int kinectMounted = 0;
xn::Context context;
xn::ImageGenerator image;

char *getSharedMemory(int memory_id, bool create) {
	int sharedMemoryId;

	short flag = 0;
	if(create) {
		flag = IPC_CREAT;
	} else {
		flag = IPC_EXCL;
	}

	if ((sharedMemoryId = shmget(memory_id, sizeof(char) * KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS, IPC_CREAT | 0x1ff)) < 0) {
		printf("erro na criacao da memoria\n");
	}
	char *maskPixels = (char*) ((shmat(sharedMemoryId, (char*) ((0)), 0)));
	if (maskPixels == (char*) ((-1))) {
		printf("erro no attach\n");
	}
	return maskPixels;
}



/**
 * Transforma um array de XnRGB24Pixel para um array de char representando a mesma infromação.
 */
char* transformToCharAray(const XnRGB24Pixel* source) {
	if (source == NULL) {
		return NULL;
	}

	char* result = (char *) malloc(KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS * sizeof(char));
	for (int i = 0; i < KINECT_HEIGHT_CAPTURE; i++) {
		for (int j = 0; j < KINECT_WIDTH_CAPTURE; j++) {
			XnRGB24Pixel *aux = (XnRGB24Pixel *) &source[(i * KINECT_WIDTH_CAPTURE) + j];
			result[(i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS) + 2] = aux->nRed;
			result[(i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS) + 1] = aux->nGreen;
			result[(i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS) + 0] = aux->nBlue;
		}
	}

	return result;
}

void setPixel(unsigned short int *source, int posicao) {
	if (posicao > 0 && posicao < KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE) {
		// colocando 1 na coluna da esquerda
		if (!source[posicao]) {
			source[posicao] = 2;
		}
	}
}

void transformAreaVision(short unsigned int* source) {
	if (source != NULL) {
		for (int i = 0; i < KINECT_HEIGHT_CAPTURE; i++) {
			for (int j = 0; j < KINECT_WIDTH_CAPTURE; j++) {
				if (source[(i * KINECT_WIDTH_CAPTURE) + j] == 1) {
					int posicaoAcima = ((i - 1) * KINECT_WIDTH_CAPTURE) + j;
					int posicaoAbaixo = ((i + 1) * KINECT_WIDTH_CAPTURE) + j;
					int posicaoEsquerda = (i * KINECT_WIDTH_CAPTURE) + (j - 1);
					int posicaoDireita = (i * KINECT_WIDTH_CAPTURE) + (j + 1);

					if (i > 0) {
						// colocando 1 na linha de cima
						//setPixel(source, posicaoAcima);
					}

					if (i < KINECT_HEIGHT_CAPTURE) {
						// colocando 1 na linha de cima
						//setPixel(source, posicaoAbaixo);
					}

					if (j > 0) {
						// colocando 1 na coluna da esquerda
						setPixel(source, posicaoDireita);
						setPixel(source, posicaoDireita - 1);
						setPixel(source, posicaoDireita - 2);
						setPixel(source, posicaoDireita - 3);
						setPixel(source, posicaoDireita - 4);
					}

					if (j < KINECT_WIDTH_CAPTURE) {
						// colocando 1 na coluna da esquerda
						setPixel(source, posicaoDireita);
						setPixel(source, posicaoDireita + 1);
						setPixel(source, posicaoDireita + 2);
						setPixel(source, posicaoDireita + 3);
						setPixel(source, posicaoDireita + 4);
					}
				}
			}
		}
	}

}

/**
 * Inicia o Kinect
 */
char mountKinect() {
	XnStatus nRetVal = XN_STATUS_OK;

	// Initialize context object
	nRetVal = context.Init();

	if (nRetVal != XN_STATUS_OK) {
		return false;
	}

	printf("\nInitializing Kinect...\n");

	// Create a ImageGenerator node
	nRetVal = image.Create(context);

	if (nRetVal != XN_STATUS_OK) {
		return false;
	}

	// Make it start generating data
	nRetVal = context.StartGeneratingAll();

	if (nRetVal != XN_STATUS_OK) {
		return false;
	}

	XnMapOutputMode mapMode;
	mapMode.nXRes = KINECT_HEIGHT_CAPTURE;
	mapMode.nYRes = KINECT_WIDTH_CAPTURE;
	mapMode.nFPS = KINECT_FPS_CAPTURE;

	nRetVal = image.SetMapOutputMode(mapMode);

	if (nRetVal != XN_STATUS_OK) {
		return false;
	}
	printf("Kinect Ready\n");

	return true;
}

/**
 * Retorna um frame do Kinect e inicia o mesmo caso ainda não o tenha sido feito.
 */
IplImage* getKinectFrame() {
	XnStatus nRetVal = XN_STATUS_OK;

	if (!kinectMounted) {
		if(!mountKinect()) {
			return NULL;
		}
		kinectMounted = 1;
	}

	// Wait for new data to be available
	nRetVal = context.WaitOneUpdateAll(image);
	if (nRetVal != XN_STATUS_OK) {
		printf("Failed updating data: %s\n", xnGetStatusString(nRetVal));
		return NULL;
	}
	IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
	char* result = transformToCharAray(image.GetRGB24ImageMap());
	frame->imageData = result;
	return frame;
}
