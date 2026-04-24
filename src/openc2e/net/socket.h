#pragma once

#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int SOCKET;
#define INVALID_SOCKET ((SOCKET)-1)
#endif

int sockinit();
int sockquit();
int sockgetlasterror();
void sockdestroy(SOCKET sock);
SOCKET sockcreatetcplistener(uint32_t host, uint16_t port);
SOCKET sockacceptnonblocking(SOCKET sock);
uint32_t sockgetpeeraddress(SOCKET sock);
int sockrecvblocking(SOCKET sock, char* buf, size_t len, int flags);
int socksendblocking(SOCKET sock, const char* buf, size_t len, int flags);