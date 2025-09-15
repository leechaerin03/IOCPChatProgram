#include "stdafx.h"
#include "WinAPI.h"
#include "resource.h" 
#include <process.h> 
#include <cstdio>
#include <Ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib") 

HWND WinAPI::WinAPI_hwnd = nullptr;
WinAPI* WinAPI::pWinAPI = nullptr;

// DLGPROC는 INT_PTR CALLBACK
static INT_PTR CALLBACK mainDlgdProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return WinAPI::pWinAPI->MainDlgProc(hWnd, msg, wParam, lParam);
}

static INT_PTR CALLBACK loginDlgdProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return WinAPI::pWinAPI->LoginDlgProc(hWnd, msg, wParam, lParam);
}

WinAPI::WinAPI()
{
    pWinAPI = this;
}

WinAPI::~WinAPI()
{
    pWinAPI = nullptr;

    if (commsock != INVALID_SOCKET) {
        shutdown(commsock, SD_BOTH);
        closesocket(commsock);
        commsock = INVALID_SOCKET;
    }
    WSACleanup();
}

bool WinAPI::Init(HINSTANCE hInstance, int /*nCmdShow*/)
{
   
    // 리소스 존재 확인
    if (!FindResource(hInstance, MAKEINTRESOURCE(IDD_CHAT_CLIENT), RT_DIALOG)) {
        DWORD e = GetLastError();
        TCHAR buf[128]; _stprintf_s(buf, TEXT("FindResource(IDD_CHAT_CLIENT) 실패, GetLastError=%lu"), e);
        MessageBox(NULL, buf, TEXT("Init"), MB_OK | MB_ICONERROR);
        return false;
    }
    if (!FindResource(hInstance, MAKEINTRESOURCE(IDD_CHAT_LOGIN), RT_DIALOG)) {
        DWORD e = GetLastError();
        TCHAR buf[128]; _stprintf_s(buf, TEXT("FindResource(IDD_CHAT_LOGIN) 실패, GetLastError=%lu"), e);
        MessageBox(NULL, buf, TEXT("Init"), MB_OK | MB_ICONERROR);
        return false;
    }

    // 메인 대화상자 생성
    hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_CHAT_CLIENT), NULL, ::mainDlgdProc);
    if (hDlg == NULL) {
        DWORD e = GetLastError();
        TCHAR buf[128]; _stprintf_s(buf, TEXT("CreateDialog 실패, GetLastError=%lu"), e);
        MessageBox(NULL, buf, TEXT("Init"), MB_OK | MB_ICONERROR);
        return false;
    }

    // WinSock 초기화
    wVersionRequested = MAKEWORD(nMajor, nMinor);
    nRet = WSAStartup(wVersionRequested, &wsaData);
    if (nRet == SOCKET_ERROR || LOBYTE(wsaData.wVersion) != nMajor || HIBYTE(wsaData.wVersion) != nMinor) {
        MessageBox(hDlg, TEXT("WSAStartup 실패/버전 미스매치"), TEXT("에러"), MB_OK | MB_ICONERROR);
        return false;
    }

    // 로그인 모달 다이얼로그
    INT_PTR nID = DialogBox(hInstance, MAKEINTRESOURCE(IDD_CHAT_LOGIN), NULL, ::loginDlgdProc);
    if (nID == -1) {
        DWORD e = GetLastError();
        TCHAR buf[128]; _stprintf_s(buf, TEXT("DialogBox(IDD_CHAT_LOGIN) 실패, GetLastError=%lu"), e);
        MessageBox(hDlg, buf, TEXT("에러"), MB_OK | MB_ICONERROR);
        return false;
    }
    if (nID == IDCANCEL) {
        // 사용자가 취소눌럿을때
        return false;
    }

    ShowWindow(hDlg, SW_SHOW);
    return true;
}

