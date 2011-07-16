//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------
#include <XnOpenNI.h>
#include <XnCodecIDs.h>
#include <XnCppWrapper.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <signal.h>

#include <iostream>
#include <map>
#include <string>

#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

#include "SceneDrawer.h"
#include "UserUtil.h"
#include "KinectUtil.h"
#include "MessageQueue.h"
#include <stdio.h>
#include <curses.h>

using namespace std;

#define SAMPLE_XML_PATH "Config/SamplesConfig.xml"

#define CHECK_RC(nRetVal, what) \
	if (nRetVal != XN_STATUS_OK){\
		printf("%s failed: %s\n", what, xnGetStatusString(nRetVal));\
		return nRetVal; \
	}

//---------------------------------------------------------------------------
// Globals
//---------------------------------------------------------------------------
xn::Context g_Context;
xn::DepthGenerator g_DepthGenerator;
xn::ImageGenerator g_ImageGenerator;
xn::UserGenerator g_UserGenerator;
xn::SceneAnalyzer g_SceneAnalyzer;

XnBool g_bNeedPose = FALSE;
XnChar g_strPose[20] = "";
XnBool g_bDrawBackground = TRUE;
XnBool g_bDrawPixels = TRUE;
XnBool g_bDrawSkeleton = TRUE;
XnBool g_bPrintID = TRUE;
XnBool g_bPrintState = TRUE;

#define INTERVAL_IN_MILISECONDS_TREAT_RESPONSE 1000
#define INTERVAL_IN_MILISECONDS_RECHECK 10000

int idQueueRequest;
int idQueueResponse;
int faceRecId;
int sharedMemoryCount = 0;

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define GL_WIN_SIZE_X 720
#define GL_WIN_SIZE_Y 480

XnBool g_bPause = false;
XnBool g_bRecord = false;
XnBool g_bQuit = false;

map<int, char *> users;
map<int, map<string, float> > usersNameConfidence;
map<int, map<string, int> > usersNameAttempts;
map<int, float> usersConfidence;

/*************************************************************************
 * UTIL
 *************************************************************************/

/**
 * Calcula a proxima key da memoria compartilhada.
 */
unsigned int getMemoryKey() {
	return SHARED_MEMORY + (sharedMemoryCount * 4);
}

/**
 * Busca o frame do usuario isolando os seus pixels
 */
void getFrameFromUserId(XnUserID nId, char *maskPixels) {

	// Busca a area da imagem onde o usuario está.
	xn::SceneMetaData sceneMD;
	g_UserGenerator.GetUserPixels(nId, sceneMD);

	// Busca um frame da tela
	const XnRGB24Pixel *source = g_ImageGenerator.GetRGB24ImageMap();
	char *result = transformToCharAray(source);

	// Busca em cima do frame da tela só a area de pixels do usuario
	unsigned short *scenePixels = (unsigned short int*) (sceneMD.Data());

	transformAreaVision(scenePixels);

	for (int i = 0; i < KINECT_HEIGHT_CAPTURE; i++) {
		for (int j = 0; j < KINECT_WIDTH_CAPTURE; j++) {
			int index = (i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS);
			if (scenePixels[(i * KINECT_WIDTH_CAPTURE) + j]) {
				maskPixels[index + 2] = result[index + 2];
				maskPixels[index + 1] = result[index + 1];
				maskPixels[index + 0] = result[index + 0];
			} else {
				maskPixels[index + 2] = 0;
				maskPixels[index + 1] = 0;
				maskPixels[index + 0] = 0;
			}
		}
	}
}

/**
 * Recalcula as estatisticas de confianca e tentativas a partir da mensagem de resposta.
 */
void calculateNewStatistics(MessageResponse *messageResponse) {
	// Inicio - recalcula a confiança da nova label
	map<string, int> *nameAttempts = &usersNameAttempts[messageResponse->user_id];
	map<string, float> *nameConfidence = &usersNameConfidence[messageResponse->user_id];

	float acumulatedConfidence = (*nameConfidence)[messageResponse->user_name];
	int attempts = (*nameAttempts)[messageResponse->user_name];

	// confidence
	(*nameConfidence)[messageResponse->user_name] = ((acumulatedConfidence * attempts) + messageResponse->confidence) / (attempts + 1);

	// tentativas
	(*nameAttempts)[messageResponse->user_name] = attempts + 1;
}

/**
 * Escolhe estatisticamente baseado no numero de vezes que o reconhecedor escolheu a label e o grau de confiança que o mesmo a escolheu.
 */
