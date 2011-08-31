#include <stdio.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <string.h>
#include <opencv/cv.h>
#include <opencv/cvaux.h>
#include <opencv/highgui.h>
#include <XnCppWrapper.h>
#include <sys/shm.h>

#include <unistd.h>
#include <cstdlib>
#include <csignal>

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "KeyboardUtil.h"
#include "ImageUtil.h"
#include "KinectUtil.h"
#include "Definitions.h"

using namespace std;

// Haar Cascade file, usado na deteccao
const char *frontalFaceCascadeFilename = HAARCASCADE_FRONTALFACE;

//Mudar para 0 se voce nao querer que os eigevectors sejam armazenados em arquivos
int SAVE_EIGENFACE_IMAGES = 1; 

/******************* VARIAVEIS GLOBAIS ************************/
IplImage ** faceImgArr = 0; // array de imagens facial
CvMat * personNumTruthMat = 0; // array dos numeros das pessoas
vector<string> personNames; // array dos nomes das pessoas (idexado pelo numero da pessoa).
int faceWidth = 120; // dimensoes default das faces no banco de  faces
int faceHeight = 90; //	"		"		"		"		"		"		"		"
int nPersons = 0; // numero de pessoas no cenario de treino
int nTrainFaces = 0; // numero de imagens de trienamento
int nEigens = 0; // numero de eigenvalues
IplImage * pAvgTrainImg = 0; // a imagem media
IplImage ** eigenVectArr = 0; // eigenvectors
CvMat * eigenValMat = 0; // eigenvalues
CvMat * projectedTrainFaceMat = 0; // projected training faces

CvCapture* camera = 0; 

void learn(char *szFileTrain);
void doPCA();
void storeTrainingData();
int loadTrainingData(CvMat ** pTrainPersonNumMat);
int findNearestNeighbor(float * projectedTestFace, float *pConfidence);
int loadFaceImgArray(char * filename);
void recognizeFromCam(void);
CvMat* retrainOnline(void);

int main(int argc, char** argv) {
	printf("register\n");

	if (argc >= 2 && strcmp(argv[1], "train") == 0) {
		learn("Eigenfaces/train.txt");
	} else {
		recognizeFromCam();
	}
}

// salva todos os eigenvectors
void storeEigenfaceImages() {
	//armazena a imagem media em um arquivo
	printf("Salvando a imagem da face media como 'out_averageImage.bmp'.\n");
	cvSaveImage("Eigenfaces/out_averageImage.bmp", pAvgTrainImg);

	//cria uma nova imagem feita de varias imagens eigenfaces 
	printf("Salvando o eigenvector %d como 'out_eigenfaces.bmp'\n", nEigens);
	if (nEigens > 0) {
		//coloca todas imagens uma do lado da outra
		int COLUMNS = 8; // poe 8 imagens em uma linha
		int nCols = min(nEigens, COLUMNS);
		int nRows = 1 + (nEigens / COLUMNS); 
		int w = eigenVectArr[0]->width;
		int h = eigenVectArr[0]->height;
		CvSize size;
		size = cvSize(nCols * w, nRows * h);
		IplImage *bigImg = cvCreateImage(size, IPL_DEPTH_8U, 1); // 8-bit Greyscale UCHAR image
		for (int i = 0; i < nEigens; i++) {
			// pega a imagem eigenface
			IplImage *byteImg = convertFloatImageToUcharImage(eigenVectArr[i]);
			// coloca ela na posicao correta
			int x = w * (i % COLUMNS);
			int y = h * (i / COLUMNS);
			CvRect ROI = cvRect(x, y, w, h);
			cvSetImageROI(bigImg, ROI);
			cvCopyImage(byteImg, bigImg);
			cvResetImageROI(bigImg);
			cvReleaseImage(&byteImg);
		}
		cvSaveImage("Eigenfaces/out_eigenfaces.bmp", bigImg);
		cvReleaseImage(&bigImg);
	}
}

