#include "stdafx.h"
#include "WinAPI.h"

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int nCmdShow)
{
    WinAPI windowAPI;
    if (!windowAPI.Init(hInstance, nCmdShow))
        return 0;
    return windowAPI.Run();
}