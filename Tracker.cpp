/*****************************************************************************
*                                                                            *
*  OpenNI 1.0 Alpha                                                          *
*  Copyright (C) 2010 PrimeSense Ltd.                                        *
*                                                                            *
*  This file is part of OpenNI.                                              *
*                                                                            *
*  OpenNI is free software: you can redistribute it and/or modify            *
*  it under the terms of the GNU Lesser General Public License as published  *
*  by the Free Software Foundation, either version 3 of the License, or      *
*  (at your option) any later version.                                       *
*                                                                            *
*  OpenNI is distributed in the hope that it will be useful,                 *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              *
*  GNU Lesser General Public License for more details.                       *
*                                                                            *
*  You should have received a copy of the GNU Lesser General Public License  *
*  along with OpenNI. If not, see <http://www.gnu.org/licenses/>.            *
*                                                                            *
*****************************************************************************/




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
#include <signal.h>
#include "SceneDrawer.h"
#include "UserUtil.h"
#include "MessageQueue.h"

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

#define INTERVAL_IN_MILISECONDS 10

int idQueueRequest;
int idQueueResponse;
int faceRecId;

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

//---------------------------------------------------------------------------
// Code
//---------------------------------------------------------------------------
void recognitionCallback(int i) {
	printf(".");
	glutTimerFunc(INTERVAL_IN_MILISECONDS, recognitionCallback, 0);
}

void CleanupExit(){
	g_Context.Shutdown();

	//matando a fila de mensagens
	msgctl(idQueueRequest, IPC_RMID, NULL);
	msgctl(idQueueResponse, IPC_RMID, NULL);
	kill(faceRecId, SIGKILL);

	exit (1);
}

// Callback: New user was detected
void XN_CALLBACK_TYPE User_NewUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie){
	int *pshm, idshm;

	printf("New User %d\n", nId);

	if ((idshm = shmget(0x1223, sizeof(int), IPC_CREAT|0x1ff)) < 0) {
		printf("erro na criacao da fila\n");
		exit(1);
	}

	pshm = (int *) shmat(idshm, (char *)0, 0);
	if (pshm == (int *)-1) {
		printf("erro no attach\n");
		exit(1);
	}


	messageRequest messageReq;
	messageReq.user_id = nId;
	messageReq.id_memoria = idshm;

	xn::SceneMetaData sceneMD;
	generator.GetUserPixels(nId, sceneMD);  
     
  unsigned short *depthPixels;
  char maskPixels[307200];
    
	for (int i =0 ; i < 640 * 480; i++) {  
        if (depthPixels[i] != 0) {  
                      maskPixels[i] = 0;  
        } else {  
              maskPixels[i] = 255;  
        }  
      }
  

	// Create a GUI window for the user to see the camera image.
	cvNamedWindow("Recognition", CV_WINDOW_AUTOSIZE);
	cvMoveWindow("Recognition", 100, 100);


	IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, 1);
    frame->imageData = (char*) maskPixels;
    cvShowImage("Recognition", frame);

  	if(msgsnd(idQueueRequest, &messageReq, sizeof(messageRequest) - sizeof(long), 0) > 0) {
		printf("Erro no envio de mensagem\n");
	} else {
		printf("Mensagem enviada com sucesso\n");
	}

	//printf("Dados: %d\n", metaData->pData);

}
// Callback: An existing user was lost
void XN_CALLBACK_TYPE User_LostUser(xn::UserGenerator& generator, XnUserID nId, void* pCookie){
	printf("Lost user %d\n", nId);
}

// this function is called each frame
void glutDisplay (void){

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

	if (!g_bPause){
		// Read next available data
		g_Context.WaitAndUpdateAll();
	}

		// Process the data
		g_DepthGenerator.GetMetaData(depthMD);
		// g_ImageGenerator.GetMetaData(imageMD);
		g_UserGenerator.GetUserPixels(0, sceneMD);
		DrawDepthMap(depthMD, sceneMD);

	glutSwapBuffers();
}

void glutIdle (void){
	if (g_bQuit) {
		CleanupExit();
	}

	// Display the frame
	glutPostRedisplay();
}

void glutKeyboard (unsigned char key, int x, int y){
	switch (key){
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
	case'p':
		g_bPause = !g_bPause;
		break;
	case'c':
		printUsersCoM(g_UserGenerator, g_SceneAnalyzer);
	}
}
void glInit (int * pargc, char ** argv){
	glutInit(pargc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(GL_WIN_SIZE_X, GL_WIN_SIZE_Y);
	glutCreateWindow ("User Localization in a Smart Space");
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

int main(int argc, char **argv){
	//variavel que mantem o status: caso um erro seja lançado, da para acessa-lo por meio desta
	XnStatus nRetVal = XN_STATUS_OK; 

	if (argc > 1){
		nRetVal = g_Context.Init();
		CHECK_RC(nRetVal, "Init");

		//verifica se passamos algum arquivo pela linha de comando, para que seja gravado a saida do kinect no msm
		nRetVal = g_Context.OpenFileRecording(argv[1]); 
		if (nRetVal != XN_STATUS_OK){
			printf("Can't open recording %s: %s\n", argv[1], xnGetStatusString(nRetVal));
			return 1;
		}
	}
	else{
		xn::EnumerationErrors errors;

		//le as configuracoes do Sample.xml, que define as configuracoes iniciais acima
		nRetVal = g_Context.InitFromXmlFile(SAMPLE_XML_PATH, &errors); 
		
		if (nRetVal == XN_STATUS_NO_NODE_PRESENT){
			XnChar strError[1024];
			errors.ToString(strError, 1024);
			printf("%s\n", strError);
			return (nRetVal);
		}
		else if (nRetVal != XN_STATUS_OK){
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
	if (nRetVal != XN_STATUS_OK){
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

