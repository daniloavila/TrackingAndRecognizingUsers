//////////////////////////////////////////////////////////////////////////////////////
// OnlineFaceRec.cpp, by Shervin Emami (www.shervinemami.co.cc) on 2nd June 2010.
// Online Face Recognition from a camera using Eigenfaces.
//////////////////////////////////////////////////////////////////////////////////////
//
// Some parts are based on the code example by Robin Hewitt (2007) at:
// "http://www.cognotics.com/opencv/servo_2007_series/part_5/index.html"
//
// Command-line Usage (for offline mode, without a webcam):
//
// First, you need some face images. I used the ORL face database.
// You can download it for free at
//    www.cl.cam.ac.uk/research/dtg/attarchive/facedatabase.html
//
// List the training and test face images you want to use in the
// input files train.txt and test.txt. (Example input files are provided
// in the download.) To use these input files exactly as provided, unzip
// the ORL face database, and place train.txt, test.txt, and eigenface.exe
// at the root of the unzipped database.
//
// To run the learning phase of eigenface, enter in the command prompt:
//    OnlineFaceRec train <train_file>
// To run the recognition phase, enter:
//    OnlineFaceRec test <test_file>
// To run online recognition from a camera, enter:
//    OnlineFaceRec
//
//////////////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <curses.h>
//#include <conio.h>		// For _kbhit()
//#include <direct.h>		// For mkdir()
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <string.h>
#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>
#include <XnCppWrapper.h>
#include <sys/shm.h>

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
const int NUMBER_OF_PHOTOS = 19;
extern const int VK_ESCAPE;

int SAVE_EIGENFACE_IMAGES = 1; // Set to 0 if you dont want images of the Eigenvectors saved to files (for debugging).
//#define USE_MAHALANOBIS_DISTANCE	// You might get better recognition accuracy if you enable this.

// Global variables
IplImage ** faceImgArr = 0; // array of face images
CvMat * personNumTruthMat = 0; // array of person numbers
//#define	MAX_NAME_LENGTH 256		// Give each name a fixed size for easier code.
//char **personNames = 0;			// array of person names (indexed by the person number). Added by Shervin.
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

CvCapture* camera = 0; // The camera device.

// Function prototypes
void doPCA();
int loadTrainingData(CvMat ** pTrainPersonNumMat);
int findNearestNeighbor(float * projectedTestFace);
int findNearestNeighbor(float * projectedTestFace, float *pConfidence);
int loadFaceImgArray(char * filename);
char* recognizeFromCam(IplImage *camImg, CvHaarClassifierCascade* faceCascade, CvMat * trainPersonNumMat, float * projectedTestFace);

// Startup routine.
int main(int argc, char** argv) {
	int i;
	CvMat * trainPersonNumMat; // the person numbers during training
	float * projectedTestFace;
	CvHaarClassifierCascade* faceCascade;

	int messageIn;
	messageResponse messageOut;
	char* nome;

	char *pshm;
	int sharedMemoryId;

	// Load the previously saved training data
	if (loadTrainingData(&trainPersonNumMat)) {
		faceWidth = pAvgTrainImg->width;
		faceHeight = pAvgTrainImg->height;
	}

	printf("FaceRecognition - 1\n");

	// Project the test images onto the PCA subspace
	projectedTestFace = (float *) cvAlloc(nEigens * sizeof(float));

	// Make sure there is a "data" folder, for storing the new person.
	mkdir("Eigenfaces/data", 0777);

	printf("FaceRecognition - 2\n");

	// Load the HaarCascade classifier for face detection.
	faceCascade = (CvHaarClassifierCascade*) cvLoad(faceCascadeFilename, 0, 0, 0);
	if (!faceCascade) {
		printf("ERROR in recognizeFromCam(): Could not load Haar cascade Face detection classifier in '%s'.\n", faceCascadeFilename);
		exit(1);
	}

	printf("FaceRecognition - 3\n");

	idQueueRequest = getMessageQueue(MESSAGE_QUEUE_REQUEST);
	idQueueResponse = getMessageQueue(MESSAGE_QUEUE_RESPONSE);

	while (1) {
		printf("FaceRecognition - 4\n");
		msgrcv(idQueueRequest, &messageIn, sizeof(int), 0, 0);

		printf("FaceRecognition - 5\n");

		if ((sharedMemoryId = shmget(messageIn, sizeof(char) * KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS, IPC_EXCL | 0x1ff)) < 0) {
			printf("erro na criacao da fila\n");
			exit(1);
		}

		printf("FaceRecognition - 6\n");

		pshm = (char *) shmat(sharedMemoryId, (char *) 0, 0);
		if (pshm == (char *) -1) {
			printf("erro no attach\n");
			exit(1);
		}

		IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
		frame->imageData = pshm;
//
		IplImage* shownImg = cvCloneImage(frame);

		printf("FaceRecognition - 7\n");

		nome = recognizeFromCam(shownImg, faceCascade, trainPersonNumMat, projectedTestFace);
		cvReleaseImage(&shownImg);

		printf("FaceRecognition - 9\n");

		shmctl(sharedMemoryId, IPC_RMID, NULL);

		printf("FaceRecognition - 8\n");

		printf("Nome:%s\n", nome);
		if (msgsnd(idQueueResponse, nome, sizeof(char) * 15, 0) > 0) {
			printf("Erro no envio de mensagem para o usuario\n");
		}

	}

	cvReleaseHaarClassifierCascade(&faceCascade);

}

