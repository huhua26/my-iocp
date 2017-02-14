#include "StdAfx.h"
#include "IOCPTcpFunc.h"
#include "TcpLink.h"
#include "AllocContextMgr.h"
#include <list>

HANDLE g_hIOCP = NULL;
std::list<HANDLE> g_HandleList;
std::list<DWORD> g_ThreadIdList;

HANDLE  g_hTimeoutThread;
volatile bool g_bTimeOutThreadQuit;

bool __stdcall ms_iocp_tcp_init()
{
	WSADATA wsd;
	if( WSAStartup( MAKEWORD(2, 2), &wsd ) != 0 ){
		return false;
	}

	std::string str_send_name = "SendAlloc";
	std::string str_recv_name = "RecvAlloc";
	std::string str_accept_name = "AcceptAlloc";
	CSingletonAllocSend::Instance()->MgrName( str_send_name );
	CSingletonAllocRecv::Instance()->MgrName( str_recv_name );
	CSingletonAllocAccept::Instance()->MgrName( str_accept_name );

	g_hIOCP = ::CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, NULL, 0 );
	if( NULL == g_hIOCP ){
		//创建失败
		return false;
	}

	SYSTEM_INFO si;
	GetSystemInfo( &si );
	int i_thread_count = si.dwNumberOfProcessors * 2;
	for( int i = 0; i < i_thread_count; i++ ){
		DWORD dwThreadId = 0;
		HANDLE hThread = CreateThread( NULL, 0, IOCPThreadProc, (LPVOID)g_hIOCP, 0, &dwThreadId );
		g_HandleList.push_back( hThread );
	}

	g_bTimeOutThreadQuit = false;
	g_hTimeoutThread = CreateThread( NULL, 0, TimeoutThreadProc, NULL, 0, NULL );
	
	return true;
}

void __stdcall ms_iocp_tcp_uninit()
{
	if( NULL == g_hIOCP ){
		return;
	}

	//停止完成端口线程池
	int nThreadCount = g_HandleList.size();
	for( int i = 0; i < nThreadCount; i++ ){
		PostQueuedCompletionStatus( g_hIOCP, 0, 0, 0 );
	}
	WaitForSingleObject( g_hIOCP, INFINITE );

	std::list<HANDLE>::iterator it = g_HandleList.begin();
	for( ; it != g_HandleList.end(); it++ ){
		//等待线程结束，关闭句柄
		WaitForSingleObject( *it, INFINITE );
		CloseHandle( *it );
	}

	g_HandleList.clear();
	g_ThreadIdList.clear();

	//停止Timeout线程
	g_bTimeOutThreadQuit = true;
	WaitForSingleObject( g_hTimeoutThread, INFINITE );

	//释放link_map
	MsTcpLink::free_all_link_map();

	//关闭句柄
	CloseHandle( g_hIOCP );
	g_hIOCP = NULL;

	CSingletonAllocSend::Release();
	CSingletonAllocRecv::Release();
	CSingletonAllocAccept::Release();

	WSACleanup();
	
	return;
}

HTCPLINK __stdcall ms_tcp_create( TcpIOCallBack backFunc, const char* p_localAddr, unsigned short us_localPort )
{
	if( NULL == backFunc ){
		return INVALID_HTCPLINK;
	}
	MsTcpLink *pLink = new MsTcpLink();
	if( NULL == pLink ){
		return INVALID_HTCPLINK;
	}
	if( pLink->link_create( backFunc, p_localAddr, us_localPort ) ){
		CRefCountPtr<MsTcpLink> p_reflink = CRefCountPtr<MsTcpLink>( pLink );
		HTCPLINK h_link = INVALID_HTCPLINK;
		MsTcpLink::add_tcp_link( h_link, p_reflink );
		p_reflink->set_link( h_link );
		p_reflink->send_link_event( EVT_TCP_CREATED );

		return h_link;
	} else {
		delete pLink;
	}

	return INVALID_HTCPLINK;
}

bool __stdcall ms_tcp_destroy( HTCPLINK h_link )
{
	CRefCountPtr<MsTcpLink> pLink;
	if( !MsTcpLink::find_link( h_link, pLink ) ){
		return false;
	}

	pLink->link_destroy( true );
	return true;
}

bool __stdcall ms_tcp_connect( HTCPLINK h_link, const char* p_addr, unsigned short us_port )
{
	CRefCountPtr<MsTcpLink> pLink;
	if( !MsTcpLink::find_link( h_link, pLink ) ){
		return false;
	}

	return pLink->link_connect( p_addr, us_port );	
}

bool __stdcall ms_tcp_listen( HTCPLINK h_link, unsigned int ui_block )
{
	if( (ui_block< 5) || (ui_block > 16) ){
		return false;
	}

	CRefCountPtr<MsTcpLink> pLink;
	if( !MsTcpLink::find_link( h_link, pLink ) ){
		return false;
	}

	return pLink->link_listen( ui_block );
}

bool __stdcall ms_tcp_send( HTCPLINK h_link, const char* p_data, unsigned int ui_size )
{
	CRefCountPtr<MsTcpLink> pLink;
	if( !MsTcpLink::find_link( h_link, pLink ) ){
		return false;
	}

	return pLink->link_send( p_data, ui_size );
}

bool __stdcall ms_tcp_get_link_addr( HTCPLINK h_link, unsigned int ui_type, unsigned int *pui_ip, unsigned short *pus_port )
{
	CRefCountPtr<MsTcpLink> pLink;
	if( !MsTcpLink::find_link( h_link, pLink ) ){
		return false;
	}

	return pLink->get_link_addr( ui_type, pui_ip, pus_port );
}