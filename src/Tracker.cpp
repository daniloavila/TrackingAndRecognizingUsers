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

int idQueueRequest;
int idQueueResponse;
int idQueueComunicationJavaC;
int faceRecId;

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

map<int, UserStatus> users;

#ifdef SAVE_LOG
	FILE *trackerLogFile;
	char trackerLogFileName[255];
	char trackerLogFolderFrames[255];
#endif

#ifdef DEBUG
	FILE *loggerFile;
#endif

void verifyDeslocationObject(int userId);
void requestRecognition(int id);
void glInit(int * pargc, char ** argv);

void lostTrackerSignals();
void getTrackerSignals();

#ifdef SAVE_LOG
	void saveLogNewUser(XnUserID nId);
	void saveLogUserLost(XnUserID nId);
#endif

#ifdef JAVA_INTEGRATION
	void sendChoice(int id, MessageType type, char* last_name);
#endif

/**
 * Recebe as mensagens da fila de resposta e reenvia as frames dos usuarios não reconhecidos.
 */
void treatQueueResponse(int i) {
	MessageResponse messageResponse;

	printLogConsole("Log - Tracker diz: . TREAT QUEUE RESPONSE . \n", "tALES");

	while (msgrcv(idQueueResponse, &messageResponse, sizeof(MessageResponse) - sizeof(long), 0, IPC_NOWAIT) >= 0) {

		printLogConsole("---------------------------------------------\n");

		printLogConsole("Log - Tracker diz: Recebi mensagem de usuario reconhecido. user => %ld - '%s' - %f\n", messageResponse.user_id, messageResponse.user_name,
				messageResponse.confidence);

		// verifica se o usuário ainda está sendo trackeado.
		if (users.find(messageResponse.user_id) == users.end()) {
			printLogConsole("Log - Tracker diz: Usuário não está mais sendo rastreado\n", "TALES");
			continue;
		}

		int total = getTotalAttempts(messageResponse.user_id);

		// verifica se é uma label válida
		if (messageResponse.user_name == NULL || strlen(messageResponse.user_name) == 0) {
			printLogConsole("Log - Tracker diz: Nome está vazio\n");
			//  adicionado pois superlotava a fila de mensagens
			if (total < ATTEMPTS_INICIAL_RECOGNITION) {
				requestRecognition(messageResponse.user_id);
				verifyDeslocationObject(messageResponse.user_id);
			}
			continue;
		} else if(strcmp(messageResponse.user_name, UNKNOWN) == 0) {
			if(users[messageResponse.user_id].name == NULL || strlen(users[messageResponse.user_id].name) == 0) {
				users[messageResponse.user_id].name = (char *) malloc(strlen(UNKNOWN) + 1);
				strcpy(users[messageResponse.user_id].name, UNKNOWN);

				#ifdef JAVA_INTEGRATION
					sendChoice(messageResponse.user_id, NEW_USER, NULL);
				#endif
			}

			if (total < ATTEMPTS_INICIAL_RECOGNITION) {
				requestRecognition(messageResponse.user_id);
				verifyDeslocationObject(messageResponse.user_id);
			}
			continue;
		}

		#ifdef JAVA_INTEGRATION
			char last_name[255];
			if(users[messageResponse.user_id].name != NULL)
				strcpy(last_name, users[messageResponse.user_id].name);
		#endif

		calculateNewStatistics(&messageResponse);

		choiceNewLabelToUser(&messageResponse, &users);

		#ifdef JAVA_INTEGRATION
			if(strcmp(last_name, UNKNOWN) == 0) {
				sprintf(last_name, "%s_%d", UNKNOWN, messageResponse.user_id);
			}

			if(total == 0 && strncmp(last_name, UNKNOWN, strlen(UNKNOWN)) != 0){
				sendChoice(messageResponse.user_id, NEW_USER, NULL);
			}else{
				sendChoice(messageResponse.user_id, RECHECK, last_name);
			}
		#endif

		// Calcula o total de vezes que o usuario foi reconhecido. Independentemente da resposta.
		printLogConsole("Log - Tracker diz: Total de tentativas de reconhecimento do usuario %ld => %d\n", messageResponse.user_id, total);

		if (total < ATTEMPTS_INICIAL_RECOGNITION) {
			requestRecognition(messageResponse.user_id);
		}
	}

	printLogConsole("---------------------------------------------\n");

	glutTimerFunc(INTERVAL_IN_MILISECONDS_TREAT_RESPONSE, treatQueueResponse, 0);
}

/**
 * Pede para reconhecer todos os usuários.
 */
void recheckUsers(int i) {
	map<int, UserStatus>::iterator it;

	printLogConsole("Log - Tracker diz: - RECHECK - \n");

	for (it = users.begin(); it != users.end(); it++) {
		if (getTotalAttempts((*it).first) >= ATTEMPTS_INICIAL_RECOGNITION) {
			requestRecognition((*it).first);
			verifyDeslocationObject((*it).first);
		}
	}

	glutTimerFunc(INTERVAL_IN_MILISECONDS_RECHECK, recheckUsers, 0);
}

