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

#define INTERVAL_IN_MILISECONDS 1000

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
map<int, float> usersConfidence;

void getFrameFromUserId(XnUserID nId, char *maskPixels);

/**
 * Calcula a proxima key da memoria compartilhada.
 */
unsigned int getMemoryKey() {
	return SHARED_MEMORY + (sharedMemoryCount * 4);
}

/**
 * Recebe as mensagens da fila de resposta e reenvia as frames dos usuarios não reconhecidos.
 */
void recognitionCallback(int i) {
	map<int, string>::iterator it;

	char nome[7];
	MessageResponse messageResponse;

	printf(".\n");

	if (msgrcv(idQueueResponse, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0, IPC_NOWAIT) >= 0) {
		users[messageResponse.user_id] = (char *) malloc(sizeof(char) * strlen(messageResponse.user_name));
		usersConfidence[messageResponse.user_id] = messageResponse.confidence;
		strcpy(users[messageResponse.user_id], messageResponse.user_name);

		printf("Recebeu mensagem de usuario reconhecido - user => %d - %s - %f\n", messageResponse.user_id, users[messageResponse.user_id], messageResponse.confidence);

		// TODO : Verificação removida para que fique sempre mandado verificar no banco de dados.
		// if (strlen(messageResponse.user_name) == 0) {
		int id = messageResponse.user_id;

		MessageRequest messageRequest;

		printf("User %d\n", id);

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

//		}
	}

	// Criar loop para reenviar o n
	/*	for (it = users.begin(); it != users.end(); it++) {
	 if ((*it).second.length() == 0) {

	 int id = (*it).first;

	 MessageRequest messageRequest;

	 printf("User %d\n", id);

	 messageRequest.user_id = id;
	 messageRequest.memory_id = getMemoryKey();
	 char *maskPixels = getSharedMemory(messageRequest.memory_id);

	 // Busca a area da imagem onde o usuario está.
	 getFrameFromUserId(id, maskPixels);

	 if (msgsnd(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0) > 0) {
	 printf("Erro no envio de mensagem para o usuario %d\n", messageRequest.memory_id);
	 }

	 sharedMemoryCount++;
	 }
	 }*/

	glutTimerFunc(INTERVAL_IN_MILISECONDS, recognitionCallback, 0);
}

void CleanupExit() {
	g_Context.Shutdown();

	//matando a fila de mensagens
	msgctl(idQueueRequest, IPC_RMID, NULL);
	msgctl(idQueueResponse, IPC_RMID, NULL);
	kill(faceRecId, SIGKILL);

	exit(1);
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

// Callback: New user was detected
void XN_CALLBACK_TYPE User_NewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	int sharedMemoryId;
	MessageRequest messageRequest;

	int id = (int) (nId);
	printf("New User %d\n", nId);

	users[id] = "";

	messageRequest.user_id = id;
	messageRequest.memory_id = getMemoryKey();

	char *maskPixels = getSharedMemory(messageRequest.memory_id);

	// Busca a area da imagem onde o usuario está.
	getFrameFromUserId(nId, maskPixels);

	printf("Enviando pedido de reconhecimento -> user_id = %d id_memoria = %d\n", messageRequest.user_id, messageRequest.memory_id);

	if (msgsnd(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0) > 0) {
		printf("Erro no envio de mensagem para o usuario %d\n", messageRequest.memory_id);
	}

	sharedMemoryCount++;
}

// Callback: An existing user was lost
void XN_CALLBACK_TYPE User_LostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	printf("Lost user %d\n", nId);
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
		CleanupExit();
	}

	// Display the frame
	glutPostRedisplay();
}

void glutKeyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 27:
		CleanupExit();
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

	glutTimerFunc(INTERVAL_IN_MILISECONDS, recognitionCallback, 0);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

#define SAMPLE_XML_PATH "Config/SamplesConfig.xml"

#define CHECK_RC(nRetVal, what) \
	if (nRetVal != XN_STATUS_OK){\
		printf("%s failed: %s\n", what, xnGetStatusString(nRetVal));\
		return nRetVal; \
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
		execl("recognition", "recognition", (char *) 0);
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
	g_UserGenerator.RegisterUserCallbacks(User_NewUser, User_LostUser, NULL, hUserCallbacks);

	// if(g_DepthGenerator.IsCapabilitySupported("AlternativeViewPoint")){ 
	//   nRetVal = g_DepthGenerator.GetAlternativeViewPointCap().SetViewPoint(g_ImageGenerator); 
	//   CHECK_RC(nRetVal, "SetViewPoint for depth generator"); 
	// } 

	//comecar a ler dados do kinect
	nRetVal = g_Context.StartGeneratingAll();
	CHECK_RC(nRetVal, "StartGenerating");

	glInit(&argc, argv);
	glutMainLoop();

}

