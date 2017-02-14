#ifndef INC_ALLOCCONTEXTMGR_H
#define INC_ALLOCCONTEXTMGR_H

#include "StdAfx.h"
#include "TcpOvlp.h"
#include "Guard.h"
#include "SingletonHandler.h"
#include "Mutex.h"

template <typename T>
class CAllocContextMgr
{
public:
	CAllocContextMgr() : m_strMgrName("")
	{ 
		m_nAllocContextCnt = 0;
		m_nHighWaterAllocCnt = 1024;	
	}

	~CAllocContextMgr() { FreeAllContext(); }

	void MgrName(std::string &strName)	{ m_strMgrName = strName;}

	void InitMemoryPoolSize(unsigned int nMemoryPoolSize)
	{
		m_nHighWaterAllocCnt = nMemoryPoolSize;
	}

	T* GetContext()
	{
		CGuardLock<MSMutex> guard( &m_csAlloc );
		T* pContext = NULL;
		if ( m_lsAlloc.empty() ){
			if ( m_nAllocContextCnt > m_nHighWaterAllocCnt ){
				return NULL;
			}
			for ( unsigned int i=0; i<AllocContextUnit; ++i ){
				m_lsAlloc.push_back( new T );
				m_nAllocContextCnt ++;
			}
		}

		if ( !m_lsAlloc.empty() ){
			pContext = m_lsAlloc.front();				
			m_lsAlloc.pop_front();
		} else{
			pContext = NULL;
		}
			
		return pContext;	
	}

	void FreeContext(T* pContext)
	{
		if ( pContext == NULL ){
			return;
		}

		pContext->Reset();

		CGuardLock<MSMutex> guard( &m_csAlloc );
		m_lsAlloc.push_back( pContext );
		while ( (unsigned int)m_lsAlloc.size() > m_nHighWaterAllocCnt/2 ){
			m_nAllocContextCnt --;
			T *pContext = m_lsAlloc.front();
			m_lsAlloc.pop_front();				
			delete pContext;
		}
	}

	void FreeContextList( std::list<T*>& ls )
	{
		CGuardLock<MSMutex> guard( &m_csAlloc );
		T* pContext = NULL;
		std::list<T*>::iterator it = ls.begin();
		while( true ){
			pContext = *it;
			if( pContext == NULL ){
				continue;
			}
			pContext->Reset();
			m_lsAlloc.push_back( pContext );
		}

		while ( (unsigned int)m_lsAlloc.size() > m_nHighWaterAllocCnt/2 ){
			m_nAllocContextCnt --;
			T *pContext = m_lsAlloc.front();
			m_lsAlloc.pop_front();				
			delete pContext;
		}
	}

	void FreeAllContext()
	{
		CGuardLock<MSMutex> guard(&m_csAlloc);
		while ( !m_lsAlloc.empty() ){
			T *pContext = m_lsAlloc.front();
			m_lsAlloc.pop_front();				
			delete pContext;
		}			
	}

	unsigned int WarningCount() const	{ return m_nHighWaterAllocCnt/2; }

private:
	MSMutex m_csAlloc;
	std::list<T *> m_lsAlloc;
	unsigned int m_nHighWaterAllocCnt;
	unsigned int m_nAllocContextCnt;
	static const unsigned int AllocContextUnit = 128;

	std::string		 m_strMgrName;
};

typedef CAllocContextMgr<SOvlpSendContext> CAllocSendContextMgr;
typedef CAllocContextMgr<SOvlpRecvContext> CAllocRecvContextMgr;
typedef CAllocContextMgr<SOvlpAcceptContext> CAllocAcceptContextMgr;

typedef CSingletonHandler<CAllocSendContextMgr> CSingletonAllocSend;
typedef CSingletonHandler<CAllocRecvContextMgr> CSingletonAllocRecv;
typedef CSingletonHandler<CAllocAcceptContextMgr> CSingletonAllocAccept;

#endif