/**
 * sendtra a entrada de um novo usuário e pede para o mesmo ser reconhecido.
 */
void XN_CALLBACK_TYPE registerNewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	int id = (int) nId;

	printLogConsole("Log - Tracker diz: Novo usuário detectado %d\n", id);
	users[id].name = (char *) malloc(sizeof(char));
	users[id].name[0] = '\0';
	users[id].canRecognize = true;
	requestRecognition(id);
	verifyDeslocationObject(id);

	#ifdef SAVE_LOG
		saveLogNewUser(nId);
	#endif
}

/**
 * Apaga a entrada do novo usuário.
 */
void XN_CALLBACK_TYPE registerLostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	printLogConsole("Log - Tracker diz: Usuário %d perdido\n", nId);

	#ifdef SAVE_LOG
		saveLogUserLost(nId);
	#endif

	#ifdef JAVA_INTEGRATION
		if(users[nId].name != NULL && strlen(users[nId].name) > 0 && strcmp(users[nId].name, "Object") != 0)
			sendChoice(nId, LOST_USER, NULL);
	#endif

	users.erase((int) nId);
	statisticsClear((int) nId);
}

void cleanupQueueAndExit(int i) {
	lostTrackerSignals();

	printLogConsole("\n SIGNAL => %d\n", i);

	g_Context.Shutdown();

	//matando a fila de mensagens que o tracker recebe respostas
	msgctl(idQueueResponse, IPC_RMID, NULL);
	kill(faceRecId, SIGUSR1);

#ifdef DEBUG
	printfLogComplete(&users, stdout);
#endif

#ifdef SAVE_LOG
	fprintf(trackerLogFile, "\n\n");
	printfLogComplete(&users, trackerLogFile);
	fflush(trackerLogFile);

	printLogConsole("\n###################################\n");
	printLogConsole("### Nome do arquivo de log: %s\n", trackerLogFileName);
	printLogConsole("### Nome da pasta com as frames dos usuários: %s\n", trackerLogFolderFrames);
	printLogConsole("### Acompanhe o log para mais detalhes.\n");
	printLogConsole("###################################\n");
	fflush(stdout);
	fclose(trackerLogFile);
#endif

	exit(EXIT_SUCCESS);
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
			fprintf(stderr, "Can't open recording %s: %s\n", argv[1], xnGetStatusString(nRetVal));
			return 1;
		}
	} else {
		xn::EnumerationErrors errors;

		//le as configuracoes do Sample.xml, que define as configuracoes iniciais acima
		nRetVal = g_Context.InitFromXmlFile(SAMPLE_XML_PATH, &errors);

		if (nRetVal == XN_STATUS_NO_NODE_PRESENT) {
			XnChar strError[1024];
			errors.ToString(strError, 1024);
			fprintf(stderr, "%s\n", strError);
			return (nRetVal);
		} else if (nRetVal != XN_STATUS_OK) {
			fprintf(stderr, "Open failed: %s\n", xnGetStatusString(nRetVal));
			return (nRetVal);
		}
		errors.~EnumerationErrors();
	}

	idQueueRequest = createMessageQueue(MESSAGE_QUEUE_REQUEST);
	idQueueResponse = createMessageQueue(MESSAGE_QUEUE_RESPONSE);
	idQueueComunicationJavaC = createMessageQueue(MESSAGE_QUEUE_COMUNICATION_JAVA_C);

	users.clear();
	statisticsClearAll();

	//criando um processo filho. Este processo sera transformado do deamon utilizando o execl
	faceRecId = fork();
	if (faceRecId < 0) {
		fprintf(stderr, "Erro ao tentar criar o processo 'Recognizer' atraves do fork.\n");
		exit(1);
	}

	//inicinado o processo que reconhece os novoc usuarios encontrados
	if (faceRecId == 0) {
		execl("recognizer", "recognizer", (char *) 0);
	}

	nRetVal = g_SceneAnalyzer.Create(g_Context);

	#ifdef SAVE_LOG
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
		printLogConsole("Iniciado em: %s\n", asctime(local));
		printLogConsole("Nome do arquivo de log: %s\n", trackerLogFileName);
		printLogConsole("Nome da pasta com as frames dos usuários: %s\n", trackerLogFolderFrames);

		fclose(trackerLogFile);
		trackerLogFile = fopen(trackerLogFileName, "a+");
	#endif

	#ifdef DEBUG
		loggerFile = fopen("LOG/log.out", "w");
	#endif

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

	getTrackerSignals();

	glInit(&argc, argv);
	glutMainLoop();

	printLogConsole("FIM TRACKER");

	return EXIT_SUCCESS;
}

void reexecute(int signal) {
	cleanupQueueAndExit(signal);
	execl("tracker", "tracker", (char *) 0);
}


// ##########################################################
// ##########################################################
// ####                       Util                       ####
// ##########################################################
// ##########################################################

