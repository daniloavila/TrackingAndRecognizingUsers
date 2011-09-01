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
#include <time.h>

#include <iostream>
#include <map>
#include <string>

#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>

#include "SceneDrawer.h"
#include "KinectUtil.h"
#include "MessageQueue.h"
#include "StatisticsUtil.h"

#include "Definitions.h"

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

int idQueueRequest;
int idQueueResponse;
int faceRecId;

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

XnBool g_bPause = false;
XnBool g_bRecord = false;
XnBool g_bQuit = false;

map<int, UserStatus> users;

FILE *trackerLogFile;
char trackerLogFileName[255];
char trackerLogFolderFrames[255];

void verifyDeslocationObject(int userId);
void requestRecognition(int id);
void saveLogNewUser(XnUserID nId);
void saveLogUserLost(XnUserID nId);
void glInit(int * pargc, char ** argv);

/**
 * Recebe as mensagens da fila de resposta e reenvia as frames dos usuarios não reconhecidos.
 */
void treatQueueResponse(int i) {
	MessageResponse messageResponse;

	printf("Log - Tracker diz: . TREAT QUEUE RESPONSE . \n");

	while (msgrcv(idQueueResponse, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0, IPC_NOWAIT) >= 0) {

		printf("---------------------------------------------\n");

		printf("Log - Tracker diz: Recebi mensagem de usuario reconhecido. user => %ld - '%s' - %f\n", messageResponse.user_id, messageResponse.user_name,
				messageResponse.confidence);

		// verifica se o usuário ainda está sendo trackeado.
		if (users.find(messageResponse.user_id) == users.end()) {
			printf("Log - Tracker diz: Usuário não está mais sendo rastreado\n");
			continue;
		}

		int total = getTotalAttempts(messageResponse.user_id);

		// verifica se é uma label válida
		if (messageResponse.user_name == NULL || strlen(messageResponse.user_name) == 0) {
			printf("Log - Tracker diz: Nome está vazio\n");
			//  adicionado pois superlotava a fila de mensagens
			if (total < ATTEMPTS_INICIAL_RECOGNITION) {
				requestRecognition(messageResponse.user_id);
				verifyDeslocationObject(messageResponse.user_id);
			}
			continue;
		} else if(strcmp(messageResponse.user_name, UNKNOWN) == 0) {
            if(total == 1) {
            	users[messageResponse.user_id].name = (char *) malloc(strlen(UNKNOWN) + 1);
            	strcpy(users[messageResponse.user_id].name, UNKNOWN);
            }

			if (total < ATTEMPTS_INICIAL_RECOGNITION) {
				requestRecognition(messageResponse.user_id);
				verifyDeslocationObject(messageResponse.user_id);
			}
			continue;
		}

		calculateNewStatistics(&messageResponse);

		choiceNewLabelToUser(&messageResponse, &users);

		// Calcula o total de vezes que o usuario foi reconhecido. Independentemente da resposta.
		printf("Log - Tracker diz: Total de tentativas de reconhecimento do usuario %ld => %d\n", messageResponse.user_id, total);

		if (total < ATTEMPTS_INICIAL_RECOGNITION) {
			requestRecognition(messageResponse.user_id);
		}
	}

	printf("---------------------------------------------\n");
	fflush(stdout);

	glutTimerFunc(INTERVAL_IN_MILISECONDS_TREAT_RESPONSE, treatQueueResponse, 0);
}

/**
 * Pede para reconhecer todos os usuários.
 */
void recheckUsers(int i) {
	map<int, UserStatus>::iterator it;

	printf("Log - Tracker diz: - RECHECK - \n");
	fflush(stdout);

	for (it = users.begin(); it != users.end(); it++) {
		if (getTotalAttempts((*it).first) >= ATTEMPTS_INICIAL_RECOGNITION) {
			requestRecognition((*it).first);
			verifyDeslocationObject((*it).first);
		}
	}

	glutTimerFunc(INTERVAL_IN_MILISECONDS_RECHECK, recheckUsers, 0);
}

/**
 * Registra a entrada de um novo usuário e pede para o mesmo ser reconhecido.
 */
void XN_CALLBACK_TYPE registerNewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	int id = (int) nId;

	printf("Log - Tracker diz: Novo usuário detectado %d\n", id);
	users[id].name = "";
	users[id].canRecognize = true;
	requestRecognition(id);
	verifyDeslocationObject(id);
	saveLogNewUser(nId);
}

/**
 * Apaga a entrada do novo usuário.
 */
