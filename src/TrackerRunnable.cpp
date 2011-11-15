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
#include <stdlib.h>

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

using namespace std;

//#define CLASS_NAME "TrackerRunnable"
//#define METHOD_NAME "registerUser"
//#define METHOD_SIGNATURE "(ILjava/lang/String;F)V"

#define CLASS_NAME "br/unb/unbiquitous/ubiquitos/uos/driver/UserDriverNativeSupport"

#define FIELD_NAME_DRIVER "userDriver"
#define FIELD_SIGNATURE_DRIVER "Lbr/unb/unbiquitous/ubiquitos/uos/driver/UserDriverNativeSupport;"

#define METHOD_NAME_NEW_USER "registerNewUserEvent"
#define METHOD_SIGNATURE_NEW_USER "(Ljava/lang/String;FFFF)V"

#define METHOD_NAME_LOST_USER "registerLostUserEvent"
#define METHOD_SIGNATURE_LOST_USER "(Ljava/lang/String;)V"

#define METHOD_NAME_RECHECK_USER "registerRecheckUserEvent"
#define METHOD_SIGNATURE_RECHECK_USER "(Ljava/lang/String;Ljava/lang/String;FFFF)V"

#define CLASS_NAME_LIST "java/util/ArrayList"

#define METHOD_NAME_ADD_LIST "add"
#define METHOD_SIGNATURE_ADD_LIST "(Ljava/lang/Object;)Z"

#define METHOD_NAME_CONTAINS_LIST "contains"
#define METHOD_SIGNATURE_CONTAINS_LIST "(Ljava/lang/Object;)Z"

#define METHOD_NAME_CONSTRUCTOR_LIST "<init>"
#define METHOD_SIGNATURE_CONSTRUCTOR_LIST "()V"

#define NAME_SERVICE_CALL_EXCEPTION "br/unb/unbiquitous/ubiquitos/uos/adaptabitilyEngine/ServiceCallException"
#define NAME_CLASS_NOT_FOUND_EXCEPTION "java/lang/ClassNotFoundException"
#define NAME_NO_SUCH_METHOD_EXCEPTION "java/lang/NoSuchMethodException"

