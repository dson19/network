# Lecture 09 — Remote procedure calls

Source: `Lec09.RPC.pdf`

## Programming model and stubs

A local procedure call transfers control and arguments inside one program. RPC exposes a remote service through a similar calling interface even though execution occurs in another process and can occur on another machine. The client stub represents the remote operation locally. It encodes the operation and arguments, sends a request, waits for a reply, decodes the result, and returns it to the caller. On the other side, the server dispatcher selects a server stub, which decodes the request, invokes the local implementation, and encodes the result for the reply. The application mostly deals with typed procedure interfaces, while the stubs and runtime handle messages, encoding, and dispatch.

Remote calls have network latency and partial failures. The client can remain alive while the server, connection, or reply path fails. A timeout therefore leaves uncertainty about whether the request was lost, whether the function is still running, or whether it finished and its reply was lost. This matters most when a repeated operation changes state.

## Retries and call semantics

Retrying can produce at-least-once execution under conditions that let a request eventually reach a working server. A repeated call can execute more than once, so this works most naturally with idempotent operations. At-most-once execution uses request identifiers and server-side duplicate detection. A retransmitted identifier can receive a cached reply instead of triggering another execution. At-most-once permits zero executions. If the server loses its duplicate history after restarting, handling duplicates across that failure requires durable state or another application-specific mechanism. The client still needs to account for an unknown outcome when it never gets a reply.

## Marshalling, interfaces, and binding

Marshalling encodes arguments and results into a transferable format. Peers may differ in integer representation, byte order, alignment, and string conventions. A wire format defines a shared representation. Pointers contain addresses in the sender's address space. A stub can serialize pointed-to data explicitly, and an interface can describe input, output, or input/output values so the caller can copy results back. Shared memory identity and arbitrary aliasing do not pass automatically through the network.

An interface definition language declares procedures and data types in a form a tool can use to generate stubs and serialization routines. Binding identifies a reachable instance of the interface and connects the client to the appropriate endpoint. The server can register its service with a directory or binder. The client looks up the service and version, obtains an endpoint, and then sends calls to that endpoint. The lecture also compares Java RMI, which exposes Java objects and methods remotely, and CORBA, a language-neutral distributed object system. Web RPC frameworks preserve this general process while using encodings and transports suitable for Web applications, such as XML-RPC or JSON-RPC.

## Sun ONC RPC

ONC RPC identifies a remote operation with a program number, version number, and procedure number. The input and output types and those identifiers are declared in a `.x` file. Running `rpcgen` generates the common header, client stubs, server dispatch code, and XDR serialization code. The server registers the program and version with the local port mapper or `rpcbind`. The client resolves the service on the target host, and the runtime uses the registered address to contact it.

In the lecture's addition example, the interface groups two integers in one argument record and declares a procedure returning the sum. The server implements the generated service entry point. The client creates a handle for the host, program, version, and transport, fills the two fields, invokes the generated client function, and checks whether it got a result or an RPC failure before using the returned value.
