#include "TrackerRunnable.h"

#include <jni.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <signal.h>
#include <time.h>

#include "ImageUtil.h"
#include "MessageQueue.h"
#include "Definitions.h"

#include <stdio.h>

using namespace std;

//#define CLASS_NAME "TrackerRunnable"
//#define METHOD_NAME "registerUser"
//#define METHOD_SIGNATURE "(ILjava/lang/String;F)V"

#define CLASS_NAME "br/unb/unbiquitous/ubiquitos/uos/driver/UserDriver"

#define METHOD_NAME_NEW_USER "registerNewUser"
#define METHOD_SIGNATURE_NEW_USER "(Ljava/lang/String;FFFF)V"

#define METHOD_NAME_LOST_USER "registerLostUser"
#define METHOD_SIGNATURE_LOST_USER "(Ljava/lang/String;)V"

#define METHOD_NAME_RECHECK_USER "registerRecheckUser"
#define METHOD_SIGNATURE_RECHECK_USER "(Ljava/lang/String;Ljava/lang/String;FFFF)V"

#ifdef __cplusplus
extern "C" {
#endif

//****************************************************************************************
//				ASSINATURA DAS FUNCOES
//****************************************************************************************
void lostTrackerRunnalbeSignals();
void getTrackerRunnalbeSignals();
int getNumberOfRegisteredPersons();
void learn(char *szFileTrain);
void cleanupQueue(int signal);


int idQueueComunicationJavaC = 0;
int trackerId = 0;
int registerId = 0;

//****************************************************************************************
//				VARIAVEIS UTILIZADAS NO TREINO
//****************************************************************************************
int nTrainFaces = 0;
CvMat * projectedTrainFaceMat = 0; // projected training faces
int nEigens = 0; // numero de eigenvalues
IplImage ** faceImgArr = 0; // array de imagens facial
IplImage ** eigenVectArr = 0; // eigenvectors
IplImage * pAvgTrainImg = 0; // a imagem media
int SAVE_EIGENFACE_IMAGES = 1;
CvMat * personNumTruthMat = 0; // array dos numeros das pessoas
vector<string> personNames; // array dos nomes das pessoas (idexado pelo numero da pessoa).
int nPersons = 0; // numero de pessoas no cenario de treino
CvMat * eigenValMat = 0; // eigenvalues


//****************************************************************************************
//				MÉTODOS DA CLASSE JAVA IMPLEMENTADAS AQUI
//****************************************************************************************

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_TrackerRunnale
 * Method:    doTracker
 * Signature: ()V
 */
void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_00024TrackerRunnable_doTracker(JNIEnv * env, jobject obj) {
	printf("[INFO] Initializing Tracker Comunication...\n");

	idQueueComunicationJavaC = createMessageQueue(MESSAGE_QUEUE_COMUNICATION_JAVA_C);

	jclass trackerRunnableClass = (env)->FindClass(CLASS_NAME);
	if (trackerRunnableClass == NULL) {
		fprintf(stderr, "Não encontrou a classe %s.\n", CLASS_NAME); /* error handling */
	}

	jmethodID midNewUser = (env)->GetStaticMethodID(trackerRunnableClass, METHOD_NAME_NEW_USER, METHOD_SIGNATURE_NEW_USER);
	if (midNewUser == NULL) {
		fprintf(stderr, "Não encontrou o metodo %s %s.\n", METHOD_NAME_NEW_USER, METHOD_SIGNATURE_NEW_USER); /* error handling */
	}

	jmethodID midLostUser = (env)->GetStaticMethodID(trackerRunnableClass, METHOD_NAME_LOST_USER, METHOD_SIGNATURE_LOST_USER);
	if (midLostUser == NULL) {
		fprintf(stderr, "Não encontrou o metodo %s %s.\n", METHOD_NAME_LOST_USER, METHOD_SIGNATURE_LOST_USER); /* error handling */
	}

	jmethodID midRecheckUser = (env)->GetStaticMethodID(trackerRunnableClass, METHOD_NAME_RECHECK_USER, METHOD_SIGNATURE_RECHECK_USER);
	if (midLostUser == NULL) {
		fprintf(stderr, "Não encontrou o metodo %s %s.\n", METHOD_NAME_RECHECK_USER, METHOD_SIGNATURE_RECHECK_USER); /* error handling */
	}

	while (1) {
		MessageEvents messageEvents;
		printLogConsole("+++++++++++> Esperando escolha.\n");
		msgrcv(idQueueComunicationJavaC, &messageEvents, sizeof(MessageEvents) - sizeof(long), 0, 0);
		printLogConsole("+++++++++++> %s\n\n", messageEvents.user_name);

		jstring name = (env)->NewStringUTF(messageEvents.user_name);

		jstring last_name = NULL;
		if(strlen(messageEvents.last_name) > 0)
			last_name = (env)->NewStringUTF(messageEvents.last_name);

		if (messageEvents.type == NEW_USER) {
			(env)->CallStaticVoidMethod(trackerRunnableClass, midNewUser, name, messageEvents.confidence, messageEvents.x, messageEvents.y, messageEvents.z);
		} else if (messageEvents.type == LOST_USER) {
			(env)->CallStaticVoidMethod(trackerRunnableClass, midLostUser, name);
		} else if (messageEvents.type == RECHECK) {
			(env)->CallStaticVoidMethod(trackerRunnableClass, midRecheckUser, name, last_name, messageEvents.confidence, messageEvents.x, messageEvents.y, messageEvents.z);
		}
	}

	printLogConsole("FIM Tracker");
}


/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver
 * Method:    startTracker
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_startTracker(JNIEnv *, jobject) {
	printf("[INFO] Starting TRUE System\n");

	//criando um processo filho. Este processo sera transformado do deamon utilizando o execl
	trackerId = fork();
	if (trackerId < 0) {
		fprintf(stderr, "Erro na criação do Tracker através do fork\n");
		exit(1);
	}

	//inicinado o processo que reconhece os novoc usuarios encontrados
	if (trackerId == 0) {
		execl("tracker", "tracker", (char *) 0);
	}
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver
 * Method:    stopTracker
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_stopTracker(JNIEnv * env, jobject obj) {
	printf("[INFO] Stopping TRUE System\n");

	cleanupQueue(SIGINT);
}


/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver
 * Method:    train
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_train(JNIEnv * env, jobject obj) {
	printf("[INFO] Training TRUE System\n");

	learn((char *) TRAIN_DATA);

	printf("[INFO] Finished training TRUE System \n");
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver
 * Method:    saveImage
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_saveImage(JNIEnv * env, jobject obj, jstring name, jint index, jbyteArray image) {
	IplImage *greyImg;
	IplImage *faceImg;
	IplImage *sizedImg;
	IplImage *equalizedImg;
	CvRect faceRect;
	CvHaarClassifierCascade* faceCascade;
	char cstr[256];
	int nPeople;

	// le o classificador utilizado para detecao
	faceCascade = (CvHaarClassifierCascade*) cvLoad(HAARCASCADE_FRONTALFACE, 0, 0, 0);
	if (!faceCascade) {
		printf("ERROR em recognizeFromCam(): Classificador '%s' nao pode ser lido.\n", HAARCASCADE_FRONTALFACE);
		return;
	}

	// Buscando o nome da pessoa
	const char *newPersonName = env->GetStringUTFChars(name, NULL);

	// Buscando o tamanho da imagem
    int arrayLength = env->GetArrayLength(image);

    // Buscando a imagem
	jbyte *imageArray = env->GetByteArrayElements(image, NULL);
	char *imageArrayConverted = (char *)(malloc(sizeof (char) * arrayLength));
    for(int i = 0;i < arrayLength;++i){
        imageArrayConverted[arrayLength - i] = (char)(imageArray[i]);
    }

    // Criando a IplImage
    IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
    frame->imageData = (char*)(imageArrayConverted);

	// transforma a imagem em escala de cinza
	greyImg = convertImageToGreyscale(frame);

	// realiza deteccao facial na imagem de entrada, usando o classificador
	faceRect = detectFaceInImage(greyImg, faceCascade);					

	// veririfica se uma faxe foi detectada
	if (faceRect.width > 0) {
		faceImg = cropImage(greyImg, faceRect); // pega a face detectada
		// redimensiona a imagem
		sizedImg = resizeImage(faceImg, FACE_WIDTH, FACE_HEIGHT);
		// da a imagem o brilho e contraste padrao
		equalizedImg = cvCreateImage(cvGetSize(sizedImg), 8, 1); // cria uma imagem vazia em escala de cinza
		cvEqualizeHist(sizedImg, equalizedImg);

		//salvando a imagem
		nPeople = getNumberOfRegisteredPersons();
		sprintf(cstr, NEW_IMAGES_SCHEME, nPeople + 1, newPersonName, index);

		cvSaveImage(cstr, equalizedImg, NULL);

		// libera os recursos
		cvReleaseImage(&greyImg);
		cvReleaseImage(&faceImg);
		cvReleaseImage(&sizedImg);
		cvReleaseImage(&equalizedImg);

		FILE *trainFile = fopen(TRAIN_DATA, "a");
		fprintf(trainFile, "%d %s %s\n", nPeople + 1, newPersonName, cstr);
		fclose(trainFile);
	}

}

/**
 * TODO : Refatorar esse metodo para que busque o ultimo ID do Train e não o numero de pessoas conhecidas do FACEDATA
 * pois da erro quando eu adiciono dois usuários seguidamente e não treino ao final da inserção
 */
int getNumberOfRegisteredPersons(){
	CvFileStorage * fileStorage;

	// cria uma interface arqivo - armazenamento
	fileStorage = cvOpenFileStorage(FACE_DATA, 0, CV_STORAGE_READ);
	if (!fileStorage) {
		printf("Arquivo 'facedata.xml' nao pode ser aberto.\n");
		return 0;
	}

	// le os nomes das pessoas
	int nPersons = cvReadIntByName(fileStorage, 0, "nPersons", 0);
	if (nPersons == 0) {
		printf("Nenhuma pessoa encontrada em 'facedata.xml'.\n");
		return 0;
	}

	return nPersons;
}

//****************************************************************************************
//				FUNCOES UTILIZADAS PARA INTERCEPTAR OS DIVERSOS TIPOS DE SINAIS
//****************************************************************************************

void cleanupQueue(int signal) {
	lostTrackerRunnalbeSignals();
	printLogConsole("Signal TrackerRunnable - %d\n", signal);

	kill(trackerId, signal);
	wait((int*)0);
}

void getTrackerRunnalbeSignals() {
	signal(SIGINT, cleanupQueue);
	signal(SIGQUIT, cleanupQueue);
	signal(SIGILL, cleanupQueue);
	signal(SIGTRAP, cleanupQueue);
	signal(SIGABRT, cleanupQueue);
	signal(SIGKILL, cleanupQueue);
	signal(SIGSEGV, cleanupQueue);
	signal(SIGTERM, cleanupQueue);
	signal(SIGSYS, cleanupQueue);
}

void lostTrackerRunnalbeSignals() {
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGILL, SIG_IGN);
	signal(SIGTRAP, SIG_IGN);
	signal(SIGABRT, SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	signal(SIGSEGV, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGSYS, SIG_IGN);
}

//****************************************************************************************
//							FUNCOES UTILIZADAS NO TREINO DO ALGORITMO
//****************************************************************************************


// le os nomes e as imagens das pessoas de um arquivo txt, e le todas as imagens listadas
int loadFaceImgArray(char * filename) {
	FILE * imgListFile = 0;
	char imgFilename[512];
	int iFace, nFaces = 0;
	int i;

	// abre o arquivo de input
	if (!(imgListFile = fopen(filename, "r"))) {
		fprintf(stderr, "Arquivo %s nao pode ser aberto\n", filename);
		return 0;
	}

	// conta o numero de faces
	while (fgets(imgFilename, 512, imgListFile))
		++nFaces;
	rewind(imgListFile);

	// alloca o array face-imagem 
	faceImgArr = (IplImage **) cvAlloc(nFaces * sizeof(IplImage *));
	personNumTruthMat = cvCreateMat(1, nFaces, CV_32SC1);

	personNames.clear(); 
	nPersons = 0;

	// armazena as imagens de faces em um array
	for (iFace = 0; iFace < nFaces; iFace++) {
		char personName[256];
		string sPersonName;
		int personNumber;

		// le o numero da pessoa, seu nome e o nome da imagem
		fscanf(imgListFile, "%d %s %s", &personNumber, personName, imgFilename);
		sPersonName = personName;

		// verifica se um nova pessoa esta sendo lida
		if (personNumber > nPersons) {
			// alloca memoria para a pessoa extra
			for (i = nPersons; i < personNumber; i++) {
				personNames.push_back(sPersonName);
			}
			nPersons = personNumber;
		}

		personNumTruthMat->data.i[iFace] = personNumber;

		// le a imagem da face
		faceImgArr[iFace] = cvLoadImage(imgFilename, CV_LOAD_IMAGE_GRAYSCALE);

		if (!faceImgArr[iFace]) {
			fprintf(stderr, "Imagem nao pode ser lida de %s\n", imgFilename);
			return 0;
		}
	}

	fclose(imgListFile);

	printf("Data lido de '%s': (%d imagens de %d pessoas).\n", filename, nFaces, nPersons);
	printf("Pessoas: ");
	if (nPersons > 0)
		printf("<%s>", personNames[0].c_str());
	for (i = 1; i < nPersons; i++) {
		printf(", <%s>", personNames[i].c_str());
	}
	printf(".\n");

	return nFaces;
}

// faz a analize das componentes principais, achando a imagem media
// e os eigenfaces que representa qualquer imagem no ambiente de dados
void doPCA() {
	int i;
	CvTermCriteria calcLimit;
	CvSize faceImgSize;

	// seta o numero de eigenvalues para usar
	nEigens = nTrainFaces - 1;

	// armazena as imagens eigenvector
	faceImgSize.width = faceImgArr[0]->width;
	faceImgSize.height = faceImgArr[0]->height;
	eigenVectArr = (IplImage**) cvAlloc(sizeof(IplImage*) * nEigens);
	for (i = 0; i < nEigens; i++)
		eigenVectArr[i] = cvCreateImage(faceImgSize, IPL_DEPTH_32F, 1);

	// armazena o array de eigenvalue
	eigenValMat = cvCreateMat(1, nEigens, CV_32FC1);

	// armazena a imagem media
	pAvgTrainImg = cvCreateImage(faceImgSize, IPL_DEPTH_32F, 1);

	// seta o criterio de termino do PCA
	calcLimit = cvTermCriteria(CV_TERMCRIT_ITER, nEigens, 1);

	// computa a imagem media, eigenvalues e os eigenvectors
	cvCalcEigenObjects(nTrainFaces, (void*) faceImgArr, (void*) eigenVectArr, CV_EIGOBJ_NO_CALLBACK, 0, 0, &calcLimit, pAvgTrainImg, eigenValMat->data.fl);

	cvNormalize(eigenValMat, eigenValMat, 1, 0, CV_L1, 0);
}

// salva todos os eigenvectors
void storeEigenfaceImages() {
	//armazena a imagem media em um arquivo
	printf("Salvando a imagem da face media como 'out_averageImage.bmp'.\n");
	cvSaveImage(AVERAGE_IMAGE, pAvgTrainImg);

	//cria uma nova imagem feita de varias imagens eigenfaces 
	printf("Salvando o eigenvector %d como 'out_eigenfaces.bmp'\n", nEigens);
	if (nEigens > 0) {
		//coloca todas imagens uma do lado da outra
		int COLUMNS = 8; // poe 8 imagens em uma linha
		int nCols = min(nEigens, COLUMNS);
		int nRows = 1 + (nEigens / COLUMNS); 
		int w = eigenVectArr[0]->width;
		int h = eigenVectArr[0]->height;
		CvSize size;
		size = cvSize(nCols * w, nRows * h);
		IplImage *bigImg = cvCreateImage(size, IPL_DEPTH_8U, 1); // 8-bit Greyscale UCHAR image
		for (int i = 0; i < nEigens; i++) {
			// pega a imagem eigenface
			IplImage *byteImg = convertFloatImageToUcharImage(eigenVectArr[i]);
			// coloca ela na posicao correta
			int x = w * (i % COLUMNS);
			int y = h * (i / COLUMNS);
			CvRect ROI = cvRect(x, y, w, h);
			cvSetImageROI(bigImg, ROI);
			cvCopyImage(byteImg, bigImg);
			cvResetImageROI(bigImg);
			cvReleaseImage(&byteImg);
		}
		cvSaveImage(EIGEN_FACES, bigImg);
		cvReleaseImage(&bigImg);
	}
}

// armazena os dados de treinamento no arquivo facedata.xml
void storeTrainingData() {
	CvFileStorage * fileStorage;
	int i;

	// cria a interface arquivo-armazenamento
	fileStorage = cvOpenFileStorage(FACE_DATA, 0, CV_STORAGE_WRITE);

	// armazena os nomes das pessoas
	cvWriteInt(fileStorage, "nPersons", nPersons);
	for (i = 0; i < nPersons; i++) {
		char varname[200];
		sprintf(varname, "personName_%d", (i + 1));
		cvWriteString(fileStorage, varname, personNames[i].c_str(), 0);
	}

	// armazena todos os dados
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

	cvReleaseFileStorage(&fileStorage);
}

//armazena os dados treinados em um arquivo facedata.xml
void learn(char *szFileTrain) {
	int i, offset;

	// le os dados de treinamento
	printf("Lendo imagens de treinamento em '%s'\n", szFileTrain);
	nTrainFaces = loadFaceImgArray(szFileTrain);
	printf("%d imagens de treino obtidas.\n", nTrainFaces);
	if (nTrainFaces < 2) {
		fprintf(stderr, "Necessario 2 ou mais faces de trieno\n"
				"Arquivo de entrada contem somente %d\n", nTrainFaces);
		return;
	}

	// faz o PCA nas faces de treinamento
	doPCA();

	// projeta as imagens de trienamento no subespaco PCA
	projectedTrainFaceMat = cvCreateMat(nTrainFaces, nEigens, CV_32FC1);
	offset = projectedTrainFaceMat->step / sizeof(float);
	for (i = 0; i < nTrainFaces; i++) {
		cvEigenDecomposite(faceImgArr[i], nEigens, eigenVectArr, 0, 0, pAvgTrainImg,
		projectedTrainFaceMat->data.fl + i * offset);
	}

	// armazena os dados de reconhecimento como um arquivo xml
	storeTrainingData();

	// salva todos os eigenvectors como imagens, para que eles possam ser utilizados
	if (SAVE_EIGENFACE_IMAGES) {
		storeEigenfaceImages();
	}

}

#ifdef __cplusplus
}
#endif
