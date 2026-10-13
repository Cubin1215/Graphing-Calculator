// graph.cpp - Step 3: plots up to 3 functions, with mouse-wheel zoom and drag to pan.
// Uses only the Win32 API (comes with Windows). Uses the ANSI ("A") versions of
// the functions so we can use plain std::string.
#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM / GET_Y_LPARAM
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include "expression.h"

const int ID_PLOT = 100;
const int ID_RESET = 101;
const int ID_EDIT_FIRST = 1;  // edit boxes use ids 1, 2, 3
const int NUM_FUNCS = 3;
const int TOP_BAR = 40;  // height of the strip holding the text boxes and button

// One text box + color per function.
HWND editBoxes[NUM_FUNCS];
HWND labels[NUM_FUNCS];
std::string formulas[NUM_FUNCS] = {"sin(x)", "x^2/10", ""};
std::string errors[NUM_FUNCS];
const COLORREF colors[NUM_FUNCS] = {RGB(30, 90, 220), RGB(220, 40, 40), RGB(20, 150, 60)};

// The view: what point is in the middle of the graph, and how wide it is in math units.
double centerX = 0, centerY = 0;
double xRange = 20;  // total width shown (so x goes from -10 to 10 at the start)

// Mouse dragging state
bool dragging = false;
int lastMouseX = 0, lastMouseY = 0;

// Where the mouse is over the graph (window coordinates), for the coordinate readout.
bool mouseOnGraph = false;
int hoverX = 0, hoverY = 0;

// ---- Converting between math coordinates and screen pixels ----
struct View {
    int w, h;  // size of the graph area in pixels
    double xMin, xMax, yMin, yMax;

    double toX(int px) const { return xMin + (xMax - xMin) * px / w; }
    double toY(int py) const { return yMax - (yMax - yMin) * py / h; }
    int toPx(double x) const { return (int)((x - xMin) / (xMax - xMin) * w); }
    int toPy(double y) const { return (int)((yMax - y) / (yMax - yMin) * h); }
};

View makeView(int w, int h) {
    View v;
    v.w = w;
    v.h = h;
    // Same scale on both axes so circles look like circles.
    double yRange = xRange * h / w;
    v.xMin = centerX - xRange / 2;
    v.xMax = centerX + xRange / 2;
    v.yMin = centerY - yRange / 2;
    v.yMax = centerY + yRange / 2;
    return v;
}

// Picks a "nice" grid spacing (1, 2, 5, 10, 20, 50, ... or 0.5, 0.2, 0.1, ...)
// so that grid lines are at least minPixels apart.
double niceStep(double pxPerUnit, double minPixels) {
    double step = std::pow(10.0, std::floor(std::log10(minPixels / pxPerUnit)));
    const double multipliers[] = {1, 2, 5, 10};
    for (double m : multipliers)
        if (step * m * pxPerUnit >= minPixels) return step * m;
    return step * 10;
}

void drawLabel(HDC dc, int x, int y, double value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%g", value);
    TextOutA(dc, x, y, buf, (int)std::strlen(buf));
}

// ---- Drawing ----
void drawGridAndAxes(HDC dc, const View& v) {
    HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(225, 225, 225));
    HPEN axisPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HGDIOBJ oldPen = SelectObject(dc, gridPen);

    double pxPerUnit = v.w / (v.xMax - v.xMin);
    double step = niceStep(pxPerUnit, 50);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(100, 100, 100));

    // Keep the axis labels on screen even when the axes are scrolled away.
    int axisPy = v.toPy(0), axisPx = v.toPx(0);
    int labelY = axisPy < 0 ? 2 : (axisPy > v.h - 18 ? v.h - 18 : axisPy + 2);
    int labelX = axisPx < 0 ? 2 : (axisPx > v.w - 50 ? v.w - 50 : axisPx + 4);

    // Vertical grid lines + x labels
    for (long long i = (long long)std::ceil(v.xMin / step); i * step <= v.xMax; i++) {
        double x = i * step;
        int px = v.toPx(x);
        SelectObject(dc, gridPen);
        MoveToEx(dc, px, 0, NULL);
        LineTo(dc, px, v.h);
        if (i != 0) drawLabel(dc, px + 2, labelY, x);
    }
    // Horizontal grid lines + y labels
    for (long long i = (long long)std::ceil(v.yMin / step); i * step <= v.yMax; i++) {
        double y = i * step;
        int py = v.toPy(y);
        SelectObject(dc, gridPen);
        MoveToEx(dc, 0, py, NULL);
        LineTo(dc, v.w, py);
        if (i != 0) drawLabel(dc, labelX, py - 16, y);
    }

    // Axes on top of the grid
    SelectObject(dc, axisPen);
    MoveToEx(dc, 0, axisPy, NULL);
    LineTo(dc, v.w, axisPy);
    MoveToEx(dc, axisPx, 0, NULL);
    LineTo(dc, axisPx, v.h);

    SelectObject(dc, oldPen);
    DeleteObject(gridPen);
    DeleteObject(axisPen);
}

