#ifndef APPLICATION_H
#define APPLICATION_H

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <memory>
#include "D3DEngine.h"

class Application
{
public:
    Application();
    ~Application();

    int createWindow(int x = CW_USEDEFAULT, int y = CW_USEDEFAULT, int width = 800, int height = 600);

    void run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    std::unique_ptr<D3DEngine> m_engine;
    HWND m_hwnd;

    const wchar_t* className = L"ApplicationWindowClass";
};

#endif //APPLICATION_H