#ifndef __TCP_OVLP_H_
#define __TCP_OVLP_H_

#include "StdAfx.h"

#define IOREQ_SEND   0x01
#define IOREQ_RECV   0x02
#define IOREQ_ACCEPT 0x04

#define IOBUF_SENDMAXSIZE	(36*1024)
#define IOBUF_RECVMAXSIZE	(128*1024)
#define IOREQ_NOTIFY_CALL   0xffffffff

struct SOvlpIOContext
{
	OVERLAPPED ovlp;	//Overlapped struct
	BYTE byIORType;		//IO Request Type
	SOCKET hSock;		//Socket Handle
	DWORD dwFlags;
	DWORD dwBytes;
	HTCPLINK hHandle;
};

struct SOvlpSendContext : public SOvlpIOContext
{
	char data[IOBUF_SENDMAXSIZE];
	SOvlpSendContext()
	{
		Reset();
	}

	void Reset()
	{   
		//数据区域不memset
		memset(this, 0, sizeof(SOvlpIOContext));
	}
};

struct SOvlpRecvContext : public SOvlpIOContext
{
	char data[IOBUF_RECVMAXSIZE];
	SOvlpRecvContext()
	{
		Reset();
	}
	void Reset()
	{   
		//数据区域不memset
		memset(this, 0, sizeof(SOvlpIOContext));
	}
};

struct SOvlpAcceptContext : public SOvlpIOContext
{
	SOCKET hAcceptSock;	
	BYTE data[1024];
	SOvlpAcceptContext()
	{
		Reset();
	}
	void Reset()
	{
		memset(this, 0, sizeof(SOvlpAcceptContext));
	}
};

#endif