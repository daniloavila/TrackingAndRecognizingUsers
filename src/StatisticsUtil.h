#ifndef STATISTICS_UTILS_H_
#define STATISTICS_UTILS_H_

#include <iostream>
#include <map>
#include <list>
#include <string>

#include <time.h>

#include "MessageQueue.h"

#include "Definitions.h"

using namespace std;

void calculateNewStatistics(MessageResponse *messageResponse);

void choiceNewLabelToUser(MessageResponse *messageResponse, map<int, UserStatus> *users);

int getTotalAttempts(int user_id);

void statisticsClear(int user_id);

void statisticsClearAll();

void printfLogComplete(map<int, UserStatus> *users, FILE *file);

void printfLogCompleteByUser(int id, map<int, UserStatus> *users, FILE *file, int identationLevel = 1);

#endif
