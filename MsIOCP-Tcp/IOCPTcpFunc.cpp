#include "IOCPTcpFunc.h"
#include "TcpLink.h"
#include "AllocContextMgr.h"

extern volatile bool g_bTimeOutThreadQuit;

void IOCPCompletionFunc(DWORD dwStatus, DWORD dwBytes, LPOVERLAPPED pOvlp)
{
	if (pOvlp == NULL)
		return;

	SOvlpIOContext* pContext = CONTAINING_RECORD(pOvlp, SOvlpIOContext, ovlp);
	SOvlpSendContext* pSend = (SOvlpSendContext*)pContext;
	SOvlpRecvContext* pRecv = (SOvlpRecvContext*)pContext;
	SOvlpAcceptContext* pAccept = (SOvlpAcceptContext*)pContext;

	CRefCountPtr<MsTcpLink> pLink;
	if( !MsTcpLink::find_link( pContext->hHandle, pLink ) ){
		if( pContext->byIORType == IOREQ_SEND ){
			CSingletonAllocSend::Instance()->FreeContext(pSend);
		} else if( pContext->byIORType == IOREQ_RECV ){
			CSingletonAllocRecv::Instance()->FreeContext(pRecv);
		} else if( pContext->byIORType == IOREQ_ACCEPT ){
			CSingletonAllocAccept::Instance()->FreeContext(pAccept);
		}

		return;
	}

	if (dwStatus == NO_ERROR) {
		pContext->dwBytes = dwBytes;
		if ( dwBytes == 0 ) {
			if (pContext->byIORType == IOREQ_SEND){
				pLink->on_send_error( pSend, dwStatus );
			} else if (pContext->byIORType == IOREQ_RECV){
				pLink->on_recv_error( pRecv, dwStatus );
			} else if (pContext->byIORType == IOREQ_ACCEPT){
				pLink->on_accept( pAccept );
			}	
		} else {
			if (pContext->byIORType == IOREQ_SEND){
				pLink->on_send( pSend );
			} else if (pContext->byIORType == IOREQ_RECV){
				pLink->on_recv( pRecv );
			} else if (pContext->byIORType == IOREQ_ACCEPT){
				pLink->on_accept( pAccept );
			}	
		}
	} else {
		if (pContext->byIORType == IOREQ_SEND){
			pLink->on_send_error( pSend, dwStatus );
		} else if (pContext->byIORType == IOREQ_RECV){
			pLink->on_recv_error( pRecv, dwStatus );
		} else if (pContext->byIORType == IOREQ_ACCEPT){
			pLink->on_accept_error( pAccept, dwStatus );
		}			
	}

	return;
}

DWORD WINAPI TimeoutThreadProc(LPVOID lpParam)
{
	while( !g_bTimeOutThreadQuit ){
		MsTcpLink::check_link_timeout();
		Sleep( 1000 );			
	}

	return 0;
}

DWORD WINAPI IOCPThreadProc( LPVOID lpParam )
{
	LPOVERLAPPED pOvlp = NULL;
	DWORD dwStatus = 0;
	DWORD dwBytes = 0;
	DWORD pfnCompletionKey = NULL;
	HANDLE hIOCP = (HANDLE)lpParam;

	while( true ){
		if( GetQueuedCompletionStatus( hIOCP, &dwBytes, &pfnCompletionKey, &pOvlp, INFINITE ) ){
			dwStatus = ERROR_SUCCESS;
		} else {
			dwStatus = GetLastError();
		}

		if( NULL == pfnCompletionKey ){
			break;
		}

		((IOCPCallBack)pfnCompletionKey)( dwStatus, dwBytes, pOvlp );
	}

	return -1;
}

DWORD IssueSend(SOvlpSendContext* pContext)
{
	DWORD dwRet = 0;
	WSABUF wsaBufs[1];
	wsaBufs[0].buf = (char*)pContext->data;
	wsaBufs[0].len = pContext->dwBytes;
	pContext->dwFlags = 0;
	pContext->byIORType = IOREQ_SEND;

	DWORD dwBytes = pContext->dwBytes;
	if( WSASend( pContext->hSock, wsaBufs, 1, &dwBytes, pContext->dwFlags, &pContext->ovlp, NULL ) == SOCKET_ERROR ){
		dwRet = WSAGetLastError();
	}

	return dwRet;
}

DWORD IssueRecv(SOvlpRecvContext* pContext)
{
	DWORD dwRet = 0;
	DWORD dwFlags = 0;
	WSABUF wsaBufs[1];
	wsaBufs[0].buf = (char*)pContext->data;
	wsaBufs[0].len = IOBUF_RECVMAXSIZE;
	pContext->dwFlags = 0;
	pContext->byIORType = IOREQ_RECV;

	DWORD dwBytes = 0;
	if ( WSARecv( pContext->hSock, wsaBufs, 1, &dwBytes, &dwFlags, &pContext->ovlp,NULL ) == SOCKET_ERROR ){
		dwRet = WSAGetLastError();
	}

	return dwRet;
}