//armazena os dados treinados em um arquivo facedata.xml
void learn(char *szFileTrain) {
	int i, offset;

	// le os dados de treinamento
	printf("Lendo imagens de treinamento em '%s'\n", szFileTrain);
	nTrainFaces = loadFaceImgArray(szFileTrain);
	printf("%d imagens de treino obtidas.\n", nTrainFaces);
	if (nTrainFaces < 2) {
		fprintf(stderr, "Necessario 2 ou mais faces de trieno\n"
				"Arquivo de entrada contem somente %d\n", nTrainFaces);
		return;
	}

	// faz o PCA nas faces de treinamento
	doPCA();

	// projeta as imagens de trienamento no subespaco PCA
	projectedTrainFaceMat = cvCreateMat(nTrainFaces, nEigens, CV_32FC1);
	offset = projectedTrainFaceMat->step / sizeof(float);
	for (i = 0; i < nTrainFaces; i++) {
		cvEigenDecomposite(faceImgArr[i], nEigens, eigenVectArr, 0, 0, pAvgTrainImg,
		projectedTrainFaceMat->data.fl + i * offset);
	}

	// armazena os dados de reconhecimento como um arquivo xml
	storeTrainingData();

	// salva todos os eigenvectors como imagens, para que eles possam ser utilizados
	if (SAVE_EIGENFACE_IMAGES) {
		storeEigenfaceImages();
	}

}

// le os dados de treinamento do arquivo facedata.xml
int loadTrainingData(CvMat ** pTrainPersonNumMat) {
	CvFileStorage * fileStorage;
	int i;

	// cria uma interface arqivo - armazenamento
	fileStorage = cvOpenFileStorage("Eigenfaces/facedata.xml", 0, CV_STORAGE_READ);
	if (!fileStorage) {
		printf("Arquivo 'facedata.xml' nao pode ser aberto.\n");
		return 0;
	}

	// le os nomes das pessoas
	personNames.clear(); // faz com q comece vazio
	nPersons = cvReadIntByName(fileStorage, 0, "nPersons", 0);
	if (nPersons == 0) {
		printf("Nenhuma pessoa encontrada em 'facedata.xml'.\n");
		return 0;
	}
	// le o nome de cada pessoa
	for (i = 0; i < nPersons; i++) {
		string sPersonName;
		char varname[200];
		sprintf(varname, "personName_%d", (i + 1));
		sPersonName = cvReadStringByName(fileStorage, 0, varname);
		personNames.push_back(sPersonName);
	}

	// le os dados
	nEigens = cvReadIntByName(fileStorage, 0, "nEigens", 0);
	nTrainFaces = cvReadIntByName(fileStorage, 0, "nTrainFaces", 0);
	*pTrainPersonNumMat = (CvMat *) cvReadByName(fileStorage, 0, "trainPersonNumMat", 0);
	eigenValMat = (CvMat *) cvReadByName(fileStorage, 0, "eigenValMat", 0);
	projectedTrainFaceMat = (CvMat *) cvReadByName(fileStorage, 0, "projectedTrainFaceMat", 0);
	pAvgTrainImg = (IplImage *) cvReadByName(fileStorage, 0, "avgTrainImg", 0);
	eigenVectArr = (IplImage **) cvAlloc(nTrainFaces * sizeof(IplImage *));
	for (i = 0; i < nEigens; i++) {
		char varname[200];
		sprintf(varname, "eigenVect_%d", i);
		eigenVectArr[i] = (IplImage *) cvReadByName(fileStorage, 0, varname, 0);
	}

	cvReleaseFileStorage(&fileStorage);

	printf("Lendo dados de treino (%d imagens de treino de %d pessoas):\n", nTrainFaces, nPersons);
	printf("Pessoas: ");
	if (nPersons > 0)
		printf("<%s>", personNames[0].c_str());
	for (i = 1; i < nPersons; i++) {
		printf(", <%s>", personNames[i].c_str());
	}
	printf(".\n");

	return 1;
}

