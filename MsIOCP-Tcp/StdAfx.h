#pragma once
#define  _CRT_SECURE_NO_WARNINGS
#define  _CRT_NONSTDC_NO_DEPRECATE

#include <iostream>
#include <tchar.h>
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// 某些 CString 构造函数将为显式的

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// 从 Windows 头中排除极少使用的资料
#endif


#include <atlbase.h>
#include <sstream>
#include <iostream>
#include <list>
#include <set>
#include <vector>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>


#include <fstream>
#include <ostream>
#include <sstream>
#include <locale>
#include <cstdlib>

#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

#include "IOCP-Tcp.h"