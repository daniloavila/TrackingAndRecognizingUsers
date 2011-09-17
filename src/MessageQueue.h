#ifndef MESSAGE_QUEUE_H_
#define MESSAGE_QUEUE_H_

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include "KinectUtil.h"
#include "Definitions.h"

// estrutura da mensagem trocada entre "Tracker" e "Recognizer"
typedef struct MessageRequest {
	long user_id;
	int memory_id;
} MessageRequest;

// estrutura da mensagem trocada entre "Recognizer" e "Tracker"
typedef struct MessageResponse {
	long user_id;
	char user_name[255];
	float confidence;
} MessageResponse;

enum MessageType{ NEW_USER, LOST_USER, RECHECK};

// estrutura da mensagem trocada entre "TrackerRunnable" e "Tracker"
typedef struct MessageEvents {
	char user_name[255];
	float confidence;
	float x;
	float y;
	float z;
	MessageType type;
} MessageEvents;


//cria fila de mensagens para comunicao entre o processo tracker e recognition
int createMessageQueue(int key);

//cria fila de mensagens para comunicao entre o processo tracker e recognition
int getMessageQueue(int key);

#endif 