// armazena os dados de treinamento no arquivo facedata.xml
void storeTrainingData() {
	CvFileStorage * fileStorage;
	int i;

	// cria a interface arquivo-armazenamento
	fileStorage = cvOpenFileStorage("Eigenfaces/facedata.xml", 0, CV_STORAGE_WRITE);

	// armazena os nomes das pessoas
	cvWriteInt(fileStorage, "nPersons", nPersons);
	for (i = 0; i < nPersons; i++) {
		char varname[200];
		sprintf(varname, "personName_%d", (i + 1));
		cvWriteString(fileStorage, varname, personNames[i].c_str(), 0);
	}

	// armazena todos os dados
	cvWriteInt(fileStorage, "nEigens", nEigens);
	cvWriteInt(fileStorage, "nTrainFaces", nTrainFaces);
	cvWrite(fileStorage, "trainPersonNumMat", personNumTruthMat, cvAttrList(0, 0));
	cvWrite(fileStorage, "eigenValMat", eigenValMat, cvAttrList(0, 0));
	cvWrite(fileStorage, "projectedTrainFaceMat", projectedTrainFaceMat, cvAttrList(0, 0));
	cvWrite(fileStorage, "avgTrainImg", pAvgTrainImg, cvAttrList(0, 0));
	for (i = 0; i < nEigens; i++) {
		char varname[200];
		sprintf(varname, "eigenVect_%d", i);
		cvWrite(fileStorage, varname, eigenVectArr[i], cvAttrList(0, 0));
	}

	cvReleaseFileStorage(&fileStorage);
}

// acha a pessoa mais parecia baseado com a deteccao. Retorna o index e armazena o indice de confianca em pConfidence
int findNearestNeighbor(float * projectedTestFace, float *pConfidence) {
	double leastDistSq = DBL_MAX;
	int i, iTrain, iNearest = 0;

#ifdef USE_MAHALANOBIS_DISTANCE
	double leastDistSqMahalanobis = DBL_MAX;
	int iNearestMahalanobis = 0;
#endif

	for (iTrain = 0; iTrain < nTrainFaces; iTrain++) {
		double distSq = 0, distSq2 = 0;
#ifdef USE_MAHALANOBIS_DISTANCE
		double distSqMahalanobis = 0;
#endif

		for (i = 0; i < nEigens; i++) {
			float d_i = projectedTestFace[i] - projectedTrainFaceMat->data.fl[iTrain * nEigens + i];

#ifdef USE_MAHALANOBIS_DISTANCE
			distSq += d_i * d_i; // Euclidean distance.
			distSqMahalanobis += d_i * d_i / eigenValMat->data.fl[i]; // Mahalanobis distance
#else
			distSq += d_i * d_i; // Euclidean distance.
#endif
		}

		if (distSq < leastDistSq) {
			leastDistSq = distSq;
			iNearest = iTrain;
		}

#ifdef USE_MAHALANOBIS_DISTANCE
		if (distSqMahalanobis < leastDistSqMahalanobis) {
			leastDistSqMahalanobis = distSqMahalanobis;
			iNearestMahalanobis = iTrain;
		}
#endif
	}

	// retornar o nivel de confianca baseado na distancia euclidiana ou mahalanobis,
	// para que imagens similares deem indice de confianca entre 0.5 - 1.0
	// e imagens muitos diferentes deem indice de confianca entre 0.0 - 0.5
#ifdef USE_MAHALANOBIS_DISTANCE
	*pConfidence = 1.0f - sqrt(leastDistSq / (float) (nTrainFaces * nEigens)) / 255.0f;
	if (iNearest == iNearestMahalanobis) {
		printf(".\n");
		return iNearest;
	}
	return -1;
#else

	*pConfidence = 1.0f - sqrt(leastDistSq / (float) (nTrainFaces * nEigens)) / 255.0f;
	return iNearest;
#endif

}

