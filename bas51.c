/*
This source file is part of the TRS-8051 project.  BAS51 is a work alike
rewrite of the Microsoft Level 2 BASIC for the TRS-80 that is intended to run
on an 8051 microprocessor.  This version is in intended to also run under
Windows for testing purposes.

Copyright (C) 2017  Dennis Hawkins

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

If you use this code in any way, I would love to hear from you.  My email
address is: dennis@galliform.com
*/

/*
BAS51.C - Main program.  Also contains the windows driver code.  Note that
Windows runs in Thread 1 and Bas51 runs under Thread 2.  This and the Drivers.C
file create a hardware abstraction layer so the remainder of the program can be
coded without (much) regard for the hardware that it runs on.
*/

#include "bas51.h"

#ifdef __BORLANDC__


#include <process.h>

void PROGMAIN(void);
void DrawScreen(HDC hdc);

const char g_szClassName[] = "myWindowClass";
//HDC G_hdc;
HWND G_hwnd;
WORD KeyBuf[8];
int  KeyIn=0, KeyOut=0;
BIT ForcePaintFlag;
FLOAT WinScaleX, WinScaleY;


// Step 4: the Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_CREATE:
        	VGA_ClrScrn();
            break;

        case WM_PAINT:            //InvalidateRect
        {
            PAINTSTRUCT ps;

            if (BeginPaint(hwnd, &ps))
            {
                DrawScreen(ps.hdc);
                EndPaint(hwnd, &ps);
            }
		}
            break;

        case WM_SYSKEYUP:
        case WM_KEYUP:
            switch((WORD) wParam)
            {
                case VK_SHIFT:
                    if ((lParam & 0x00FF0000) == 0x2A0000) KeyFlags &= ~0x01; // LEFT
                    else KeyFlags &= ~0x02;                             // RIGHT
                    break;

                case VK_CONTROL:
                    if (lParam & 0x01000000) KeyFlags &= ~0x08;      // RIGHT
                    else KeyFlags &= ~0x04;                          // LEFT
                    break;

                case VK_MENU:   // ALT
                    if (lParam & 0x01000000) KeyFlags &= ~0x20;      // RIGHT
                    else KeyFlags &= ~0x10;                          // LEFT
                    break;

                case VK_LWIN:   // LEFT WINDOWS BUTTON
                    KeyFlags &= ~0x40;
                    break;

                case VK_RWIN:   // RIGHT WINDOWS BUTTON
                    KeyFlags &= (BYTE) ~0x80;
                    break;

                default:      // ignore other control keys
                    break;
            }
            return(0);

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN: //WM_CHAR
            switch((WORD) wParam)
            {
                case VK_UP:
                case VK_DOWN:
                case VK_LEFT:
                case VK_RIGHT:
                case VK_DELETE:
                case VK_END:
                case VK_HOME:
                case VK_PRIOR:
                case VK_NEXT:
                case VK_PAUSE:
                    wParam <<= 8;
                    break;

                case VK_SHIFT:
                    if ((lParam & 0x00FF0000) == 0x2A0000) KeyFlags |= 0x01; // LEFT
                    else KeyFlags |= 0x02;                             // RIGHT
                    return(0);

                case VK_CONTROL:
                    if (lParam & 0x01000000) KeyFlags |= 0x08;      // RIGHT
                    else KeyFlags |= 0x04;                          // LEFT
                    return(0);

                case VK_MENU:   // ALT
                    if (lParam & 0x01000000) KeyFlags |= 0x20;      // RIGHT
                    else KeyFlags |= 0x10;                          // LEFT
                    return(0);

                case VK_LWIN:   // LEFT WINDOWS BUTTON
                    KeyFlags |= 0x40;
                    return(0);

                case VK_RWIN:   // RIGHT WINDOWS BUTTON
                    KeyFlags |= 0x80;
                    return(0);

                default:      // ignore other control keys
                    return(0);
            }
            // Fall through

        case WM_CHAR:
           	KeyBuf[KeyIn] = (WORD) wParam;
           	KeyIn++;
           	KeyIn &= 0x7;
            break;

        /*case WM_GETMINMAXINFO:
		{
    		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
    		lpMMI->ptMinTrackSize.x = 66*8*2;
    		lpMMI->ptMinTrackSize.y = 27*14*2;
            ForcePaintFlag = TRUE;
	        break;
		} */

        case WM_SIZE:
		    WinScaleX = LOWORD(lParam) / 512.0;
		    WinScaleY = HIWORD(lParam) / 350.0;
//            ForcePaintFlag = TRUE;
            InvalidateRect(G_hwnd, NULL, FALSE);
//            break;
			// fall through

        case WM_MOVE:
            ForcePaintFlag = TRUE;
        //    InvalidateRect(G_hwnd, NULL, FALSE);
            break;

        case WM_CLOSE:
            KillTimer(hwnd, 1);
            DestroyWindow(hwnd);
        	break;

        case WM_DESTROY:
            PostQuitMessage(0);
        	break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


#pragma argsused
void thread_code(void *threadno)
{
//    VGA_printf("Executing thread number %d\n", (int)threadno);

    PROGMAIN();
    //_endthread();
}

#pragma argsused
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;
    RECT rc;
    int Xpos, Ypos, Xsize, Ysize;

    // Calculate initial window size to be 2/3rds the screen size and centered
    hwnd = GetDesktopWindow();
    GetWindowRect(hwnd, &rc);
    Xsize = rc.right * 2 / 3;
    Ysize = rc.bottom * 2 / 3;
    Xpos = (rc.right - Xsize) / 2;
    Ypos = (rc.bottom - Ysize) / 2;
    WinScaleX = Xsize / 512.0;
    WinScaleY = Ysize / 350.0;



    //Step 1: Registering the Window Class
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOWTEXT;   //(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if(!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    // Step 2: Creating the Window
    G_hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        "TRS8051 BASIC LEVEL II",
        WS_OVERLAPPEDWINDOW,
        Xpos, Ypos, Xsize, Ysize,
        NULL, NULL, hInstance, NULL);

    if(G_hwnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }


    ShowWindow(G_hwnd, nCmdShow);
    UpdateWindow(G_hwnd);

 //   G_hdc = GetDC(hwnd);

    _beginthread(thread_code, 4096, NULL);   // CreateThread

    // Step 3: The Message Loop
    while(GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
//    ReleaseDC(hwnd, G_hdc);
    return Msg.wParam;
}









#endif









void PROGMAIN(void)
{

    // Initialize memory system
    MemInit51();
    InitScreenCount();

    EditLinePtr = 0;
    AutoLine = 0;
    //ExecLinePtr = 0;
    SyntaxErrorCode = ERROR_NONE;
    Running = FALSE;

    VGA_ClrScrn();
    VGA_print("TRS-8051 LEVEL 2 BASIC v1.01\n(c) 2017 By Dennis Hawkins\n\n");
    VGA_printf("%d Bytes Free.\n", BytesFree());


    // main loop
    CurChar = '\r';
    while (1)
    {
        if (Running)
        {
            // Execute line and return pointer to next line
            ExecuteLine();
        }
        else   // edit mode
        {
	        WORD LinePtr;

            if (EditLinePtr == 0 && AutoLine == 0) VGA_print("\nReady\n");
            LinePtr = EditBasicLine(EditLinePtr);
            EditLinePtr = 0;
            if (LinePtr == 0xFFFF)   // no line number, execute now
            {
            	// Immediate lines are stored in the CmdLine Area.
			    CurChar = '\r';
                SetStream51(BasicVars.CmdLine);
                Running = TRUE;
            }
        }
    }

}


