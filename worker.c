#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include "worker.h"
#define NOB_IMPLEMENTATION
#include "nob.h"

int main() {
    key_t key = ftok(SOFI_WORKER_QUEUE_NAME, SOFI_WORKER_QUEUE_ID);
    QueueMessage message = {0};
    int queue_id = msgget(key, 0666 | IPC_CREAT);

    for (;;) {
    if (msgrcv(queue_id, &message, sizeof(message), 1, 0) == -1) {
            perror("msgrcv");
            continue;
        }

        pid_t pid = fork();

        if(pid<0){
            perror("fork fail");

        } else if (pid == 0) {
            execlp(message.text, nob_temp_file_name(message.text), NULL);

        } else {
            int status;
            while (waitpid(-1, &status, WNOHANG) > 0) { }
        }
    }

    msgctl(queue_id, IPC_RMID, NULL);
    return 0;
}
