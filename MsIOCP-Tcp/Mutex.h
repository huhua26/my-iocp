#ifndef _MUTEX_H__
#define _MUTEX_H__

#include "StdAfx.h"

class MSMutex{
public:
	MSMutex();
	~MSMutex();

	void Lock() const;
	void Unlock() const;

private:
	HANDLE m_mutex;
};

#endif
