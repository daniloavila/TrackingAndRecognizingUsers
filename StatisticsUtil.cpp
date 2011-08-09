#include "StatisticsUtil.h"

// Mapas auxiliares pra ajudar a calcular a mulher label e o seu indice de confiança.
map<int, map<string, float> > usersNameConfidence;
map<int, map<string, int> > usersNameAttempts;

int idUserInTime = 0;
double statisticConfidence;

bool compareNameByConfidence(string first, string second) {
	map<string, int> *nameAttempts = &usersNameAttempts[idUserInTime];
	map<string, float> *nameConfidence = &usersNameConfidence[idUserInTime];

	double firstDif = abs((*nameConfidence)[first] - statisticConfidence);
	double secondDif = abs((*nameConfidence)[second] - statisticConfidence);

	printf("CONFIANCA1 - %s - %f - %f\n", first.c_str(), (*nameConfidence)[first], firstDif);
	printf("CONFIANCA2- %s - %f - %f\n\n", second.c_str(), (*nameConfidence)[second], secondDif);

	if (firstDif <= secondDif) {
		return true;
	} else {
		return false;
	}
}

/**
 * Recalcula as estatisticas de confianca e tentativas a partir da mensagem de resposta.
 */
void calculateNewStatistics(MessageResponse *messageResponse) {
	// Inicio - recalcula a confiança da nova label
	map<string, int> *nameAttempts = &usersNameAttempts[messageResponse->user_id];
	map<string, float> *nameConfidence = &usersNameConfidence[messageResponse->user_id];

	float acumulatedConfidence = (*nameConfidence)[messageResponse->user_name];
	int attempts = (*nameAttempts)[messageResponse->user_name];

	// confidence
	(*nameConfidence)[messageResponse->user_name] = ((acumulatedConfidence * attempts) + messageResponse->confidence) / (attempts + 1);

	// tentativas
	(*nameAttempts)[messageResponse->user_name] = attempts + 1;
}

bool verifyChoice(string name, float confidence, map<int, char *> *users) {
	map<int, char *>::iterator itUsers;
	printf("&&&&&&&&&&&&&&& 1\n");
	for (itUsers = users->begin(); itUsers != users->end(); itUsers++) {
		string name2(itUsers->second);
		if (name.compare(name2) == 0 && itUsers->first != idUserInTime) {
			printf("&&&&&&&&&&&&&&& 2 -> %s == %s\n", name.c_str(), name2.c_str());
			map<string, float> *nameConfidence = &usersNameConfidence[itUsers->first];
			if ((*nameConfidence)[name] > confidence) {
				printf("&&&&&&&&&&&&&&& 4\n");
				return false;
			}
		}
	}
	return true;
}

/**
 * Escolhe estatisticamente baseado no numero de vezes que o reconhecedor escolheu a label e o grau de confiança que o mesmo a escolheu.
 */
void choiceNewLabelToUser(MessageResponse *messageResponse, map<int, char *> *users, map<int, float> *usersConfidence) {
	// Inicio - escolhe a nova label do usuario de acordo com a maior confiança estatistica
	map<string, int> *nameAttempts = &usersNameAttempts[messageResponse->user_id];
	map<string, float> *nameConfidence = &usersNameConfidence[messageResponse->user_id];

	idUserInTime = messageResponse->user_id;

	string name;
	float confidence;


	statisticConfidence = 0.0;
	int totalAttempts = 0;
	map<string, int>::iterator itAttempts;

	for (itAttempts = nameAttempts->begin(); itAttempts != nameAttempts->end(); itAttempts++) {
		statisticConfidence += itAttempts->second * (*nameConfidence)[itAttempts->first];
		totalAttempts += itAttempts->second;
	}

	statisticConfidence = statisticConfidence / totalAttempts;
	printf("CONFIANCA PONDERADA - %f\n", statisticConfidence);

	list<string> listNameOrdered;
	for (itAttempts = nameAttempts->begin(); itAttempts != nameAttempts->end(); itAttempts++) {
		listNameOrdered.push_back(itAttempts->first);
	}

	listNameOrdered.sort(compareNameByConfidence);

	while (listNameOrdered.size() > 0) {
		name = listNameOrdered.front();
		confidence = (*nameConfidence)[name];

		if (verifyChoice(name, confidence, users)) {
			break;
		} else {
			name = "";
			printf("#######################################\n");
			printf("======> Verificou que existe outro usuário no com a mesma label.\n");
			printf("%d - %s - %.2f\n", idUserInTime, name.c_str(), confidence);
			printf("#######################################\n");
			listNameOrdered.pop_front();
		}
	}

	if (name.length() > 0) {
		(*users)[messageResponse->user_id] = (char*) (malloc(sizeof(char) * name.size()));
		strcpy((*users)[messageResponse->user_id], name.c_str());
		(*usersConfidence)[messageResponse->user_id] = confidence;
	}

	printf("Log - Tracker diz: Nome => '%s' - Tentativas => %d - Confiança => %f\n", messageResponse->user_name, (*nameAttempts)[messageResponse->user_name],
			(*nameConfidence)[messageResponse->user_name]);
	printf("Log - Tracker diz: A label escolhida foi '%s' com o índice de confiança igual a %f\n", (*users)[messageResponse->user_id],
			(*usersConfidence)[messageResponse->user_id]);
}

/**
 * Calcula o numero total de vezes que foi tentado rechonhecer o usuário com o id passado
 */
int getTotalAttempts(int user_id) {
	// Calcula o total de vezes que o usuario foi reconhecido. Independentemente da resposta.
	map<string, int>::iterator itAttempts;
	int total = 0;
	for (itAttempts = usersNameAttempts[user_id].begin(); itAttempts != usersNameAttempts[user_id].end(); itAttempts++) {
		total = total + (*itAttempts).second;
	}
	return total;
}

void statisticsClear(int user_id) {
	usersNameConfidence.erase(user_id);
	usersNameAttempts.erase(user_id);
}

void printfLogComplete(map<int, char *> *users, map<int, float> *usersConfidence) {
	printf("###################################\n");
	printf("############# LOG #################\n");
	map<int, map<string, int> >::iterator it;
	for (it = usersNameAttempts.begin(); it != usersNameAttempts.end(); it++) {
		map<string, int>::iterator itAttempts;
		map<string, float> *nameConfidence = &usersNameConfidence[it->first];
		printf("Usuário %d: (%s - %f)\n", it->first, (*users)[it->first], (*usersConfidence)[it->first]);
		map<string, int> *second = &(it->second);
		for (itAttempts = (*second).begin(); itAttempts != (*second).end(); itAttempts++) {
			printf("\t %s => %d - %f\n", (*itAttempts).first.c_str(), (*itAttempts).second, (*nameConfidence)[(*itAttempts).first]);
		}
	}
	printf("###################################\n");
}
