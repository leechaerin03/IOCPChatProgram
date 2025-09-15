#include "stdafx.h"
#include "WinAPI.h"

_Use_decl_annotations_ 
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpszCmdLine, int nCmdShow)
{
    WinAPI windowAPI;
    if (!windowAPI.Init(hInstance, nCmdShow))  //초기화 실패하면 
        return 0;   // 0으로 리턴
    return windowAPI.Run(); //성공시 run 코드 실행

}