void XN_CALLBACK_TYPE registerLostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	printf("Log - Tracker diz: Usuário %d perdido\n", nId);

	saveLogUserLost(nId);

	users.erase((int) nId);
	statisticsClear((int) nId);
}

void cleanupQueueAndExit() {
	g_Context.Shutdown();

	//matando a fila de mensagens
	msgctl(idQueueResponse, IPC_RMID, NULL);
	kill(faceRecId, SIGUSR1);

	printfLogComplete(&users, stdout);
	fprintf(trackerLogFile, "\n\n");
	printfLogComplete(&users, trackerLogFile);

	printf("\n###################################\n");
	printf("### Nome do arquivo de log: %s\n", trackerLogFileName);
	printf("### Nome da pasta com as frames dos usuários: %s\n", trackerLogFolderFrames);
	printf("### Acompanhe o log para mais detalhes.\n");
	printf("###################################\n");

	fflush(stdout);
	fflush(trackerLogFile);

	fclose(trackerLogFile);

	exit(1);
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
		errors.~EnumerationErrors();
	}

	idQueueRequest = createMessageQueue(MESSAGE_QUEUE_REQUEST);
	idQueueResponse = createMessageQueue(MESSAGE_QUEUE_RESPONSE);

	users.clear();
	statisticsClearAll();

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

	mkdir(LOG_FOLDER, 0777);

	struct tm *local;
	time_t t;
	t = time(NULL);
	local = localtime(&t);

	sprintf(trackerLogFileName, "%s%s_%02d-%02d-%04d_%02d:%02d.log", LOG_FOLDER, NAME_OF_LOG_FILE, local->tm_mday, local->tm_mon + 1, local->tm_year + 1900, local->tm_hour,
			local->tm_min);

	sprintf(trackerLogFolderFrames, "%s%s_%02d-%02d-%04d_%02d:%02dFrames/", LOG_FOLDER, NAME_OF_LOG_FILE, local->tm_mday, local->tm_mon + 1, local->tm_year + 1900, local->tm_hour,
			local->tm_min);
	mkdir(trackerLogFolderFrames, 0777);

	trackerLogFile = fopen(trackerLogFileName, "w+");

	fprintf(trackerLogFile, "Iniciado em: %s\n", asctime(local));
	printf("Iniciado em: %s\n", asctime(local));
	printf("Nome do arquivo de log: %s\n", trackerLogFileName);
	printf("Nome da pasta com as frames dos usuários: %s\n", trackerLogFolderFrames);

	fclose(trackerLogFile);
	trackerLogFile = fopen(trackerLogFileName, "a+");

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



