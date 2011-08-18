#include "ImageUtil.h"

/**
 * Retorna uma nova imagem em escala de cinza. Lembre de liberar o espaco da memoria da imagem 
 * retornada utilizando cvReleaseImage() quando terminar.
 */
IplImage* convertImageToGreyscale(const IplImage *imageSrc) {
	IplImage *imageGrey;
	// Tanto converte uma imagem para escala de cinza, ou copia uma imagem que ja esta na escala cinza
	// Isso garante que o usario sempre chame cvReleaseImage() na saida
	if (imageSrc->nChannels == 3) {
		imageGrey = cvCreateImage(cvGetSize(imageSrc), IPL_DEPTH_8U, 1);
		cvCvtColor(imageSrc, imageGrey, CV_BGR2GRAY);
	} else {
		imageGrey = cvCloneImage(imageSrc);
	}
	return imageGrey;
}

/**
 * Cria uma nova copia da imagem  para um tamanho desejado.
 * Lembre de liberar o espaco da imagem posteriormente
 */
IplImage* resizeImage(const IplImage *origImg, int newWidth, int newHeight) {
	IplImage *outImg = 0;
	int origWidth;
	int origHeight;
	if (origImg) {
		origWidth = origImg->width;
		origHeight = origImg->height;
	}
	if (newWidth <= 0 || newHeight <= 0 || origImg == 0 || origWidth <= 0 || origHeight <= 0) {
		printf("ERROR in resizeImage: Bad desired image size of %dx%d\n.", newWidth, newHeight);
		exit(1);
	}

	// modifica as dimensoes da imagem, msm se o aspect ratior mude
	outImg = cvCreateImage(cvSize(newWidth, newHeight), origImg->depth, origImg->nChannels);
	if (newWidth > origImg->width && newHeight > origImg->height) {
		// aumenta a imagem
		cvResetImageROI((IplImage*) origImg);
		cvResize(origImg, outImg, CV_INTER_LINEAR); // CV_INTER_CUBIC or CV_INTER_LINEAR is good for enlarging
	} else {
		//diminui a imagem
		cvResetImageROI((IplImage*) origImg);
		cvResize(origImg, outImg, CV_INTER_AREA); // CV_INTER_AREA is good for shrinking / decimation, but bad at enlarging.
	}

	return outImg;
}

/**
 * Retorna uma nova imagem que e uma versao cortada da imagem original.
 */
IplImage* cropImage(const IplImage *img, const CvRect region) {
	IplImage *imageTmp;
	IplImage *imageRGB;
	CvSize size;
	size.height = img->height;
	size.width = img->width;

	if (img->depth != IPL_DEPTH_8U) {
		printf("ERROR in cropImage: Unknown image depth of %d given in cropImage() instead of 8 bits per pixel.\n", img->depth);
		exit(1);
	}

	// primeiro cria uma nova imagem IPL (colorida ou em cinza) e copia os conteudos da img nela.
	imageTmp = cvCreateImage(size, IPL_DEPTH_8U, img->nChannels);
	cvCopy(img, imageTmp, NULL);

	// cria uma nova imagem da regiao detectada
	// seta a regiao de interesse que e ao redor da face
	cvSetImageROI(imageTmp, region);
	//copia a imagem de interesse na nova iplImage (imageRGB) e a retorna
	size.width = region.width;
	size.height = region.height;
	imageRGB = cvCreateImage(size, IPL_DEPTH_8U, img->nChannels);
	cvCopy(imageTmp, imageRGB, NULL);//copia somente a regiao

	cvReleaseImage(&imageTmp);
	return imageRGB;
}

/**
 * Obtem uma Float image de 8-bit equivalente a 32-bit
 * Retonar uma nova imagem, entao lembre de usar cvReleaseImage() no resultado
 */
IplImage* convertFloatImageToUcharImage(const IplImage *srcImg) {
	IplImage *dstImg = 0;
	if ((srcImg) && (srcImg->width > 0 && srcImg->height > 0)) {

		// espalha os float pixels de 32 bits para caber no range de 8bit pixel
		double minVal, maxVal;
		cvMinMaxLoc(srcImg, &minVal, &maxVal);

		//cout << "FloatImage:(minV=" << minVal << ", maxV=" << maxVal << ")." << endl;

		// Lida com os valores NaN e outros valores extremos, pois o DFT retorna alguns valores NaN
		if (cvIsNaN(minVal) || minVal < -1e30)
			minVal = -1e30;
		if (cvIsNaN(maxVal) || maxVal > 1e30)
			maxVal = 1e30;
		if (maxVal - minVal == 0.0f)
			maxVal = minVal + 0.001; // remove potenciais errors de divisao de zero

		// converte o formato
		dstImg = cvCreateImage(cvSize(srcImg->width, srcImg->height), 8, 1);
		cvConvertScale(srcImg, dstImg, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));
	}
	return dstImg;
}

