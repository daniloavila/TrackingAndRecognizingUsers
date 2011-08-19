#include <stdio.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <list>
#include <string.h>
#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>
#include <XnCppWrapper.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "KeyboardUtil.h"
#include "ImageUtil.h"
#include "KinectUtil.h"
#include "MessageQueue.h"

using namespace std;

int idQueueRequest;
int idQueueResponse;

// Haar Cascade file, used for Face Detection.
const char *faceCascadeFilename = "Eigenfaces/haarcascade_frontalface_alt.xml";

// Global variables
IplImage ** faceImgArr = 0; // array of face images
CvMat * personNumTruthMat = 0; // array of person numbers

vector<string> personNames; // array of person names (indexed by the person number). Added by Shervin.
int faceWidth = 120; // Default dimensions for faces in the face recognition database. Added by Shervin.
int faceHeight = 90; //	"		"		"		"		"		"		"		"
int nPersons = 0; // the number of people in the training set. Added by Shervin.
int nTrainFaces = 0; // the number of training images
int nEigens = 0; // the number of eigenvalues
IplImage * pAvgTrainImg = 0; // the average image
IplImage ** eigenVectArr = 0; // eigenvectors
CvMat * eigenValMat = 0; // eigenvalues
CvMat * projectedTrainFaceMat = 0; // projected training faces

// Function prototypes
void cleanup(int i);
int loadTrainingData(CvMat ** pTrainPersonNumMat);
int findNearestNeighbor(float * projectedTestFace, float *pConfidence);
char* recognizeFromCamImg(IplImage *camImg, CvHaarClassifierCascade* faceCascade, CvMat * trainPersonNumMat, float * projectedTestFace, float * pointerConfidence);

/**
 * Verifica se o nome é algum dos conhecidos.
 */
bool validarNome(char *& nome) {
	vector<string>::iterator it;
	for (it = personNames.begin(); it < personNames.end(); it++) {
		if ((*it).compare(nome) == 0) {
			printf("Log - Recognizer diz: A label '%s' é valida.\n", nome);
			return true;
		}
	}
	printf("Log - Recognizer diz: A label '%s' não é valida.\n", nome);
	return false;
}

// Startup routine.
int main(int argc, char** argv) {
	int i;
	CvMat * trainPersonNumMat; // the person numbers during training
	float * projectedTestFace;
	CvHaarClassifierCascade* faceCascade;

	MessageRequest messageRequest;
	MessageResponse messageResponse;
	int *sharedMemoryId;
	char* nome;

	char *pshm;

	signal(SIGUSR1, cleanup);

	// Load the previously saved training data
	if (loadTrainingData(&trainPersonNumMat)) {
		faceWidth = pAvgTrainImg->width;
		faceHeight = pAvgTrainImg->height;
	}

	// Project the test images onto the PCA subspace
	projectedTestFace = (float *) cvAlloc(nEigens * sizeof(float));

	// Make sure there is a "data" folder, for storing the new person.
	mkdir("Eigenfaces/data", 0777);

	// Load the HaarCascade classifier for face detection.
	faceCascade = (CvHaarClassifierCascade*) cvLoad(faceCascadeFilename, 0, 0, 0);
	if (!faceCascade) {
		printf("ERROR in recognizeFromCam(): Could not load Haar cascade Face detection classifier in '%s'.\n", faceCascadeFilename);
		exit(1);
	}

	idQueueRequest = getMessageQueue(MESSAGE_QUEUE_REQUEST);
	idQueueResponse = getMessageQueue(MESSAGE_QUEUE_RESPONSE);

	sharedMemoryId = (int *) malloc(sizeof(int));

	while (1) {
		msgrcv(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0, 0);
		printf("Log - Recognizer diz: Recebi pedido de reconhecimento. user_id = %d e id_memoria = %d\n", messageRequest.user_id, messageRequest.memory_id);

		pshm = getSharedMemory(messageRequest.memory_id, false, sharedMemoryId);

		IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
		frame->imageData = pshm;

		IplImage* shownImg = cvCloneImage(frame);

//		cvNamedWindow("Input", CV_WINDOW_AUTOSIZE);
//		cvMoveWindow("Input", 10, 10);
//		cvShowImage("Input", shownImg);
//		cvWaitKey(10);

		float confidence = 0.0;
		nome = recognizeFromCamImg(shownImg, faceCascade, trainPersonNumMat, projectedTestFace, &confidence);
		cvReleaseImage(&shownImg);


		// Verificando se nome é valido
		if(nome != NULL && !validarNome(nome)) {
			nome[0] = NULL;
		}

		// deleta memoria compartilhada
		shmdt(pshm);
		shmctl(*sharedMemoryId, IPC_RMID, NULL);

		MessageResponse messageResponse;
		messageResponse.user_id = messageRequest.user_id;
		messageResponse.confidence = confidence;
		if (nome != NULL) {
			strcpy(messageResponse.user_name, nome);
		} else {
			messageResponse.user_name[0] = NULL;
		}

		printf("Log - Recognizer diz: Enviando mensagem de usuario reconhecido. user_id = %d e nome = '%s'\n", messageRequest.user_id, nome);

		if (msgsnd(idQueueResponse, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0) > 0) {
			printf("Erro no envio de mensagem para o usuario\n");
		}

	}

	cvReleaseHaarClassifierCascade(&faceCascade);

	return 0;
}

