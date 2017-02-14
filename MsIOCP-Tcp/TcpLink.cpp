#include "TcpLink.h"
#include "IOCPTcpFunc.h"
#include "SingletonHandler.h"
#include "AllocContextMgr.h"

extern HANDLE g_hIOCP;
extern std::list<DWORD> g_ThreadIdList;

MSMutex MsTcpLink::m_link_lock;
std::map<HTCPLINK, CRefCountPtr<MsTcpLink> > MsTcpLink::m_map_link;
std::map<HTCPLINK, SOCKET> MsTcpLink::m_link_socket_map;

MsTcpLink::MsTcpLink()
{
	mh_socket = INVALID_SOCKET;
	mh_link = INVALID_HTCPLINK;
	
	m_pfnIOCB = NULL;
	mn_status = LS_NotOpen;
	mui_last_alive_time = 0;
	mb_need_check_alive = false;	
}

MsTcpLink::~MsTcpLink()
{

}

void MsTcpLink::InitLinkMap()
{
	return;
}

bool MsTcpLink::find_link( HTCPLINK h_link, CRefCountPtr<MsTcpLink> &p_link )
{
	CGuardLock<MSMutex> guard( &m_link_lock );
	std::map<HTCPLINK, CRefCountPtr<MsTcpLink> >::iterator it = m_map_link.find( h_link );
	if( it == m_map_link.end() ){
		return false;
	}
	
	p_link = it->second;
	return true;
}

bool MsTcpLink::del_link( HTCPLINK h_link )
{
	CGuardLock<MSMutex> guard( &m_link_lock );
	std::map<HTCPLINK, CRefCountPtr<MsTcpLink> >::iterator it = m_map_link.find( h_link );
	if( it == m_map_link.end() ){
		return false;
	}
	m_map_link.erase( it );
	
	std::map<HTCPLINK, SOCKET >::iterator iter = m_link_socket_map.find( h_link );
	if( iter != m_link_socket_map.end() ){
		m_link_socket_map.erase( iter );
	}

	return true;
}

void MsTcpLink::free_all_link_map()
{
	std::list<CRefCountPtr<MsTcpLink> > link_list;
	do{
		CGuardLock<MSMutex> guard( &m_link_lock );
		if( m_map_link.empty() ){
			return;
		}
		std::map<HTCPLINK, CRefCountPtr<MsTcpLink> >::iterator it = m_map_link.begin();
		for( ; it != m_map_link.end(); ++it ){
			CRefCountPtr<MsTcpLink> p_link = it->second;
			link_list.push_back( p_link );
		}
	}while( 0 );

	std::list<CRefCountPtr<MsTcpLink> >::iterator it_link = link_list.begin();
	for( ; it_link != link_list.end(); ++it_link ){
		CRefCountPtr<MsTcpLink> p_link = *it_link;
		p_link->link_destroy( true );
	}

	return;
}

void MsTcpLink::check_link_timeout()
{
	std::list<CRefCountPtr<MsTcpLink> > link_list;
	do{
		CGuardLock<MSMutex> guard( &m_link_lock );
		if( m_map_link.empty() ){
			return;
		}

		unsigned int ui_tick_count = GetTickCount();
		std::map<HTCPLINK, CRefCountPtr<MsTcpLink> >::iterator it = m_map_link.begin();
		for( ; it != m_map_link.end(); ++it ){
			CRefCountPtr<MsTcpLink> p_link = it->second;
			if( !p_link->is_alive( ui_tick_count )  ){
				link_list.push_back( p_link );
			}
		}
	}while( 0 );

	std::list<CRefCountPtr<MsTcpLink> >::iterator it_link = link_list.begin();
	for( ; it_link != link_list.end(); ++it_link ){
		CRefCountPtr<MsTcpLink> p_link = *it_link;
		p_link->link_destroy( true );
	}

	return;
}

void MsTcpLink::add_tcp_link( HTCPLINK &h_link, CRefCountPtr<MsTcpLink> p_link )
{
	CGuardLock<MSMutex> guard( &m_link_lock );
	static unsigned int i_link = 0;
	h_link = (++i_link)%0x3ffffffff;
	m_map_link[h_link] = p_link;
	m_link_socket_map[h_link] = p_link->get_socket();

	return;
}

