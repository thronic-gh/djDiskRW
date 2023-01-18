
//
// djDiskRW
// ©2023 Dag J Nedrelid <thronic@gmail.com>
//
// Leser og skriver til diskoverflater.
// 
// Leser hver sektor i bulker på 1MiB.
// - Verifiserer 0x00 verdier og rapporterer non-null bytes etter f.eks. sletting.
// - Lesing av hele overflaten vil kunne avdekke problemer med sektorer via SMART.
// 
// Skriver hver sektor i bulker på 1MiB.
// - Skriver 0x00 og verifiserer samtidig resultat. Kan regnes som trygg sletting.
// - Volumer på disk blir koblet fra og låst under skriving.
// 
// For dem som mener at en full zero pass ikke er nok på SSD, 
// finn meg noen som kan gjenopprette meningsfull data etterpå.
//

#include "djDiskRW.h"

//
//	Prototyper.
//
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
void				PrepFonts();
void				AddReportText(std::wstring s, bool bold, COLORREF textcolor, bool clear);
void				HentValgtMenyElement(HWND MenyElement, std::wstring &Menyelement);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	// Tegnsett.
	_setmode(_fileno(stdout), _O_U8TEXT);

    // TODO: Place code here.
	PrepFonts();

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DJDISKRW, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DJDISKRW));

    MSG msg;

	// Last inn diskinfo via WMI.
	w.GetDriveInfo();

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DJDISKRW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DJDISKRW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//	Funksjon for oppsett av vinduer og kontroller. Brukes i WinMain.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	// Legg på versjonsnummer i vindutittel.
	std::wstring WindowTitle;
	WindowTitle.append(szTitle);
	WindowTitle.append(L" 1.0");

	// Lag hovedvinduet.
	// WS_OVERLAPPEDWINDOW byttet med WS_OVERLAPPED for å unngå maksimering, evt. &~WS_MAXIMIZEBOX.
	// Inkluderer WS_THICKFRAME for å beholde sizing. Men begrenset horisontalt i WndProc (WM_SIZING)
	MainWindow = CreateWindowW(
		szWindowClass, 
		WindowTitle.c_str(), 
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME, 
		CW_USEDEFAULT, 
		0, 
		600, 
		300, 
		nullptr, 
		nullptr, 
		hInstance, 
		nullptr
	);
	if (!MainWindow)
		return false;

	// Sett font for hovedvindu.
	SendMessage(TextReportHwnd, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Lag tekstboks for rapportering.
	if (LoadLibraryW(L"Msftedit.dll") == 0)
		MiscStaticFuncsClass::GetErrorW(L"Could not load Msftedit.dll from the system.",true);

	TextReportHwnd = CreateWindowExW(
		WS_EX_CLIENTEDGE, // Bytt WS_EX_CLIENTEDGE med NULL for å fjerne kantlinjer.
		MSFTEDIT_CLASS, // L"EDIT" for normal edit. Bruker RICHEDIT50W for flere muligheter. Samme som WordPad i Windows 10(Spy++).
		L"", 
		WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY,
		2, 50, 580, 498, 
		MainWindow, 
		0, // Trengs egentlig kun for ting i dialoger for å få tak i dem, hvis HWND ikke er nok.
		0, 
		0
	);
	SendMessage(TextReportHwnd, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Label for å velge disk.
	lblVelgDisk = CreateWindowExW(
		NULL, 
		L"STATIC", 
		L"Choose disk:",
		WS_VISIBLE | WS_CHILD,
		5, 5, 100, 15,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	SendMessage(lblVelgDisk, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Lag kanselleringsknapp stoppe lesing/skriving.
	CancelBtnHwnd = CreateWindowExW(
		NULL, 
		L"BUTTON",
		L"",
		BS_BITMAP | WS_VISIBLE | WS_CHILD, 
		305, 19, 20, 21,
		MainWindow, 
		(HMENU)ID_STOPPKNAPP, 
		hInstance,
		NULL
	);
	HANDLE CancelButtonImg = LoadImageW(
		GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDB_CANCELBTN),
		IMAGE_BITMAP,
		NULL,
		NULL,
		LR_DEFAULTCOLOR
	);
	SendMessage(CancelBtnHwnd, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)CancelButtonImg); 

	// Lag tooltip for sjekkboksen for automatisk oppstart.
	CancelBtnTooltip = CreateWindowExW(
		WS_EX_TOPMOST, 
		TOOLTIPS_CLASSW, 
		NULL,
		WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		MainWindow, 
		NULL, 
		hInstance,
		NULL
	);
	TOOLINFOW tf = {0};
	tf.cbSize = TTTOOLINFOW_V1_SIZE;
	tf.hwnd = MainWindow;			// dialogboks eller hovedvindu.
	tf.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	tf.uId = (UINT_PTR)CancelBtnHwnd;	// HWND til kontroll tooltip skal knyttes til.
	tf.lpszText = (wchar_t*)L"Cancel current read/write.";
	if (SendMessage(CancelBtnTooltip, TTM_ADDTOOLW, 0, (LPARAM)&tf) == 1)
		SendMessage(CancelBtnTooltip, TTM_ACTIVATE, 1, 0);

	// Deaktiver stoppknapp før en lesing/skriving begynner.
	EnableWindow(CancelBtnHwnd, false);
	ShowWindow(CancelBtnHwnd, 0);

	// Lag leseknapp for å starte lesing.
	ReadBtnHwnd = CreateWindowExW(
		NULL, 
		L"BUTTON",
		L"Read Disk",
		WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
		335, 20, 100, 20,
		MainWindow, 
		(HMENU)ID_LESEKNAPP, 
		hInstance,
		NULL
	);
	SendMessage(ReadBtnHwnd, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Lag skriveknapp for å starte skriving.
	WriteBtnHwnd = CreateWindowExW(
		NULL, 
		L"BUTTON",
		L"Write Disk",
		WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
		440, 20, 100, 20,
		MainWindow, 
		(HMENU)ID_SKRIVEKNAPP, 
		hInstance,
		NULL
	);
	SendMessage(WriteBtnHwnd, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Lag meny for kvalitetsvalg.
	DiskRulleMeny = CreateWindowExW(
		WS_EX_CLIENTEDGE, 
		L"COMBOBOX", 
		L"",
		CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_VISIBLE,
		5, 20, 300, 300,	// Height må ha nok høyde til å kunne vise alle valg.
		MainWindow, 
		(HMENU)ID_DISKMENY, 
		hInstance,
		NULL
	);
	SendMessage(DiskRulleMeny, WM_SETFONT, (WPARAM)StandardFont, 1);

	// Trenger kun sette hånd-peker 1 gang for alle knapper,
	// da det ser ut som den påvirker standardklassen globalt.
	// GCL_HCURSOR hvis 32-bit, GCLP_ hvis 64-bit.
	SetClassLongPtrW(ReadBtnHwnd, GCLP_HCURSOR, (long long)LoadCursorW(0, IDC_HAND));

	ShowWindow(MainWindow, nCmdShow);
	UpdateWindow(MainWindow);

	// Hold systemet våkent.
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);

	return true;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_COMMAND:
        {
			switch (HIWORD(wParam)) 
			{
				case CBN_SELCHANGE:
				{
					if ((HWND)lParam == DiskRulleMeny) {
						
						// Sett måldisk ..
						std::wstring Menyelement;
						HentValgtMenyElement((HWND)lParam, Menyelement);

						for (int i=0; i<FoundDiskCollection.size(); i++) {
							if (FoundDiskCollection.at(i).Model == Menyelement)
								w.LoadTargetDiskData(FoundDiskCollection.at(i).Index, td);
						}
						td.GBsize = (int)((td.Size / 1000) / 1000) / 1000;

						AddReportText(L"Setting disk to " + Menyelement, true, BlueRGB, true);
						AddReportText(
							L"\nDeviceID: "+ td.DeviceID +L"\n"+
							L"Bytes Per Sector: "+ std::to_wstring(td.BytesPerSector) +L"\n"+
							L"Total Sectors: "+ std::to_wstring(td.TotalSectors) +L"\n"+
							L"Size: "+ std::to_wstring(td.GBsize) +L" GB ("+ std::to_wstring(td.Size) +L" B).\n\n",
							false, BlueRGB, false
						);

						// Juster sektorstørrelse til 1MiB for litt raskere skriving og lesing.
						if (td.BytesPerSector == 512) {
							td.BytesPerSector = 1048576;
							td.TotalSectors = td.TotalSectors / 2048;

						} else if (td.BytesPerSector == 4096) {
							td.BytesPerSector = 1048576;
							td.TotalSectors = td.TotalSectors / 256;
						}
					}
				}
					break;
			}

            switch (LOWORD(wParam))
            {
				case IDM_ABOUT:
					MessageBoxW(
						MainWindow, 
						L"djDiskRW 1.0\n\n"
						L"A program for reading and writing zero sectors on a disk.\n\n"
						L"©2023 Dag J Nedrelid <dj@thronic.com>",
						L"About", 
						MB_OK
					);
					break;

				case IDM_RELOAD_DISKS:
					if (EnabledMenu == 0)
						break;
					SendMessage(DiskRulleMeny, CB_RESETCONTENT, 0, 0);
					td.Index = -1;
					w.GetDriveInfo();
					break;

				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;

				case ID_LESEKNAPP:
				{
					if (td.Index == -1) {
						MessageBoxW(
							MainWindow,
							L"You have to choose a disk.",
							L"No disk selected",
							MB_OK
						);
						break;
					}

					int verifisering = MessageBoxW(
						MainWindow,
						L"You are about to read the entire disk.\n"
						L"Are you sure you want to start?",
						L"ARE YOU SURE?",
						MB_OKCANCEL
					);

					if (verifisering != IDOK)
						break;

					//
					//	Start selve lesingen.
					//
					EnableUI(0);
					std::thread readthread(&readwrite::Read, &rw, td);
					readthread.detach();
					StoppKnapp(true);
				}
					break;

				case ID_SKRIVEKNAPP:
				{
					if (td.Index == -1) {
						MessageBoxW(
							MainWindow,
							L"You have to choose a disk.",
							L"No disk selected",
							MB_OK
						);
						break;
					}

					int verifisering = MessageBoxW(
						MainWindow,
						L"You are about to DELETE/WIPE/ZEROPASS the disk!\n"
						L"Are you sure you want to permanently wipe data?",
						L"ARE YOU SURE?",
						MB_OKCANCEL
					);

					if (verifisering != IDOK) 
						break;
					
					//
					//	Start selve skrivingen.
					//
					EnableUI(0);
					std::thread writethread(&readwrite::Write, &rw, td);
					writethread.detach();
					StoppKnapp(true);
				}
					break;

				case ID_STOPPKNAPP:
				{
					StopRunningTask = true;
				}
					break;

				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
			break;

		// Tilpass rapport teksten ved hvilken som helst size event.
		case WM_SIZE:
		{
			RECT CurMainClientSize;
			GetClientRect(hWnd, &CurMainClientSize);
			
			// Oppdater edit kontrollen.
			SetWindowPos(
				TextReportHwnd, 
				HWND_TOP, 
				2, 
				50, 
				580, 
				CurMainClientSize.bottom - 52,
				SWP_NOMOVE
			);
		}
			break;

		// Kun tillat vertikal resize.
		case WM_SIZING:
		{
			// Hent alle størrelser.
			RECT CurMainWindowSize, CurMainClientSize, ReportTextSize;
			RECT *NewMainWindowSize = (RECT*)lParam;
			GetClientRect(TextReportHwnd, &ReportTextSize);
			GetWindowRect(hWnd, &CurMainWindowSize);
			GetClientRect(hWnd, &CurMainClientSize);
			
			// Tilpass edit hvis noen vil dra opp eller ned på vanlig vis.
			if (wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM) {

				// Oppdater edit kontrollen.
				SetWindowPos(
					TextReportHwnd, 
					HWND_TOP, 
					2, 
					50, 
					580, 
					CurMainClientSize.bottom - 52, 
					SWP_NOMOVE
				);

			} else {
				// Behold størrelse horisontalt.
				NewMainWindowSize->left = CurMainWindowSize.left;
				NewMainWindowSize->right = CurMainWindowSize.right;
			} 

			return (LRESULT)1;
		}
			break;

		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORSTATIC:
		{
			// Håndter farger.
			HDC hdc = (HDC)wParam;
			HWND hwnd = (HWND)lParam;

			if (
				hwnd == lblVelgDisk || 
				hwnd == TextReportHwnd
			) {
				SetTextColor(hdc, RGB(0,0,0));
				SetBkColor(hdc, RGB(255,255,255));
				SetDCBrushColor(hdc, RGB(255,255,255));
			
			}

			// Returnerer DC_BRUSH som standard er hvit. 
			// Returnert brush brukes til å farge bakgrunn.
			// Vi setter likevel eksplisitt ovenfor, 
			// fordi vi er noobs og vil være sikre..
			return (LRESULT) GetStockObject(DC_BRUSH);
		}
			break;

		case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//
// Funksjon for oppsett av fonter. Brukes i WinMain.
//
void PrepFonts()
{
	// Sett opp standard fonter jeg vil bruke.
	StandardFont = CreateFont(
		13,			// Høyde.
		0,			// Bredde.
		0, 
		0, 
		400,			// 0-1000. 400=Normal, 700=bold.
		false,			// Italic.
		false,			// Underline.
		false,			// Strikeout.
		DEFAULT_CHARSET,	// Charset.
		OUT_OUTLINE_PRECIS,	
		CLIP_DEFAULT_PRECIS, 
		ANTIALIASED_QUALITY,	// Kvalitet.
		VARIABLE_PITCH,	
		L"Verdana"		// Font.
	);
}

//
// Hjelpefunksjon for WndProc.
//
void HentValgtMenyElement(HWND MenyElement, std::wstring &Menyelement)
{
	wchar_t ListItem[100];
	SendMessage(
		MenyElement, 
		CB_GETLBTEXT, 
		SendMessage(MenyElement, CB_GETCURSEL, NULL, NULL), // ItemIndex.
		(LPARAM)ListItem
	);
	Menyelement = ListItem;
}