#pragma once

class Threads
{
public:
    Threads();
    ~Threads();
    unsigned __stdcall CommThread(void* pArguments);
    unsigned __stdcall InnerCommThread(void);

    static unsigned __stdcall communicationThread(void* pArguments);
    static Threads* pThreads;

protected:
    

private:
    void OnAccept(SOCKET commsock, char* AcceptBuf);
    void AddString(HWND hDlg, UINT nID, const wchar_t* msg);
    void OnRead(OVERLAPPEDSOCK* pMOV, char* buf, int nRead);
    void OnClose(OVERLAPPEDSOCK* pMOV);

    // 공유 상태
    THREADDATA* threadData = nullptr;

    long* threadCount = nullptr;
    HWND   hDlg = nullptr;
    HANDLE hIOCP = nullptr;
    SOCKET lstnsock = INVALID_SOCKET;
    char* acceptBuf = nullptr;

    ACCEPTOVERLAPPED* pAOV = nullptr;           
    std::list<OVERLAPPEDSOCK*>* plistOverlapped = nullptr;
    CRITICAL_SECTION* pcs = nullptr;         

};