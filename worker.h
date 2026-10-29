#ifndef WORKER_H_
#define WORKER_H_

#define SOFI_WORKER_QUEUE_NAME "sofi_worker"
#define SOFI_WORKER_QUEUE_ID 6767

typedef struct {
    long type;
    char text[100];
} QueueMessage;

#endif // !WORKER_H_
