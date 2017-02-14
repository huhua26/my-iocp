/*
* ˵��������ģʽʵ��
*/

#pragma once

#include "comm-func.h"
#include "StdAfx.h"
#include "Mutex.h"

template <typename LOCK>
class CGuardLock
{
public:
	explicit CGuardLock(const LOCK* p) : m_pLock(const_cast<LOCK*>(p)) { m_pLock->Lock(); }
	~CGuardLock() { m_pLock->Unlock(); }

private:
	CGuardLock(const CGuardLock&);
	CGuardLock& operator=(const CGuardLock&);
	LOCK* m_pLock;
};

template <typename T, typename LOCK = MSMutex>
class CSingletonHandler : public T
{
public:
	//�ڵ�һ��ʵ������ʱ��ʹ��DoubleCheckNull��ʽ��Ⲣ����
	static T* Instance()
	{
		if (m_pInstance == NULL)
		{
			CGuardLock<LOCK> guard(&m_Lock);
			if (m_pInstance == NULL)
			{
				m_pInstance = new T;
			}
		}
		return m_pInstance;
	}

	static void Release()
	{
		if (m_pInstance != NULL)
		{
			CGuardLock<LOCK> guard(&m_Lock);
			if (m_pInstance != NULL)
			{
				delete m_pInstance;
				m_pInstance = NULL;
			}
		}
	}
private:
	CSingletonHandler();
	~CSingletonHandler();

	CSingletonHandler(const CSingletonHandler& singletonhandler);
	CSingletonHandler& operator=(const CSingletonHandler& singletonhandler);

	static T* m_pInstance;
	static LOCK m_Lock;
};

template <typename T, typename LOCK>
T* CSingletonHandler<T, LOCK>::m_pInstance = NULL;

template<typename T, typename LOCK>
LOCK CSingletonHandler<T, LOCK>::m_Lock;
