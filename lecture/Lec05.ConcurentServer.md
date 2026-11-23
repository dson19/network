# Lecture 05 — Concurrent TCP servers

Source: `Lec05.ConcurentServer.pdf`

## I/O models and the iterative server

An iterative TCP echo server accepts a client, reads and echoes until that session finishes, then accepts the next client. A connected client that stays idle can occupy a blocking read and delay all later clients. The lecture distinguishes when the application learns that data is ready from when the actual transfer finishes.

| Model | How the application gets data |
| --- | --- |
| Blocking | The I/O call sleeps until it can transfer data or report an event such as EOF. |
| Nonblocking | The call returns immediately when progress would require waiting. The caller decides when to retry. |
| Multiplexed | `select` or `poll` waits on several descriptors. The application then performs I/O on ready ones. |
| Signal-driven | The kernel signals readiness, and the application performs the transfer afterward. |
| Asynchronous | The application submits an operation and receives notification after the transfer has completed. |

## One child process per client

After `accept`, the parent calls `fork`. The parent closes its copy of the connected descriptor and resumes accepting. The child closes its inherited listener, handles its own connection, closes the connected descriptor, and exits. The copied descriptors initially refer to the same underlying sockets, so closing unneeded copies matters for connection lifetime. Handle a failed fork by releasing the just-accepted connection in the parent.

A terminated child leaves exit status to collect. Install a `SIGCHLD` handler and call `waitpid(-1, &status, WNOHANG)` repeatedly while it returns a positive PID. One signal can correspond to multiple exits, so a single `wait` can leave zombies, and a blocking wait in the handler can stall the server. Preserve `errno` in the handler and keep handler operations signal-safe. A signal can interrupt `accept` or another blocking call with `EINTR`. Retry when appropriate, while respecting bytes already transferred or a deliberate shutdown.

## One thread per client

The accept loop can create a worker using `pthread_create` with an argument that identifies its connection. Give each worker stable argument storage. Passing the address of a loop variable reused for each accept allows another iteration to change the value before the worker reads it. A worker owns and closes its connection, and each thread is either joined or detached so its resources are reclaimed.

Threads share the process's memory and descriptors. Unlike after a fork, the accept loop closing the shared connection descriptor would affect the worker as well. Shared mutable state needs synchronization. A mutex protects the shared state during each critical section. A condition variable lets a waiter sleep until that state may satisfy a predicate. The waiter locks the mutex, checks the predicate in a loop, and calls `pthread_cond_wait`, which releases the mutex while waiting and reacquires it before returning. A producer updates the predicate under the mutex and signals or broadcasts. The loop handles spurious wakeups and races with other waiters.

Processes offer separate address spaces and stronger isolation at the cost of coordinating shared information explicitly. Threads share data conveniently, generally have lower overhead, and can bring down the entire process if a worker fails catastrophically.
