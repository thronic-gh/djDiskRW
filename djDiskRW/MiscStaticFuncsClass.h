#pragma once

//
// Multiverktøy-klasse for diverse nyttige statiske funksjoner.
//

class MiscStaticFuncsClass
{
	private:
	public:

	//
	// lpszFunction = Manuell feilbeskjed.
	// HandleExit = Exit(EXIT_FAILURE).
	//
	static void GetErrorW(std::wstring lpszFunction, bool HandleExit)
	{
		unsigned long err = GetLastError();
		std::wstring lpDisplayBuf;
		wchar_t* lpMsgBuf;

		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&lpMsgBuf,
			0,
			NULL
		);

		lpDisplayBuf.append(lpszFunction + L"\n\n");
		lpDisplayBuf.append(L"Details: (" + std::to_wstring(err) + L") ");
		lpDisplayBuf.append(lpMsgBuf);

		MessageBoxW(
			NULL,
			(LPCWSTR)lpDisplayBuf.c_str(),
			L"Critical Message",
			MB_OK | MB_ICONINFORMATION
		);

		if (HandleExit) {
			// EXIT_FAILURE = 1. 
			// EXIT_SUCCESS = 0.
			exit(EXIT_FAILURE);
		}
	}

	// Charformats for riktekst.
	// dwMask validerer (slår på) ting, som CFM_COLOR for crTextColor, men også dwEffects.
	// https://docs.microsoft.com/en-us/windows/win32/api/richedit/ns-richedit-charformata
	// eks. dwEffects = CFE_BOLD, dwMask = CFM_BOLD | CFM_COLOR
	// dwMask validerer da bruk av CFE_BOLD og crTextColor eller CFE_AUTOCOLOR.
	static void SetCharFormat(HWND hwnd, DWORD dwEffects, DWORD dwMask, COLORREF crTextColor)
	{
		CHARFORMAT2W cfm;
		cfm.cbSize = sizeof(cfm);

		cfm.dwEffects = dwEffects;
		cfm.crTextColor = crTextColor;
		cfm.dwMask = dwMask;

		SendMessage(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfm);
	}
};


//
// Funksjon for å legge til tekst i rapportboksen (TextReportHwnd). Brukes i WinMain og HovedLoop.
//
void AddReportText(std::wstring s, bool bold, COLORREF textcolor, bool clear) 
{	
	// Reserver minne til mellomlager, for å sjekke om tekst er tom eller ikke.
	int TextReportLen = GetWindowTextLengthW(TextReportHwnd);
	wchar_t* tmpTextReport = new wchar_t[TextReportLen+1]; // Husk å delete[].
	if (GetWindowTextW(TextReportHwnd, tmpTextReport, TextReportLen+1) == 0 && TextReportLen != 0) {
		MiscStaticFuncsClass::GetErrorW(L"Could not retrieve previous report text (1).", false);
		return;
	}

	// Sett inn ny tekst på inngangspunkt (slutten).
	// Begynn kun med \r\n (ny linje) hvis innholdet ikke er tomt fra før.
	std::wstring s2 = tmpTextReport;
	delete[] tmpTextReport;

	std::wstring s3 = L"";
	if (clear) {
		s3 = s;
		SendMessage(TextReportHwnd, EM_SETSEL, 0, TextReportLen);
	} else {
		s3 = (s2==L""?L"":L"\r\n")+ s;
		SendMessage(TextReportHwnd, EM_SETSEL, TextReportLen, TextReportLen);
	}

	if (bold)
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, CFE_BOLD, CFM_BOLD | CFM_COLOR, textcolor);
	else
		MiscStaticFuncsClass::SetCharFormat(TextReportHwnd, 0, CFE_BOLD | CFM_COLOR, textcolor);

	SendMessage(TextReportHwnd, EM_REPLACESEL, 0, (LPARAM)s3.c_str());

	// Skroll nedover.
	//SendMessage(TextReportHwnd, WM_VSCROLL, SB_BOTTOM, 0);
}

//
// Slår av og på evt. UI kontroller/elementer. Brukes i WndProc.
//
void EnableUI(int Enable)
{
	EnableWindow(DiskRulleMeny, Enable);
	EnableWindow(ReadBtnHwnd, Enable);
	EnableWindow(WriteBtnHwnd, Enable);
	EnabledMenu = Enable;
}

//
//	Slår av og på stoppknapp.
//
void StoppKnapp(bool onoff)
{
	if (onoff) {
		EnableWindow(CancelBtnHwnd, true);
		ShowWindow(CancelBtnHwnd, 1);
	} else {
		EnableWindow(CancelBtnHwnd, false);
		ShowWindow(CancelBtnHwnd, 0);
	}
}