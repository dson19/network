#ifndef WORKER_H
#define WORKER_H

#include "session.h"

/* Number of threads that each run their own poll() loop. */
#define WORKER_COUNT 4

/**
 * @function startWorkers: Start the worker threads. Each worker serves its
 *           share of the clients with one poll() loop, which has no limit on
 *           the number of descriptors: its arrays grow with the clients.
 *
 * @return: 0 if all the workers are running.
 *          -1 if a worker cannot be started.
 */
int startWorkers(void);

/**
 * @function assignSession: Hand a new session over to the next worker, in
 *           turn, and wake that worker up. Must be called from one thread
 *           only (the accepting thread).
 *
 * @param session: The new session; the worker now owns it.
 */
void assignSession(Session *session);

#endif
