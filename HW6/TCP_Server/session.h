#ifndef SESSION_H
#define SESSION_H

/**
 * @function handleClient: Serve one connected client until it disconnects:
 *           send the welcome code, then answer each USER / POST / BYE
 *           request with its result code. An account can be logged in on
 *           only one client at a time: a login to an account held by
 *           another session gets "214". The account is freed by BYE and
 *           also when the session ends for any other reason. Unknown or too
 *           long requests get "300" and the session goes on. Safe to call
 *           from several threads at once.
 *
 * @param connFd: The connected client socket (closed by the caller).
 * @param clientAddress: Client address as "IPAddress:PortNumber".
 * @param accountPath: Path of the account file.
 */
void handleClient(int connFd, const char *clientAddress, const char *accountPath);

#endif
