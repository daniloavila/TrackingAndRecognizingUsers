#ifndef MESSAGE_QUEUE_H_
#define MESSAGE_QUEUE_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include "KinectUtil.h"
#include "Definitions.h"

// estrutura da mensagem trocada entre "UserTracker" e "FaceRec"
typedef struct MessageRequest {
	long user_id;
	int memory_id;
} MessageRequest;

// estrutura da mensagem trocada entre "FaceRec" e "UserTracker"
typedef struct MessageResponse {
	long user_id;
	char user_name[255];
	float confidence;
} MessageResponse;

//cria fila de mensagens para comunicao entre o processo tracker e recognition
int createMessageQueue(int key);

//cria fila de mensagens para comunicao entre o processo tracker e recognition
int getMessageQueue(int key);

#endif 
