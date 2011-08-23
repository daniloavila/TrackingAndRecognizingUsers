#include "StatisticsUtil.h"

// Mapas auxiliares pra ajudar a calcular a mulher label e o seu indice de confiança.
map<int, map<string, float> > usersNameConfidence;
map<int, map<string, int> > usersNameAttempts;

int idUserInTime = 0;
double statisticConfidence;

/**
 * Função que compara uma label com outra apartir da sua confiança.
 */
bool compareNameByConfidence(string first, string second) {
	map<string, int> *nameAttempts = &usersNameAttempts[idUserInTime];
	map<string, float> *nameConfidence = &usersNameConfidence[idUserInTime];

	double firstDif = abs((*nameConfidence)[first] - statisticConfidence);
	double secondDif = abs((*nameConfidence)[second] - statisticConfidence);

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

/**
 * Verifica se a escolha é valida, ou seja, se já não existe outro usuário com a mesma label, e se existe qual deve prevalecer a partir da confiança.
 */
bool verifyChoice(string name, float confidence, map<int, char *> *users) {
	map<int, char *>::iterator itUsers;
	for (itUsers = users->begin(); itUsers != users->end(); itUsers++) {
		string name2(itUsers->second);
		if (name.compare(name2) == 0 && itUsers->first != idUserInTime) {
			map<string, float> *nameConfidence = &usersNameConfidence[itUsers->first];
			if ((*nameConfidence)[name] > confidence) {
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
//	printf("CONFIANCA PONDERADA - %f\n", statisticConfidence);

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
			printf("Log - StatisticsUtil diz: Verificou que existe outro usuário no com a mesma label.\n");
			listNameOrdered.pop_front();
		}
	}

	if (name.length() > 0) {
		(*users)[messageResponse->user_id] = (char*) (malloc(sizeof(char) * name.size()));
		strcpy((*users)[messageResponse->user_id], name.c_str());
		// TODO: Verificar qual das duas confianças usar.
		(*usersConfidence)[messageResponse->user_id] = confidence;
//		(*usersConfidence)[messageResponse->user_id] = statisticConfidence;
	}

	printf("Log - StatisticsUtil diz: Nome => '%s' - Tentativas => %d - Confiança => %f\n", messageResponse->user_name, (*nameAttempts)[messageResponse->user_name],
			(*nameConfidence)[messageResponse->user_name]);
	printf("Log - StatisticsUtil diz: A label escolhida foi '%s' com o índice de confiança igual a %f\n", (*users)[messageResponse->user_id],
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

void printfLogCompleteByUser(int id, map<int, char*> *users, map<int, float> *usersConfidence, FILE *file, int identationLevel) {
	map<string, int>::iterator itAttempts;
	map<string, float> *nameConfidence = &usersNameConfidence[id];
	for (int i = 0; i < identationLevel - 1; ++i) {
		fprintf(file, "\t");
	}
	fprintf(file, " Usuário %d: (%s - %f)\n", id, (*users)[id], (*usersConfidence)[id]);
	map<string, int> *second = &(usersNameAttempts[id]);
	for (itAttempts = (*second).begin(); itAttempts != (*second).end(); itAttempts++) {
		for (int i = 0; i < identationLevel; ++i) {
			fprintf(file, "\t");
		}
		fprintf(file, " %s => %d - %f\n", (*itAttempts).first.c_str(), (*itAttempts).second, (*nameConfidence)[(*itAttempts).first]);
	}
}

void printfLogComplete(map<int, char *> *users, map<int, float> *usersConfidence, FILE *file) {
	struct tm *local;
	time_t t;
	t = time(NULL);
	local = localtime(&t);
	fprintf(file, "###################################\n");
	fprintf(file, "###  %s", asctime(local));
	fprintf(file, "####### LOG USERS TRACKING ########\n");
	map<int, map<string, int> >::iterator it;
	if (usersNameAttempts.begin() != usersNameAttempts.end()) {
		for (it = usersNameAttempts.begin(); it != usersNameAttempts.end(); it++) {
			printfLogCompleteByUser(it->first, users, usersConfidence, file);
		}
	} else {
		fprintf(file, "###  Nenhum usuário sendo rastreado no momento.\n");
	}
	fprintf(file, "###################################\n");
}