// faz a analize das componentes principais, achando a imagem media
// e os eigenfaces que representa qualquer imagem no ambiente de dados
void doPCA() {
	int i;
	CvTermCriteria calcLimit;
	CvSize faceImgSize;

	// seta o numero de eigenvalues para usar
	nEigens = nTrainFaces - 1;

	// armazena as imagens eigenvector
	faceImgSize.width = faceImgArr[0]->width;
	faceImgSize.height = faceImgArr[0]->height;
	eigenVectArr = (IplImage**) cvAlloc(sizeof(IplImage*) * nEigens);
	for (i = 0; i < nEigens; i++)
		eigenVectArr[i] = cvCreateImage(faceImgSize, IPL_DEPTH_32F, 1);

	// armazena o array de eigenvalue
	eigenValMat = cvCreateMat(1, nEigens, CV_32FC1);

	// armazena a imagem media
	pAvgTrainImg = cvCreateImage(faceImgSize, IPL_DEPTH_32F, 1);

	// seta o criterio de termino do PCA
	calcLimit = cvTermCriteria(CV_TERMCRIT_ITER, nEigens, 1);

	// computa a imagem media, eigenvalues e os eigenvectors
	cvCalcEigenObjects(nTrainFaces, (void*) faceImgArr, (void*) eigenVectArr, CV_EIGOBJ_NO_CALLBACK, 0, 0, &calcLimit, pAvgTrainImg, eigenValMat->data.fl);

	cvNormalize(eigenValMat, eigenValMat, 1, 0, CV_L1, 0);
}

// le os nomes e as imagens das pessoas de um arquivo txt, e le todas as imagens listadas
int loadFaceImgArray(char * filename) {
	FILE * imgListFile = 0;
	char imgFilename[512];
	int iFace, nFaces = 0;
	int i;

	// abre o arquivo de input
	if (!(imgListFile = fopen(filename, "r"))) {
		fprintf(stderr, "Arquivo %s nao pode ser aberto\n", filename);
		return 0;
	}

	// conta o numero de faces
	while (fgets(imgFilename, 512, imgListFile))
		++nFaces;
	rewind(imgListFile);

	// alloca o array face-imagem 
	faceImgArr = (IplImage **) cvAlloc(nFaces * sizeof(IplImage *));
	personNumTruthMat = cvCreateMat(1, nFaces, CV_32SC1);

	personNames.clear(); 
	nPersons = 0;

	// armazena as imagens de faces em um array
	for (iFace = 0; iFace < nFaces; iFace++) {
		char personName[256];
		string sPersonName;
		int personNumber;

		// le o numero da pessoa, seu nome e o nome da imagem
		fscanf(imgListFile, "%d %s %s", &personNumber, personName, imgFilename);
		sPersonName = personName;

		// verifica se um nova pessoa esta sendo lida
		if (personNumber > nPersons) {
			// alloca memoria para a pessoa extra
			for (i = nPersons; i < personNumber; i++) {
				personNames.push_back(sPersonName);
			}
			nPersons = personNumber;
		}

		personNumTruthMat->data.i[iFace] = personNumber;

		// le a imagem da face
		faceImgArr[iFace] = cvLoadImage(imgFilename, CV_LOAD_IMAGE_GRAYSCALE);

		if (!faceImgArr[iFace]) {
			fprintf(stderr, "Imagem nao pode ser lida de %s\n", imgFilename);
			return 0;
		}
	}

	fclose(imgListFile);

	printf("Data lido de '%s': (%d imagens de %d pessoas).\n", filename, nFaces, nPersons);
	printf("Pessoas: ");
	if (nPersons > 0)
		printf("<%s>", personNames[0].c_str());
	for (i = 1; i < nPersons; i++) {
		printf(", <%s>", personNames[i].c_str());
	}
	printf(".\n");

	return nFaces;
}

