#ifndef __TCP_LINK_H_
#define __TCP_LINK_H_

#include <map>
#include <list>
#include "StdAfx.h"
#include "Guard.h"
#include "TcpOvlp.h"
#include "Mutex.h"


class MsTcpLink
{
public:
	enum{
		link_TypeNull = 0,
		link_TypeListen = 1,
		link_TypeConnect = 2,
		link_TypeAccept = 3,
		link_TypeConnected = 4,

		link_TypeError
	};

	enum CLinkStatus
	{
		LS_NotOpen, 
		LS_Open, 
		LS_Connected,
	};

	MsTcpLink();
	virtual ~MsTcpLink();	

public:
	static void InitLinkMap();
	static bool find_link( HTCPLINK h_link, CRefCountPtr<MsTcpLink> &p_link );
	static bool del_link( HTCPLINK h_link );
	static void free_all_link_map();
	static void check_link_timeout();
	
	static void add_tcp_link( HTCPLINK &h_link, CRefCountPtr<MsTcpLink> p_link );

public:
	operator HTCPLINK() const { return mh_link; }
	bool link_create( TcpIOCallBack iocpFunc, const char *pAddr, unsigned short nPort );
	bool link_connect( const char* p_addr, unsigned short us_port );
	bool link_listen( unsigned int ui_block );
	void link_destroy( bool b_self );
	bool link_send( const char *p_data, unsigned int ui_size );

	void on_send( SOvlpSendContext* pContext );
	void on_recv( SOvlpRecvContext* pContext );
	void on_accept( SOvlpAcceptContext* pContext );
	void on_send_error( SOvlpSendContext* pContext, int i_error );
	void on_recv_error( SOvlpRecvContext* pContext, int i_error );
	void on_accept_error( SOvlpAcceptContext* pContext, int i_error );
	

public:
	HTCPLINK get_link(){ return mh_link; }
	void set_link( HTCPLINK h_link ){ mh_link = h_link; }

	void send_link_event( unsigned int ui_event );
	void send_accept_link( HTCPLINK h_accept_link );
	void send_recv_data( char *buffer, int size, int i_status );

	void update_alive_time( unsigned int ui_time );
	unsigned int get_last_alive_time();
	bool is_alive( unsigned int ui_time );

	SOCKET get_socket(){ return mh_socket; }
	void set_socket( SOCKET s_socket ){ mh_socket = s_socket; }

	void register_callback( TcpIOCallBack callback ){ m_pfnIOCB = callback; }

	void set_link_addr( unsigned int ui_type, SOCKADDR_IN *p_addr );
	bool get_link_addr( unsigned int ui_type, unsigned int *pui_ip, unsigned short *pus_port );

	void attach_accept( SOCKET h_socket );
	void do_attach_accept();

private:
	void build_link_context( SOvlpIOContext *pContext );
private:
	static MSMutex m_link_lock;
	static std::map<HTCPLINK, CRefCountPtr<MsTcpLink> > m_map_link;
	static std::map<HTCPLINK, SOCKET> m_link_socket_map;
	static const unsigned int ui_check_alive_timeout = 5 * 60;

private:
	SOCKET	    mh_socket;
	HTCPLINK	mh_link;

	TcpIOCallBack m_pfnIOCB;

	SOCKADDR_IN m_saRemote;
	SOCKADDR_IN m_saLocal;

	CLinkStatus mn_status;

	MSMutex      ms_last_alive_lock;
	unsigned int mui_last_alive_time;
	bool         mb_need_check_alive;

	MSMutex      mo_send_list_lock;
	std::list<SOvlpSendContext *> m_send_buf_list;
};


#endif