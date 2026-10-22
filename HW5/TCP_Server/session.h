#ifndef SESSION_H
#define SESSION_H

/**
 * @function handleClient: Serve one connected client until it disconnects:
 *           send the welcome code, then answer each USER / POST / BYE
 *           request with its result code. The login state belongs to this
 *           connection only, so the same account may be logged in on other
 *           clients at the same time. Unknown or too long requests get
 *           "300" and the session goes on.
 *
 * @param connFd: The connected client socket (closed by the caller).
 * @param clientAddress: Client address as "IPAddress:PortNumber".
 * @param accountPath: Path of the account file.
 */
void handleClient(int connFd, const char *clientAddress, const char *accountPath);

#endif
