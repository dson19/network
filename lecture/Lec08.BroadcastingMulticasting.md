# Lecture 08 — IPv4 broadcasting and multicasting

Source: `Lec08.BroadcastingMulticasting.pdf`

## UDP broadcast

Broadcast sends a datagram to devices in a broadcast domain. The limited broadcast address `255.255.255.255` is local to the network. A subnet-directed broadcast uses the subnet prefix with all host bits set. Whether it can be forwarded across routers depends on configuration. All reachable hosts on the broadcast network can see the packet, but only sockets listening on the destination UDP port receive it in their applications.

The example creates an IPv4 datagram socket, enables `SO_BROADCAST` at level `SOL_SOCKET`, then uses `sendto` with a broadcast IP address and an agreed port. A receiver normally binds the port on a wildcard address. Broadcast is convenient for local discovery and has the delivery semantics of UDP. The lecture’s sender opens a five-second window after each broadcast and collects replies from multiple responders, using the source endpoint returned by `recvfrom` to tell them apart.

## Multicast groups

Multicast addresses name receiver groups. IPv4 reserves `224.0.0.0/4` for multicast. A sender can send to a group without being a member, while a receiver joins a group on an interface to receive traffic from that group. Hosts and multicast-capable network infrastructure use membership and forwarding state to limit delivery to interested networks. Membership, port binding, and packet delivery are separate concerns.

At socket option level `IPPROTO_IP`, three options configure outgoing traffic:

- `IP_MULTICAST_IF` selects the outgoing interface.
- `IP_MULTICAST_TTL` sets the time-to-live, which limits how far routers can forward packets.
- `IP_MULTICAST_LOOP` controls whether outgoing packets are also delivered to local group members.

Membership options change receive behavior. `IP_ADD_MEMBERSHIP` and `IP_DROP_MEMBERSHIP` join and leave a group on an interface using an `ip_mreq`. `IP_ADD_SOURCE_MEMBERSHIP` and `IP_DROP_SOURCE_MEMBERSHIP` add or remove a specific sender for a source-filtered join. `IP_BLOCK_SOURCE` and `IP_UNBLOCK_SOURCE` exclude and restore a sender in ordinary group membership. The source-specific options use a structure with group, source, and interface addresses.

## Sender and receiver sequence

The sender creates a UDP socket, sets outgoing multicast options, and sends to the group IP and destination port. The receiver creates a UDP socket, enables reuse before binding if several local receivers need the same port and the platform supports that arrangement, binds to the agreed port, and joins the group on the desired interface. It then reads datagrams and optionally inspects the source with `recvfrom`. Drop the membership explicitly when listening should end before the socket closes. A successful send leaves delivery dependent on membership, the selected interface, network support, scope, and UDP delivery.
