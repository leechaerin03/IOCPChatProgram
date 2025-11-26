# 💬 IOCP Real-time Multi-Chat Server
> **Windows IOCP(Input Output Completion Port) 기반의 고성능 비동기 채팅 서버 & WinAPI 클라이언트 프로젝트**

<br>

## 📅 Project Background
메이플스토리월드에서 게임을 개발하던 도중, 기획자나 아트 등 함께할 팀원을 모집하는 과정에서 소통의 불편함을 겪었습니다.
동시에 **"웹 서버가 아닌, 근본적인 네트워크 통신 원리를 깊게 파고들고 싶다"**는 열망이 생겨, 학습과 실전 문제 해결을 동시에 잡기 위해 **IOCP 기반의 멀티 채팅방**을 직접 구현하게 되었습니다.

<br>

## 🛠 Tech Stack

<div align="left">
  <h3>Environment</h3>
  <img src="https://img.shields.io/badge/Visual%20Studio%202022-5C2D91?style=flat&logo=visual-studio&logoColor=white"/>
  <img src="https://img.shields.io/badge/Windows%2010%20(x64)-0078D6?style=flat&logo=windows&logoColor=white"/>
  <br>
  <h3>Development</h3>
  <img src="https://img.shields.io/badge/C++-00599C?style=flat&logo=c%2B%2B&logoColor=white"/>
  <img src="https://img.shields.io/badge/Win32%20API-0078D6?style=flat&logo=windows&logoColor=white"/>
  <img src="https://img.shields.io/badge/Socket%20Programming-000000?style=flat&logo=socket.io&logoColor=white"/>
  <br>
  <h3>Key Technology</h3>
  <img src="https://img.shields.io/badge/IOCP-Async%20I%2FO-FF4785?style=flat"/>
  <img src="https://img.shields.io/badge/Multithreading-Synchronization-FF4785?style=flat"/>
</div>

<br>

## 💡 Key Features
* **IOCP 비동기 서버:** 입출력 완료 포트(IOCP)를 활용하여 적은 수의 스레드로 다수의 클라이언트 접속 및 데이터 처리를 효율적으로 수행합니다.
* **실시간 멀티 채팅:** TCP/IP 소켓을 통해 다중 클라이언트 간 끊김 없는 실시간 대화를 지원합니다.
* **서버 모니터링 UI:** WinAPI 기반의 대화형 다이얼로그를 통해 접속자 IP, 현재 인원수, 접속/종료 로그를 실시간으로 확인합니다.
* **안전한 동기화:** Critical Section 및 Interlocked 함수를 사용하여 멀티스레드 환경에서의 데이터 경쟁을 방지했습니다.

<br>

## 🏗 Architecture & Logic

### 1. Server Architecture (IOCP Model)
서버는 **Proactor 패턴** 기반의 IOCP 모델을 채택했습니다. I/O 작업(Recv, Send)을 운영체제에 비동기로 요청하고, 작업이 완료되면 Worker Thread가 완료 큐(Completion Queue)에서 결과를 꺼내 처리하는 구조입니다.

* **AcceptEx 활용:** 일반 `accept` 대신 비동기 함수인 `AcceptEx`를 사용하여, 연결 수락 과정에서도 블로킹 없이 파이프라인이 유지되도록 설계했습니다.
* **Worker Thread Pool:** CPU 코어 수에 최적화된 스레드 풀을 운영하여 Context Switching 비용을 최소화했습니다.

### 2. Client Architecture (EventSelect Model)
클라이언트는 UI 스레드의 응답성을 해치지 않기 위해 **WSAEventSelect** 모델을 사용했습니다.

* **Network Thread 분리:** 별도의 스레드에서 네트워크 이벤트를 감시(`WSAWaitForMultipleEvents`)합니다.
* **UI Message Passing:** 데이터 수신이나 접속 종료 이벤트 발생 시, `PostMessage`를 통해 UI 스레드로 메시지를 전달하여 안전하게 화면을 갱신합니다.

<br>

## 💻 Core Code Review

### 1. Server: IOCP Worker Thread
`GetQueuedCompletionStatus`를 통해 I/O 완료 통지(Packet)를 대기하며, 완료된 작업의 종류(Accept, Recv, Close)에 따라 분기 처리합니다. 특히 `AcceptEx` 완료 시 다음 연결을 위해 즉시 새로운 비동기 Accept를 예약하는 것이 핵심입니다.

```cpp
unsigned __stdcall Threads::InnerCommThread(void)
{
    // ... (변수 선언 생략)
    while (1) {
        // 완료 큐에서 I/O 작업 결과 확인
        BOOL ok = GetQueuedCompletionStatus(hIOCP, &cbTransferred, &completionKey, &pOV, INFINITE);
        SOCKET sock = (SOCKET)completionKey; 

        // 1. 새로운 클라이언트 접속 (AcceptEx 완료)
        if (sock == lstnsock) {
            // ... (소켓 컨텍스트 업데이트)
            OnAccept(pAcceptOV ? pAcceptOV->commsock : INVALID_SOCKET, acceptBuf);

            // 중요: 끊김 없는 접속 처리를 위해 다음 AcceptEx 미리 등록 (Pipeline 유지)
            SOCKET commsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
            AcceptEx(lstnsock, commsock, acceptBuf, 0, ..., &dwBytes, (WSAOVERLAPPED*)pAOV);
            continue;
        }
        
        // 2. 데이터 수신 (Recv 완료)
        OVERLAPPEDSOCK* pMOV = (OVERLAPPEDSOCK*)pOV; 
        OnRead(pMOV, pMOV->wsaBuf.buf, (int)cbTransferred); // 내부에서 Recv 재요청(Post)
    }
    return 0;
}
