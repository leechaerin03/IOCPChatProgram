#include "stdafx.h"
#include "Threads.h"
#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <stdio.h>

#include <Ws2tcpip.h> 
#include <string>     

Threads* Threads::pThreads = nullptr;

Threads::Threads()
{
    pThreads = this;
}

Threads::~Threads()
{
    pThreads = nullptr;
}

unsigned __stdcall Threads::CommThread(void* pArguments)
{
    threadData = (THREADDATA*)pArguments;

    hDlg = threadData->hDlg;
    hIOCP = threadData->hIOCP;
    lstnsock = threadData->lstnsock;
    acceptBuf = threadData->acceptBuf;
    pAOV = threadData->pAOV;           
    plistOverlapped = threadData->plistOverlapped;
    pcs = threadData->pcs;            
    threadCount = threadData->threadCount;

    InterlockedIncrement(threadCount);
    __try {
        __try {
            return InnerCommThread();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    __finally {
        InterlockedDecrement(threadCount);
    }
    return 0;
}

unsigned __stdcall Threads::InnerCommThread(void)
{
    try
    {
        while (1)
        {
            DWORD cbTransferred = 0; ULONG_PTR completionKey = 0; LPOVERLAPPED pOV = nullptr;
            BOOL ok = GetQueuedCompletionStatus(hIOCP, &cbTransferred,&completionKey,&pOV,INFINITE);
            SOCKET sock = (SOCKET)completionKey; 

            if (sock == lstnsock)
            {
                // AcceptEx 끝!
                ACCEPTOVERLAPPED* pAcceptOV = (ACCEPTOVERLAPPED*)pOV;

                if (pAcceptOV && pAcceptOV->commsock != INVALID_SOCKET)
                {
                    // 업데이트
                    setsockopt(pAcceptOV->commsock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                        (char*)&lstnsock, sizeof(lstnsock));
                }
                // IOCP 연결
                OnAccept(pAcceptOV ? pAcceptOV->commsock : INVALID_SOCKET, acceptBuf);

                // 다음 AcceptEx 미리 등록해두기
                SOCKET commsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (commsock != INVALID_SOCKET)
                {
                    pAOV->commsock = commsock;
                    DWORD dwBytes = 0;
                    AcceptEx(
                        lstnsock,
                        commsock,
                        acceptBuf,
                        0,
                        sizeof(sockaddr_in) + 16,
                        sizeof(sockaddr_in) + 16,
                        &dwBytes,
                        (WSAOVERLAPPED*)pAOV
                    );
                }
                continue;
            }

            if (sock == 0)
                break;

            if (!ok) // 타임아웃, 실패
            {
                if (pOV == nullptr)
                    continue;
            }

            if (cbTransferred == 0) //정상종료
            {
                OnClose((OVERLAPPEDSOCK*)pOV);
                continue;
            }

            OVERLAPPEDSOCK* pMOV = (OVERLAPPEDSOCK*)pOV;  //데이터 수신
            OnRead(pMOV, pMOV->wsaBuf.buf, (int)cbTransferred);  // 내부에서 Recv 재포스트
        }
    }
    catch (char* errmsg)
    {
        LPVOID lpOSMsg;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR)&lpOSMsg, 0, NULL);
        MessageBox(hDlg, (LPCTSTR)lpOSMsg, (LPCTSTR)errmsg, MB_OK);
        LocalFree(lpOSMsg);
    }
    return 0;
}

unsigned __stdcall Threads::communicationThread(void* pArguments)
{
    return pThreads->CommThread(pArguments);
}

void Threads::OnAccept(SOCKET commsock, char* AcceptBuf)
{
    if (commsock == INVALID_SOCKET) return;

    // IOCP 연결
    CreateIoCompletionPort((HANDLE)commsock, hIOCP, (ULONG_PTR)commsock, 0);

    // 준비 + 첫 수신
    OVERLAPPEDSOCK* pMOV = new OVERLAPPEDSOCK{};
    ZeroMemory(&pMOV->ov, sizeof(pMOV->ov));
    pMOV->wsaBuf.len = 2048;
    pMOV->wsaBuf.buf = new char[pMOV->wsaBuf.len];
    pMOV->commsock = commsock;

    DWORD dwBytes = 0, dwFlags = 0;
    if (WSARecv(commsock, &pMOV->wsaBuf, 1, &dwBytes, &dwFlags, &pMOV->ov, NULL) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            closesocket(commsock);
            delete[] pMOV->wsaBuf.buf;
            delete pMOV;
            return;
        }
    }

    // 리스트에 추가  + 사용자수 확보
    size_t userCount = 0;
    EnterCriticalSection(pcs);
    plistOverlapped->push_back(pMOV);
    userCount = plistOverlapped->size();
    LeaveCriticalSection(pcs);

    // 원격 주소 파싱
    SOCKADDR* pLocal = nullptr, * pRemote = nullptr;
    int localLen = 0, remoteLen = 0;
    GetAcceptExSockaddrs(
        AcceptBuf, 0,
        sizeof(sockaddr_in) + 16,
        sizeof(sockaddr_in) + 16,  
        &pLocal, &localLen,
        &pRemote, &remoteLen
    );

    wchar_t ip[64] = L"unknown";
    if (pRemote && pRemote->sa_family == AF_INET) {
        InetNtopW(AF_INET, &((sockaddr_in*)pRemote)->sin_addr, ip, _countof(ip));
    }

    // 유니코드
    wchar_t msg[128];
    swprintf_s(msg, L"접속자:(%s), 사용자수:%d", ip, (int)userCount);
    AddString(hDlg, IDC_CONN_LIST, msg);
}

void Threads::AddString(HWND hDlg, UINT nID, const wchar_t* msg)
{
    HWND hList = GetDlgItem(hDlg, nID);
    int n = (int)SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)msg);
    SendMessage(hList, LB_SETTOPINDEX, n, 0);
}

void Threads::OnRead(OVERLAPPEDSOCK* pMOV, char* buf, int nRead)
{
    if (nRead <= 0) {
        OnClose(pMOV);
        return;
    }

    //  모든 클라이언트에 전송
    EnterCriticalSection(pcs);
    for (auto it = plistOverlapped->begin(); it != plistOverlapped->end(); ++it) {
        OVERLAPPEDSOCK* p = *it;
        if (p && p->commsock != INVALID_SOCKET) {
            // nRead 바이트 그대로 전송 
            send(p->commsock, buf, nRead, 0);
        }
    }
    LeaveCriticalSection(pcs);

    // 다음 비동기 수신 재등록
    DWORD dwFlags = 0, dwBytes = 0;
    WSARecv(pMOV->commsock, &pMOV->wsaBuf, 1, &dwBytes, &dwFlags, &pMOV->ov, NULL);

}

void Threads::OnClose(OVERLAPPEDSOCK* pMOV)
{
    // 리스트 제거 + 남은 사용자수 확보
    size_t userCount = 0;
    EnterCriticalSection(pcs);
    plistOverlapped->remove(pMOV);
    userCount = plistOverlapped->size();
    LeaveCriticalSection(pcs);

    // 정리
    closesocket(pMOV->commsock);
    delete[] pMOV->wsaBuf.buf;
    delete pMOV;

    // 유니코드 메시지 출력
    wchar_t msg[64];
    swprintf_s(msg, L"사용자수:%d", (int)userCount);
    AddString(hDlg, IDC_CONN_LIST, msg);
}