// Open the training data from the file 'facedata.xml'.
int loadTrainingData(CvMat ** pTrainPersonNumMat) {
	CvFileStorage * fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = cvOpenFileStorage("Eigenfaces/facedata.xml", 0, CV_STORAGE_READ);
	if (!fileStorage) {
		printf("Can't open training database file 'facedata.xml'.\n");
		return 0;
	}

	// Load the person names. Added by Shervin.
	personNames.clear(); // Make sure it starts as empty.
	nPersons = cvReadIntByName(fileStorage, 0, "nPersons", 0);
	if (nPersons == 0) {
		printf("No people found in the training database 'facedata.xml'.\n");
		return 0;
	}
	// Load each person's name.
	for (i = 0; i < nPersons; i++) {
		string sPersonName;
		char varname[200];
		sprintf(varname, "personName_%d", (i + 1));
		sPersonName = cvReadStringByName(fileStorage, 0, varname);
		personNames.push_back(sPersonName);
	}

	// Load the data
	nEigens = cvReadIntByName(fileStorage, 0, "nEigens", 0);
	nTrainFaces = cvReadIntByName(fileStorage, 0, "nTrainFaces", 0);
	*pTrainPersonNumMat = (CvMat *) cvReadByName(fileStorage, 0, "trainPersonNumMat", 0);
	eigenValMat = (CvMat *) cvReadByName(fileStorage, 0, "eigenValMat", 0);
	projectedTrainFaceMat = (CvMat *) cvReadByName(fileStorage, 0, "projectedTrainFaceMat", 0);
	pAvgTrainImg = (IplImage *) cvReadByName(fileStorage, 0, "avgTrainImg", 0);
	eigenVectArr = (IplImage **) cvAlloc(nTrainFaces * sizeof(IplImage *));
	for (i = 0; i < nEigens; i++) {
		char varname[200];
		sprintf(varname, "eigenVect_%d", i);
		eigenVectArr[i] = (IplImage *) cvReadByName(fileStorage, 0, varname, 0);
	}

	// release the file-storage interface
	cvReleaseFileStorage(&fileStorage);

	printf("Training data loaded (%d training images of %d people):\n", nTrainFaces, nPersons);
	printf("People: ");
	if (nPersons > 0)
		printf("<%s>", personNames[0].c_str());
	for (i = 1; i < nPersons; i++) {
		printf(", <%s>", personNames[i].c_str());
	}
	printf(".\n");

	return 1;
}