bool MsTcpLink::link_create( TcpIOCallBack iocpFunc, const char *pAddr, unsigned short nPort )
{
	m_pfnIOCB = iocpFunc;
	mh_socket = TCPSocket( pAddr, nPort );
	if( (mh_socket == INVALID_SOCKET) ||
		!BindIOCPSocket( mh_socket, g_hIOCP, (DWORD)IOCPCompletionFunc ) ){
		if( mh_socket != INVALID_SOCKET ){
			closesocket( mh_socket );
			mh_socket = INVALID_SOCKET;
		}
		return false;
	}

	mn_status = LS_Open;
	int i_name_len = sizeof( m_saLocal ) + 16;
	getsockname( mh_socket, (sockaddr*)&m_saLocal, &i_name_len );

	return true;
}

bool MsTcpLink::link_connect( const char* p_addr, unsigned short us_port )
{
	SOCKADDR_IN saAddr;
	saAddr.sin_family = AF_INET;
	saAddr.sin_port = htons( us_port );
	saAddr.sin_addr.s_addr = inet_addr( p_addr );
	if (saAddr.sin_addr.s_addr == INADDR_NONE) {
		LPHOSTENT lphost = gethostbyname( p_addr );
		if (lphost != NULL){
			saAddr.sin_addr.s_addr = ((LPIN_ADDR)lphost->h_addr)->s_addr;
		} else {
			WSASetLastError(WSAEINVAL);
			return false;
		}
	}
	int nRet = connect( mh_socket, (PSOCKADDR)&saAddr, sizeof(saAddr) );
	if (nRet == SOCKET_ERROR){
		send_link_event( EVT_TCP_DISCONNECTED );
		return false;
	}	

	memcpy( &m_saRemote, &saAddr, sizeof( SOCKADDR_IN ) );
	int nNameLen = sizeof(m_saLocal) + 16;
	getsockname( mh_socket, (sockaddr*)&m_saLocal, &nNameLen );

	mn_status = LS_Connected;

	NotifyIOCallback( g_hIOCP, mh_link, IOREQ_RECV );
	
	//等待第一个IOREQ_RECV投递出去
	Sleep( 10 );

	send_link_event( EVT_TCP_CONNECTED );
	return true;
}

bool MsTcpLink::link_listen( unsigned int ui_block )
{
	DWORD dwRet = listen( mh_socket, ui_block );;				
	if ( SOCKET_ERROR == dwRet ){
		return false;
	}
		
	for ( unsigned int i = 0; i < ui_block; ++i ) {
		SOvlpAcceptContext* pAccept = CSingletonAllocAccept::Instance()->GetContext(); 
		if (pAccept != NULL){			
			build_link_context( pAccept );
			pAccept->hAcceptSock = INVALID_SOCKET;
			dwRet = IssueAccept( pAccept );
			if ( (dwRet != 0) && (dwRet != WSA_IO_PENDING) ){
				on_accept_error( pAccept, dwRet );
			}
		}
	}
	return TRUE;
}

void MsTcpLink::link_destroy( bool b_self )
{
	bool b_ret = MsTcpLink::del_link( mh_link );

	if( b_ret && (mh_socket != INVALID_SOCKET) ){
		shutdown( mh_socket, SD_BOTH );
		SOCKET hTemp = mh_socket;
		mh_socket = INVALID_SOCKET;
		closesocket( hTemp );
		if( b_self ){
			send_link_event( EVT_TCP_CLOSED );
		} else {
			send_link_event( EVT_TCP_DISCONNECTED );
		}
		
		mn_status = LS_NotOpen;
	}

	return;
}

bool MsTcpLink::link_send( const char *p_data, unsigned int ui_size )
{	
	if( mn_status != LS_Connected ){
		return false;
	}

	bool b_alloc_failed = false;
	std::list<SOvlpSendContext *> ls_ovlp_send;
	unsigned int ui_need_pkg_cnt = ui_size / IOBUF_SENDMAXSIZE + 1;
	for( unsigned int i = 0; i < ui_need_pkg_cnt; i++ ){
		SOvlpSendContext *p_tmp = CSingletonAllocSend::Instance()->GetContext();
		if( NULL == p_tmp ){
			b_alloc_failed = true;
			break;
		} 
		ls_ovlp_send.push_back( p_tmp );
	}

	if( b_alloc_failed ){
		CSingletonAllocSend::Instance()->FreeContextList( ls_ovlp_send );
		return false;
	}

	do{
		CGuardLock<MSMutex> guard( &mo_send_list_lock );
		unsigned int ui_alloc_len = 0;
		std::list<SOvlpSendContext *>::iterator it = ls_ovlp_send.begin();
		for( ; it != ls_ovlp_send.end(); ++it ){
			SOvlpSendContext *pContext = *it;
			build_link_context( pContext );
			unsigned int ui_copy = (ui_size - ui_alloc_len > IOBUF_SENDMAXSIZE) ? IOBUF_SENDMAXSIZE : (ui_size - ui_alloc_len);			
			memcpy( pContext->data, p_data + ui_alloc_len, ui_copy );			
			pContext->dwBytes = ui_copy;
			ui_alloc_len += ui_copy;

			m_send_buf_list.push_back( pContext );
		}
	} while( 0 );

	on_send( NULL );
 	return true;
}

