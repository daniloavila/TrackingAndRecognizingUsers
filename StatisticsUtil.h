#ifndef STATISTICS_UTILS_H_
#define STATISTICS_UTILS_H_

#include <iostream>
#include <map>
#include <list>
#include <string>

#include "MessageQueue.h"

using namespace std;

void calculateNewStatistics(MessageResponse *messageResponse);

void choiceNewLabelToUser(MessageResponse *messageResponse, map<int, char *> *users, map<int, float> *usersConfidence);

int getTotalAttempts(int user_id);

void statisticsClear(int user_id);

void printfLogComplete(map<int, char *> *users, map<int, float> *usersConfidence);

#endif