// retreina o novo database de faces sem para a execucao
CvMat* retrainOnline(void) {
	CvMat *trainPersonNumMat;
	int i;

	// libera e reinicializa as variaveis globais
	if (faceImgArr) {
		for (i = 0; i < nTrainFaces; i++) {
			if (faceImgArr[i])
				cvReleaseImage(&faceImgArr[i]);
		}
	}cvFree( &faceImgArr);
	// array de imagens de face
	cvFree( &personNumTruthMat);
	// array de numeros das pessoas
	personNames.clear(); // array dos nomes das pessoas indextada pelo numero
	nPersons = 0; // o numero das pessoas no cenario de treino
	nTrainFaces = 0; // o numero de imagens de treinamento
	nEigens = 0; // o numero de eigenvalues
	cvReleaseImage(&pAvgTrainImg); // a imagem media
	for (i = 0; i < nTrainFaces; i++) {
		if (eigenVectArr[i])
			cvReleaseImage(&eigenVectArr[i]);
	}cvFree( &eigenVectArr);
	// eigenvectors
	cvFree( &eigenValMat);
	// eigenvalues
	cvFree( &projectedTrainFaceMat);

	printf("Retreino com a nova pessoa ...\n");
	learn("Eigenfaces/train.txt");
	printf("Retrinamento completo.\n");

	if (!loadTrainingData(&trainPersonNumMat)) {
		printf("ERROR em recognizeFromCam(): Dados de treino nao puderam ser lidos!\n");
		exit(1);
	}

	return trainPersonNumMat;
}

// pega a proxima frame da camera. Espera ate que a proxima frame esteja pronta,
// e prove o acesso direta a mesma, entao nao modifique o retorno da imagem ou o libere.
IplImage* getCameraFrame(void) {
	IplImage *frame;

	// se a camera não inicializou, entao inicializa
	if (!camera) {
		printf("Acessando a camera ...\n");
		camera = cvCaptureFromCAM(0);
		if (!camera) {
			printf("ERROR em getCameraFrame(): A camera nao pode ser acessada.\n");
			exit(1);
		}
		//seta a resolucao da camera
		cvSetCaptureProperty(camera, CV_CAP_PROP_FRAME_WIDTH, 640);
		cvSetCaptureProperty(camera, CV_CAP_PROP_FRAME_HEIGHT, 480);
		// espera um tempo, para que a camera se auto-ajuste
		sleep(10);
		frame = cvQueryFrame(camera); // pega a primeira frame para ter crtz que a camera foi inicializada
		if (frame) {
			printf("Um camera com a resolucao %dx%d foi obtida.\n", (int) cvGetCaptureProperty(camera, CV_CAP_PROP_FRAME_WIDTH),
					(int) cvGetCaptureProperty(camera, CV_CAP_PROP_FRAME_HEIGHT));
		}
	}

	frame = cvQueryFrame(camera);
	if (!frame) {
		fprintf(stderr, "ERROR em recognizeFromCam(): Camera ou arquivo de video nao pode ser acessado.\n");
		exit(1);
		//return NULL;
	}
	return frame;
}

int saveRotateImages(int personId, char *newPersonName, int numberOfSavedFaces) {
	printf("Imagens Rotacionadas...\n");
	for (int i = 1; i <= NUMBER_OF_ROTATE_IMAGES; i++) {
		char cstrRotate[256], cstr[256];
		IplImage *processedFaceImg;

		int personFace = rand() % numberOfSavedFaces;
		sprintf(cstr, "Eigenfaces/data/%d_%s%d.pgm", personId, newPersonName, personFace);
		processedFaceImg = cvLoadImage(cstr);
		int randNumber = rand();
		int angle = randNumber % MAX_ANGLE_OF_ROTATE;
		int signedAngle = randNumber % 2;
		if (signedAngle == 1) {
			angle = angle * -1;
		}
		IplImage rotateFaceImg = rotateImage(processedFaceImg, (double) angle);
		sprintf(cstrRotate, "Eigenfaces/data/%d_%s%d.pgm", personId, newPersonName, numberOfSavedFaces + i);
		printf("Armazecando a face corrente de '%s' na imagem '%s'.\n", newPersonName, cstrRotate);
		cvSaveImage(cstrRotate, &rotateFaceImg, NULL);
	}

	return NUMBER_OF_ROTATE_IMAGES;
}

