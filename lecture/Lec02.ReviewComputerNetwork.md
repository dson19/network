# Lecture 02 — Internet protocols and services

Source: `Lec02.ReviewComputerNetwork.pdf`

## Layering and application architecture

End systems run applications, access links connect them to networks, and routers forward packets between networks. A protocol defines the format and order of messages and the actions taken when sending, receiving, or reacting to events. The lecture uses a five-layer model: application, transport, network, link, and physical. Sending down the stack adds protocol information around the data. The destination removes each layer's information and delivers the application message. A router forwards at the network layer and uses link and physical services for each hop.

An application consists of processes on end systems that exchange messages. In client/server designs, a server listens at a known endpoint and clients initiate contact. Peers in a peer-to-peer design can both request and provide services. Hybrid designs can use a central service for discovery and direct peer connections for data. A socket is the process's interface to transport services. Processes on distinct hosts need an IP address to identify a destination interface and a transport port to identify its endpoint.

An application protocol specifies message types, field syntax and meaning, and rules for valid exchanges. Application requirements determine the transport choice. File transfers prioritize integrity, while real-time applications may trade loss tolerance for timely delivery. Throughput, latency, and reliability are separate requirements.

## TCP, UDP, and demultiplexing

Transport provides logical process-to-process communication above IP's host-to-host delivery. The sender multiplexes data from several sockets. The receiver demultiplexes incoming segments to the right socket using header fields.

- **UDP** sends distinct datagrams with low protocol overhead and no connection handshake. Delivery, order, and duplicate suppression are not guaranteed. In the lecture's basic unconnected model, the local address and destination port select the receiving socket, so datagrams from different senders can arrive through the same descriptor. The application can obtain the sender's address and port per datagram.
- **TCP** establishes a connection with a SYN, SYN-ACK, and ACK exchange. It presents a reliable, ordered byte stream, with flow control protecting the receiver and congestion control adapting to network conditions. A TCP connection is distinguished by the source IP and port together with the destination IP and port. A server can therefore have many active connections using the same local port.

## IP forwarding, addressing, and NAT

Forwarding is the local act of moving a packet from an input to an output. Routing computes paths and supplies the information used for forwarding. IP gives heterogeneous networks a common packet and addressing layer. IPv4 uses 32-bit addresses written as four decimal octets. Historical classful addressing used fixed network-prefix lengths. CIDR explicitly gives a prefix length, as in `192.168.1.0/24`, and allows address allocation and routing aggregation at different sizes. Routers generally prefer the longest matching prefix.

Private IPv4 ranges are `10.0.0.0/8`, `172.16.0.0/12`, and `192.168.0.0/16`. Many private hosts can share a public IPv4 address through network address and port translation. For an outbound flow, the NAT records a mapping from the internal address and port to a public address and translated port, then rewrites the packet. A corresponding reply is looked up and rewritten back toward the internal host.

An inbound connection needs a way to reach an internal endpoint. The lecture compares manual port forwarding, automatic mapping through UPnP, relaying through a publicly reachable host, and NAT hole punching. Hole punching coordinates outbound traffic from both peers so compatible NATs create usable mappings. STUN can help discover the public-facing address and port. Its success depends on NAT behavior, so a relay can be necessary.