void drawFunction(HDC dc, const View& v, int index) {
    errors[index].clear();
    if (formulas[index].empty()) return;

    Expression f(formulas[index]);
    HPEN pen = CreatePen(PS_SOLID, 2, colors[index]);
    HGDIOBJ oldPen = SelectObject(dc, pen);

    bool penDown = false;
    int lastPy = 0;

    // One point per pixel column.
    for (int px = 0; px < v.w; px++) {
        double y = f.evaluate(v.toX(px));
        if (!f.ok()) {
            errors[index] = f.error();
            break;
        }
        // Skip undefined values (sqrt(-1), 1/0, ...) and absurdly large ones.
        if (!std::isfinite(y) || std::fabs(y) > 1e9) {
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

    int errorY = 8;
    for (int i = 0; i < NUM_FUNCS; i++) {
        drawFunction(dc, v, i);
        if (!errors[i].empty()) {
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, colors[i]);
            std::string msg = "f" + std::to_string(i + 1) + " error: " + errors[i];
            TextOutA(dc, 8, errorY, msg.c_str(), (int)msg.size());
            errorY += 18;
        }
    }

    // Show the math coordinates under the mouse in the bottom-left corner.
    if (mouseOnGraph) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "x = %.4g   y = %.4g",
                      v.toX(hoverX), v.toY(hoverY - TOP_BAR));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(60, 60, 60));
        TextOutA(dc, 8, h - 22, buf, (int)std::strlen(buf));
    }

    BitBlt(screen, 0, TOP_BAR, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

// ---- Window ----
void readFormulas() {
    for (int i = 0; i < NUM_FUNCS; i++) {
        char buf[256];
        GetWindowTextA(editBoxes[i], buf, sizeof(buf));
        formulas[i] = buf;
    }
}

void zoomAt(HWND hwnd, int mouseX, int mouseY, double factor) {
    RECT client;
    GetClientRect(hwnd, &client);
    int w = client.right, h = client.bottom - TOP_BAR;
    if (w <= 0 || h <= 0) return;

    // Remember which math point is under the cursor, zoom, then shift the center
    // so that same point is still under the cursor.
    View before = makeView(w, h);
    double mx = before.toX(mouseX), my = before.toY(mouseY - TOP_BAR);

    xRange *= factor;
    if (xRange < 1e-6) xRange = 1e-6;
    if (xRange > 1e6) xRange = 1e6;

    View after = makeView(w, h);
    centerX += mx - after.toX(mouseX);
    centerY += my - after.toY(mouseY - TOP_BAR);
    InvalidateRect(hwnd, NULL, FALSE);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        int x = 10;
        for (int i = 0; i < NUM_FUNCS; i++) {
            std::string name = "f" + std::to_string(i + 1) + " =";
            labels[i] = CreateWindowA("STATIC", name.c_str(), WS_CHILD | WS_VISIBLE,
                                      x, 12, 30, 20, hwnd, NULL, NULL, NULL);
            editBoxes[i] = CreateWindowA("EDIT", formulas[i].c_str(),
                                         WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                                         x + 32, 8, 165, 24, hwnd,
                                         (HMENU)(INT_PTR)(ID_EDIT_FIRST + i), NULL, NULL);
            x += 210;
        }
        // BS_DEFPUSHBUTTON makes Enter press "Plot" (see IsDialogMessage in WinMain).
        CreateWindowA("BUTTON", "Plot", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                      x, 8, 60, 24, hwnd, (HMENU)ID_PLOT, NULL, NULL);
        CreateWindowA("BUTTON", "Reset view", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                      x + 66, 8, 80, 24, hwnd, (HMENU)ID_RESET, NULL, NULL);
        return 0;
    }

    // Color each "f1 =" label to match its line on the graph.
    case WM_CTLCOLORSTATIC:
        for (int i = 0; i < NUM_FUNCS; i++) {
            if ((HWND)lp == labels[i]) {
                HDC dc = (HDC)wp;
                SetTextColor(dc, colors[i]);
                SetBkMode(dc, TRANSPARENT);
                return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
            }
        }
        break;

    case WM_COMMAND:
        // IDOK is what Enter sends (through IsDialogMessage).
        if (LOWORD(wp) == ID_PLOT || LOWORD(wp) == IDOK) {
            readFormulas();
            InvalidateRect(hwnd, NULL, FALSE);  // ask Windows to repaint
        } else if (LOWORD(wp) == ID_RESET) {
            centerX = 0;
            centerY = 0;
            xRange = 20;
            InvalidateRect(hwnd, NULL, FALSE);
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

    // ---- Pan with the left mouse button ----
    case WM_LBUTTONDOWN:
        if (GET_Y_LPARAM(lp) >= TOP_BAR) {
            SetFocus(hwnd);  // so the mouse wheel goes to the window, not a text box
            dragging = true;
            lastMouseX = GET_X_LPARAM(lp);
            lastMouseY = GET_Y_LPARAM(lp);
            SetCapture(hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        hoverX = GET_X_LPARAM(lp);
        hoverY = GET_Y_LPARAM(lp);
        mouseOnGraph = hoverY >= TOP_BAR;
        if (!dragging) InvalidateRect(hwnd, NULL, FALSE);
        if (dragging) {
            int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
            RECT client;
            GetClientRect(hwnd, &client);
            double unitsPerPixel = xRange / client.right;
            centerX -= (mx - lastMouseX) * unitsPerPixel;
            centerY += (my - lastMouseY) * unitsPerPixel;
            lastMouseX = mx;
            lastMouseY = my;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        dragging = false;
        ReleaseCapture();
        return 0;

    // ---- Zoom with the mouse wheel ----
    case WM_MOUSEWHEEL: {
        POINT p = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};  // these are screen coordinates
        ScreenToClient(hwnd, &p);
        if (p.y >= TOP_BAR) {
            bool up = GET_WHEEL_DELTA_WPARAM(wp) > 0;
            zoomAt(hwnd, p.x, p.y, up ? 0.85 : 1 / 0.85);
        }
        return 0;
    }

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
                              CW_USEDEFAULT, CW_USEDEFAULT, 940, 640,
                              NULL, NULL, inst, NULL);
    ShowWindow(hwnd, show);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        // Lets Enter press the default button and Tab move between the boxes.
        if (IsDialogMessageA(hwnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
