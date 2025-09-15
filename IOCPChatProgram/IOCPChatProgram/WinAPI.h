#pragma once
#include "Threads.h"

class WinAPI : public Threads
{
public:
    WinAPI();
    ~WinAPI();
    bool Init(HINSTANCE hInstance, int nCmdShow);
    int Run();
    HWND GetHwnd();

    INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static WinAPI* pWinAPI;

private:
    static HWND WinAPI_hwnd;

    SYSTEM_INFO si;
    HWND hDlg = nullptr;
    HANDLE hIOCP = nullptr;
    WSADATA wsaData;
    BYTE nMajor = 2, nMinor = 2;
    WORD wVersionRequested;
    int	nRet;
    CRITICAL_SECTION cs;
    SOCKET lstnsock = INVALID_SOCKET;
    char acceptBuf[(sizeof(sockaddr_in) + 16) * 2];
    ACCEPTOVERLAPPED AOV = { 0 };
    long threadCount = 0;
    std::list<OVERLAPPEDSOCK*> listOverlapped;

    BOOL CreateListenSocket(HWND hDlg);

    THREADDATA* threadData;
};
