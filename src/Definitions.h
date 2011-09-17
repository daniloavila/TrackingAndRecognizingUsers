#ifndef DEFINITIONS_H_
#define DEFINITIONS_H_

#define USE_MAHALANOBIS_DISTANCE // Define o uso da distancia mahalanobis para auxiliar a euclidiana

#define JAVA_INTEGRATION // Define se será ou não enviado mensagens para o processo java.

// #define DEBUG // Define se será ou não mostrado no console informações de log.

//#define SAVE_LOG // Define se será salvo um log sobre os usuários encontrados.


/*********************************
 ****** Register parameters ******
 *********************************/
#define NUMBER_OF_SAVED_FACES_FRONTAL 6
#define NUMBER_OF_SAVED_FACES_RIGHT 2
#define NUMBER_OF_SAVED_FACES_LEFT 2

/*********************************
 ****** Tracker parameters ******
 *********************************/
#define GL_WIN_SIZE_X 720
#define GL_WIN_SIZE_Y 480

//intervalo em milisegundos definido como tempo de espera para que a funcao de tratar a resposta do recognition
//e que faz reconhecimento dos usarios ja reocnhecidos novamente
#define INTERVAL_IN_MILISECONDS_TREAT_RESPONSE 500 // 0.5 segundos
#define INTERVAL_IN_MILISECONDS_RECHECK 5000 // 5 segundos
//numero de frames capturadas do usario ao ser rastreado pela primeira vez
#define ATTEMPTS_INICIAL_RECOGNITION 20

// distancia minima que o rastreado tenha que se deslocar para ser considerada uma pessoa
#define MIN_OBJECT_DESLOCATION 3.0
#define MAX_TIMES_OF_OBJECT_NO_DESLOCATION 5

// arquivos de configuracao do kinect
#define SAMPLE_XML_PATH "Config/SamplesConfig.xml"
#define SAMPLE_XML_PATH_REGISTER "Config/Register.xml"

//define o numero de ajuste dos pixels do usuario da camera depth para o rgb
#define ADJUSTMENT_LEFT 40
#define ADJUSTMENT_RIGHT 20

//valor que o pixel da area expandida do usuario recebe
#define ADJUSTED 999

// Avalia os erros do Kinect
#define CHECK_RC(nRetVal, what) \
	if (nRetVal != XN_STATUS_OK){\
		fprintf(stderr, "%s failed: %s\n", what, xnGetStatusString(nRetVal));\
		return nRetVal; \
	}

// Ajuda no debug da aplicação
#ifdef DEBUG
	extern FILE *log;
	#define printLogConsole(...) \
		do { \
			fprintf(log, __VA_ARGS__); \
		} while (0)
#else
	#define printLogConsole(...)
#endif

// Message parameters
// key da fila de messagens entre "UserTracker" e "FaceRec". Utilizada para enviar requisicoes de reconhecimento ao "FaceRec".
#define MESSAGE_QUEUE_REQUEST 0x1223

// key da fila de messagens entre "FaceRec" e "UserTracker". Utilizado para responder as requisicoes enviadas pelo "UserTracker".
#define MESSAGE_QUEUE_RESPONSE 0x1228

// key da fila de messagens
#define MESSAGE_QUEUE_COMUNICATION_JAVA_C 0x1232

// key da memoria compartilhada entre "FaceRec" e "UserTracker". Utilizado para enviar a imagem do novo usuario detectado.
#define SHARED_MEMORY 0x1230
#define MAX_SHARED_MEMORY 0x12bc

// classificador para localizar a face em uma imagem
#define HAARCASCADE_FRONTALFACE "Eigenfaces/haarcascade_frontalface_alt.xml"

#define NAME_OF_LOG_FILE "trackerLog"
#define LOG_FOLDER "LOG/"

// limiar de confianca para identificar o usuario
#define THRESHOLD 0.9

// label de estado definida para o usuario não reocnhecido
#define UNKNOWN "Unknown"

// label definida para objetos que foram rastreados por entrar em contato 
#define OBJECT "Object"

typedef struct DeslocationStatus {
	XnPoint3D lastPosition;
	int numberTimesNotMoved; // numero de vezes que não se movimentou
	int numberTimesMoved; // numero de vezes que se movimentou
} DeslocationStatus;

typedef struct UserStatus {
	char *name;
	float confidence;
	bool canRecognize; // Pode reconhecer o usuário
	// bool canShow; // Pode mostrar a sua label. TODO: Será usado posteriomente
	DeslocationStatus deslocationStatus;
} UserStatus;

#endif
