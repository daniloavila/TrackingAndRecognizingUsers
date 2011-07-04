#include "MessageQueue.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

int createMessageQueue(int key) {
	int idFila = 0;

	if ((idFila = msgget(key, IPC_EXCL | 0x1B6)) < 0) {
		printf("erro na criacao da fila\n");
		exit(1);
	}

	return idFila;
}