int WinAPI::Run()
{
    MSG msg{};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!IsDialogMessage(hDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
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
    case WM_INITDIALOG:
    {
        // 라벨 텍스트 (원하는 이름)
        SetDlgItemText(hDlg, IDC_ROOM_TITLE, TEXT("메이플월드 게임개발 파티원 모집 채팅방"));

        // 볼드 폰트 생성
        HDC hdc = GetDC(hDlg);
        int dpiY = (hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96);
        if (hdc) ReleaseDC(hDlg, hdc);

        int height = -MulDiv(18, dpiY, 72);  // 18pt 
        hTitleFont = CreateFont(
            height, 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_SWISS,
            TEXT("맑은 고딕")       
        );

        if (hTitleFont) {
            SendMessage(GetDlgItem(hDlg, IDC_ROOM_TITLE), WM_SETFONT, (WPARAM)hTitleFont, TRUE);
        }
        return (INT_PTR)TRUE;
    }
    case WM_SOCKET_NOTIFY:
    {
        // 비동기 소켓 이벤트
        switch (WSAGETSELECTEVENT(lParam))
        {
        case FD_READ:
            OnRead(hDlg);
            break;
        case FD_CLOSE:
            // 서버가 끊었을 때
            AddString(hDlg, IDC_MSG_LIST, TEXT("[서버 연결 종료]"));
            if (commsock != INVALID_SOCKET) {
                closesocket(commsock);
                commsock = INVALID_SOCKET;
            }
            break;
        }
        return (INT_PTR)TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_SEND:
            OnSend(hDlg);
            return (INT_PTR)TRUE;

        case IDCANCEL:
            DestroyWindow(hDlg);
            PostQuitMessage(0);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK WinAPI::LoginDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        // 직무 채우기
        HWND hCombo = GetDlgItem(hDlg, IDC_ROLE);
        if (hCombo) {
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("기획"));
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("개발"));
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)TEXT("사업"));
            SendMessage(hCombo, CB_SETCURSEL, 1, 0); // 기본: 개발
        }
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_LOGIN:
            if (OnLogIn(hDlg))
                EndDialog(hDlg, IDC_LOGIN);
            return (INT_PTR)TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

BOOL WinAPI::OnLogIn(HWND hLoginDlg)
{
    SetCursor(LoadCursor(NULL, IDC_WAIT));

    //  소켓 생성
    commsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (commsock == INVALID_SOCKET)
    {
        MessageBox(hLoginDlg, TEXT("socket 생성 실패"), TEXT("연결 에러"), MB_OK | MB_ICONERROR);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return FALSE;
    }

    // 서버 주소 구성 
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(42006);

#ifdef UNICODE
    if (InetPtonW(AF_INET, CHAT_SERVER_IP, &addr.sin_addr) != 1)
#else
    if (InetPtonA(AF_INET, CHAT_SERVER_IP, &addr.sin_addr) != 1)
#endif
    {
        MessageBox(hLoginDlg, TEXT("IP 파싱 실패 (CHAT_SERVER_IP)"), TEXT("연결 에러"), MB_OK | MB_ICONERROR);
        closesocket(commsock); commsock = INVALID_SOCKET;
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return FALSE;
}

    //  접속
    if (connect(commsock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        TCHAR buf[128];
        _stprintf_s(buf, TEXT("서버 접속 실패 (WSA=%d)"), e);
        MessageBox(hLoginDlg, buf, TEXT("연결 에러"), MB_OK | MB_ICONERROR);
        closesocket(commsock); commsock = INVALID_SOCKET;
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return FALSE;
    }

    // 비동기 통지 이벤트 객체 생성
    hSockEvent = WSACreateEvent();
    if (hSockEvent == WSA_INVALID_EVENT) {
        MessageBox(hLoginDlg, TEXT("WSACreateEvent 실패"), TEXT("에러"), MB_OK | MB_ICONERROR);
        closesocket(commsock); commsock = INVALID_SOCKET;
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return FALSE;
    }

    // 소켓에 이벤트 바인딩
    if (WSAEventSelect(commsock, hSockEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR) {
        int e = WSAGetLastError();
        TCHAR buf[128];
        _stprintf_s(buf, TEXT("WSAEventSelect 실패 (WSA=%d)"), e);
        MessageBox(hLoginDlg, buf, TEXT("에러"), MB_OK | MB_ICONERROR);
        WSACloseEvent(hSockEvent); hSockEvent = WSA_INVALID_EVENT;
        closesocket(commsock); commsock = INVALID_SOCKET;
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return FALSE;
    }

    // 네트워크 이벤트 대기 쓰레드 시작
    hNetThread = (HANDLE)_beginthreadex(nullptr, 0, &WinAPI::SockWaitThread, this, 0, nullptr);
    if (!hNetThread) {
        MessageBox(hLoginDlg, TEXT("네트워크 쓰레드 생성 실패"), TEXT("에러"), MB_OK | MB_ICONERROR);
        WSACloseEvent(hSockEvent); hSockEvent = WSA_INVALID_EVENT;
        closesocket(commsock); commsock = INVALID_SOCKET;
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return FALSE;
    }

    // 닉네임 읽기 
    GetDlgItemText(hLoginDlg, IDC_CHAT_NAME, chatName, (int)_countof(chatName));

    // 직무 선택 읽기
    int sel = (int)SendDlgItemMessage(hLoginDlg, IDC_ROLE, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel > 2) sel = 1; // 기본: 개발
    roleIndex = sel;

    const TCHAR* roles[] = { TEXT("기획"), TEXT("개발"), TEXT("사업") };

    // [직무]닉네임 형태로 chatName을 덮어쓰기
    TCHAR combined[128] = { 0 };
    _stprintf_s(combined, TEXT("[%s]%s"), roles[roleIndex], chatName);
    _tcscpy_s(chatName, _countof(chatName), combined);

    SetCursor(LoadCursor(NULL, IDC_ARROW));
    return TRUE;
}

void WinAPI::OnSend(HWND hDlg)
{
    TCHAR msg[256] = { 0 };
    GetDlgItemText(hDlg, IDC_MSG, msg, (int)_countof(msg));
    SetDlgItemText(hDlg, IDC_MSG, TEXT(""));

    if (msg[0] == 0 || commsock == INVALID_SOCKET)
        return;

    // "닉네임>>메시지" 구성
    TCHAR line[128 + 256 + 8] = { 0 };
    _stprintf_s(line, TEXT("%s>> %s"), chatName, msg);

    // 네트워크로는 멀티바이트(ACP)로 전송
    char sendbuf[1024]{};
    NarrowFromTChar(line, sendbuf, (int)sizeof(sendbuf));
    int sent = send(commsock, sendbuf, (int)strlen(sendbuf), 0);
    if (sent == SOCKET_ERROR)
    {
        int e = WSAGetLastError();
        TCHAR buf[128];
        _stprintf_s(buf, TEXT("send 실패 (WSA=%d)"), e);
        MessageBox(hDlg, buf, TEXT("에러"), MB_OK | MB_ICONERROR);
    }
}

void WinAPI::OnRead(HWND hDlg)
{
    if (commsock == INVALID_SOCKET) return;

    char bufA[512] = { 0 };
    int n = recv(commsock, bufA, (int)sizeof(bufA) - 1, 0);
    if (n <= 0)
    {
        // 종료or에러
        AddString(hDlg, IDC_MSG_LIST, TEXT("[연결 종료]"));
        closesocket(commsock);
        commsock = INVALID_SOCKET;
        return;
    }
    bufA[n] = '\0';

    // UI에 표시하기 위해 TCHAR로 변환
    TCHAR line[512] = { 0 };
    TCharFromNarrow(bufA, line, (int)_countof(line));
    AddString(hDlg, IDC_MSG_LIST, line);
}

void WinAPI::AddString(HWND hDlg, UINT nID, const TCHAR* msg)
{
    HWND hList = GetDlgItem(hDlg, nID);
    int n = (int)SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)msg);
    SendMessage(hList, LB_SETTOPINDEX, n, 0);
}

