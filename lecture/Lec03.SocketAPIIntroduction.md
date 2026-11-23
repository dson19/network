# Lecture 03 — The socket API

Source: `Lec03.SocketAPIIntroduction.pdf`

## Socket types and endpoints

A socket is an operating-system communication endpoint represented on POSIX by a descriptor. The address family determines the address representation, such as `AF_INET` for IPv4 or `AF_INET6` for IPv6. The socket type selects semantics. Internet `SOCK_STREAM` normally uses TCP, which provides ordered bytes after a connection is established. Internet `SOCK_DGRAM` normally uses UDP, which preserves datagram boundaries and lets each receive report the sender.

A TCP server creates a socket, binds a local endpoint, listens for connection requests, and accepts each connection on a new descriptor. The client creates its own socket and uses `connect` with the server's endpoint. The connected endpoints exchange data with calls such as `send` and `recv`. A UDP server typically binds and uses `recvfrom` and `sendto`; the sender can supply a different destination on each call. Lecture 04 develops these sequences and their error cases.

## Addresses and byte order

APIs accept a generic `struct sockaddr *` along with a length. Actual storage must match the family. An IPv4 `sockaddr_in` contains a family, port, and `in_addr`. An IPv6 `sockaddr_in6` contains a family, port, and `in6_addr`, with additional flow and scope fields. A length together with `sockaddr_storage` lets common code receive addresses for multiple families.

Network byte order is big-endian. Use `htons` and `htonl` when encoding 16-bit and 32-bit host-order integers, and `ntohs` and `ntohl` when decoding. A socket address's port needs network order when assigned directly. Family constants are native values. Address conversion helpers already produce network-format address bytes.

- `inet_pton` parses numeric presentation strings for a specified family. It returns 1 on success, 0 for invalid input, and -1 for an unsupported family.
- `inet_ntop` formats address bytes into a caller-provided buffer. Allocate an appropriate size, such as `INET_ADDRSTRLEN` or `INET6_ADDRSTRLEN`.
- `inet_aton`, `inet_addr`, and `inet_ntoa` are traditional IPv4 interfaces. `inet_addr` has an ambiguous failure sentinel, and `inet_ntoa` returns storage that can be overwritten on a subsequent call.

## DNS, host and service lookups

IPv4 address scarcity motivates IPv6's larger 128-bit space. DNS provides distributed name resolution through a hierarchy of domains and authoritative servers. A recursive resolver follows referrals from the root through the relevant top-level and authoritative servers, then caches answers according to their TTL. An iterative query can return a referral for the caller to follow. DNS commonly uses UDP port 53 and can also use TCP. Resource records carry a name, type, class, TTL, data length, and type-specific data. A host name can resolve to several addresses. A and AAAA records carry IPv4 and IPv6 addresses respectively. Reverse DNS uses PTR records, and a reverse lookup can fail even for a usable numeric address.

Use `getaddrinfo(node, service, hints, &results)` to resolve a host and numeric or named service into candidate socket endpoints. Hints describe family, type, and flags. `AF_UNSPEC` allows both Internet families, and `AI_PASSIVE` with a null node gives wildcard addresses for binding. Each result includes family, type, protocol, address, and length. Try candidates until socket creation and connection or binding succeed, then release the list with `freeaddrinfo`. Resolver errors use `gai_strerror`.

`getnameinfo` goes in the other direction, formatting a socket address into host and service strings. Numeric flags avoid name lookups. `gethostname` reads the local host name, and `getpeername` obtains the peer address already associated with a connected socket. Older host lookup calls `gethostbyname` and `gethostbyaddr` return a `hostent` with a canonical name, aliases, address type and length, and address list. `getservbyname` and `getservbyport` look up a service in the system service database. Its `servent` includes the protocol and port in network order.