// Find the most likely person based on a detection. Returns the index, and stores the confidence value into pConfidence.
int findNearestNeighbor(float * projectedTestFace, float *pConfidence) {
	//double leastDistSq = 1e12;
	double leastDistSq = DBL_MAX;
	int i, iTrain, iNearest = 0;

	for (iTrain = 0; iTrain < nTrainFaces; iTrain++) {
		double distSq = 0;

		for (i = 0; i < nEigens; i++) {
			float d_i = projectedTestFace[i] - projectedTrainFaceMat->data.fl[iTrain * nEigens + i];
#ifdef USE_MAHALANOBIS_DISTANCE
			distSq += d_i*d_i / eigenValMat->data.fl[i]; // Mahalanobis distance (might give better results than Eucalidean distance)
#else
			distSq += d_i * d_i; // Euclidean distance.
#endif
		}

		if (distSq < leastDistSq) {
			leastDistSq = distSq;
			iNearest = iTrain;
		}
	}

	// Return the confidence level based on the Euclidean distance,
	// so that similar images should give a confidence between 0.5 to 1.0,
	// and very different images should give a confidence between 0.0 to 0.5.
	*pConfidence = 1.0f - sqrt(leastDistSq / (float) (nTrainFaces * nEigens)) / 255.0f;

	// Return the found index.
	return iNearest;
}

// Continuously recognize the person in the camera.
char* recognizeFromCamImg(IplImage *camImg, CvHaarClassifierCascade* faceCascade, CvMat * trainPersonNumMat, float * projectedTestFace, float * pointerConfidence) {
	int i;
	char cstr[256];
	int saveNextFaces = FALSE;
	char newPersonName[256];
	int newPersonFaces;

	int iNearest, nearest, truth;
	IplImage *greyImg;
	IplImage *faceImg;
	IplImage *sizedImg;
	IplImage *equalizedImg;
	IplImage *processedFaceImg;
	CvRect faceRect;
	IplImage *shownImg;
	int keyPressed = 0;
	FILE *trainFile;
	float confidence;

	if (!camImg) {
		printf("ERROR in recognizeFromCam(): Bad input image!\n");
		exit(1);
	}
	// Make sure the image is greyscale, since the Eigenfaces is only done on greyscale image.
	greyImg = convertImageToGreyscale(camImg);

	// Perform face detection on the input image, using the given Haar cascade classifier.
	faceRect = detectFaceInImage(greyImg, faceCascade);

	// Make sure a valid face was detected.
	if (faceRect.width > 0) {

		faceImg = cropImage(greyImg, faceRect); // Get the detected face image.
		// Make sure the image is the same dimensions as the training images.
		sizedImg = resizeImage(faceImg, faceWidth, faceHeight);
		// Give the image a standard brightness and contrast, in case it was too dark or low contrast.
		equalizedImg = cvCreateImage(cvGetSize(sizedImg), 8, 1); // Create an empty greyscale image
		cvEqualizeHist(sizedImg, equalizedImg);
		processedFaceImg = equalizedImg;
		if (!processedFaceImg) {
			printf("ERROR in recognizeFromCam(): Don't have input image!\n");
			exit(1);
		}

		// If the face rec database has been loaded, then try to recognize the person currently detected.
		if (nEigens > 0) {
			// project the test image onto the PCA subspace
			cvEigenDecomposite(processedFaceImg, nEigens, eigenVectArr, 0, 0, pAvgTrainImg, projectedTestFace);
			// Check which person it is most likely to be.
			iNearest = findNearestNeighbor(projectedTestFace, &confidence);
			nearest = trainPersonNumMat->data.i[iNearest];

			*pointerConfidence = confidence;

			//printf("Most likely person in camera: '%s' (confidence=%f).\n", personNames[nearest - 1].c_str(), confidence);

			return (char*) personNames[nearest - 1].c_str();
		}

		// Free the resources used for this frame.
		cvReleaseImage(&greyImg);
		cvReleaseImage(&faceImg);
		cvReleaseImage(&sizedImg);
		cvReleaseImage(&equalizedImg);
	}

	return NULL;
}

void cleanup(int i){
	MessageRequest messageRequest;
	int sharedMemoryId;

	while(msgrcv(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0, IPC_NOWAIT) >= 0){
		sharedMemoryId = shmget(messageRequest.memory_id, sizeof(char) * KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS, IPC_EXCL | 0x1ff);
		shmctl(sharedMemoryId, IPC_RMID, NULL);	
	};

	msgctl(idQueueRequest, IPC_RMID, NULL);

	exit(1);
			
}