void choiceNewLabelToUser(MessageResponse *messageResponse) {
	// Inicio - escolhe a nova label do usuario de acordo com a maior confiança estatistica
	map<string, int> *nameAttempts = &usersNameAttempts[messageResponse->user_id];
	map<string, float> *nameConfidence = &usersNameConfidence[messageResponse->user_id];

	string name;
	float confidence;
	double maxStatisticConfidence = 0;
	map<string, int>::iterator itAttempts;
	for (itAttempts = nameAttempts->begin(); itAttempts != nameAttempts->end(); itAttempts++) {
		double statisticConfidence = itAttempts->second * (*nameConfidence)[itAttempts->first];
		if (statisticConfidence > maxStatisticConfidence) {
			maxStatisticConfidence = statisticConfidence;
			name = itAttempts->first;
			confidence = (*nameConfidence)[itAttempts->first];
		}
	}

	users[messageResponse->user_id] = (char*) (((malloc(sizeof(char) * strlen(messageResponse->user_name)))));
	if (name.length() > 0) {
		strcpy(users[messageResponse->user_id], name.c_str());
	}
	usersConfidence[messageResponse->user_id] = confidence;
	printf("Nome: %s - Tentativas => %d - Confiança => %f\n", messageResponse->user_name, (*nameAttempts)[messageResponse->user_name],
			(*nameConfidence)[messageResponse->user_name]);
	printf("========> %s - %f\n", users[messageResponse->user_id], usersConfidence[messageResponse->user_id]);
}

/**
 * Pede para o processo de reconhecimento reconhecer o usuario com esse id passado.
 */
void requestRecognition(int id) {
	MessageRequest messageRequest;

	messageRequest.user_id = id;
	messageRequest.memory_id = getMemoryKey();

	char *maskPixels = getSharedMemory(messageRequest.memory_id);

	// Busca a area da imagem onde o usuario está.
	getFrameFromUserId((XnUserID) id, maskPixels);

	printf("Enviando pedido de reconhecimento -> user_id = %d id_memoria = %d\n", messageRequest.user_id, messageRequest.memory_id);

	if (msgsnd(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0) > 0) {
		printf("Erro no envio de mensagem para o usuario %d\n", messageRequest.memory_id);
	}

	sharedMemoryCount++;
}

/*************************************************************************
 * FIM - UTIL
 *************************************************************************/

/**
 * Recebe as mensagens da fila de resposta e reenvia as frames dos usuarios não reconhecidos.
 */
void treatQueueResponse(int i) {
	char nome[7];
	MessageResponse messageResponse;

	printf(".\n");

	if (msgrcv(idQueueResponse, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0, IPC_NOWAIT) >= 0) {

		calculateNewStatistics(&messageResponse);

		printf("Recebeu mensagem de usuario reconhecido - user => %d - %s - %f\n", messageResponse.user_id, messageResponse.user_name, messageResponse.confidence);

		choiceNewLabelToUser(&messageResponse);

		map<string, int>::iterator itAttempts;
		int total = 0;
		for (itAttempts = usersNameAttempts[messageResponse.user_id].begin(); itAttempts != usersNameAttempts[messageResponse.user_id].end(); itAttempts++) {
			total = total + (*itAttempts).second;
		}

		printf("Total => %d\n", total);

		if (strlen(messageResponse.user_name) == 0 || total < 5) {
			int id = messageResponse.user_id;

			MessageRequest messageRequest;
			messageRequest.user_id = id;
			messageRequest.memory_id = getMemoryKey();
			char *maskPixels = getSharedMemory(messageRequest.memory_id);

			// Busca a area da imagem onde o usuario está.
			getFrameFromUserId(id, maskPixels);

			printf("Enviando pedido de reconhecimento -> user_id = %d id_memoria = %d\n", messageRequest.user_id, messageRequest.memory_id);

			if (msgsnd(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0) > 0) {
				printf("Erro no envio de mensagem para o usuario %d\n", messageRequest.memory_id);
			}

			sharedMemoryCount++;
		}
	}

	glutTimerFunc(INTERVAL_IN_MILISECONDS_TREAT_RESPONSE, treatQueueResponse, 0);
}

/**
 * Pede para reconhecer todos os usuários.
 */
void recheckUsers(int i) {
	map<int, char *>::iterator it;

	printf("-\n");

	for (it = users.begin(); it != users.end(); it++) {
		requestRecognition((*it).first);
	}

	glutTimerFunc(INTERVAL_IN_MILISECONDS_RECHECK, recheckUsers, 0);
}

void cleanupQueueAndExit() {
	g_Context.Shutdown();

	//matando a fila de mensagens
	msgctl(idQueueRequest, IPC_RMID, NULL);
	msgctl(idQueueResponse, IPC_RMID, NULL);
	kill(faceRecId, SIGKILL);

	exit(1);
}

/**
 * Registra a entrada de um novo usuário e pede para o mesmo ser reconhecido.
 */
void XN_CALLBACK_TYPE registerNewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	int id = (int) ((nId));

	printf("New User %d\n", id);
	users[id] = "";
	requestRecognition(id);
}

/**
 * Apaga a entrada do novo usuário.
 */
void XN_CALLBACK_TYPE registerLostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	printf("Lost user %d\n", nId);
	// TODO : Verificar se ao apagar e apos disso receber a mensagem não cria um usuário fantasma.
	users.erase((int) nId);
}

