#pragma once

#include <winsock2.h>
#include <windows.h>
#include <tchar.h>

#pragma comment(lib, "Ws2_32.lib")

// 소켓 통지용 사용자 메시지
#ifndef WM_SOCKET_NOTIFY
#define WM_SOCKET_NOTIFY (WM_USER + 1)
#endif

// 접속할 서버 IP
#ifndef CHAT_SERVER_IP
#define CHAT_SERVER_IP TEXT("127.0.0.1")
#endif

class WinAPI
{
public:
    WinAPI();
    ~WinAPI();

    bool Init(HINSTANCE hInstance, int nCmdShow);
    int  Run();
    HWND GetHwnd();

    INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR CALLBACK LoginDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

    static WinAPI* pWinAPI;

private:
    static HWND WinAPI_hwnd;

    HWND   hDlg = nullptr;                 // 메인 대화상자 핸들
    WSADATA wsaData{};
    BYTE   nMajor = 2, nMinor = 2;
    WORD   wVersionRequested = 0;
    int    nRet = 0;
    SOCKET commsock = INVALID_SOCKET;
    TCHAR  chatName[128] = { 0 };
    WSAEVENT hSockEvent = WSA_INVALID_EVENT;
    HANDLE   hNetThread = nullptr;
    static unsigned __stdcall SockWaitThread(void* ctx);

    int  roleIndex = 1; // 기본값: 개발 (0=기획, 1=개발, 2=사업)
    HFONT hTitleFont = nullptr; //제목라벨폰트

    BOOL OnLogIn(HWND hDlg);
    void OnSend(HWND hDlg);
    void OnRead(HWND hDlg);
    void AddString(HWND hDlg, UINT nID, const TCHAR* msg);

    // 유틸TCHAR <-> char 변환 
    static void NarrowFromTChar(const TCHAR* src, char* dst, int dstBytes);
    static void TCharFromNarrow(const char* src, TCHAR* dst, int dstChars);
};
