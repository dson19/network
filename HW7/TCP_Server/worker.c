#include "worker.h"
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define INITIAL_CAPACITY 64
#define WAKE_BUFFER_SIZE 256

/* One poll() thread. `sessions` and `fds` belong to the worker's own thread:
 * only the queue of new sessions is touched by the accepting thread too.
 * fds[0] is the wake-up pipe, fds[i + 1] is the socket of sessions[i]. */
typedef struct {
    Session **sessions;
    struct pollfd *fds;
    size_t count;
    size_t capacity;

    pthread_mutex_t queueLock;
    Session *queueHead; /* new sessions, protected by queueLock */
    Session *queueTail;
    int wakeRead;  /* a byte written to wakeWrite interrupts the worker's poll() */
    int wakeWrite;
} Worker;

static Worker workers[WORKER_COUNT];

/* Milliseconds on a clock that is never set back. */
static int64_t currentTimeMs(void){
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* Make room for one more session. Returns 0 if success, -1 if out of memory. */
static int ensureCapacity(Worker *worker){
    if (worker->count < worker->capacity){
        return 0;
    }

    size_t capacity = worker->capacity * 2;
    Session **sessions = realloc(worker->sessions, capacity * sizeof(*sessions));
    if (sessions == NULL){
        return -1;
    }
    worker->sessions = sessions;

    struct pollfd *fds = realloc(worker->fds, (capacity + 1) * sizeof(*fds));
    if (fds == NULL){
        return -1;
    }
    worker->fds = fds;
    worker->capacity = capacity;
    return 0;
}

/* Close a session and move the last one into its place in the array. */
static void removeSession(Worker *worker, size_t index){
    destroySession(worker->sessions[index]);
    worker->count--;
    worker->sessions[index] = worker->sessions[worker->count];
}

/* Empty the wake-up pipe and move the queued sessions into the poll array. */
static void adoptNewSessions(Worker *worker, int64_t nowMs){
    char bytes[WAKE_BUFFER_SIZE];
    while (read(worker->wakeRead, bytes, sizeof(bytes)) > 0){
    }

    pthread_mutex_lock(&worker->queueLock);
    Session *session = worker->queueHead;
    worker->queueHead = NULL;
    worker->queueTail = NULL;
    pthread_mutex_unlock(&worker->queueLock);

    while (session != NULL){
        Session *next = session->next;
        session->next = NULL;
        if (ensureCapacity(worker) != 0){
            fprintf(stderr, "Error: out of memory, client %s dropped\n", session->clientAddress);
            destroySession(session);
        } else if (sessionAdvance(session, nowMs) != 0){
            destroySession(session); /* the welcome code could not be sent */
        } else {
            worker->sessions[worker->count++] = session;
        }
        session = next;
    }
}

/* Fill the pollfd array for the next poll() and return its timeout: the time
 * until the first paused session may go on, or -1 if none is paused. */
static int preparePoll(Worker *worker, int64_t nowMs){
    int64_t firstResume = 0;

    worker->fds[0].fd = worker->wakeRead;
    worker->fds[0].events = POLLIN;
    worker->fds[0].revents = 0;

    for (size_t i = 0; i < worker->count; i++){
        const Session *session = worker->sessions[i];
        worker->fds[i + 1].fd = session->fd;
        worker->fds[i + 1].events = sessionPollEvents(session);
        worker->fds[i + 1].revents = 0;
        if (session->resumeAtMs != 0 && (firstResume == 0 || session->resumeAtMs < firstResume)){
            firstResume = session->resumeAtMs;
        }
    }

    if (firstResume == 0){
        return -1;
    }
    int64_t waitMs = firstResume - nowMs;
    if (waitMs <= 0){
        return 0;
    }
    return (waitMs > INT_MAX) ? INT_MAX : (int)waitMs;
}

/* Serve one session after poll(). Returns 0 if it goes on, -1 if it is over. */
static int serviceSession(Session *session, short revents, int64_t nowMs){
    if (revents & (POLLERR | POLLHUP | POLLNVAL)){
        return -1;
    }
    if ((revents & POLLIN) && sessionReceive(session) != 0){
        return -1;
    }

    bool resumeDue = session->resumeAtMs != 0 && session->resumeAtMs <= nowMs;
    if (revents != 0 || resumeDue){
        return sessionAdvance(session, nowMs);
    }
    return 0;
}

/* Thread body: the poll() loop of one worker. */
static void *workerThread(void *arg){
    Worker *worker = arg;

    for (;;){
        int timeoutMs = preparePoll(worker, currentTimeMs());
        int ready = poll(worker->fds, worker->count + 1, timeoutMs);
        if (ready < 0){
            if (errno != EINTR){
                perror("poll");
                sleep(1); /* avoid spinning on a persistent failure */
            }
            continue;
        }

        int64_t nowMs = currentTimeMs();

        /* From the end: removeSession moves the last session, already served,
         * so `fds` still matches every index left to visit. */
        for (size_t i = worker->count; i-- > 0;){
            if (serviceSession(worker->sessions[i], worker->fds[i + 1].revents, nowMs) != 0){
                removeSession(worker, i);
            }
        }

        if (worker->fds[0].revents & POLLIN){
            adoptNewSessions(worker, nowMs);
        }
    }
    return NULL;
}

/* Set up one worker and start its thread. Returns 0 if success, -1 otherwise. */
static int startWorker(Worker *worker){
    worker->count = 0;
    worker->capacity = INITIAL_CAPACITY;
    worker->sessions = malloc(worker->capacity * sizeof(*worker->sessions));
    worker->fds = malloc((worker->capacity + 1) * sizeof(*worker->fds));
    if (worker->sessions == NULL || worker->fds == NULL){
        fprintf(stderr, "Error: out of memory\n");
        return -1;
    }

    worker->queueHead = NULL;
    worker->queueTail = NULL;
    int error = pthread_mutex_init(&worker->queueLock, NULL);
    if (error != 0){
        fprintf(stderr, "Error: cannot create a mutex: %s\n", strerror(error));
        return -1;
    }

    int pipeFds[2];
    if (pipe(pipeFds) != 0){
        perror("pipe");
        return -1;
    }
    worker->wakeRead = pipeFds[0];
    worker->wakeWrite = pipeFds[1];
    if (setNonBlocking(worker->wakeRead) != 0 || setNonBlocking(worker->wakeWrite) != 0){
        perror("fcntl");
        return -1;
    }

    pthread_t thread;
    error = pthread_create(&thread, NULL, workerThread, worker);
    if (error != 0){
        fprintf(stderr, "Error: cannot create a worker thread: %s\n", strerror(error));
        return -1;
    }
    pthread_detach(thread);
    return 0;
}

int startWorkers(void){
    for (int i = 0; i < WORKER_COUNT; i++){
        if (startWorker(&workers[i]) != 0){
            return -1;
        }
    }
    return 0;
}

void assignSession(Session *session){
    static int nextWorker = 0;
    Worker *worker = &workers[nextWorker];
    nextWorker = (nextWorker + 1) % WORKER_COUNT;

    pthread_mutex_lock(&worker->queueLock);
    if (worker->queueTail == NULL){
        worker->queueHead = session;
    } else {
        worker->queueTail->next = session;
    }
    worker->queueTail = session;
    pthread_mutex_unlock(&worker->queueLock);

    /* A full pipe (EAGAIN) is fine: the worker is already going to wake up. */
    char byte = 1;
    if (write(worker->wakeWrite, &byte, 1) < 0 && errno != EAGAIN){
        perror("write");
    }
}