// this function is called each frame
void glutDisplay(void) {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Setup the OpenGL viewpoint
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	xn::SceneMetaData sceneMD;
	xn::DepthMetaData depthMD;
	// xn::ImageMetaData imageMD;

	g_DepthGenerator.GetMetaData(depthMD);
	glOrtho(0, depthMD.XRes(), depthMD.YRes(), 0, -1.0, 1.0);

	glDisable(GL_TEXTURE_2D);

	if (!g_bPause) {
		// Read next available data
		g_Context.WaitAndUpdateAll();
	}

	// Process the data
	g_DepthGenerator.GetMetaData(depthMD);
	// g_ImageGenerator.GetMetaData(imageMD);
	g_UserGenerator.GetUserPixels(0, sceneMD);
	DrawDepthMap(depthMD, sceneMD, users, usersConfidence);

	glutSwapBuffers();
}

void glutIdle(void) {
	if (g_bQuit) {
		cleanupQueueAndExit();
	}

	// Display the frame
	glutPostRedisplay();
}

void glutKeyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 27:
		cleanupQueueAndExit();
	case 'b':
		// Draw background?
		g_bDrawBackground = !g_bDrawBackground;
		break;
	case 'x':
		// Draw pixels at all?
		g_bDrawPixels = !g_bDrawPixels;
		break;
	case 'i':
		// Print label?
		g_bPrintID = !g_bPrintID;
		break;
	case 'l':
		// Print ID & state as label, or only ID?
		g_bPrintState = !g_bPrintState;
		break;
	case 'p':
		g_bPause = !g_bPause;
		break;
	case 'c':
		printUsersCoM(g_UserGenerator, g_SceneAnalyzer);
	}
}
void glInit(int * pargc, char ** argv) {
	glutInit(pargc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	glutCreateWindow("User Localization in a Smart Space");
	//glutFullScreen();
	//glutSetCursor(GLUT_CURSOR_NONE);

	glutKeyboardFunc(glutKeyboard); //define acoes para determinandas teclas
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);

	glutTimerFunc(INTERVAL_IN_MILISECONDS_TREAT_RESPONSE, treatQueueResponse, 0);
	glutTimerFunc(INTERVAL_IN_MILISECONDS_RECHECK, recheckUsers, 0);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

int main(int argc, char **argv) {
	//variavel que mantem o status: caso um erro seja lançado, da para acessa-lo por meio desta
	XnStatus nRetVal = XN_STATUS_OK;

	if (argc > 1) {
		nRetVal = g_Context.Init();
		CHECK_RC(nRetVal, "Init");

		//verifica se passamos algum arquivo pela linha de comando, para que seja gravado a saida do kinect no msm
		nRetVal = g_Context.OpenFileRecording(argv[1]);
		if (nRetVal != XN_STATUS_OK) {
			printf("Can't open recording %s: %s\n", argv[1], xnGetStatusString(nRetVal));
			return 1;
		}
	} else {
		xn::EnumerationErrors errors;

		//le as configuracoes do Sample.xml, que define as configuracoes iniciais acima
		nRetVal = g_Context.InitFromXmlFile(SAMPLE_XML_PATH, &errors);

		if (nRetVal == XN_STATUS_NO_NODE_PRESENT) {
			XnChar strError[1024];
			errors.ToString(strError, 1024);
			printf("%s\n", strError);
			return (nRetVal);
		} else if (nRetVal != XN_STATUS_OK) {
			printf("Open failed: %s\n", xnGetStatusString(nRetVal));
			return (nRetVal);
		}
	}

	idQueueRequest = createMessageQueue(MESSAGE_QUEUE_REQUEST);
	idQueueResponse = createMessageQueue(MESSAGE_QUEUE_RESPONSE);

	//criando um processo filho. Este processo sera transformado do deamon utilizando o execl
	faceRecId = fork();
	if (faceRecId < 0) {
		printf("erro no fork\n");
		exit(1);
	}

	//inicinado o processo que reconhece os novoc usuarios encontrados
	if (faceRecId == 0) {
		execl("recognizer", "recognizer", (char *) 0);
	}

	nRetVal = g_SceneAnalyzer.Create(g_Context);

	//procura por um node Depth nas configuracoes
	nRetVal = g_Context.FindExistingNode(XN_NODE_TYPE_DEPTH, g_DepthGenerator);
	CHECK_RC(nRetVal, "Find depth generator");

	//procura por um node image nas configuracoes
	nRetVal = g_Context.FindExistingNode(XN_NODE_TYPE_IMAGE, g_ImageGenerator);
	CHECK_RC(nRetVal, "Find image generator");

	//verifica se ha alguma config para User, se não ele cria
	nRetVal = g_Context.FindExistingNode(XN_NODE_TYPE_USER, g_UserGenerator);
	if (nRetVal != XN_STATUS_OK) {
		nRetVal = g_UserGenerator.Create(g_Context);
		CHECK_RC(nRetVal, "Find user generator");
	}

	XnCallbackHandle hUserCallbacks, hCalibrationCallbacks, hPoseCallbacks;

	//define os metodos responsaveis quando os eventos de novo usuario encontrado e usuario perdido ocorrem 
	g_UserGenerator.RegisterUserCallbacks(registerNewUser, registerLostUser, NULL, hUserCallbacks);

	//comecar a ler dados do kinect
	nRetVal = g_Context.StartGeneratingAll();
	CHECK_RC(nRetVal, "StartGenerating");

	glInit(&argc, argv);
	glutMainLoop();

}

