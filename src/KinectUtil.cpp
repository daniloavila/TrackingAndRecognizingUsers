#include "KinectUtil.h"
#include "Definitions.h"

int kinectMounted = 0;
xn::Context context;
xn::ImageGenerator image;
xn::SceneAnalyzer sceneAnalyzer;

int sharedMemoryCount = 0;

/**
 * Calcula a proxima key da memoria compartilhada.
 */
unsigned int getMemoryKey() {
	int key = SHARED_MEMORY + (sharedMemoryCount++ * 4);
	if(key == MAX_SHARED_MEMORY) {
		sharedMemoryCount = 0;
	}
	return key;
}

/**
 * Obtem ou cria a memoria compartilhada com id passado pelo parametro.
 */
char *getSharedMemory(int memory_id, bool create, int *sharedMemoryId) {
	bool mallocMemory = false;

	if(sharedMemoryId == NULL){
 		sharedMemoryId = (int *) malloc(sizeof(int));
 		mallocMemory = true;
 	}

	unsigned short flag = IPC_CREAT;
	if (!create) {
		flag = IPC_EXCL;
	}

	if ((*sharedMemoryId = shmget(memory_id, sizeof(char) * KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS, flag | 0x1ff)) < 0) {
		// printf("Log - KinectUtil diz: Erro na criacao da memoria - %s\n", create ? "CREATE" : "EXCL");
		return NULL;
	}
	char *maskPixels = (char*) ((shmat(*sharedMemoryId, (char*) ((0)), 0)));
	if (maskPixels == (char*) ((-1))) {
		// printf("Log - KinectUtil diz: Erro no attach da memoria - %s\n", create ? "CREATE" : "EXCL");
		return NULL;
	}

	if(mallocMemory) free(sharedMemoryId);

	return maskPixels;
}

/**
 * Transforma um array de XnRGB24Pixel para um array de char representando a mesma infromação.
 */
char* transformToCharAray(const XnRGB24Pixel* source) {
	if (source == NULL) {
		return NULL;
	}

	char* result = (char *) malloc(KINECT_WIDTH_CAPTURE * KINECT_HEIGHT_CAPTURE * KINECT_NUMBER_OF_CHANNELS * sizeof(char));
	for (int i = 0; i < KINECT_HEIGHT_CAPTURE; i++) {
		for (int j = 0; j < KINECT_WIDTH_CAPTURE; j++) {
			XnRGB24Pixel *aux = (XnRGB24Pixel *) &source[(i * KINECT_WIDTH_CAPTURE) + j];
			result[(i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS) + 2] = aux->nRed;
			result[(i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS) + 1] = aux->nGreen;
			result[(i * KINECT_WIDTH_CAPTURE * KINECT_NUMBER_OF_CHANNELS) + (j * KINECT_NUMBER_OF_CHANNELS) + 0] = aux->nBlue;
		}
	}

	return result;
}

/**
 * Seta o valor de um pixel, que nao seja um pixel do usuario em questao, no vetor com valor ADJUSTED.
 */
void setPixel(unsigned short int *source, int posicaoInicial, int posicaoFinal, int id) {
	for(int i = posicaoInicial; i <= posicaoFinal; i++){
		if(source[i] != id) source[i] = ADJUSTED;
	}
}

/**
 * Expande a area de pixels do usuario com id passado como parametro.
 */
void transformAreaVision(short unsigned int* source, int id) {
	if (source != NULL) {

		for (int i = 0; i < KINECT_HEIGHT_CAPTURE; i++) {
			for (int j = 0; j < KINECT_WIDTH_CAPTURE; j++) {

				if (source[(i * KINECT_WIDTH_CAPTURE) + j] == id) {
					int posicaoEsquerda;
					int posicaoDireita;

					//verifica se a area que sera expandido nao vai extrapolar os limites da imagem
					if(KINECT_WIDTH_CAPTURE - j >  ADJUSTMENT_LEFT){
						posicaoEsquerda = j + ADJUSTMENT_LEFT;
					}else{
						posicaoEsquerda = KINECT_WIDTH_CAPTURE - 1;
					}

					//verifica se a area que sera expandido nao vai extrapolar os limites da imagem
					if(j - ADJUSTMENT_RIGHT <= 0){
						posicaoDireita = 0;
					}else{
						posicaoDireita = j - ADJUSTMENT_RIGHT;
					}

					setPixel(source, i * KINECT_WIDTH_CAPTURE + j, i * KINECT_WIDTH_CAPTURE + posicaoEsquerda, id);
					setPixel(source, i * KINECT_WIDTH_CAPTURE + posicaoDireita, i * KINECT_WIDTH_CAPTURE + j, id);

				}
			}
		}

	}

}

/**
 * Inicia o Kinect
 */
char mountKinect(){
	XnStatus nRetVal = XN_STATUS_OK;

	xn::EnumerationErrors errors;

	//le as configuracoes do Sample.xml, que define as configuracoes iniciais acima
	nRetVal = context.InitFromXmlFile(SAMPLE_XML_PATH_REGISTER, &errors);

	if (nRetVal == XN_STATUS_NO_NODE_PRESENT) {
		XnChar strError[1024];
		errors.ToString(strError, 1024);
		printf("%s\n", strError);
		return false;
	} else if (nRetVal != XN_STATUS_OK) {
		printf("Open failed: %s\n", xnGetStatusString(nRetVal));
		return false;
	}

	nRetVal = sceneAnalyzer.Create(context);

	//procura por um node image nas configuracoes
	nRetVal = context.FindExistingNode(XN_NODE_TYPE_IMAGE, image);
	CHECK_RC(nRetVal, "Find image generator");

	//comecar a ler dados do kinect
	nRetVal = context.StartGeneratingAll();
	CHECK_RC(nRetVal, "StartGenerating");

	return true;
}

/**
 * Retorna um frame do Kinect e inicia o mesmo caso ainda não o tenha sido feito.
 */
IplImage* getKinectFrame() {
	XnStatus nRetVal = XN_STATUS_OK;

	if (!kinectMounted) {
		if(!mountKinect()) {
			return NULL;
		}
		kinectMounted = 1;
	}

	// Wait for new data to be available
	nRetVal = context.WaitOneUpdateAll(image);
	if (nRetVal != XN_STATUS_OK) {
		printf("Falha ao atualizar os dados: %s\n", xnGetStatusString(nRetVal));
		return NULL;
	}
	IplImage* frame = cvCreateImage(cvSize(KINECT_HEIGHT_CAPTURE, KINECT_WIDTH_CAPTURE), IPL_DEPTH_8U, KINECT_NUMBER_OF_CHANNELS);
	char* result = transformToCharAray(image.GetRGB24ImageMap());
	frame->imageData = result;
	return frame;
}
