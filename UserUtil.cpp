#include <XnOpenNI.h>
#include <XnCodecIDs.h>
#include <XnCppWrapper.h>

XnStatus printUserCoM(xn::UserGenerator& generator, XnUserID nId, XnPoint3D& com, XnPlane3D& floor){
	generator.GetCoM(nId, com);
	printf(" User %d -> X:%lf Y:%lf Z:%lf \n", nId,(com).X,(com).Y,(com).Z);

	return XN_STATUS_OK;
}

void printUsersCoM(xn::UserGenerator& generator, xn::SceneAnalyzer& analyzer){
	XnPoint3D* com = new XnPoint3D();
	XnUserID* allUsers = NULL;
	XnUInt16 numberOfUsers = generator.GetNumberOfUsers();

	allUsers = new XnUserID[numberOfUsers];


	XnPlane3D* floor = new XnPlane3D();
	analyzer.GetFloor(*floor);
	// printf("Floor poniter -> X:%lf Y:%lf Z:%lf\n",(*floor).ptPoint.X,(*floor).ptPoint.Y,(*floor).ptPoint.Z);
	// printf("Floor normal -> X:%lf Y:%lf Z:%lf\n",(*floor).vNormal.X,(*floor).vNormal.Y,(*floor).vNormal.Z);

	generator.GetUsers(allUsers, numberOfUsers);
	for(int i=0; i< numberOfUsers; i++){		
		printUserCoM(generator, allUsers[i], *com, *floor);
	}
	
}