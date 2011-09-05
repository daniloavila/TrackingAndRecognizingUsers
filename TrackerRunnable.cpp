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

int idQueueRequest;
int idQueueResponse;
int faceRecId;

map<int, UserStatus> users;

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     TrackerRunnable
 * Method:    doTracker
 * Signature: ()V
 */
void JNICALL Java_TrackerRunnable_doTracker
(JNIEnv * env, jobject obj) {
	printf("Init Tracker Comunication...\n");

	int idQueueComunicationJavaC = createMessageQueue(MESSAGE_QUEUE_COMUNICATION_JAVA_C);

	//criando um processo filho. Este processo sera transformado do deamon utilizando o execl
	int trackerId = fork();
	if (trackerId < 0) {
		printf("erro no fork\n");
		exit(1);
	}

	//inicinado o processo que reconhece os novoc usuarios encontrados
	if (trackerId == 0) {
		execl("tracker", "tracker", (char *) 0);
	}

	while (1) {
		MessageResponse messageResponse;
		printf("\n+++++++++++> Esperando escolha.\n");
		msgrcv(idQueueComunicationJavaC, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0, 0);
		printf("\n+++++++++++> %s\n\n", messageResponse.user_name);

		jmethodID mid;

		jclass trackerRunnableClass = (env)->FindClass("TrackerRunnable");
		if (trackerRunnableClass == NULL) {
			printf("Não encontrou a classe.\n"); /* error handling */
		}

		mid = (env)->GetMethodID(trackerRunnableClass, "registerUser", "(ILjava/lang/String;F)V");
		if (mid == NULL) {
			printf("Não encontrou o metodo.\n"); /* error handling */
		}

		jstring name = (env)->NewStringUTF(messageResponse.user_name);

		(env)->CallVoidMethod(obj, mid, messageResponse.user_id, name, messageResponse.confidence);
	}

	printf("FIM Tracker");
}

#ifdef __cplusplus
}
#endif