#ifdef __cplusplus
extern "C" {
#endif

//****************************************************************************************
//				ASSINATURA DAS FUNCOES
//****************************************************************************************
void isTrackerRunning();
void lostTrackerRunnalbeSignals();
void getTrackerRunnalbeSignals();
int getMaxPersonNumber(JNIEnv * env);
void learn(JNIEnv * env, const char *szFileTrain);
void cleanupQueue(int signal);
void throwNewException(JNIEnv *env, const char *nameOfException, const char *message);

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
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_TrackerRunnale
 * Method:    doTracker
 * Signature: ()V
 */
void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_00024TrackerRunnable_doTracker(JNIEnv * env, jobject obj) {
	char msgException[256];

	idQueueComunicationJavaC = createMessageQueue(MESSAGE_QUEUE_COMUNICATION_JAVA_C);

	jclass trackerRunnableClass = (env)->FindClass(CLASS_NAME);
	if (trackerRunnableClass == NULL) {
		sprintf(msgException, "Could not find class %s.", CLASS_NAME);
		throwNewException(env, NAME_CLASS_NOT_FOUND_EXCEPTION, msgException);
		return;
	}

	jfieldID fid = (env)->GetStaticFieldID(trackerRunnableClass, FIELD_NAME_DRIVER, FIELD_SIGNATURE_DRIVER);
	if (fid == NULL) {
		sprintf(msgException, "Could not find field %s%s.", FIELD_NAME_DRIVER, FIELD_SIGNATURE_DRIVER);
		throwNewException(env, NAME_CLASS_NOT_FOUND_EXCEPTION, msgException);
		return;
	}

	jobject driver =  (env)->GetStaticObjectField(trackerRunnableClass, fid);
	if (driver == NULL) {
		sprintf(msgException, "Could not find object %s%s.", FIELD_NAME_DRIVER, FIELD_SIGNATURE_DRIVER);
		throwNewException(env, NAME_CLASS_NOT_FOUND_EXCEPTION, msgException);
		return;
	}

	jmethodID midNewUser = (env)->GetMethodID(trackerRunnableClass, METHOD_NAME_NEW_USER, METHOD_SIGNATURE_NEW_USER);
	if (midNewUser == NULL) {
		sprintf(msgException, "Could not find method %s%s.", METHOD_NAME_NEW_USER, METHOD_SIGNATURE_NEW_USER);
		throwNewException(env, NAME_NO_SUCH_METHOD_EXCEPTION, msgException);
		return;
	}

	jmethodID midLostUser = (env)->GetMethodID(trackerRunnableClass, METHOD_NAME_LOST_USER, METHOD_SIGNATURE_LOST_USER);
	if (midLostUser == NULL) {
		char msgException[256];
		sprintf(msgException, "Could not find method %s%s.", METHOD_NAME_LOST_USER, METHOD_SIGNATURE_LOST_USER);
		throwNewException(env, NAME_NO_SUCH_METHOD_EXCEPTION, msgException);
		return;
	}

	jmethodID midRecheckUser = (env)->GetMethodID(trackerRunnableClass, METHOD_NAME_RECHECK_USER, METHOD_SIGNATURE_RECHECK_USER);
	if (midLostUser == NULL) {
		sprintf(msgException, "Could not find method %s%s.", METHOD_NAME_RECHECK_USER, METHOD_SIGNATURE_RECHECK_USER);
		throwNewException(env, NAME_NO_SUCH_METHOD_EXCEPTION, msgException);
		return;
	}

	while (1) {
		MessageEvents messageEvents;
		printLogConsole("+++++++++++> Esperando escolha.\n");
		msgrcv(idQueueComunicationJavaC, &messageEvents, sizeof(MessageEvents) - sizeof(long), 0, 0);
		printLogConsole("+++++++++++> %s\n\n", messageEvents.user_name);

		jstring name = (env)->NewStringUTF(messageEvents.user_name);

		jstring last_name = NULL;
		if (strlen(messageEvents.last_name) > 0)
			last_name = (env)->NewStringUTF(messageEvents.last_name);

		if (messageEvents.type == NEW_USER) {
			(env)->CallVoidMethod(driver, midNewUser, name, messageEvents.confidence, messageEvents.x, messageEvents.y, messageEvents.z);
		} else if (messageEvents.type == LOST_USER) {
			(env)->CallVoidMethod(driver, midLostUser, name);
		} else if (messageEvents.type == RECHECK) {
			(env)->CallVoidMethod(driver, midRecheckUser, name, last_name, messageEvents.confidence, messageEvents.x, messageEvents.y, messageEvents.z);
		} else if (messageEvents.type == RECHECK_POSITION){
			(env)->CallVoidMethod(driver, midRecheckUser, name, last_name, messageEvents.confidence, messageEvents.x, messageEvents.y, messageEvents.z);
		}

	}

	printLogConsole("FIM Tracker");
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport
 * Method:    startTracker
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_startTracker(JNIEnv *, jobject) {
	//criando um processo filho. Este processo sera transformado do deamon utilizando o execl
	trackerId = fork();
	if (trackerId < 0) {
		printLogConsole("Erro na criação do Tracker através do fork\n");
		exit(EXIT_FAILURE);
	}

	//inicinado o processo que reconhece os novoc usuarios encontrados
	if (trackerId == 0) {
		execl(TRACKER_PATH, EXEC_NAME_TRACKER, (char *) 0);
	}
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport
 * Method:    stopTracker
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_stopTracker(JNIEnv * env, jobject obj) {
	cleanupQueue(SIGINT);
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport
 * Method:    train
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_train(JNIEnv * env, jobject obj) {
	learn(env, TRAIN_DATA);
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport
 * Method:    saveImage
 */
JNIEXPORT jboolean JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_saveImage(JNIEnv * env, jobject obj, jstring name, jint index, jbyteArray image) {
	IplImage *greyImg;
	IplImage *faceImg;
	IplImage *sizedImg;
	IplImage *equalizedImg;
	CvRect faceRect;
	CvHaarClassifierCascade* faceCascade;
	char cstr[256];
	int nPeople;

	// lê o classificador utilizado para detecção
	faceCascade = (CvHaarClassifierCascade*) cvLoad(HAARCASCADE_FRONTALFACE, 0, 0, 0);
	if (!faceCascade) {
		char msgException[256];
		sprintf(msgException, "Classifier '%s' can not be read.", TRAIN_DATA);
		throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
		return false;
	}

	// Buscando o nome da pessoa
	const char *newPersonName = env->GetStringUTFChars(name, NULL);

	// Buscando o tamanho da imagem
	int arrayLength = env->GetArrayLength(image);

	// Buscando a imagem
	jbyte *imageArray = env->GetByteArrayElements(image, NULL);
	char *imageArrayConverted = (char *) (malloc(sizeof(char) * arrayLength));
	for (int i = 0; i < arrayLength; ++i) {
		imageArrayConverted[arrayLength - i] = (char) (imageArray[i]);
	}

	// Criando a IplImage
	IplImage* frame = cvCreateImage(cvSize(KINECT_WIDTH_CAPTURE, KINECT_HEIGHT_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
	frame->imageData = (char*) (imageArrayConverted);

	// transforma a imagem em escala de cinza
	greyImg = convertImageToGreyscale(frame);

	// realiza deteccao facial na imagem de entrada, usando o classificador
	faceRect = detectFaceInImage(greyImg, faceCascade);

	cvNamedWindow("teste", CV_WINDOW_AUTOSIZE);
	cvMoveWindow("teste", 500, 500);
	cvShowImage("teste", frame);
	cvWaitKey();

	printf("faceRect.width -> %d", faceRect.width);

	// veririfica se uma faxe foi detectada
	if (faceRect.width > 0) {
		faceImg = cropImage(greyImg, faceRect); // pega a face detectada
		// redimensiona a imagem
		sizedImg = resizeImage(faceImg, FACE_WIDTH, FACE_HEIGHT);
		// da a imagem o brilho e contraste padrao
		equalizedImg = cvCreateImage(cvGetSize(sizedImg), 8, 1); // cria uma imagem vazia em escala de cinza
		cvEqualizeHist(sizedImg, equalizedImg);

		//salvando a imagem
		nPeople = getMaxPersonNumber(env);
		sprintf(cstr, NEW_IMAGES_SCHEME, nPeople + 1, newPersonName, index);

		cvSaveImage(cstr, equalizedImg, NULL);

		// libera os recursos
		cvReleaseImage(&greyImg);
		cvReleaseImage(&faceImg);
		cvReleaseImage(&sizedImg);
		cvReleaseImage(&equalizedImg);

		FILE *trainFile = fopen(TRAIN_DATA, "a");
		if (trainFile == NULL) {
			char msgException[256];
			sprintf(msgException, "File %s can not be opened.", TRAIN_DATA);
			throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
			return false;
		}
		fprintf(trainFile, "%d %s %s\n", nPeople + 1, newPersonName, cstr);
		fclose(trainFile);

		return true;
	}

	return false;
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport
 * Method:    removeUser
 */
JNIEXPORT jboolean JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_removeUser(JNIEnv * env, jobject obj, jstring name) {
	FILE *trainFileImages;
	FILE *trainFileImagesAuxiliar;

	char personName[256];
	int personNumber;
	char imageFileName[512], newImageFileName[512];

	// Buscando o nome da pessoa
	const char *personNameToRemove = env->GetStringUTFChars(name, NULL);

	// abre o arquivo de TRAIN
	if (!(trainFileImages = fopen(TRAIN_DATA, "r"))) {
		char msgException[256];
		sprintf(msgException, "File %s can not be opened.", TRAIN_DATA);
		throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
		return false;
	}

	// abre o arquivo de TRAIN AUXILIAR
	if (!(trainFileImagesAuxiliar = fopen(TRAIN_DATA_AUX, "w+"))) {
		char msgException[256];
		sprintf(msgException, "File %s can not be opened.", TRAIN_DATA_AUX);
		throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
		return false;
	}

	// itera linha a linha procurando pelo usuario a ser removido
	int lastPersonNumber = 0;
	int lastLinePersonNumber = 0;
	bool removed = false;
	while (!feof(trainFileImages)) {
		if (fscanf(trainFileImages, "%d %s %s", &personNumber, personName, imageFileName) < 0)
			continue;

		if (lastLinePersonNumber != personNumber) {
			lastPersonNumber = lastLinePersonNumber;
		}

		if (strcmp(personName, personNameToRemove) == 0) {
			remove(imageFileName);
			removed = true;
		} else {
			if (removed) {
				char s1[50], s2[50];
				sprintf(s1, "%02d_", personNumber);
				sprintf(s2, "%02d_", lastPersonNumber);

				strcpy(newImageFileName, replaceInString(imageFileName, s1, s2));

				rename(imageFileName, newImageFileName);

				fprintf(trainFileImagesAuxiliar, "%d %s %s\n", lastPersonNumber, personName, imageFileName);
			} else {
				fprintf(trainFileImagesAuxiliar, "%d %s %s\n", personNumber, personName, imageFileName);
			}
		}

		lastLinePersonNumber = personNumber;
	}

	fflush(trainFileImages);
	fflush(trainFileImagesAuxiliar);

	// remove o arquivo antigo
	remove(TRAIN_DATA);

	rename(TRAIN_DATA_AUX, TRAIN_DATA);

	return removed;
}

JNIEXPORT jobject JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_listUsers(JNIEnv * env, jobject obj) {
	FILE *trainFileImages;
	char personName[256];
	int personNumber;
	char imageFileName[512];

	// abre o arquivo de TRAIN
	if (!(trainFileImages = fopen(TRAIN_DATA, "r"))) {
		char msgException[256];
		sprintf(msgException, "File %s can not be opened.", TRAIN_DATA);
		throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
		return false;
	}

	jclass listClass = (env)->FindClass(CLASS_NAME_LIST);
	if (listClass == NULL) {
		fprintf(stderr, "Não encontrou a classe %s.", CLASS_NAME_LIST); /* error handling */
		return NULL;
	}

	jmethodID constructorList = (env)->GetMethodID(listClass, METHOD_NAME_CONSTRUCTOR_LIST, METHOD_SIGNATURE_CONSTRUCTOR_LIST);
	if (constructorList == NULL) {
		fprintf(stderr, "Não encontrou o metodo %s %s.", METHOD_NAME_CONSTRUCTOR_LIST, METHOD_SIGNATURE_CONSTRUCTOR_LIST); /* error handling */
		return NULL;
	}

	jobject mylist = (env)->NewObject(listClass, constructorList);

	jmethodID midAddList = (env)->GetMethodID(listClass, METHOD_NAME_ADD_LIST, METHOD_SIGNATURE_ADD_LIST);
	if (midAddList == NULL) {
		fprintf(stderr, "Não encontrou o metodo %s %s.", METHOD_NAME_ADD_LIST, METHOD_SIGNATURE_ADD_LIST); /* error handling */
		return NULL;
	}

	jmethodID midContainsList = (env)->GetMethodID(listClass, METHOD_NAME_CONTAINS_LIST, METHOD_SIGNATURE_CONTAINS_LIST);
	if (midContainsList == NULL) {
		fprintf(stderr, "Não encontrou o metodo %s %s.", METHOD_NAME_CONTAINS_LIST, METHOD_SIGNATURE_CONTAINS_LIST); /* error handling */
		return NULL;
	}

	while (!feof(trainFileImages)) {
		if (fscanf(trainFileImages, "%d %s %s", &personNumber, personName, imageFileName) < 0)
			continue;

		if (strstr(personName, ":") != NULL) {
			jstring id = (env)->NewStringUTF(personName);
			jboolean result = (env)->CallBooleanMethod(mylist, midContainsList, id);
			if(!result)
				result = (env)->CallBooleanMethod(mylist, midAddList, id);
		}
	}

	return mylist;
}

int getMaxPersonNumber(JNIEnv * env) {
	FILE *trainFileImages;
	char personName[256];
	int personNumber, maxPersonNumber;
	char imageFileName[512];

	// abre o arquivo de TRAIN
	if (!(trainFileImages = fopen(TRAIN_DATA, "r"))) {
		char msgException[256];
		sprintf(msgException, "File %s can not be opened.", TRAIN_DATA);
		throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
		return 0;
	}

	maxPersonNumber = 0;
	while (!feof(trainFileImages)) {
		if (fscanf(trainFileImages, "%d %s %s", &personNumber, personName, imageFileName) < 0)
			continue;

		if (personNumber > maxPersonNumber)
			maxPersonNumber = personNumber;
	}

	return maxPersonNumber;
}

//****************************************************************************************
//				FUNCOES UTILIZADAS PARA INTERCEPTAR OS DIVERSOS TIPOS DE SINAIS
//****************************************************************************************

// ps -lp pid
string getStdoutFromCommand(string cmd){
  // setup
  string data;
  FILE *stream;
  char buffer[255];

  stream = popen(cmd.c_str(), "r");
  while ( fgets(buffer, 255, stream) != NULL )
    data.append(buffer);
  pclose(stream);

  return data;
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_Daemon
 * Method:    isProcessRunning
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriverNativeSupport_00024Daemon_isProcessRunning(JNIEnv *env, jobject object){
	string cmd, output;
	std::stringstream out;
	out << trackerId;

	cmd = "ps -lp ";
	cmd.append(out.str());

	output = getStdoutFromCommand(cmd);

	#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
		int headSize = 100;
	#else
		int headSize = 70;
	#endif

	if(output.find("Z", headSize) != -1){
		return false;
	}

	return true;
}

void cleanupQueue(int signal) {
	lostTrackerRunnalbeSignals();
	printLogConsole("Signal TrackerRunnable - %d\n", signal);

	kill(trackerId, signal);
#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
	wait((int*)0);
#else
	wait();
#endif

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

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
	signal(SIGEMT, cleanupQueue);
#endif
	signal(SIGFPE , cleanupQueue);
	signal(SIGKILL, cleanupQueue);
	signal(SIGBUS , cleanupQueue);
	signal(SIGSEGV, cleanupQueue);
	signal(SIGSYS , cleanupQueue);
	signal(SIGPIPE, cleanupQueue);
	signal(SIGALRM, cleanupQueue);
	signal(SIGURG , cleanupQueue);
	signal(SIGTSTP, cleanupQueue);
	signal(SIGUSR1, cleanupQueue);
	signal(SIGUSR2, cleanupQueue);
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

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
	signal(SIGEMT, SIG_IGN); 
#endif
	signal(SIGFPE , SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	signal(SIGBUS , SIG_IGN);
	signal(SIGSEGV, SIG_IGN);
	signal(SIGSYS , SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGURG , SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

}

//****************************************************************************************
//							FUNCOES UTILIZADAS NO TREINO DO ALGORITMO
//****************************************************************************************

// le os nomes e as imagens das pessoas de um arquivo txt, e le todas as imagens listadas
int loadFaceImgArray(JNIEnv * env, const char * filename) {
	FILE * imgListFile = 0;
	char imgFilename[512];
	int iFace, nFaces = 0;
	int i;

	// abre o arquivo de input
	if (!(imgListFile = fopen(filename, "r"))) {
		char msgException[256];
		sprintf(msgException, "File %s can not be opened", filename);
		throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
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
			char msgException[256];
			sprintf(msgException, "Image can not be read from %s.", imgFilename);
			throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
			return 0;
		}
	}

	fclose(imgListFile);

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
	printLogConsole("Salvando a imagem da face media como 'out_averageImage.bmp'.\n");
	cvSaveImage(AVERAGE_IMAGE, pAvgTrainImg);

	//cria uma nova imagem feita de varias imagens eigenfaces 
	printLogConsole("Salvando o eigenvector %d como 'out_eigenfaces.bmp'\n", nEigens);
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
void learn(JNIEnv * env, const char *szFileTrain) {
	int i, offset;

	// le os dados de treinamento
	printLogConsole("Lendo imagens de treinamento em '%s'\n", szFileTrain);
	nTrainFaces = loadFaceImgArray(env, szFileTrain);
	printLogConsole("%d imagens de treino obtidas.\n", nTrainFaces);
	if (nTrainFaces < 2) {
		char msgException[256];
		sprintf(msgException, "Required two or more faces for training. Input file contains only %d.", nTrainFaces);
		throwNewException(env, NAME_SERVICE_CALL_EXCEPTION, msgException);
		return;
	}

	// faz o PCA nas faces de treinamento
	doPCA();

	// projeta as imagens de trienamento no subespaco PCA
	projectedTrainFaceMat = cvCreateMat(nTrainFaces, nEigens, CV_32FC1);
	offset = projectedTrainFaceMat->step / sizeof(float);
	for (i = 0; i < nTrainFaces; i++) {
		cvEigenDecomposite(faceImgArr[i], nEigens, eigenVectArr, 0, 0, pAvgTrainImg, projectedTrainFaceMat->data.fl + i * offset);
	}

	// armazena os dados de reconhecimento como um arquivo xml
	storeTrainingData();

	// salva todos os eigenvectors como imagens, para que eles possam ser utilizados
	if (SAVE_EIGENFACE_IMAGES) {
		storeEigenfaceImages();
	}

}


/**
 * Lança a esceção passada no sistema com a devida mensagem
 */
void throwNewException(JNIEnv *env, const char *nameOfException, const char *message) {
	jclass exceptionClass = (env)->FindClass(nameOfException);
	if (exceptionClass == NULL) {
		exceptionClass = (env)->FindClass(NAME_CLASS_NOT_FOUND_EXCEPTION);
	}

	(env)->ThrowNew(exceptionClass, message);
}

#ifdef __cplusplus
}
#endif
