#include "StatisticsUtil.h"

// Mapas auxiliares pra ajudar a calcular a mulher label e o seu indice de confiança.
map<int, map<string, float> > usersNameConfidence;
map<int, map<string, int> > usersNameAttempts;

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
 * Escolhe estatisticamente baseado no numero de vezes que o reconhecedor escolheu a label e o grau de confiança que o mesmo a escolheu.
 */
void choiceNewLabelToUser(MessageResponse *messageResponse, map<int, char *> *users, map<int, float> *usersConfidence) {
	// Inicio - escolhe a nova label do usuario de acordo com a maior confiança estatistica
	map<string, int> *nameAttempts = &usersNameAttempts[messageResponse->user_id];
	map<string, float> *nameConfidence = &usersNameConfidence[messageResponse->user_id];

	string name;
	float confidence;
	double maxStatisticConfidence = 0;
	map<string, int>::iterator itAttempts;
	for (itAttempts = nameAttempts->begin(); itAttempts != nameAttempts->end(); itAttempts++) {
		double statisticConfidence = itAttempts->second * (*nameConfidence)[itAttempts->first];
		if (statisticConfidence > maxStatisticConfidence) {
			maxStatisticConfidence = statisticConfidence;
			name = itAttempts->first;
			confidence = (*nameConfidence)[itAttempts->first];
		}
	}

	(*users)[messageResponse->user_id] = (char*) (((malloc(sizeof(char) * strlen(messageResponse->user_name)))));
	if (name.length() > 0) {
		strcpy((*users)[messageResponse->user_id], name.c_str());
	}
	(*usersConfidence)[messageResponse->user_id] = confidence;

	printf("Log - Tracker diz: Nome => '%s' - Tentativas => %d - Confiança => %f\n", messageResponse->user_name, (*nameAttempts)[messageResponse->user_name],
			(*nameConfidence)[messageResponse->user_name]);
	printf("Log - Tracker diz: A label escolhida foi '%s' com o índice de confiança igual a %f\n", (*users)[messageResponse->user_id], (*usersConfidence)[messageResponse->user_id]);
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