int saveFlipImages(int personId, char *newPersonName, int numberOfSavedFaces) {
	printf("Imagens espelhadas...\n");
	for (int i = 1; i <= NUMBER_OF_FLIP_IMAGES; i++) {
		char cstrFlip[256], cstr[256];
		IplImage *processedFaceImg;

		int personFace = rand() % numberOfSavedFaces;

		sprintf(cstr, "Eigenfaces/data/%d_%s%d.pgm", personId, newPersonName, personFace);
		processedFaceImg = cvLoadImage(cstr);

		sprintf(cstrFlip, "Eigenfaces/data/%d_%s%d.pgm", personId, newPersonName, numberOfSavedFaces + i);
		printf("Armazecando a face corrente de '%s' na imagem '%s'.\n", newPersonName, cstrFlip);

		Mat source(processedFaceImg, false);
		flip(source, source, 1);
		IplImage rotateImage = IplImage(source);
		cvSaveImage(cstrFlip, &rotateImage, NULL);
	}

	return NUMBER_OF_FLIP_IMAGES;
}

int saveNoiseImages(int personId, char *newPersonName, int numberOfSavedFaces) {
	printf("Imagens com ruido...\n");
	for (int i = 1; i <= NUMBER_OF_NOISE_IMAGES; i++) {
		char cstrNoise[256], cstr[256];
		IplImage *processedFaceImg;

		int personFace = rand() % numberOfSavedFaces;
		sprintf(cstr, "Eigenfaces/data/%d_%s%d.pgm", personId, newPersonName, personFace);
		processedFaceImg = cvLoadImage(cstr);

		sprintf(cstrNoise, "Eigenfaces/data/%d_%s%d.pgm", personId, newPersonName, numberOfSavedFaces + i);
		printf("Armazecando a face corrente de '%s' na imagem '%s'.\n", newPersonName, cstrNoise);

		IplImage *noiseFaceImg = generateNoiseImage(processedFaceImg, LEVEL_OF_NOISE_IMAGES);
		if(noiseFaceImg == NULL) {
			i--;
			continue;
		}
		cvSaveImage(cstrNoise, noiseFaceImg, NULL);
	}

	return NUMBER_OF_NOISE_IMAGES;
}

