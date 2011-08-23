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

// Haar Cascade file, usado para detecao facial
const char *frontalFaceCascadeFilename = HAARCASCADE_FRONTALFACE;

/******************* VARIAVEIS GLOBAIS ************************/
IplImage ** faceImgArr = 0; // array de imagens facial
CvMat * personNumTruthMat = 0; // array dos numeros das pessoas

vector<string> personNames; // array dos nomes das pessoas (idexado pelo numero da pessoa).
int faceWidth = 120; // dimensoes default das faces no banco de  faces
int faceHeight = 90; //	"		"		"		"		"		"		"		"
int nPersons = 0; // numero de pessoas no cenario de treino
int nTrainFaces = 0; // numero de imagens de trienamento
int nEigens = 0; // numero de eigenvalues
IplImage * pAvgTrainImg = 0; // a imagem media
IplImage ** eigenVectArr = 0; // eigenvectors
CvMat * eigenValMat = 0; // eigenvalues
CvMat * projectedTrainFaceMat = 0; // projected training faces

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

int main(int argc, char** argv) {
	int i;
	CvMat * trainPersonNumMat; // o numero das pessoas durante o treinamento
	float * projectedTestFace;
	CvHaarClassifierCascade *frontalFaceCascade;
	MessageRequest messageRequest;
	MessageResponse messageResponse;
	int *sharedMemoryId;
	char* nome;
	char *pshm;

	signal(SIGUSR1, cleanup);

	// lendo os dados do treinamento
	if (loadTrainingData(&trainPersonNumMat)) {
		faceWidth = pAvgTrainImg->width;
		faceHeight = pAvgTrainImg->height;
	}

	// projetando as imagens do subespaco PCA
	projectedTestFace = (float *) cvAlloc(nEigens * sizeof(float));

	mkdir("Eigenfaces/data", 0777);

	// lendo o classificador
	frontalFaceCascade = (CvHaarClassifierCascade*) cvLoad(frontalFaceCascadeFilename, 0, 0, 0);
	if (!frontalFaceCascade) {
		printf("ERROR em recognizeFromCam(): O classificador de face em cascata nao pode ser lido em '%s'.\n", frontalFaceCascadeFilename);
		exit(1);
	}

	idQueueRequest = getMessageQueue(MESSAGE_QUEUE_REQUEST);
	idQueueResponse = getMessageQueue(MESSAGE_QUEUE_RESPONSE);

	sharedMemoryId = (int *) malloc(sizeof(int));

	while (1) {
		msgrcv(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0, 0);
		printf("Log - Recognizer diz: Recebi pedido de reconhecimento. user_id = %ld e id_memoria = %x\n", messageRequest.user_id, messageRequest.memory_id);

		pshm = getSharedMemory(messageRequest.memory_id, false, sharedMemoryId);

		IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
		frame->imageData = pshm;

		IplImage* shownImg = cvCloneImage(frame);

//		cvNamedWindow("Input", CV_WINDOW_AUTOSIZE);
//		cvMoveWindow("Input", 10, 10);
//		cvShowImage("Input", shownImg);
//		cvWaitKey(10);

		float confidence = 0.0;
		nome = recognizeFromCamImg(shownImg, frontalFaceCascade, trainPersonNumMat, projectedTestFace, &confidence);
		cvReleaseImage(&shownImg);
		cvReleaseImage(&frame);


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

		printf("Log - Recognizer diz: Enviando mensagem de usuario reconhecido. user_id = %ld e nome = '%s'\n", messageRequest.user_id, nome);

		if (msgsnd(idQueueResponse, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0) > 0) {
			printf("Erro no envio de mensagem para o usuario\n");
		}


	}

	cvReleaseHaarClassifierCascade(&frontalFaceCascade);

	return 0;
}

