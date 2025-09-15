#pragma once
#ifndef STDAFX_H
#define STDAFX_H

#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <process.h>
#include <cstdlib>
#include <list>
#include <tchar.h>
#include "resource.h"
#pragma comment(lib,"WS2_32.LIB")
#pragma comment(lib,"Mswsock.lib")
#pragma comment(lib, "User32.lib")

#pragma warning(disable : 4996)
using namespace std;


struct OVERLAPPEDSOCK
{
	WSAOVERLAPPED ov;
	WSABUF		  wsaBuf;
	SOCKET		  commsock;
};
struct ACCEPTOVERLAPPED
{
	WSAOVERLAPPED ov;
	SOCKET		  commsock;
};
struct THREADDATA
{
    long* threadCount;
    HWND hDlg;
    HANDLE hIOCP;
    SOCKET lstnsock;
    char* acceptBuf;                                   // 공유 버퍼 포인터
    ACCEPTOVERLAPPED* pAOV;                            // 공유 AcceptEx OVERLAPPED
    std::list<OVERLAPPEDSOCK*>* plistOverlapped;       // 공유 연결 리스트
    CRITICAL_SECTION* pcs;                              // 공유 크리티컬 섹션
};
#endif // !STDAFX_H