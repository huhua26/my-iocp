#include "Mutex.h"

MSMutex::MSMutex()
{
	m_mutex = ::CreateMutex( NULL, FALSE, NULL );
}

MSMutex::~MSMutex()
{
	::CloseHandle( m_mutex );
}

void MSMutex::Lock() const
{
	DWORD d = WaitForSingleObject( m_mutex, INFINITE );
}

void MSMutex::Unlock() const
{
	::ReleaseMutex( m_mutex );
}