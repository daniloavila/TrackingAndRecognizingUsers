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

void lostTrackerRunnalbeSignals();
void getTrackerRunnalbeSignals();
int getNumberOfRegisteredPersons();


int idQueueComunicationJavaC = 0;
int trackerId = 0;

// TODO : Tales - 10/09/2011 - Metodo não é chamado quando mata o processo java. Verificar o que acontece com os sinais.
void cleanupQueue(int signal) {
	lostTrackerRunnalbeSignals();
	printLogConsole("Signal TrackerRunnable - %d\n", signal);	
	msgctl(idQueueComunicationJavaC, IPC_RMID, NULL);
	kill(trackerId, signal);
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_TrackerRunnale
 * Method:    doTracker
 * Signature: ()V
 */
void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_00024TrackerRunnable_doTracker(JNIEnv * env, jobject obj) {
	printLogConsole("Init Tracker Comunication...\n");

	idQueueComunicationJavaC = createMessageQueue(MESSAGE_QUEUE_COMUNICATION_JAVA_C);

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
 * Method:    stopTracker
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_stopTracker(JNIEnv *, jobject) {
	cleanupQueue(SIGINT);
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

	IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
	frame->imageData = (char *) image;

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
		sprintf(cstr, NEW_IMAGES_SCHEME, nPeople + 1, name, index);
		cvSaveImage(cstr, equalizedImg, NULL);

		// libera os recursos
		cvReleaseImage(&greyImg);
		cvReleaseImage(&faceImg);
		cvReleaseImage(&sizedImg);
		cvReleaseImage(&equalizedImg);
	}

}

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

#ifdef __cplusplus
}
#endif
