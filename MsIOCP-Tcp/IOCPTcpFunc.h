#ifndef __IOCP_TCP_FUNC_H_
#define __IOCP_TCP_FUNC_H_

#include "TcpOvlp.h"

typedef void (*IOCPCallBack)( DWORD dwStatus, DWORD dwBytes, LPOVERLAPPED pOvlp );
void IOCPCompletionFunc(DWORD dwStatus, DWORD dwBytes, LPOVERLAPPED pOvlp);

DWORD WINAPI TimeoutThreadProc(LPVOID lpParam);
DWORD WINAPI IOCPThreadProc( LPVOID lpParam );
DWORD IssueSend(SOvlpSendContext* pContext);
DWORD IssueRecv(SOvlpRecvContext* pContext);
DWORD IssueAccept(SOvlpAcceptContext* pContext);
bool NotifyIOCallback(HANDLE hIOCP, HTCPLINK hLink, BYTE byIORType); 
bool BindIOCPSocket(SOCKET hBind, HANDLE hIOCP, DWORD dwCPKey);
SOCKET TCPSocket(const char* pLocalAddr, unsigned short nLocalPort);

#endif