DWORD IssueAccept(SOvlpAcceptContext* pContext)
{
	DWORD dwRet = 0;
	SOCKET hSock = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP,  NULL, 0, WSA_FLAG_OVERLAPPED );
	if (hSock == INVALID_SOCKET){
		dwRet = WSAGetLastError(); 
	} else {
		pContext->hAcceptSock = hSock;
		pContext->byIORType = IOREQ_ACCEPT;

		DWORD dwBytes;
		dwBytes = pContext->dwBytes;
		if (!AcceptEx(pContext->hSock, 
			pContext->hAcceptSock, 
			pContext->data, 
			0, 
			sizeof(SOCKADDR_IN) + 16,
			sizeof(SOCKADDR_IN) + 16,
			&dwBytes, 
			&pContext->ovlp)){
			//error
			dwRet = WSAGetLastError();
		}			
	}

	return dwRet;
}

bool NotifyIOCallback(HANDLE hIOCP, HTCPLINK hLink, BYTE byIORType)
{
	SOvlpIOContext *pContext = NULL;
	if( byIORType == IOREQ_SEND ){
		pContext = CSingletonAllocSend::Instance()->GetContext();	
	} else if( byIORType == IOREQ_RECV ){
		pContext = CSingletonAllocRecv::Instance()->GetContext();
	} else if( byIORType == IOREQ_ACCEPT ){
		pContext = CSingletonAllocAccept::Instance()->GetContext();
	}

	bool bRet = false;
	if( NULL == pContext ){
		return bRet;
	}
	pContext->hHandle = hLink;
	pContext->byIORType = byIORType;
	if( PostQueuedCompletionStatus( hIOCP, IOREQ_NOTIFY_CALL, (DWORD)IOCPCompletionFunc, &pContext->ovlp ) ){
		bRet = true;
	} else {
		if( byIORType == IOREQ_SEND ){
			CSingletonAllocSend::Instance()->FreeContext( (SOvlpSendContext*)pContext );
		} else if( byIORType == IOREQ_RECV ){
			CSingletonAllocRecv::Instance()->FreeContext( (SOvlpRecvContext*)pContext );
		} else if( byIORType == IOREQ_ACCEPT ){
			CSingletonAllocAccept::Instance()->FreeContext( (SOvlpAcceptContext*)pContext );
		}
	}

	return bRet;
}

bool BindIOCPSocket(SOCKET hBind, HANDLE hIOCP, DWORD dwCPKey)
{
	HANDLE h = CreateIoCompletionPort( (HANDLE)hBind, hIOCP, dwCPKey, 0 );
	return h == hIOCP;
}

SOCKET TCPSocket(const char* pLocalAddr, unsigned short nLocalPort)
{
	SOCKADDR_IN saLocal;
	SOCKET hSock = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED );
	int nLen = sizeof( SOCKADDR_IN );
	saLocal.sin_family = AF_INET;
	saLocal.sin_port = htons( nLocalPort );
	if (pLocalAddr == NULL){
		saLocal.sin_addr.s_addr = htonl(INADDR_ANY);
	} else{
		saLocal.sin_addr.s_addr = inet_addr(pLocalAddr);
	}

	bool bNoDelay = true;
	setsockopt(hSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&bNoDelay, sizeof(bNoDelay)); 
	bool bReuseAddr = false;
	setsockopt(hSock, SOL_SOCKET, SO_REUSEADDR,(const char*)&bReuseAddr,sizeof(bReuseAddr));
	//int nRecvBufSize = 1024*32;
	//setsockopt(hSock, SOL_SOCKET, SO_RCVBUF, (const char*)&nRecvBufSize, sizeof(int));
	//setsockopt(hSock, SOL_SOCKET, SO_SNDBUF, (const char*)&nSendBufSize, sizeof(int));	

	//
	// 强制在接收/发送缓冲区有数据执行的过程中， 关闭TCP链接， 规避closescoket在忙碌状态下， 卡死在ZwClose的系统错误
	//
	struct linger lgr;
	lgr.l_onoff = 1;
	lgr.l_linger = 0;
	if (0 != setsockopt(hSock, SOL_SOCKET, SO_LINGER, (const char *)&lgr, sizeof(struct linger))){
		closesocket(hSock);
		return INVALID_SOCKET;
	}

	int nRet = bind(hSock, (SOCKADDR *)&saLocal, nLen);
	if (nRet == SOCKET_ERROR){
		closesocket(hSock);
		return INVALID_SOCKET;
	}

	return hSock;
}