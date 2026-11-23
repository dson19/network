# Lecture 07 — Advanced socket I/O

Source: `Lec07.AdvancedIO.pdf`

## Nonblocking descriptors

A blocking call can sleep while waiting for a connection request, bytes, or room in the send buffer. Setting `O_NONBLOCK` changes calls that would otherwise wait. Read the existing flags with `fcntl(fd, F_GETFL)` and set the augmented flags with `fcntl(fd, F_SETFL, flags | O_NONBLOCK)` so other status flags are preserved. The lecture also shows `ioctl` with `FIONBIO` as an alternative on platforms that provide it.

Check each result. A positive count is progress and may cover only part of the request. A positive-length TCP read returning zero means the peer has finished sending. A return of -1 with `EAGAIN` or `EWOULDBLOCK` usually means wait for readiness and try again. Other outcomes, such as interruption and connection errors, have their own handling. Busy retrying wastes CPU, so combine nonblocking sockets with a wait on readiness. The lecture's echo flow registers the listener, accepts new connections when ready, registers each client, and removes a client on EOF. Nonblocking behavior also prevents a stale readiness indication from unexpectedly putting the event loop to sleep.

## Signal-driven I/O

Signal-driven I/O separates registering for notification from moving the bytes. First install a `SIGIO` handler. Then identify the notification owner with `fcntl` and `F_SETOWN`, and enable `O_ASYNC`, preserving other flags. The lecture also shows the corresponding `ioctl` style using ownership and asynchronous-notification requests. Details depend on the platform.

On UDP, a notification can mean a datagram or an asynchronous error is pending. For TCP a notification can have several meanings, including arrivals at a listener, data, output readiness, and connection state changes. Signals can coalesce. The receiving logic should drain available datagrams until no further progress is possible. Keep asynchronous signal handlers limited to signal-safe operations. For example, a handler can notify the main loop, which then performs the socket I/O.

## Time limits and vectored I/O

The lecture compares alarms, readiness waits with a timeout, and `SO_RCVTIMEO`/`SO_SNDTIMEO` for blocking socket operations. A per-call timeout and a deadline for an entire exchange are different. The lecture illustrates an alarm-based connect wrapper that arranges for `SIGALRM` to interrupt a blocking attempt, then cancels the alarm and restores the previous handler. That approach depends on signal restart behavior and uses a process-wide timer. For a bounded TCP connect, set nonblocking mode, call `connect`, and handle `EINPROGRESS` by waiting for completion readiness. Read `SO_ERROR` with `getsockopt` to determine whether the attempt succeeded or failed. Close an expired attempt and restore the original flags on a successful socket if its caller expects blocking behavior.

`readv` and `writev` scatter and gather bytes over an array of `iovec` entries. Each entry supplies a base address and length. A single write can gather a protocol header and payload from separate buffers. Its count covers bytes across the vector, so resume at the correct buffer and offset after a short write. Scatter/gather does not create records across a TCP stream.

`recvmsg` and `sendmsg` extend vectored I/O for sockets using `msghdr`. The structure combines an optional protocol address, an I/O vector, and a control buffer for ancillary data. A receiver also gets message flags, including indications of truncation. A UDP sender can encode a header, payload, and trailer in separate buffers, pass the destination through `msg_name`, and send all segments as one datagram using `sendmsg`.
