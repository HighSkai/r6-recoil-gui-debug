#include <Windows.h>

static void dragMouseDown(int delayMilliseconds) {
    INPUT input;

    input.type = INPUT_MOUSE;
    input.mi.dx = 0;
    input.mi.dy = 1;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    SendInput(1, &input, sizeof(INPUT));

    Sleep(delayMilliseconds);
}

static void dragMouseLeft(int delayMilliseconds) {
    INPUT input;

    input.type = INPUT_MOUSE;
    input.mi.dx = -1;
    input.mi.dy = 0;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;

    SendInput(1, &input, sizeof(INPUT));

    Sleep(delayMilliseconds);
}