void MsTcpLink::on_send( SOvlpSendContext* pContext )
{
	if( pContext != NULL ){
		CSingletonAllocSend::Instance()->FreeContext( pContext );
	}

	update_alive_time( GetTickCount() );
	
	mo_send_list_lock.Lock();
	if( m_send_buf_list.empty() ){
		mo_send_list_lock.Unlock();
		return;
	}
	SOvlpSendContext *pNext = m_send_buf_list.front();
	m_send_buf_list.pop_front();
	mo_send_list_lock.Unlock();

	if( pNext != NULL ){
		DWORD dwRet = IssueSend( pNext );
		if( (dwRet != 0) && (dwRet != WSA_IO_PENDING) ){
			on_send_error( pNext, dwRet );
			return;
		} else if( dwRet == WSA_IO_PENDING ){
			//
		}
	}

	return;
}

void MsTcpLink::on_recv( SOvlpRecvContext* pContext )
{
	if( (NULL == pContext) || (NULL == pContext->data) || (pContext->dwBytes == 0) ){
		return;
	}

	if( mn_status != LS_Connected ){
		return;
	}

	if( pContext->dwBytes != IOREQ_NOTIFY_CALL ){
		send_recv_data( NULL, 0, STATUS_RECV_BEGIN );
		char *pRecv = (char *)pContext->data;	
		int ui_recv_len = (int)pContext->dwBytes;
		send_recv_data( pRecv, ui_recv_len, STATUS_RECV_MIDDLE );
		send_recv_data( NULL, 0, STATUS_RECV_END );
	}
	
	update_alive_time( GetTickCount() );

	build_link_context( pContext );
	DWORD dwRet = IssueRecv( pContext );
	if( (dwRet != 0) && (dwRet != WSA_IO_PENDING) ){
		on_recv_error( pContext, dwRet );
	}

	return;
}

void MsTcpLink::on_accept( SOvlpAcceptContext* pContext )
{
	MsTcpLink *pAccept = new MsTcpLink();	
	CRefCountPtr<MsTcpLink> p_reflink = CRefCountPtr<MsTcpLink>( pAccept );
	HTCPLINK h_link = INVALID_HTCPLINK;	
	p_reflink->set_link( h_link );
	p_reflink->set_socket( pContext->hAcceptSock );
	p_reflink->register_callback( m_pfnIOCB );
	MsTcpLink::add_tcp_link( h_link, p_reflink );

	//accept 创建的 socket 会自动继承监听 socket 的属性, AcceptEx 却不会. 因此如果有必要, 在 AcceptEx 成功接受了一个连接之后, 我们必须调用: 
	//SO_UPDATE_ACCEPT_CONTEXT来做到这一点.	
	setsockopt(pContext->hAcceptSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, 
			  (char *)&mh_socket, sizeof(mh_socket));

	SOCKADDR* psaLocal = NULL;
	SOCKADDR* psaRemote = NULL;
	int nLocalLen = 0;
	int nRemoteLen = 0;
	GetAcceptExSockaddrs(pContext->data, 0, 
						 sizeof (SOCKADDR_IN) + 16, 
						 sizeof (SOCKADDR_IN) + 16, 
						 &psaLocal, &nLocalLen,
						 &psaRemote, &nRemoteLen);
	pAccept->set_link_addr(LINK_ADDR_LOCAL, (SOCKADDR_IN*)psaLocal);
	pAccept->set_link_addr(LINK_ADDR_REMOTE, (SOCKADDR_IN*)psaRemote);

	pAccept->attach_accept( pContext->hAcceptSock );
	pAccept->send_accept_link( h_link );
	
	pAccept->do_attach_accept();

	update_alive_time( GetTickCount() );

	memset( pContext, 0, sizeof(pContext) );
	build_link_context( pContext );
	pContext->hAcceptSock = INVALID_HTCPLINK;
	DWORD dwRet = IssueAccept( pContext );
	if( (dwRet != 0) && (dwRet != WSA_IO_PENDING) ){
		on_accept_error( pContext, dwRet );
	}

	return;

	return;
}

void MsTcpLink::on_send_error( SOvlpSendContext* pContext, int i_error )
{
	CSingletonAllocSend::Instance()->FreeContext( pContext );
	link_destroy( false );
	
	return;
}

