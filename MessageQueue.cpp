#include "MessageQueue.h"

/**
 * Cria uma fila de mensagem com a key passada como parametro.
 */
int createMessageQueue(int key) {
	int idFila = 0;

	if((idFila = msgget(key, IPC_CREAT | 0x1B6)) < 0){
		printf("erro na criacao da fila\n");
		exit(1);
	}

	return idFila;
}

/**
 * obtem um fila de mensagens com a key passada como parametro.
 */
int getMessageQueue(int key) {
	int idFila = 0;

	if((idFila = msgget(key, IPC_EXCL | 0x1B6)) < 0){
		printf("erro na obtencao da fila\n");
		exit(1);
	}

	return idFila;
}

