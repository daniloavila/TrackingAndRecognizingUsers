#include "TrackerRunnable.h"

#include <jni.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <signal.h>
#include <time.h>

#include "MessageQueue.h"
#include "Definitions.h"

#include <stdio.h>

using namespace std;

//#define CLASS_NAME "TrackerRunnable"
//#define METHOD_NAME "registerUser"
//#define METHOD_SIGNATURE "(ILjava/lang/String;F)V"

#define CLASS_NAME "br/unb/unbiquitous/ubiquitos/uos/driver/UserDriver"
#define METHOD_NAME "registerUser"
#define METHOD_SIGNATURE "(ILjava/lang/String;F)V"

#ifdef __cplusplus
extern "C" {
#endif

void lostTrackerRunnalbeSignals();
void getTrackerRunnalbeSignals();

int idQueueComunicationJavaC = 0;
int trackerId = 0;

// TODO : Tales - 10/09/2011 - Metodo não é chamado quando mata o processo java. Verificar o que acontece com os sinais.
void cleanupQueue(int signal) {
	lostTrackerRunnalbeSignals();
	printLogConsole("Siganal TrackerRunnable - %d\n", signal);
	msgctl(idQueueComunicationJavaC, IPC_RMID, NULL);
	kill(trackerId, signal);
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_TrackerRunnale
 * Method:    doTracker
 * Signature: ()V
 */
void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_00024TrackerRunnable_doTracker
(JNIEnv * env, jobject obj) {
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

	while (1) {
		MessageResponse messageResponse;
		printLogConsole("+++++++++++> Esperando escolha.\n");
		msgrcv(idQueueComunicationJavaC, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0, 0);
		printLogConsole("+++++++++++> %s\n\n", messageResponse.user_name);

		jmethodID mid;

		jclass trackerRunnableClass = (env)->FindClass(CLASS_NAME);
		if (trackerRunnableClass == NULL) {
			fprintf(stderr, "Não encontrou a classe %s.\n", CLASS_NAME); /* error handling */
		}

		mid = (env)->GetStaticMethodID(trackerRunnableClass, METHOD_NAME, METHOD_SIGNATURE);
		if (mid == NULL) {
			fprintf(stderr, "Não encontrou o metodo %s %s.\n", METHOD_NAME, METHOD_SIGNATURE); /* error handling */
		}

		jstring name = (env)->NewStringUTF(messageResponse.user_name);

		(env)->CallStaticVoidMethod(trackerRunnableClass, mid, messageResponse.user_id, name, messageResponse.confidence);
		//(env)->CallVoidMethod(obj, mid, messageResponse.user_id, name, messageResponse.confidence);
	}

	printLogConsole("FIM Tracker");
}

/*
 * Class:     br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver
 * Method:    stopTracker
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_br_unb_unbiquitous_ubiquitos_uos_driver_UserDriver_stopTracker
  (JNIEnv *, jobject) {
	cleanupQueue(SIGINT);
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
