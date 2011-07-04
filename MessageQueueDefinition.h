#ifndef MESSAGE_QUEUE_DEFINITION_H_
#define MESSAGE_QUEUE_DEFINITION_H_

#include "Kinect.h"

// key da fila de messagens entre "UserTracker" e "FaceRec". Utilizada para enviar requisicoes de reconhecimento ao "FaceRec".
#define MESSAGE_QUEUE_REQUEST 0x1223

// key da fila de messagens entre "FaceRec" e "UserTracker". Utilizado para responder as requisicoes enviadas pelo "UserTracker".
#define MESSAGE_QUEUE_RESPONSE 0x1228

// estrutura da mensagem trocada entre "UserTracker" e "FaceRec"
typedef struct messageRequest {
	long user_id;
	char matriz_pixel[KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS];
} messageRequest;

// estrutura da mensagem trocada entre "FaceRec" e "UserTracker"
typedef struct messageResponse {
	long user_id;
	char user_name[255];
} messageResponse;

#endif 