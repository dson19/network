# Lecture 06 — I/O multiplexing servers

Source: `Lec06.IOMultiplexing.pdf`

## Readiness and select

An event loop can wait for activity on a listener and on many client sockets in one call. Readiness means an operation can make progress or discover an endpoint condition. For example, a readable TCP socket may hold new bytes, indicate the peer has closed its writing side, or expose a pending error. A readable listener has an arrival that can be accepted. A writable socket has room for some data or has completed a connection attempt. It may take further calls to finish sending.

`select(nfds, &readfds, &writefds, &exceptfds, &timeout)` monitors descriptor sets. Initialize sets with `FD_ZERO`, register and remove descriptors with `FD_SET` and `FD_CLR`, and inspect results with `FD_ISSET`. Pass the highest monitored descriptor plus one as `nfds`, and a null pointer for any unused set. A null timeout waits indefinitely, zero checks immediately, and a positive `timeval` bounds the wait. The call returns a positive readiness count, zero for a timeout, and -1 on error. On return, the sets contain only ready descriptors. Keep a persistent set and pass copies, and initialize the timeout again for each call that needs one. Descriptors must fit within `FD_SETSIZE`.

Conceptually the kernel checks the requested descriptors, waits if none are ready and time remains, then returns the ready subset. In the lecture's two-socket example, the program adds both to the read set, sets a 10.5-second timeout, and receives from each socket that is flagged. Each flagged socket needs its own receive call with its own descriptor.

## TCP echo event loop

1. Add the listening descriptor to the master read set and track the highest descriptor.
2. Copy the set and wait. If the listener is readable, accept the new connection and add it to the master set.
3. Inspect readable client descriptors. Echo data when bytes arrive. Remove and close a descriptor when the client finishes or a fatal error occurs.
4. Return to the wait with the updated set. Handle interruption by rebuilding the call's inputs.

A simple teaching example can send inline. A slow client can make a blocking send stall the entire event loop. A nonblocking design retains unsent bytes per connection and monitors write readiness only while output is pending. It likewise keeps partial input until framing allows a complete request.

## poll

`poll` monitors an array of `struct pollfd` records. Each record contains a descriptor, requested `events`, and returned `revents`. The count is the number of entries in the array, independent of the highest descriptor value. A negative descriptor disables an entry. The timeout is in milliseconds, with zero for an immediate check and a negative value for an indefinite wait. The return value counts entries with events, returns zero on timeout, or -1 on error.

Use `POLLIN` for readable input, `POLLOUT` for writable output, and `POLLPRI` for priority data. Check returned `POLLERR`, `POLLHUP`, and `POLLNVAL` even when not requested. A hangup and readable buffered data can be reported together. The lecture demonstrates waiting for input on one socket and output on another in the same call. Compared with `select`, `poll` keeps requested and returned events in distinct fields and scales its input to the entries supplied.
