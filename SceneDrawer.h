#ifndef XNV_POINT_DRAWER_H_
#define XNV_POINT_DRAWER_H_

#include <XnCppWrapper.h>
#include <iostream>
#include <map>

using namespace std;

void DrawDepthMap(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd, map<int, char*> users, map<int, float> usersConfidence);

#endif