//  변환 유틸 
void WinAPI::NarrowFromTChar(const TCHAR* src, char* dst, int dstBytes)
{
#ifdef UNICODE

    WideCharToMultiByte(CP_ACP, 0, src, -1, dst, dstBytes, NULL, NULL);
#else
    lstrcpynA(dst, src, dstBytes);
#endif
}

void WinAPI::TCharFromNarrow(const char* src, TCHAR* dst, int dstChars)
{
#ifdef UNICODE
    MultiByteToWideChar(CP_ACP, 0, src, -1, dst, dstChars);
#else
    lstrcpyn(dst, src, dstChars);
#endif
}

//네트워크 이벤트 대기 스레드
unsigned __stdcall WinAPI::SockWaitThread(void* ctx)
{
    WinAPI* self = static_cast<WinAPI*>(ctx);
    if (!self) return 0; //예외처리, 없으면 그냥 끝내기

    HANDLE hEvt = self->hSockEvent; // 소켓에 연결해 둔 이벤트 핸들

    // 소켓이 살아 있고 && 이벤트 핸들이 유효한 동안 계속 대기
    while (self->commsock != INVALID_SOCKET && hEvt != WSA_INVALID_EVENT)
    {
        DWORD dw = WSAWaitForMultipleEvents(1, &hEvt, FALSE, WSA_INFINITE, FALSE);
        if (dw == WSA_WAIT_FAILED) break; // 대기 실패

        //무슨 이벤트인지 조회
        WSANETWORKEVENTS ne{};
        if (WSAEnumNetworkEvents(self->commsock, hEvt, &ne) == SOCKET_ERROR)
            break; // 조회 실패

        //데이터 도착하면 UI스레드에 알려줌
        if (ne.lNetworkEvents & FD_READ) {
            PostMessage(self->hDlg, WM_SOCKET_NOTIFY,
                (WPARAM)self->commsock,
                WSAMAKESELECTREPLY(FD_READ, ne.iErrorCode[FD_READ_BIT]));
        }

        //연결 종료면 UI에 알려주고 이 스레드 종료하기
        if (ne.lNetworkEvents & FD_CLOSE) {
            PostMessage(self->hDlg, WM_SOCKET_NOTIFY,
                (WPARAM)self->commsock,
                WSAMAKESELECTREPLY(FD_CLOSE, ne.iErrorCode[FD_CLOSE_BIT]));
            break; // 종료
        }
    }
    return 0;
}