/**
 * Armazena a CvMat em escala de cinza e ponto flutuante em uma imagem BMP/JPG/GIF/PNG,
 * pois cvSaveImage() so lida com imagens 8bits, e nao 32 bits com ponto flutuante
 */
void saveFloatImage(const char *filename, const IplImage *srcImg) {
	//cout << "Saving Float Image '" << filename << "' (" << srcImg->width << "," << srcImg->height << "). " << endl;
	IplImage *byteImg = convertFloatImageToUcharImage(srcImg);
	cvSaveImage(filename, byteImg);
	cvReleaseImage(&byteImg);
}

/**
 * Realiza deteccao facial na imagem, usando o classificador em cascata Haar.
 * Retorna um retangulo para a regiao detectada na imagem
 */
CvRect detectFaceInImage(const IplImage *inputImg, const CvHaarClassifierCascade* cascade) {
	const CvSize minFeatureSize = cvSize(20, 20);
	const int flags = CV_HAAR_FIND_BIGGEST_OBJECT | CV_HAAR_DO_ROUGH_SEARCH; // procura por somente uma face
	const float search_scale_factor = 1.1f;
	IplImage *detectImg;
	IplImage *greyImg = 0;
	CvMemStorage* storage;
	CvRect rc;
	double t;
	CvSeq* rects;
	int i;

	storage = cvCreateMemStorage(0);
	cvClearMemStorage(storage);

	// se a imagem e colorida, usa uma copia em escada de cinza
	detectImg = (IplImage*) inputImg; // assume que a imagem de entrada e para ser usada.
	if (inputImg->nChannels > 1) {
		greyImg = cvCreateImage(cvSize(inputImg->width, inputImg->height), IPL_DEPTH_8U, 1);
		cvCvtColor(inputImg, greyImg, CV_BGR2GRAY);
		detectImg = greyImg; // usa a imagem em escala de cinza como input
	}

	//detecta todas as faces
	t = (double) cvGetTickCount();
	rects = cvHaarDetectObjects(detectImg, (CvHaarClassifierCascade*) cascade, storage, search_scale_factor, 3, flags, minFeatureSize);
	t = (double) cvGetTickCount() - t;
	// printf("[Face Detection took %d ms and found %d objects]\n", cvRound( t/((double)cvGetTickFrequency()*1000.0) ), rects->total );

	// obtem a primeira face detectada (a maior)
	if (rects->total > 0) {
		rc = *(CvRect*) cvGetSeqElem(rects, 0);
	} else
		rc = cvRect(-1, -1, -1, -1); // nao achou nenhuma face

	//cvReleaseHaarClassifierCascade( &cascade );
	//cvReleaseImage( &detectImg );
	if (greyImg)
		cvReleaseImage(&greyImg);
	cvReleaseMemStorage(&storage);

	return rc; // retorna a maior face encontrada, ou (-1,-1,-1,-1).

}

/**
 * Rotaciona a imagem de acordo com um angulo passado como paremtro.
 */
IplImage rotateImage(const IplImage *sourceImage, double angle) {
	Mat source(sourceImage, false);
	Point2f src_center(source.cols / 2.0F, source.rows / 2.0F);
	Mat rot_mat = getRotationMatrix2D(src_center, angle, 1.0);
	Mat dst;
	warpAffine(source, dst, rot_mat, source.size());
	IplImage rotateImage = IplImage(dst);
	return rotateImage;
}

/**
 * Retorna um numero randomico uniforme.
 */
double uniform() {
	return (rand() / (float) 0x7fff) - 0.5;
}

/**
 * Adiciona ruido na imagem rotacionada.
 */
IplImage* generateNoiseImage(IplImage* img, float amount) {
	CvSize imgSize = cvGetSize(img);
	IplImage* imgTemp = cvCloneImage(img); 

	for (int y = 0; y < imgSize.height; y++) {
		for (int x = 0; x < imgSize.width; x++) {
			int randomValue = (char) ((uniform()) * amount);
			CvScalar scalar = cvGet2D(imgTemp, y, x);

			scalar.val[0] = scalar.val[0] + randomValue;
			scalar.val[1] = scalar.val[1] + randomValue;
			scalar.val[2] = scalar.val[2] + randomValue;
			scalar.val[3] = scalar.val[3] + randomValue;

			cvSet2D(imgTemp, y, x, scalar);
		}
	}

	return imgTemp;
}
