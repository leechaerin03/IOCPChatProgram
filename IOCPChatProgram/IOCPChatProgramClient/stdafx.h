#pragma once
#ifndef STDAFX_H
#define STDAFX_H

#include <winsock2.h>   
#include <windows.h>
#include <tchar.h>
#include <Ws2tcpip.h>   
#include "resource.h"

#pragma comment(lib,"Ws2_32.lib")

#define WM_SOCKET_NOTIFY  (WM_USER + 1)
#define CHAT_SERVER_IP    TEXT("127.0.0.1")   

#endif // !STDAFX_H