// reconhece a pessoa na camera continuamente
void recognizeFromCam(void) {
	int i;
	CvMat * trainPersonNumMat; // numeros da pessoa durante o trienmento
	float * projectedTestFace;
	double timeFaceRecognizeStart;
	double tallyFaceRecognizeTime;
	CvHaarClassifierCascade* faceCascade;
	char cstr[256];
	int saveNextFaces = FALSE;
	char newPersonName[256];
	int newPersonFaces;
	int kinectFail = 0;

	trainPersonNumMat = 0; 
	projectedTestFace = 0;
	saveNextFaces = FALSE;
	newPersonFaces = 0;

	printf("Reconhecendo a pessoa na camera ...\n");

	// le os ultimos dados de treinamento salvos 
	if (loadTrainingData(&trainPersonNumMat)) {
		faceWidth = pAvgTrainImg->width;
		faceHeight = pAvgTrainImg->height;
	} else {
		//printf("ERROR in recognizeFromCam(): Couldn't load the training data!\n");
		//exit(1);
	}

	// projeta as imagnes de teste no subespaco PCA
	projectedTestFace = (float *) cvAlloc(nEigens * sizeof(float));

	// cria uma janela par ao usuario ver a imagem da camera
	cvNamedWindow("Input", CV_WINDOW_AUTOSIZE);
	cvMoveWindow("Input", 10, 10);

	mkdir("Eigenfaces/data", 0777);

	// le o classificador utilizado para detecao
	faceCascade = (CvHaarClassifierCascade*) cvLoad(frontalFaceCascadeFilename, 0, 0, 0);
	if (!faceCascade) {
		printf("ERROR em recognizeFromCam(): Classificador '%s' nao pode ser lido.\n", frontalFaceCascadeFilename);
		exit(1);
	}

	while (1) {
		int iNearest, nearest = 0, truth;
		IplImage *camImg;
		IplImage *greyImg;
		IplImage *faceImg;
		IplImage *sizedImg;
		IplImage *equalizedImg;
		IplImage *processedFaceImg;
		CvRect faceRect;
		IplImage *shownImg;
		int keyPressed = 0;
		FILE *trainFile;
		float confidence;

		// cuida o keyborad input no console
		if (kbhit())
			keyPressed = getchar();

		if (keyPressed == VK_ESCAPE) { // veririfica se o usario entrou com ESC
			break; 
		}
		switch (keyPressed) {
			case 'n': // adiciona uma nova pessoa no cenario de treino
				printf("Entre com o seu nome: ");
				strcpy(newPersonName, "newPerson");
				gets(newPersonName);
				printf("Coletando todas as imagens ate que o suaurio entre com 't' para comecar o treinamento como '%s' ...\n", newPersonName);
				newPersonFaces = 0; 
				saveNextFaces = TRUE;
				break;
			case 't': // comeca a treinar
				saveNextFaces = FALSE; // para de salvar novas faces
				// amarzena os novos dados nos arquivo de treinamento
				printf("Armazenando os dados de treinamento para nova pessoa '%s'.\n", newPersonName);
				// concate a nova pessoa no final dos dados de treino
				trainFile = fopen("Eigenfaces/train.txt", "a");
				for (i = 0; i < newPersonFaces; i++) {
					sprintf(cstr, "Eigenfaces/data/%d_%s%d.pgm", nPersons + 1, newPersonName, i + 1);
					fprintf(trainFile, "%d %s %s\n", nPersons + 1, newPersonName, cstr);
				}
				fclose(trainFile);

				// reinicializa os dados locais
				projectedTestFace = 0;
				saveNextFaces = FALSE;
				newPersonFaces = 0;

				// retreina do novo banco de dados sem parar a execucao
				cvFree( &trainPersonNumMat);
				// libera os dados antigos antes de obter os novos
				trainPersonNumMat = retrainOnline();
				// projeta as iamgens de teste no subespaco PCA
				cvFree(&projectedTestFace);
				projectedTestFace = (float *) cvAlloc(nEigens * sizeof(float));

				printf("Reconhecendo a pessoa na camera ...\n");
				continue; // Begin with the next frame.
				break;
		}

		//pega a frame da camera
		if (!kinectFail) {
			camImg = getKinectFrame();
		}

		if (kinectFail || !camImg) {
			kinectFail = 1;
			camImg = getCameraFrame();
		}

		if (!camImg) {
			printf("ERROR em recognizeFromCam(): Imagem de entrada ruim!\n");
		} else {
			// transforma a imagem em escala de cinza
			greyImg = convertImageToGreyscale(camImg);

			// realiza deteccao facial na imagem de entrada, usando o classificador
			faceRect = detectFaceInImage(greyImg, faceCascade);
			// veririfica se uma faxe foi detectada
			if (faceRect.width > 0) {
				faceImg = cropImage(greyImg, faceRect); // pega a face detectada
				// redimensiona a imagem
				sizedImg = resizeImage(faceImg, faceWidth, faceHeight);
				// da a imagem o brilho e contraste padrao
				equalizedImg = cvCreateImage(cvGetSize(sizedImg), 8, 1); // cria uma imagem vazia em escala de cinza
				cvEqualizeHist(sizedImg, equalizedImg);
				processedFaceImg = equalizedImg;
				if (!processedFaceImg) {
					printf("ERROR em recognizeFromCam(): Sem imagem de entrada!\n");
					exit(1);
				}

				// tenta reconhecer a face detectada
				if (nEigens > 0) {
					// projeta a imagem de teste do subespaco PCA
					cvEigenDecomposite(processedFaceImg, nEigens, eigenVectArr, 0, 0, pAvgTrainImg, projectedTestFace);

					// verifica qual a pessoa mais parecida com a face
					iNearest = findNearestNeighbor(projectedTestFace, &confidence);

					if(iNearest > 0)
						nearest = trainPersonNumMat->data.i[iNearest];
				} 

				if (newPersonFaces == NUMBER_OF_SAVED_FACES_FRONTAL) {
					printf("Movimente a cabeça levemente para a esquerda e tecle Enter.\n");
					fflush(stdin);
					fflush(stdout);
					getchar();
				} else if (newPersonFaces == NUMBER_OF_SAVED_FACES_FRONTAL + NUMBER_OF_SAVED_FACES_LEFT) {
					printf("Movimente a cabeça levemente para a direita e tecle Enter.\n");
					fflush(stdin);
					fflush(stdout);
					getchar();
				}

				if (saveNextFaces && newPersonFaces < NUMBER_OF_SAVED_FACES_FRONTAL + NUMBER_OF_SAVED_FACES_LEFT + NUMBER_OF_SAVED_FACES_RIGHT) {
					printf("\007");
					sleep(0.5);

					sprintf(cstr, "Eigenfaces/data/%d_%s%d.pgm", nPersons + 1, newPersonName, newPersonFaces + 1);
					printf("Armazenando a face corrente de '%s' na imagem '%s'.\n", newPersonName, cstr);
					cvSaveImage(cstr, processedFaceImg, NULL);

					newPersonFaces++;
				} else if (newPersonFaces == NUMBER_OF_SAVED_FACES_FRONTAL + NUMBER_OF_SAVED_FACES_LEFT + NUMBER_OF_SAVED_FACES_RIGHT) {

					// newPersonFaces = newPersonFaces + saveFlipImages(nPersons + 1, newPersonName, newPersonFaces);
					// newPersonFaces = newPersonFaces + saveNoiseImages(nPersons + 1, newPersonName, newPersonFaces);
					// newPersonFaces = newPersonFaces + saveRotateImages(nPersons + 1, newPersonName, newPersonFaces);

					printf("Pressione 't' para re-treinar.\n");
					fflush(stdout);
				}

				// libera os recursos
				cvReleaseImage(&greyImg);
				cvReleaseImage(&faceImg);
				cvReleaseImage(&sizedImg);
				cvReleaseImage(&equalizedImg);
			}

		// mostra os dados na tela
			shownImg = cvCloneImage(camImg);
			if (faceRect.width > 0) { // verifica se a face foi detectada
				// mostra a regiao da face detectada
				cvRectangle(shownImg, cvPoint(faceRect.x, faceRect.y), cvPoint(faceRect.x + faceRect.width - 1, faceRect.y + faceRect.height - 1), CV_RGB(0,255,0), 1, 8, 0);
				if (nEigens > 0) { // 
					// mostra o nome da pessoa reconhecida
					CvFont font;
					cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, CV_AA);
					CvScalar textColor = CV_RGB(0,255,255); // texto azul claro
					char text[256];

					if(nearest > 0)
						snprintf(text, sizeof(text) - 1, "Name: '%s'", personNames[nearest - 1].c_str());
					else
						snprintf(text, sizeof(text) - 1, "Name: '%s'", "Unknown");

					cvPutText(shownImg, text, cvPoint(faceRect.x, faceRect.y + faceRect.height + 15), &font, textColor);
					snprintf(text, sizeof(text) - 1, "Confidence: %f", confidence);
					cvPutText(shownImg, text, cvPoint(faceRect.x, faceRect.y + faceRect.height + 30), &font, textColor);
				}
			}

			// mostra a imagem
			cvShowImage("Input", shownImg);

			// da um tempo para o openCV desenhar a imagem e veriricar se alguma key foi pressionada
			keyPressed = cvWaitKey(10);
			if (keyPressed == VK_ESCAPE) { // verifica se o ESC foi pressionado
				break; 
			}

			cvReleaseImage(&shownImg);
		}
	}

	cvReleaseCapture(&camera);
	cvReleaseHaarClassifierCascade(&faceCascade);
}
