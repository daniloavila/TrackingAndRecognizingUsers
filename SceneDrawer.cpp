//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------

#include "SceneDrawer.h"
#include <time.h>

using namespace std;

#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

extern xn::UserGenerator g_UserGenerator;
extern xn::DepthGenerator g_DepthGenerator;

extern XnBool g_bDrawBackground;
extern XnBool g_bDrawPixels;
extern XnBool g_bDrawSkeleton;
extern XnBool g_bPrintID;
extern XnBool g_bPrintState;

#define MAX_DEPTH 10000
float g_pDepthHist[MAX_DEPTH];
unsigned int getClosestPowerOfTwo(unsigned int n) {
	unsigned int m = 2;
	while (m < n)
		m <<= 1;

	return m;
}
GLuint initTexture(void** buf, int& width, int& height) {
	GLuint texID = 0;
	glGenTextures(1, &texID);

	width = getClosestPowerOfTwo(width);
	height = getClosestPowerOfTwo(height);
	*buf = new unsigned char[width * height * 4];glBindTexture
	(GL_TEXTURE_2D, texID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return texID;
}

GLfloat texcoords[8];
void DrawRectangle(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY) {
	GLfloat verts[8] = { topLeftX, topLeftY, topLeftX, bottomRightY, bottomRightX, bottomRightY, bottomRightX, topLeftY };
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glFlush();
}
void DrawTexture(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY) {
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

	DrawRectangle(topLeftX, topLeftY, bottomRightX, bottomRightY);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

XnFloat Colors[][3] = { { 0, 1, 1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 1, 1, 0 }, { 1, 0, 0 }, { 1, .5, 0 }, { .5, 1, 0 }, { 0, .5, 1 }, { .5, 0, 1 }, { 1, 1, .5 }, { 1, 1, 1 } };
XnUInt32 nColors = 10;

void glPrintString(void *font, char *str) {
	int i, l = strlen(str);

	for (i = 0; i < l; i++) {
		glutBitmapCharacter(font, *str++);
	}
}

void DrawLimb(XnUserID player, XnSkeletonJoint eJoint1, XnSkeletonJoint eJoint2) {
	if (!g_UserGenerator.GetSkeletonCap().IsTracking(player)) {
		printf("not tracked!\n");
		return;
	}

	XnSkeletonJointPosition joint1, joint2;
	g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint1, joint1);
	g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint2, joint2);

	if (joint1.fConfidence < 0.5 || joint2.fConfidence < 0.5) {
		return;
	}

	XnPoint3D pt[2];
	pt[0] = joint1.position;
	pt[1] = joint2.position;

	g_DepthGenerator.ConvertRealWorldToProjective(2, pt, pt);
	glVertex3i(pt[0].X, pt[0].Y, 0);
	glVertex3i(pt[1].X, pt[1].Y, 0);
}

