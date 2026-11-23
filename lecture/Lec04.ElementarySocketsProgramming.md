# Lecture 04 — Elementary socket programming

Source: `Lec04.ElementarySocketsProgramming.pdf`

## Creating, configuring, and closing a socket

`socket(domain, type, protocol)` creates a descriptor. An Internet program usually selects IPv4 or IPv6 and a stream or datagram socket, with protocol zero requesting the default for that combination. `bind` assigns a local address and port. A wildcard address listens on eligible local interfaces, and port zero requests an ephemeral port. A client that omits an explicit bind usually gets an endpoint automatically.

Use `setsockopt` and `getsockopt` at the appropriate level with a value and its size. At `SOL_SOCKET`, `SO_BROADCAST` allows broadcast transmission, `SO_DONTROUTE` limits routing for outbound traffic, `SO_KEEPALIVE` enables TCP keepalive probes, and `SO_REUSEADDR` affects bind reuse. `SO_LINGER` controls close behavior with outstanding data on a connected socket. `SO_RCVTIMEO` and `SO_SNDTIMEO` limit waiting in relevant blocking I/O calls.

`shutdown` disables reception, transmission, or both on a connected socket. A TCP write shutdown lets the peer observe end of stream after queued data while the local process can still read. `close` relinquishes the descriptor. If a forked process or duplicate descriptor still references the socket, the underlying socket can remain open.

## UDP echo

A UDP server binds once. For each request it calls `recvfrom` with a buffer and address storage, then passes the returned sender endpoint to `sendto` when replying. A UDP client creates a datagram socket, sends a message to the server's advertised address, and receives the echo. Use the actual byte count when echoing. A zero-length UDP datagram is valid, and an oversized incoming datagram can be truncated to the receive buffer.

UDP `connect` records a default peer, permits `send` and `recv`, and filters reception by peer. It does not introduce a transport handshake or delivery guarantee. `sendto` submitting a datagram successfully means the local stack accepted it. Delivery confirmation, retransmission, and time limits remain application concerns.

## TCP echo and stream framing

The TCP server sequence is `socket → bind → listen → accept`. The backlog controls queuing of connection requests, and `accept` yields a new connected descriptor while the listener remains available. An iterative server handles one connection to completion before accepting the next. The TCP client's sequence is `socket → connect → exchange bytes → close`. In the echo example, the server returns exactly the received bytes, then closes the connected descriptor and returns to its accept loop.

Check every I/O return value. `send` can write fewer bytes than requested, so a complete write advances by the count sent and continues with the remaining bytes. `recv` returns available bytes up to the limit, zero for a TCP peer's orderly write close when asked for a positive count, and -1 for an error. Relevant flags include `MSG_PEEK` to inspect without consuming, `MSG_WAITALL` to request a full stream read subject to EOF or errors, `MSG_DONTWAIT` for a nonblocking call where supported, `MSG_DONTROUTE`, `MSG_OOB` for exceptional data, and `MSG_NOSIGNAL` where supported to suppress SIGPIPE on a failed send.

TCP preserves byte order across the connection. A receive can return part of a message, a whole message, or bytes from several messages. For example, two sends containing “bull” and “dog” can appear as one receive containing “bulldog,” and one send of “bulldog” can arrive in several reads. Both endpoints therefore agree on framing. Fixed-size records are easy to count. Delimited records need an agreed delimiter and escaping or restrictions on payloads. Length-prefixed records require a fixed header encoding, a validated length, and enough reads to assemble the body. An incremental parser retains an incomplete tail between receives and handles several complete frames from one buffer.

## Designing the application protocol

Start with the operations users need and the information each endpoint owns. Choose an application architecture and identify environment dependencies such as DNS or configuration provided by DHCP. Define control requests, responses, errors, and data transfer. Decide how each field is encoded, how a message ends, how large it can be, and what a receiver does with unsupported or malformed input. Text protocols are easy to inspect. Binary protocols can encode fixed-width numeric fields or a sequence of type-length-value fields. A C structure in memory can contain padding, pointers, and host-order numbers, so define the wire encoding independently.

Specify sequencing as a transaction diagram or state machine. In a POP-style session, the server greets the client, authentication must succeed before mailbox commands, and a quit command ends the session. FTP authentication illustrates status replies directing the next client action. An implementation separates parsing and validation from dispatch by message type and checks that the requested operation is valid in the current state.
