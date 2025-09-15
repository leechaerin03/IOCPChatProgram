#include "stdafx.h"
#include "WinAPI.h"
#include <tchar.h>  // TCHAR/TEXT 매크로

HWND WinAPI::WinAPI_hwnd = nullptr;
WinAPI* WinAPI::pWinAPI = nullptr;

static INT_PTR CALLBACK mainDlgdProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return WinAPI::pWinAPI->MainDlgProc(hWnd, msg, wParam, lParam);
}

WinAPI::WinAPI()
{
    pWinAPI = this;
    threadData = new THREADDATA;
}

WinAPI::~WinAPI()
{
    pWinAPI = nullptr;

    if (threadData != nullptr)
    {
        delete threadData;
        threadData = nullptr;
    }

    closesocket(lstnsock);

    for (auto it = listOverlapped.begin(); it != listOverlapped.end(); ++it)
    {
        OVERLAPPEDSOCK* pMOV = *it;
        closesocket(pMOV->commsock);
        delete[] pMOV->wsaBuf.buf;
        delete pMOV;
    }

    listOverlapped.clear();
    DeleteCriticalSection(&cs);
    WSACleanup();
}

bool WinAPI::Init(HINSTANCE hInstance, int nCmdShow)
{
    hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_CHAT_SERVER), NULL, ::mainDlgdProc);
    if (hDlg == NULL) {
        MessageBox(NULL, TEXT("CreateDialog 실패"), TEXT("Init 단계"), MB_OK | MB_ICONERROR);
        return false;
    }
    ShowWindow(hDlg, SW_SHOW);

    // WinSock init
    wVersionRequested = MAKEWORD(nMinor, nMajor);
    nRet = WSAStartup(wVersionRequested, &wsaData);
    if (nRet == SOCKET_ERROR || LOBYTE(wsaData.wVersion) != nMajor || HIBYTE(wsaData.wVersion) != nMinor) {
        MessageBox(hDlg, TEXT("WSAStartup 실패/버전 미스매치"), TEXT("Init 단계"), MB_OK | MB_ICONERROR);
        return false;
    }

    InitializeCriticalSection(&cs);

    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (hIOCP == NULL) {
        MessageBox(hDlg, TEXT("CreateIoCompletionPort 실패"), TEXT("Init 단계"), MB_OK | MB_ICONERROR);
        return false;
    }

    // 첫 AcceptEx 전에 AOV 초기화
    ZeroMemory(&AOV, sizeof(AOV));

    if (!CreateListenSocket(hDlg)) {
        return false;
    }

    // THREADDATA를 포인터 공유로 채움
    threadData->hDlg = hDlg;
    threadData->hIOCP = hIOCP;
    threadData->threadCount = &threadCount;
    threadData->lstnsock = lstnsock;
    threadData->acceptBuf = acceptBuf;           
    threadData->pAOV = &AOV;                // 포인터 공유
    threadData->plistOverlapped = &listOverlapped;     // 포인터 공유
    threadData->pcs = &cs;                 // 포인터 공유

    // IOCP 워커 스레드 생성
    GetSystemInfo(&si);
    int nThreads = (int)si.dwNumberOfProcessors * 2;
    for (int i = 0; i < nThreads; ++i) {
        HANDLE hThread = (HANDLE)_beginthreadex(
            NULL, 0,
            Threads::communicationThread,  
            threadData, 0, NULL
        );
        if (hThread) CloseHandle(hThread);
    }

    return true;
}

int WinAPI::Run()
{
    MSG msg = { 0 };

    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++)
        PostQueuedCompletionStatus(hIOCP, 1, 0, NULL);

    while (0 < threadCount)
        Sleep(100);

    return 0;
}

HWND WinAPI::GetHwnd()
{
    return WinAPI_hwnd;
}

INT_PTR CALLBACK WinAPI::MainDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
        case IDC_CLOSE:
            DestroyWindow(hDlg);
            PostQuitMessage(0);
            break;
        }
        return (INT_PTR)TRUE;  
    }
    return (INT_PTR)FALSE;     
}

BOOL WinAPI::CreateListenSocket(HWND hDlg)
{
    try
    {
        //  IPv4 TCP 리스닝 소켓 생성
        lstnsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (lstnsock == INVALID_SOCKET)
            throw TEXT("socket 에러");

     
        BOOL reuse = TRUE;
        setsockopt(lstnsock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        //  바인드 주소 구성
        sockaddr_in addr;
        ZeroMemory(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);   // 0.0.0.0
        addr.sin_port = htons(42006);        // 원하는 포트 넣는곳!!!!!!!

        //  바인드 + 에러 메시지
        if (bind(lstnsock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
        {
            int e = WSAGetLastError(); // 10048이면 '이미 사용 중'
            TCHAR buf[160];
            _stprintf_s(buf, TEXT("bind 실패 (WSAGetLastError=%d). 포트를 점유한 프로세스를 종료하거나 포트를 변경하세요."), e);
            MessageBox(hDlg, buf, TEXT("CreateListenSocket"), MB_OK | MB_ICONERROR);
            throw TEXT("bind 에러");
        }

        //  리슨
        if (listen(lstnsock, SOMAXCONN) == SOCKET_ERROR)
            throw TEXT("listen 에러");

        //  IOCP에 리스너 등록
        if (!CreateIoCompletionPort((HANDLE)lstnsock, hIOCP, (ULONG_PTR)lstnsock, 0))
            throw TEXT("CreateIoCompletionPort(리스너) 에러");

        // 첫 AcceptEx 포스트
        SOCKET commsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (commsock == INVALID_SOCKET)
            throw TEXT("socket 에러");

        AOV.commsock = commsock;
        DWORD dwBytes = 0;
        BOOL ok = AcceptEx(
            lstnsock,
            commsock,
            acceptBuf,
            0,
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            &dwBytes,
            (WSAOVERLAPPED*)&AOV
        );

        if (!ok)
        {
            int ae = WSAGetLastError();
            if (ae != ERROR_IO_PENDING)
            {
                TCHAR buf[160];
                _stprintf_s(buf, TEXT("AcceptEx 실패 (WSAGetLastError=%d)"), ae);
                MessageBox(hDlg, buf, TEXT("CreateListenSocket"), MB_OK | MB_ICONERROR);
                throw TEXT("AcceptEx 에러");
            }
        }
    }
    catch (LPCTSTR errmsg)
    {
        LPTSTR lpOSMsg = nullptr;
        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            WSAGetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&lpOSMsg,
            0,
            NULL
        );

        MessageBox(hDlg, lpOSMsg ? lpOSMsg : TEXT("Unknown Error"), errmsg, MB_OK | MB_ICONERROR);
        if (lpOSMsg) LocalFree(lpOSMsg);

        if (lstnsock != INVALID_SOCKET) { closesocket(lstnsock); lstnsock = INVALID_SOCKET; }
        return FALSE;
    }

    return TRUE;
}