void getTrackerSignals() {
	signal(SIGINT, cleanupQueueAndExit);
	signal(SIGQUIT, cleanupQueueAndExit);
	signal(SIGILL, cleanupQueueAndExit);
	signal(SIGTRAP, cleanupQueueAndExit);
	signal(SIGABRT, cleanupQueueAndExit);
	signal(SIGKILL, cleanupQueueAndExit);
	signal(SIGSEGV, reexecute);
	signal(SIGTERM, cleanupQueueAndExit);
	signal(SIGSYS, cleanupQueueAndExit);
}

void lostTrackerSignals() {
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

	printLogConsole("Log - Tracker diz: Enviando pedido de reconhecimento. user_id = %ld e id_memoria = %x\n", messageRequest.user_id, messageRequest.memory_id);

	if (msgsnd(idQueueRequest, &messageRequest, sizeof(MessageRequest) - sizeof(long), 0) > 0) {
		fprintf(stderr, "Erro no envio de mensagem para o recognizer do usuário %d\n", messageRequest.memory_id);
	}
}

#ifdef JAVA_INTEGRATION
void sendChoice(int id, MessageType type,char *last_name) {
	MessageEvents messageEvent;
	XnPoint3D com;
	g_UserGenerator.GetCoM(id, com);

	strcpy(messageEvent.user_name, users[id].name);
	if(strcmp(users[id].name, UNKNOWN) == 0) {
		sprintf(messageEvent.user_name, "%s_%d", messageEvent.user_name, id);
	}
	messageEvent.confidence = users[id].confidence;

	if(type == LOST_USER){
		messageEvent.x = 0;
		messageEvent.y = 0;
		messageEvent.z = 0;	
	}else{
		messageEvent.x = com.X;
		messageEvent.y = com.Y;
		messageEvent.z = com.Z;	
	}
	
	if(last_name != NULL && strlen(last_name) > 0) {
		strcpy(messageEvent.last_name, last_name);
	} else {
		strcpy(messageEvent.last_name, "");
	}

	messageEvent.type = type;

	printLogConsole("Log - Tracker diz: Enviando escolha para o JAVA.\n");

	if (msgsnd(idQueueComunicationJavaC, &messageEvent, sizeof(messageEvent) - sizeof(long), 0) > 0) {
		printLogConsole("Log - Tracker diz: Erro no envio de mensagem para o JAVA.\n");
	}
}
#endif

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

		if (variacaoX < MIN_OBJECT_DESLOCATION || variacaoY < MIN_OBJECT_DESLOCATION || variacaoZ < MIN_OBJECT_DESLOCATION) {
			(*deslocationStatus).numberTimesNotMoved = (*deslocationStatus).numberTimesNotMoved + 1;
		} else {
			(*deslocationStatus).numberTimesNotMoved = 0;
			(*deslocationStatus).numberTimesMoved = (*deslocationStatus).numberTimesMoved + 1;
		}

		if ((*deslocationStatus).numberTimesNotMoved > MAX_TIMES_OF_OBJECT_NO_DESLOCATION && strlen(users[userId].name) == 0) {
			users[userId].canRecognize = false;
			users[userId].name = (char *) malloc(sizeof(char) * (strlen(OBJECT) + 1));
			strcpy(users[userId].name, OBJECT);

			#ifdef SAVE_LOG
				struct tm *local;
				time_t t;
				t = time(NULL);
				local = localtime(&t);
				fprintf(trackerLogFile, "%s \t Usuário %d não é um usuário reconhecível.\n", asctime(local), userId);
			#endif
		}

		(*deslocationStatus).lastPosition = com;
	}
//	printLogConsole("Log - Tracker diz: - #######################\n");
}

#ifdef SAVE_LOG
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

#endif

// this function is called each frame
void glutDisplay(void) {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Setup the OpenGL viewpoint
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	xn::SceneMetaData sceneMD;
	xn::DepthMetaData depthMD;

		g_Context.WaitAndUpdateAll();

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
	// Display the frame
	glutPostRedisplay();
}

void glutKeyboard(unsigned char key, int x, int y) {
	switch (key) {
	case 27:
		cleanupQueueAndExit(0);
		break;
	}
}
void glInit(int * pargc, char ** argv) {
	glutInit(pargc, argv);

//	#ifndef JAVA_INTEGRATION
		glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
		glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
		glutCreateWindow("User Localization in a Smart Space");
		//glutFullScreen();
		//glutSetCursor(GLUT_CURSOR_NONE);

		glutKeyboardFunc(glutKeyboard); //define acoes para determinandas teclas
		glutDisplayFunc(glutDisplay);
		glutIdleFunc(glutIdle);

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);

		glEnableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
//	#endif

	glutTimerFunc(INTERVAL_IN_MILISECONDS_TREAT_RESPONSE, treatQueueResponse, 0);
	glutTimerFunc(INTERVAL_IN_MILISECONDS_RECHECK, recheckUsers, 0);
}