// abrindo os dados do treino do arquivo 'facedata.xml'.
int loadTrainingData(CvMat ** pTrainPersonNumMat) {
	CvFileStorage * fileStorage;
	int i;

	fileStorage = cvOpenFileStorage("Eigenfaces/facedata.xml", 0, CV_STORAGE_READ);
	if (!fileStorage) {
		printf("O arquivo de dados para treino facedata.xml nao pode ser aberto.\n");
		return 0;
	}

	// lendo os nomes das pessoas
	personNames.clear(); // tendo certeza que ele comece vazio
	nPersons = cvReadIntByName(fileStorage, 0, "nPersons", 0);
	if (nPersons == 0) {
		printf("Nenhuma pessoa encontrada nos dados de treinamento do facedata.xml.\n");
		return 0;
	}
	// lendo o nome de cada pessoa
	for (i = 0; i < nPersons; i++) {
		string sPersonName;
		char varname[200];
		sprintf(varname, "personName_%d", (i + 1));
		sPersonName = cvReadStringByName(fileStorage, 0, varname);
		personNames.push_back(sPersonName);
	}

	// lendo os dados
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

	cvReleaseFileStorage(&fileStorage);

	printf("Training data loaded (%d training images of %d people):\n", nTrainFaces, nPersons);
	printf("Dados de treino lidos (%d imagens de treinamento de %d pessoas):\n", nTrainFaces, nPersons);
	printf("Pessoas: ");
	if (nPersons > 0)
		printf("<%s>", personNames[0].c_str());
	for (i = 1; i < nPersons; i++) {
		printf(", <%s>", personNames[i].c_str());
	}
	printf(".\n");

	return 1;
}

// acha a pessoa mais parecida com a detectada. Retorna o indice e armazena o indice de confianca
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

	//retorna o nivel de confianca baseado na distancia euclidiana para que imagens similares deem confianca entre 0.5 e 1.0
	// e imagens muito diferentes confianca entre 0.0 e 0.5
	*pConfidence = 1.0f - sqrt(leastDistSq / (float) (nTrainFaces * nEigens)) / 255.0f;

	// Return the found index.
	return iNearest;
}

// reocnhece a pessoa na camera
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
	int keyPressed = 0;
	FILE *trainFile;
	float confidence;

	if (!camImg) {
		printf("ERROR em recognizeFromCam(): Imagem de entrada ruim!\n");
		exit(1);
	}

	// convert a imagem para escala de cinza
	greyImg = convertImageToGreyscale(camImg);

	//realiza deteccao facial utilizando um classificador haar
	faceRect = detectFaceInImage(greyImg, faceCascade);

	//verifica se alguma face foi detectada
	if (faceRect.width > 0) {

		faceImg = cropImage(greyImg, faceRect); // obtem a imagem detectada
		
		//redimensiona a imagem
		sizedImg = resizeImage(faceImg, faceWidth, faceHeight);
		// fornece a imagem o padrao de brilho e contraste
		equalizedImg = cvCreateImage(cvGetSize(sizedImg), 8, 1); // uma uma imagem na escala de cinza limpa
		cvEqualizeHist(sizedImg, equalizedImg);
		processedFaceImg = equalizedImg;
		if (!processedFaceImg) {
			printf("ERROR em recognizeFromCam(): Nao ha imagem de entrada!\n");
			exit(1);
		}

		// tenta reconhecer as pessoas detectadas
		if (nEigens > 0) {
			// projeta a imagem de teste no subespaco PCA
			cvEigenDecomposite(processedFaceImg, nEigens, eigenVectArr, 0, 0, pAvgTrainImg, projectedTestFace);
			//verifica qual pessoa eh a mais parecida
			iNearest = findNearestNeighbor(projectedTestFace, &confidence);
			nearest = trainPersonNumMat->data.i[iNearest];

			*pointerConfidence = confidence;

			//printf("Most likely person in camera: '%s' (confidence=%f).\n", personNames[nearest - 1].c_str(), confidence);

			return (char*) personNames[nearest - 1].c_str();
		}

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
