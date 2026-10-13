// graph.cpp - Step 2: a simple Windows window that plots one function.
// Uses only the Win32 API (comes with Windows). Uses the ANSI ("A") versions of
// the functions so we can use plain std::string.
#include <windows.h>
#include <cmath>
#include <string>
#include "expression.h"

const int ID_EDIT = 1;
const int ID_PLOT = 2;
const int TOP_BAR = 40;  // height of the strip holding the text box and button

// Current view of the graph (x range is fixed for now; y range follows the window shape)
double xMin = -10, xMax = 10;

HWND editBox;
std::string formula = "sin(x)";
std::string errorText;

// ---- Converting between math coordinates and screen pixels ----
struct View {
    int w, h;          // size of the graph area in pixels
    double yMin, yMax;

    double toX(int px) const { return xMin + (xMax - xMin) * px / w; }
    int toPx(double x) const { return (int)((x - xMin) / (xMax - xMin) * w); }
    int toPy(double y) const { return (int)((yMax - y) / (yMax - yMin) * h); }
};

View makeView(int w, int h) {
    View v;
    v.w = w;
    v.h = h;
    // Same scale on both axes so circles look like circles.
    double halfHeight = (xMax - xMin) * h / w / 2;
    v.yMin = -halfHeight;
    v.yMax = halfHeight;
    return v;
}

// ---- Drawing ----
void drawGridAndAxes(HDC dc, const View& v) {
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(225, 225, 225));
    HPEN axisPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HGDIOBJ oldPen = SelectObject(dc, gridPen);

    // Pick a label spacing so numbers aren't squashed together.
    double pxPerUnit = v.w / (xMax - xMin);
    int step = 1;
    while (step * pxPerUnit < 40) step *= 2;

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(100, 100, 100));

    // Vertical grid lines + x labels
    for (int i = (int)std::ceil(xMin / step) * step; i <= xMax; i += step) {
        int px = v.toPx(i);
        SelectObject(dc, gridPen);
        MoveToEx(dc, px, 0, NULL);
        LineTo(dc, px, v.h);
        if (i != 0) {
            std::string s = std::to_string(i);
            TextOutA(dc, px + 2, v.toPy(0) + 2, s.c_str(), (int)s.size());
        }
    }
    // Horizontal grid lines + y labels
    for (int i = (int)std::ceil(v.yMin / step) * step; i <= v.yMax; i += step) {
        int py = v.toPy(i);
        SelectObject(dc, gridPen);
        MoveToEx(dc, 0, py, NULL);
        LineTo(dc, v.w, py);
        if (i != 0) {
            std::string s = std::to_string(i);
            TextOutA(dc, v.toPx(0) + 4, py - 16, s.c_str(), (int)s.size());
        }
    }

    // Axes on top of the grid
    SelectObject(dc, axisPen);
    MoveToEx(dc, 0, v.toPy(0), NULL);
    LineTo(dc, v.w, v.toPy(0));
    MoveToEx(dc, v.toPx(0), 0, NULL);
    LineTo(dc, v.toPx(0), v.h);

    SelectObject(dc, oldPen);
    DeleteObject(gridPen);
    DeleteObject(axisPen);
}

void drawFunction(HDC dc, const View& v) {
    Expression f(formula);
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(30, 90, 220));
    HGDIOBJ oldPen = SelectObject(dc, pen);

    bool penDown = false;
    int lastPy = 0;
    errorText.clear();

    // One point per pixel column.
    for (int px = 0; px < v.w; px++) {
        double y = f.evaluate(v.toX(px));
        if (!f.ok()) {
            errorText = f.error();
            break;
        }
        // Skip undefined values (sqrt(-1), 1/0, ...) and absurdly large ones.
        if (!std::isfinite(y) || std::fabs(y) > 1e6) {
            penDown = false;
            continue;
        }
        int py = v.toPy(y);
        // A huge jump between neighbours is a vertical asymptote (like tan) - don't connect it.
        if (penDown && std::abs(py - lastPy) > v.h) penDown = false;

        if (penDown) LineTo(dc, px, py);
        else MoveToEx(dc, px, py, NULL);
        penDown = true;
        lastPy = py;
    }

    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void paintGraph(HDC screen, int clientW, int clientH) {
    int w = clientW, h = clientH - TOP_BAR;
    if (w <= 0 || h <= 0) return;

    // Draw into an off-screen bitmap first, then copy it, so there's no flicker.
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    RECT r = {0, 0, w, h};
    FillRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));

    View v = makeView(w, h);
    drawGridAndAxes(dc, v);
    drawFunction(dc, v);

    if (!errorText.empty()) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(200, 0, 0));
        std::string msg = "Error: " + errorText;
        TextOutA(dc, 8, 8, msg.c_str(), (int)msg.size());
    }

    BitBlt(screen, 0, TOP_BAR, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

// ---- Window ----
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateWindowA("STATIC", "f(x) =", WS_CHILD | WS_VISIBLE,
                      10, 10, 45, 20, hwnd, NULL, NULL, NULL);
        editBox = CreateWindowA("EDIT", formula.c_str(),
                                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                60, 8, 300, 24, hwnd, (HMENU)ID_EDIT, NULL, NULL);
        CreateWindowA("BUTTON", "Plot", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      370, 8, 70, 24, hwnd, (HMENU)ID_PLOT, NULL, NULL);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wp) == ID_PLOT) {
            char buf[256];
            GetWindowTextA(editBox, buf, sizeof(buf));
            formula = buf;
            InvalidateRect(hwnd, NULL, FALSE);  // ask Windows to repaint
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        paintGraph(dc, client.right, client.bottom);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "GraphWindow";
    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA("GraphWindow", "Graphing Calculator", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                              NULL, NULL, inst, NULL);
    ShowWindow(hwnd, show);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
