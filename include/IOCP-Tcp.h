#ifndef __IOCP_TCP_H_
#define __IOCP_TCP_H_

#include <stdio.h>
#include <basetsd.h>

typedef unsigned int       HTCPLINK;
#define INVALID_HTCPLINK   0xffffffff

#define LINK_ADDR_LOCAL   0x0001
#define LINK_ADDR_REMOTE  0x0002

/*
Tcp Event
*/
#define EVT_TCP_CREATED			0x0001
#define EVT_TCP_CLOSED			0x0002
#define EVT_TCP_ACCEPTED		0x0003
#define EVT_TCP_CONNECTED		0x0004
#define EVT_TCP_DISCONNECTED	0x0005
#define EVT_TCP_RECEIVEDATA		0x0006
#define EVT_TCP_KEEPALIVE		0x0007
#define EVT_TCP_ERRPROTOCOL		0x0010
#define EVT_TCP_EXCEPTION		0xffff

struct TcpEvent_S{
	unsigned int ui_event;
	HTCPLINK     h_link;
};

/*	注册IO完成的回调函数参数声明
	其中pParam1是TcpEvent_S*结构
	其中pParam2根据Event不同可能是STcpRecvDataParam或者STcpAcceptedParam
*/
typedef void (*TcpIOCallBack)(void* pParam1, void* pParam2);

/*************************************************************
when event is EVT_TCP_RECEIVEDATA
*************************************************************/
#define  STATUS_RECV_BEGIN   0x0001    //开始接收
#define  STATUS_RECV_MIDDLE  0x0002    //正在接收
#define  STATUS_RECV_END     0x0003    //接收完成
struct TcpRecvDataParam_S{
	unsigned int ui_status;   //recv data status
	char *p_RecvData;         //recv data buf
	unsigned int ui_size;     //data buf size
};

/*************************************************************
when event is EVT_TCP_ACCEPTED
*************************************************************/
struct TcpAcceptedParam_S{
	HTCPLINK  h_acceptLink;
};


#define IOCP_TCP_DLL_API_UI __declspec(dllexport) unsigned int __stdcall
#define IOCP_TCP_DLL_API_TCPLINK IOCP_TCP_DLL_API_UI

#define IOCP_TCP_DLL_API_BOOL __declspec(dllexport) bool __stdcall
#define IOCP_TCP_DLL_API_VOID __declspec(dllexport) void __stdcall


/*************************************************************
Func    name: ms_iocp_tcp_init
Descriptions: init func
Input   para:
In&Out  para:
Output  para:
Return value:
*************************************************************/
IOCP_TCP_DLL_API_BOOL  ms_iocp_tcp_init();

/*************************************************************
Func    name: ms_iocp_tcp_uninit
Descriptions: uninit func
Input   para:
In&Out  para:
Output  para:
Return value:
*************************************************************/
IOCP_TCP_DLL_API_VOID ms_iocp_tcp_uninit();

/*************************************************************
Func    name: ms_tcp_create
Descriptions: create tcp link, bind addr and port
Input   para: backFunc: when IO operator completed, call backFunc
              p_localAddr: local addr
              us_localPort: local port
In&Out  para:
Output  para:
Return value: HTCPLINK
*************************************************************/
IOCP_TCP_DLL_API_TCPLINK ms_tcp_create( TcpIOCallBack backFunc, const char* p_localAddr, unsigned short us_localPort );

/*************************************************************
Func    name: ms_tcp_destroy
Descriptions: destroy tcp link
Input   para: hLink
In&Out  para:
Output  para:
Return value: success true, failed false.
*************************************************************/
IOCP_TCP_DLL_API_BOOL ms_tcp_destroy( HTCPLINK h_link );

/*************************************************************
Func    name: ms_tcp_connect
Descriptions: call connect function by hLink, pAddr and nPort
Input   para:
In&Out  para:
Output  para:
Return value: success true, failed false.
*************************************************************/
IOCP_TCP_DLL_API_BOOL ms_tcp_connect( HTCPLINK h_link, const char* p_addr, unsigned short us_port );

/*************************************************************
Func    name: ms_tcp_listen
Descriptions: call listen function by hLink
Input   para: nBlock: listen param(5~16)
In&Out  para:
Output  para:
Return value: success true, failed false.
*************************************************************/
IOCP_TCP_DLL_API_BOOL ms_tcp_listen( HTCPLINK h_link, unsigned int ui_block );

/*************************************************************
Func    name: ms_tcp_send
Descriptions: send data
Input   para:
In&Out  para:
Output  para: 
Return value: success true, failed false.
*************************************************************/
IOCP_TCP_DLL_API_BOOL ms_tcp_send( HTCPLINK h_link, const char* p_data, unsigned int ui_size );

/*************************************************************
Func    name: ms_tcp_get_link_addr
Descriptions: get link addr
Input   para: h_link, ui_type
In&Out  para:
Output  para:
Return value: success true, failed false.
*************************************************************/
IOCP_TCP_DLL_API_BOOL ms_tcp_get_link_addr( HTCPLINK h_link, unsigned int ui_type, unsigned int *pui_ip, unsigned short *pus_port );
#endif