// ##########################################################
// ##########################################################
// ####                       Util                       ####
// ##########################################################
// ##########################################################



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

	transformAreaVision(scenePixels, nId);

	for (int i = 0; i < KINECT_HEIGHT_CAPTURE; i++) {
		for (int j = 0; j < KINECT_WIDTH_CAPTURE; j++) {
			int index = (i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS);
			if (scenePixels[(i * KINECT_WIDTH_CAPTURE) + j] == nId || scenePixels[(i * KINECT_WIDTH_CAPTURE) + j] == ADJUSTED) {
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

	sceneMD.~OutputMetaData();
	free(result);
}

/**
 * Pede para o processo de reconhecimento reconhecer o usuario com esse id passado.
 */
void requestRecognition(int id) {
	// Testa se o usuário tem permissão para continuar sendo reconhecido
	if(!users[id].canRecognize) {
		return;
	}

	MessageRequest messageRequest;

	messageRequest.user_id = id;
	messageRequest.memory_id = getMemoryKey();

	char *maskPixels = getSharedMemory(messageRequest.memory_id, true, NULL);

	// Busca a area da imagem onde o usuario está.
	getFrameFromUserId((XnUserID) id, maskPixels);

	shmdt(maskPixels);

	printf("Log - Tracker diz: Enviando pedido de reconhecimento. user_id = %ld e id_memoria = %x\n", messageRequest.user_id, messageRequest.memory_id);

	if (msgsnd(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0) > 0) {
		printf("Log - Tracker diz: Erro no envio de mensagem para o usuário %d\n", messageRequest.memory_id);
	}
}

/**
 * Verifica se o objeto deslocou ou não da ultima vez que foi chamado e caso perceba que o usuário não está se deslocando e não está sendo reconhecido retira a sua permissão para deslocar.
 */
void verifyDeslocationObject(int userId) {
	XnPoint3D com;
	g_UserGenerator.GetCoM(userId, com);
	DeslocationStatus *deslocationStatus = &(users[userId].deslocationStatus);
	if ((*deslocationStatus).lastPosition.X == 0 && (*deslocationStatus).lastPosition.Y && (*deslocationStatus).lastPosition.Z) {
		(*deslocationStatus).lastPosition = com;
	} else {
		XnFloat variacaoX, variacaoY, variacaoZ;

		variacaoX = fabs((*deslocationStatus).lastPosition.X - com.X);
		variacaoY = fabs((*deslocationStatus).lastPosition.Y - com.Y);
		variacaoZ = fabs((*deslocationStatus).lastPosition.Z - com.Z);

//		printf("Log - Tracker diz: - #######################\n");
//		printf("Log - Tracker diz: - Usuário %d.\n", userId);
//		printf("Log - Tracker diz: - Variação: X = %f - Y = %f - Z = %f.", variacaoX, variacaoY, variacaoX);

		if (variacaoX < MIN_OBJECT_DESLOCATION || variacaoY < MIN_OBJECT_DESLOCATION || variacaoZ < MIN_OBJECT_DESLOCATION) {
//			printf("IF\n");
			(*deslocationStatus).numberTimesNotMoved = (*deslocationStatus).numberTimesNotMoved + 1;
		} else {
//			printf("ELSE\n");
			(*deslocationStatus).numberTimesNotMoved = 0;
			(*deslocationStatus).numberTimesMoved = (*deslocationStatus).numberTimesMoved + 1;
		}

		printf("Log - Tracker diz: - Numero de vezes sem locomover: %d.\n", (*deslocationStatus).numberTimesNotMoved);
		printf("Log - Tracker diz: - Numero de vezes que se locomoveu: %d.\n", (*deslocationStatus).numberTimesMoved);
		if ((*deslocationStatus).numberTimesNotMoved > MAX_TIMES_OF_OBJECT_NO_DESLOCATION && strlen(users[userId].name) == 0) {
			users[userId].canRecognize = false;
			users[userId].name = (char *) malloc(sizeof(char) * (strlen(OBJECT) + 1));
			strcpy(users[userId].name, OBJECT);
			printf("Log - Tracker diz: - Usuário %d não é um usuário reconhecivel ###.\n", userId);

			struct tm *local;
			time_t t;
			t = time(NULL);
			local = localtime(&t);
			fprintf(trackerLogFile, "%s \t Usuário %d não é um usuário reconhecível.\n", asctime(local), userId);
		}

		(*deslocationStatus).lastPosition = com;
	}
//	printf("Log - Tracker diz: - #######################\n");
}

/**
 * Salva um log de usuário perdido.
 */
void saveLogUserLost(XnUserID nId) {
	struct tm *local;
	time_t t;
	t = time(NULL);
	local = localtime(&t);
	fprintf(trackerLogFile, "%s \tUsuário %d PERDIDO\n", asctime(local), nId);

	printfLogCompleteByUser((int) nId, &users, trackerLogFile, 2);
}

/**
 * Salva um log de usuário detectado.
 */
void saveLogNewUser(XnUserID nId) {
	char cstr[255];

	struct tm *local;
	time_t t;
	t = time(NULL);
	local = localtime(&t);
	fprintf(trackerLogFile, "%s \tUsuário %d DETECTADO\n", asctime(local), nId);
	char *maskPixels = (char *) malloc(sizeof(char) * KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS);
	// Busca a area da imagem onde o usuario está.
	getFrameFromUserId((XnUserID) nId, maskPixels);

	IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
	frame->imageData = maskPixels;

	sprintf(cstr, "%suser%d_%02d-%02d-%04d_%02d:%02d.pgm", trackerLogFolderFrames, (int) nId, local->tm_mday, local->tm_mon + 1, local->tm_year + 1900, local->tm_hour,
			local->tm_min);
	cvSaveImage(cstr, frame);
	cvReleaseImage(&frame);

	fprintf(trackerLogFile, "\tFrame do usuário: ");
	fprintf(trackerLogFile, "%s", cstr);
	fprintf(trackerLogFile, "\n");
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

	if (!g_bPause) {
		// Read next available data
		// printf("\t\t\tVAI DA PAU\n");
		g_Context.WaitAndUpdateAll();
		// printf("\t\t\tNAO DEU PAU\n");
	}

	// Process the data
	g_DepthGenerator.GetMetaData(depthMD);
	glOrtho(0, depthMD.XRes(), depthMD.YRes(), 0, -1.0, 1.0);

	glDisable(GL_TEXTURE_2D);

	g_UserGenerator.GetUserPixels(0, sceneMD);
	DrawDepthMap(depthMD, sceneMD, &users);

	sceneMD.~OutputMetaData();
	depthMD.~OutputMetaData();

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