// Save all the eigenvectors as images, so that they can be checked.
void storeEigenfaceImages() {
	// Store the average image to a file
	printf("Saving the image of the average face as 'out_averageImage.bmp'.\n");
	cvSaveImage("Eigenfaces/out_averageImage.bmp", pAvgTrainImg);
	// Create a large image made of many eigenface images.
	// Must also convert each eigenface image to a normal 8-bit UCHAR image instead of a 32-bit float image.
	printf("Saving the %d eigenvector images as 'out_eigenfaces.bmp'\n", nEigens);
	if (nEigens > 0) {
		// Put all the eigenfaces next to each other.
		int COLUMNS = 8; // Put upto 8 images on a row.
		int nCols = min(nEigens, COLUMNS);
		int nRows = 1 + (nEigens / COLUMNS); // Put the rest on new rows.
		int w = eigenVectArr[0]->width;
		int h = eigenVectArr[0]->height;
		CvSize size;
		size = cvSize(nCols * w, nRows * h);
		IplImage *bigImg = cvCreateImage(size, IPL_DEPTH_8U, 1); // 8-bit Greyscale UCHAR image
		for (int i = 0; i < nEigens; i++) {
			// Get the eigenface image.
			IplImage *byteImg = convertFloatImageToUcharImage(eigenVectArr[i]);
			// Paste it into the correct position.
			int x = w * (i % COLUMNS);
			int y = h * (i / COLUMNS);
			CvRect ROI = cvRect(x, y, w, h);
			cvSetImageROI(bigImg, ROI);
			cvCopyImage(byteImg, bigImg);
			cvResetImageROI(bigImg);
			cvReleaseImage(&byteImg);
		}
		cvSaveImage("Eigenfaces/out_eigenfaces.bmp", bigImg);
		cvReleaseImage(&bigImg);
	}
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

// Save the training data to the file 'facedata.xml'.
void storeTrainingData() {
	CvFileStorage * fileStorage;
	int i;

	// create a file-storage interface
	fileStorage = cvOpenFileStorage("Eigenfaces/facedata.xml", 0, CV_STORAGE_WRITE);

	// Store the person names. Added by Shervin.
	cvWriteInt(fileStorage, "nPersons", nPersons);
	for (i = 0; i < nPersons; i++) {
		char varname[200];
		sprintf(varname, "personName_%d", (i + 1));
		cvWriteString(fileStorage, varname, personNames[i].c_str(), 0);
	}

	// store all the data
	cvWriteInt(fileStorage, "nEigens", nEigens);
	cvWriteInt(fileStorage, "nTrainFaces", nTrainFaces);
	cvWrite(fileStorage, "trainPersonNumMat", personNumTruthMat, cvAttrList(0, 0));
	cvWrite(fileStorage, "eigenValMat", eigenValMat, cvAttrList(0, 0));
	cvWrite(fileStorage, "projectedTrainFaceMat", projectedTrainFaceMat, cvAttrList(0, 0));
	cvWrite(fileStorage, "avgTrainImg", pAvgTrainImg, cvAttrList(0, 0));
	for (i = 0; i < nEigens; i++) {
		char varname[200];
		sprintf(varname, "eigenVect_%d", i);
		cvWrite(fileStorage, varname, eigenVectArr[i], cvAttrList(0, 0));
	}

	// release the file-storage interface
	cvReleaseFileStorage(&fileStorage);
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

// Do the Principal Component Analysis, finding the average image
// and the eigenfaces that represent any image in the given dataset.
void doPCA() {
	int i;
	CvTermCriteria calcLimit;
	CvSize faceImgSize;

	// set the number of eigenvalues to use
	nEigens = nTrainFaces - 1;

	// allocate the eigenvector images
	faceImgSize.width = faceImgArr[0]->width;
	faceImgSize.height = faceImgArr[0]->height;
	eigenVectArr = (IplImage**) cvAlloc(sizeof(IplImage*) * nEigens);
	for (i = 0; i < nEigens; i++)
		eigenVectArr[i] = cvCreateImage(faceImgSize, IPL_DEPTH_32F, 1);

	// allocate the eigenvalue array
	eigenValMat = cvCreateMat(1, nEigens, CV_32FC1);

	// allocate the averaged image
	pAvgTrainImg = cvCreateImage(faceImgSize, IPL_DEPTH_32F, 1);

	// set the PCA termination criterion
	calcLimit = cvTermCriteria(CV_TERMCRIT_ITER, nEigens, 1);

	// compute average image, eigenvalues, and eigenvectors
	cvCalcEigenObjects(nTrainFaces, (void*) faceImgArr, (void*) eigenVectArr, CV_EIGOBJ_NO_CALLBACK, 0, 0, &calcLimit, pAvgTrainImg, eigenValMat->data.fl);

	cvNormalize(eigenValMat, eigenValMat, 1, 0, CV_L1, 0);
}

// Read the names & image filenames of people from a text file, and load all those images listed.
int loadFaceImgArray(char * filename) {
	FILE * imgListFile = 0;
	char imgFilename[512];
	int iFace, nFaces = 0;
	int i;

	// open the input file
	if (!(imgListFile = fopen(filename, "r"))) {
		fprintf(stderr, "Can\'t open file %s\n", filename);
		return 0;
	}

	// count the number of faces
	while (fgets(imgFilename, 512, imgListFile))
		++nFaces;
	rewind(imgListFile);

	// allocate the face-image array and person number matrix
	faceImgArr = (IplImage **) cvAlloc(nFaces * sizeof(IplImage *));
	personNumTruthMat = cvCreateMat(1, nFaces, CV_32SC1);

	personNames.clear(); // Make sure it starts as empty.
	nPersons = 0;

	// store the face images in an array
	for (iFace = 0; iFace < nFaces; iFace++) {
		char personName[256];
		string sPersonName;
		int personNumber;

		// read person number (beginning with 1), their name and the image filename.
		fscanf(imgListFile, "%d %s %s", &personNumber, personName, imgFilename);
		sPersonName = personName;
		//printf("Got %d: %d, <%s>, <%s>.\n", iFace, personNumber, personName, imgFilename);

		// Check if a new person is being loaded.
		if (personNumber > nPersons) {
			// Allocate memory for the extra person (or possibly multiple), using this new person's name.
			for (i = nPersons; i < personNumber; i++) {
				personNames.push_back(sPersonName);
			}
			nPersons = personNumber;
			//printf("Got new person <%s> -> nPersons = %d [%d]\n", sPersonName.c_str(), nPersons, personNames.size());
		}

		// Keep the data
		personNumTruthMat->data.i[iFace] = personNumber;

		// load the face image
		faceImgArr[iFace] = cvLoadImage(imgFilename, CV_LOAD_IMAGE_GRAYSCALE);

		if (!faceImgArr[iFace]) {
			fprintf(stderr, "Can\'t load image from %s\n", imgFilename);
			return 0;
		}
	}

	fclose(imgListFile);

	printf("Data loaded from '%s': (%d images of %d people).\n", filename, nFaces, nPersons);
	printf("People: ");
	if (nPersons > 0)
		printf("<%s>", personNames[0].c_str());
	for (i = 1; i < nPersons; i++) {
		printf(", <%s>", personNames[i].c_str());
	}
	printf(".\n");

	return nFaces;
}

// Continuously recognize the person in the camera.
char* recognizeFromCam(IplImage *camImg, CvHaarClassifierCascade* faceCascade, CvMat * trainPersonNumMat, float * projectedTestFace) {
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

			printf("Most likely person in camera: '%s' (confidence=%f.\n", personNames[nearest - 1].c_str(), confidence);

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

