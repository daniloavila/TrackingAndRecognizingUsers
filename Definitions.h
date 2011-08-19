#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

// Register parameters
#define NUMBER_OF_SAVED_FACES_FRONTAL 50
#define NUMBER_OF_SAVED_FACES_RIGHT 20
#define NUMBER_OF_SAVED_FACES_LEFT 20

#define NUMBER_OF_ROTATE_IMAGES 20
#define NUMBER_OF_FLIP_IMAGES 20
#define NUMBER_OF_NOISE_IMAGES 20

#define MAX_ANGLE_OF_ROTATE 10

// vai de 0 a 255 e quanto menor mais ruído.
#define LEVEL_OF_NOISE_IMAGES 50

// Tracker parameters
#define GL_WIN_SIZE_X 720
#define GL_WIN_SIZE_Y 480

#define INTERVAL_IN_MILISECONDS_TREAT_RESPONSE 500
#define INTERVAL_IN_MILISECONDS_RECHECK 5000

#define ATTEMPTS_INICIAL_RECOGNITION 10

#define SAMPLE_XML_PATH "Config/SamplesConfig.xml"
#define SAMPLE_XML_PATH_REGISTER "Config/Register.xml"

//define o numero de ajuste dos pixels do usuario da camera depth para o rgb
#define ADJUSTMENT_LEFT 40
#define ADJUSTMENT_RIGHT 20

//valor que o pixel da area expandida do usuario recebe
#define ADJUSTED 999

#define CHECK_RC(nRetVal, what) \
	if (nRetVal != XN_STATUS_OK){\
		printf("%s failed: %s\n", what, xnGetStatusString(nRetVal));\
		return nRetVal; \
	}

// Message parameters
// key da fila de messagens entre "UserTracker" e "FaceRec". Utilizada para enviar requisicoes de reconhecimento ao "FaceRec".
#define MESSAGE_QUEUE_REQUEST 0x1223

// key da fila de messagens entre "FaceRec" e "UserTracker". Utilizado para responder as requisicoes enviadas pelo "UserTracker".
#define MESSAGE_QUEUE_RESPONSE 0x1228

// key da memoria compartilhada entre "FaceRec" e "UserTracker". Utilizado para enviar a imagem do novo usuario detectado.
#define SHARED_MEMORY 0x1230
#define MAX_SHARED_MEMORY 0x12bc

#define HAARCASCADE_PROFILEFACE "Eigenfaces/haarcascade_profileface_umist2.xml"
#define HAARCASCADE_FRONTALFACE "Eigenfaces/haarcascade_frontalface_alt.xml"

#endif