void MsTcpLink::on_recv_error( SOvlpRecvContext* pContext, int i_error )
{
	CSingletonAllocRecv::Instance()->FreeContext( pContext );
	link_destroy( false );

	return;
}

void MsTcpLink::on_accept_error( SOvlpAcceptContext* pContext, int i_error )
{
	bool b_destroy = false;
	closesocket( pContext->hAcceptSock );
	memset( pContext, 0, sizeof(SOvlpAcceptContext) );

	pContext->hHandle = mh_link;
	pContext->hSock = mh_socket;
	pContext->hAcceptSock = INVALID_HTCPLINK;
	DWORD dwRet = IssueAccept( pContext );
	if( (dwRet != 0) && (dwRet != WSA_IO_PENDING) ){
		b_destroy = true;
	}

	if( b_destroy ){
		closesocket( pContext->hAcceptSock );
		CSingletonAllocAccept::Instance()->FreeContext( pContext );
		link_destroy( false );
	}

	return;
}

void MsTcpLink::send_link_event( unsigned int ui_event )
{
	TcpEvent_S st_event;
	st_event.h_link = mh_link;
	st_event.ui_event = ui_event;
	(*m_pfnIOCB)( &st_event, 0 );

	return;
}

void MsTcpLink::send_accept_link( HTCPLINK h_accept_link )
{
	TcpEvent_S st_event;
	st_event.h_link = mh_link;
	st_event.ui_event = EVT_TCP_ACCEPTED;

	TcpAcceptedParam_S param;
	param.h_acceptLink = h_accept_link;
	(*m_pfnIOCB)( &st_event, &param );

	return;
}

void MsTcpLink::send_recv_data( char *buffer, int size, int i_status )
{
	TcpEvent_S st_event;
	st_event.h_link = mh_link;
	st_event.ui_event = EVT_TCP_RECEIVEDATA;

	TcpRecvDataParam_S param;
	param.p_RecvData = buffer;
	param.ui_size = size;
	param.ui_status = i_status;
	(*m_pfnIOCB)( &st_event, &param );

	return;
}

void MsTcpLink::update_alive_time( unsigned int ui_time )
{
	CGuardLock<MSMutex> guard( &ms_last_alive_lock );
	mui_last_alive_time = ui_time;

	return;
}

unsigned int MsTcpLink::get_last_alive_time()
{
	CGuardLock<MSMutex> guard( &ms_last_alive_lock );
	return mui_last_alive_time;
}

bool MsTcpLink::is_alive( unsigned int ui_time )
{
	if( mb_need_check_alive ){
		if( ui_time < get_last_alive_time() ){
			update_alive_time( ui_time );
			return true;
		}

		return ( ui_time - get_last_alive_time() ) < MsTcpLink::ui_check_alive_timeout;
	} 

	return true;
}

void MsTcpLink::set_link_addr( unsigned int ui_type, SOCKADDR_IN *p_addr )
{
	if( ui_type == LINK_ADDR_LOCAL ){
		memcpy( &m_saLocal, p_addr, sizeof(m_saLocal) );
	} else if( ui_type == LINK_ADDR_REMOTE ){
		memcpy( &m_saRemote, p_addr, sizeof(m_saRemote) );
	}

	return;
}

bool MsTcpLink::get_link_addr( unsigned int ui_type, unsigned int *pui_ip, unsigned short *pus_port )
{
	SOCKADDR_IN* pAddr = NULL;
	if (ui_type == LINK_ADDR_LOCAL)
		pAddr = &m_saLocal;
	else if (ui_type == LINK_ADDR_REMOTE)
		pAddr = &m_saRemote;
	else
		return FALSE;
	*pui_ip = ntohl(pAddr->sin_addr.S_un.S_addr);
	*pus_port = ntohs(pAddr->sin_port);

	return true;
}

void MsTcpLink::attach_accept( SOCKET h_socket )
{
	mh_socket = h_socket;
	BindIOCPSocket( mh_socket, g_hIOCP, (DWORD)IOCPCompletionFunc );
	mn_status = LS_Connected;
	mb_need_check_alive = true;
	update_alive_time( GetTickCount() );

	return;
}

void MsTcpLink::do_attach_accept()
{
	NotifyIOCallback( g_hIOCP, mh_link, IOREQ_RECV );

	return;
}

void MsTcpLink::build_link_context( SOvlpIOContext *pContext )
{
	pContext->hSock = mh_socket;
	pContext->hHandle = mh_link;
	pContext->dwBytes = 0;
	pContext->dwFlags = 0;

	return;
}