void DrawDepthMap(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd, map<int, UserStatus> *users) {
	static bool bInitialized = false;
	static GLuint depthTexID;
	static unsigned char* pDepthTexBuf;
	static int texWidth, texHeight;

	float topLeftX;
	float topLeftY;
	float bottomRightY;
	float bottomRightX;
	float texXpos;
	float texYpos;

	// printf("\t\tSCENE DRAWER - INICIO\n");

	if (!bInitialized) {
		// printf("\t\tSCENE DRAWER - PRIMEIRO IF\n");
		texWidth = getClosestPowerOfTwo(dmd.XRes());
		texHeight = getClosestPowerOfTwo(dmd.YRes());

		depthTexID = initTexture((void**) &pDepthTexBuf, texWidth, texHeight);

		bInitialized = true;

		topLeftX = dmd.XRes();
		topLeftY = 0;
		bottomRightY = dmd.YRes();
		bottomRightX = 0;
		texXpos = (float) dmd.XRes() / texWidth;
		texYpos = (float) dmd.YRes() / texHeight;

		memset(texcoords, 0, 8 * sizeof(float));
		texcoords[0] = texXpos, texcoords[1] = texYpos, texcoords[2] = texXpos, texcoords[7] = texYpos;
		// printf("\t\tSCENE DRAWER - FIM IF\n");

	}
	unsigned int nValue = 0;
	unsigned int nHistValue = 0;
	unsigned int nIndex = 0;
	unsigned int nX = 0;
	unsigned int nY = 0;
	unsigned int nNumberOfPoints = 0;

	// printf("\t\tSCENE DRAWER - A\n");
	XnUInt16 g_nXRes = dmd.XRes();
	XnUInt16 g_nYRes = dmd.YRes();

	unsigned char* pDestImage = pDepthTexBuf;

	// printf("\t\tSCENE DRAWER - B\n");
	const XnDepthPixel* pDepth = dmd.Data();
	const XnLabel* pLabels = smd.Data();

	memset(g_pDepthHist, 0, MAX_DEPTH * sizeof(float));
	// printf("\t\tSCENE DRAWER - C\n");
	for (nY = 0; nY < g_nYRes; nY++) {
		for (nX = 0; nX < g_nXRes; nX++) {
			nValue = *pDepth;

			if (nValue != 0) {
				g_pDepthHist[nValue]++;
				nNumberOfPoints++;
			}

			pDepth++;
		}
	}
	// printf("\t\tSCENE DRAWER - D\n");

	for (nIndex = 1; nIndex < MAX_DEPTH; nIndex++) {
		g_pDepthHist[nIndex] += g_pDepthHist[nIndex - 1];
	}

	// printf("\t\tSCENE DRAWER - E\n");
	if (nNumberOfPoints) {
		for (nIndex = 1; nIndex < MAX_DEPTH; nIndex++) {
			g_pDepthHist[nIndex] = (unsigned int) (256 * (1.0f - (g_pDepthHist[nIndex] / nNumberOfPoints)));
		}
	}

	// printf("\t\tSCENE DRAWER - F\n");
	pDepth = dmd.Data();
	if (g_bDrawPixels) {
		XnUInt32 nIndex = 0;
		// printf("\t\tSCENE DRAWER - G\n");
		// prepara o mapa de textura
		for (nY = 0; nY < g_nYRes; nY++) {
			for (nX = 0; nX < g_nXRes; nX++, nIndex++) {

				pDestImage[0] = 0;
				pDestImage[1] = 0;
				pDestImage[2] = 0;
				if (g_bDrawBackground || *pLabels != 0) {
					nValue = *pDepth;
					XnLabel label = *pLabels;
					XnUInt32 nColorID = label % nColors;
					if (label == 0) {
						nColorID = nColors;
					}

					if (nValue != 0) {
						nHistValue = g_pDepthHist[nValue];

						pDestImage[0] = nHistValue * Colors[nColorID][0];
						pDestImage[1] = nHistValue * Colors[nColorID][1];
						pDestImage[2] = nHistValue * Colors[nColorID][2];
					}
				}

				pDepth++;
				pLabels++;
				pDestImage += 3;
			}

			pDestImage += (texWidth - g_nXRes) * 3;
		}
	} else {
		// printf("\t\tSCENE DRAWER - I\n");
		xnOSMemSet(pDepthTexBuf, 0, 3 * 2 * g_nXRes * g_nYRes);
	}

	// printf("\t\tSCENE DRAWER - J\n");

	glBindTexture(GL_TEXTURE_2D, depthTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pDepthTexBuf);

	// printf("\t\tSCENE DRAWER - L\n");
	// mostra o mapa de textura OpenGL
	glColor4f(0.75, 0.75, 0.75, 1);

	glEnable(GL_TEXTURE_2D);
	DrawTexture(dmd.XRes(), dmd.YRes(), 0, 0);
	glDisable(GL_TEXTURE_2D);

	// printf("\t\tSCENE DRAWER - M\n");

	char strLabel[50] = "";
	XnUInt16 nUsers = g_UserGenerator.GetNumberOfUsers();
	XnUserID* aUsers = new XnUserID[nUsers];

	g_UserGenerator.GetUsers(aUsers, nUsers);

	// printf("\t\tSCENE DRAWER - N\n");
	for (int i = 0; i < nUsers; ++i) {
		if (g_bPrintID) {
			XnPoint3D com, comPosition;

			g_UserGenerator.GetCoM(aUsers[i], com);
			comPosition = com;
			g_DepthGenerator.ConvertRealWorldToProjective(1, &com, &com);

			xnOSMemSet(strLabel, 0, sizeof(strLabel));
			if (!g_bPrintState) {
				// Rastreando
				sprintf(strLabel, "%d", aUsers[i]);
			} else {
				// Nada
				if (strlen((*users)[aUsers[i]].name) == 0) {
					if (!(*users)[aUsers[i]].canRecognize) {
						sprintf(strLabel, "%d - Not Recognize", aUsers[i]);
					} else {
						sprintf(strLabel, "%d - Recognizing", aUsers[i]);
					}
				} else {
					if (&com != NULL) {
						if (strcmp((*users)[aUsers[i]].name, UNKNOWN) == 0) {
							sprintf(strLabel, "%d - %s - (%.2lf, %.2lf, %.2lf)", aUsers[i], (*users)[aUsers[i]].name, comPosition.X, comPosition.Y,
									comPosition.Z);
						} else {
							sprintf(strLabel, "%d - %s\n%f - (%.2lf, %.2lf, %.2lf)", aUsers[i], (*users)[aUsers[i]].name, (*users)[aUsers[i]].confidence, comPosition.X,
									comPosition.Y, comPosition.Z);
						}
					} else {
						sprintf(strLabel, "%d - %s\n%f", aUsers[i], (*users)[aUsers[i]].name, (*users)[aUsers[i]].confidence);
					}
				}
			}

			glColor4f(1 - Colors[i % nColors][0], 1 - Colors[i % nColors][1], 1 - Colors[i % nColors][2], 1);

			glRasterPos2i(com.X, com.Y);
			glPrintString(GLUT_BITMAP_HELVETICA_18, strLabel);
		}

	}
	// printf("\t\tSCENE DRAWER - FIM\n");
}
