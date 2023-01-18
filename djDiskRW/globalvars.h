#pragma once

#define MAX_LOADSTRING 100

//
//	Globale variabler.
//
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];	
WCHAR szWindowClass[MAX_LOADSTRING];	
HWND MainWindow;
HWND TextReportHwnd;
HWND DiskRulleMeny;	
HWND lblVelgDisk;	
HWND ReadBtnHwnd, WriteBtnHwnd, CancelBtnHwnd;	
HWND CancelBtnTooltip;
HFONT StandardFont;	
COLORREF BlueRGB = RGB(0,0,255);
COLORREF RedRGB = RGB(255,0,0);
COLORREF BlackRGB = RGB(0,0,0);
int EnabledMenu = 1;
bool StopRunningTask = false;