#pragma once

//
//	Klasse for lesing og skriving av diskoverflate.
//
class readwrite
{
	//
	//	Struktur for timeouts for lesing og skriving. Tanken er å kunne gå videre ved dårlige sektorer.
	//	https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts
	//
	_COMMTIMEOUTS commts{
		5000,	// ReadIntervalTimeout
		1,		// ReadTotalTimeoutMultiplier
		5000,	// ReadTotalTimeoutConstant
		1,		// WriteTotalTimeoutMultiplier
		5000	// WriteTotalTimeoutConstant
	};

	// Diverse.
	HANDLE disk = 0;
	int PercentageDone = 0;
	time_t SecondCounter = time(0);
	DWORD BytesHandled = 0;
	LARGE_INTEGER ReadWriteJump{0};
	unsigned long long bps = 0;
	std::wstring nonzerodetected = L"";
	std::wstring errorsdetected = L"";
	int KBps = 0;



	public:

	//
	//	Lesefunksjon.
	//
	void Read(TargetDisk td)
	{
		disk = CreateFileW(td.DeviceID.c_str(),	// Drive to open
			GENERIC_READ,						// Access mode
			FILE_SHARE_READ | FILE_SHARE_WRITE,	// Share Mode
			0,									// Security Descriptor
			OPEN_EXISTING,						// How to create
			0,									// File attributes
			0									// Handle to template
		);
		if (disk == INVALID_HANDLE_VALUE) 
			MiscStaticFuncsClass::GetErrorW(L"An error occurred while opening handle to disk.", true);

		// Sett timeouts på lesing og skriving.
		SetCommTimeouts(disk, &commts);

		// Start lesing av alle sektorer.
		unsigned char *readbuf = new unsigned char[td.BytesPerSector];	
		memset(readbuf, 0x00, td.BytesPerSector);
		errorsdetected = L"";
		nonzerodetected = L"";

		for (unsigned long long SectorNum = 0; SectorNum <= td.TotalSectors; SectorNum++) {
			
			// Skal jobben stoppes?
			if (StopRunningTask) 
				break;

			// Sett lesepunkt.
			ReadWriteJump.QuadPart = SectorNum * td.BytesPerSector;

			// Pass på overlesing på slutten.
			if (td.Size < (unsigned long long)ReadWriteJump.QuadPart)
				ReadWriteJump.QuadPart = td.Size - ReadWriteJump.QuadPart;

			// Les.
			SetFilePointerEx(disk, ReadWriteJump, 0, FILE_BEGIN);
			if (!ReadFile(disk, readbuf, td.BytesPerSector, &BytesHandled, 0)) {

				if (GetLastError() == 23) {
					errorsdetected += L"\nError 23 (CRC) reading block "+ std::to_wstring(SectorNum) +L".";
				} else {
					CloseHandle(disk);
					MiscStaticFuncsClass::GetErrorW(L"An error occurred while reading disk.", true);
				}
			}

			// Prosent.
			PercentageDone = (int)((SectorNum * 100) / td.TotalSectors);
			
			// Fart.
			bps += BytesHandled;
			if (time(0) - SecondCounter >= 1) {
				KBps = (int)(bps / 1024);
				bps = 0;
				SecondCounter = time(0);
			}

			// Oppdater UI.
			AddReportText(L"Reading sector block " + 
			std::to_wstring(SectorNum) + L" of " + 
			std::to_wstring(td.TotalSectors) + 
			L" (" + std::to_wstring(PercentageDone) + L" %) " +
			std::to_wstring(KBps) + L" KiB/s. " +
			nonzerodetected + errorsdetected,
			false, BlueRGB, true);
			UpdateWindow(MainWindow);

			// Se om noen bytes er non-zero, i tilfelle verifisering av slettet disk.
			//if (nonzerodetected != L"")
			//	continue;
			for (int i=0; i<td.BytesPerSector; i++) {
				if (readbuf[i] != 0x00) {
					nonzerodetected = L"\nnon-zero-byte(s) detected in block "+ std::to_wstring(SectorNum) +L", bytenum "+ std::to_wstring(i) +L".";
					break;
				}
			}
		}

		delete[] readbuf;
		CloseHandle(disk);
		EnableUI(1);
		StoppKnapp(false);
		
		if (!StopRunningTask)
			AddReportText(L"Task Complete.\n", true, BlueRGB, false);
		else
			AddReportText(L"Task stopped before completion.\n", true, BlueRGB, false);
	
		if (nonzerodetected == L"" && !StopRunningTask)
			AddReportText(L"This is a wiped disk. Only bytes with zero (0x00) data values was found.", false, BlueRGB, false);
		
		StopRunningTask = false;
	}



	//
	//	Hjelpefunksjon for å dismount volumer på disk før skriving.
	//
	void DismountVol(std::wstring volpath, HANDLE* h)
	{
		*h = CreateFileW(volpath.c_str(),		// Volume to open
			GENERIC_WRITE|GENERIC_READ,			// Access mode
			FILE_SHARE_READ | FILE_SHARE_WRITE,	// Share Mode
			0,									// Security Descriptor
			OPEN_EXISTING,						// How to create
			0,									// File attributes
			0									// Handle to template
		);
		if (*h == INVALID_HANDLE_VALUE) {
			MiscStaticFuncsClass::GetErrorW(L"An error occurred while opening handle to volume " + volpath, false);
			return;
		}

		if (!DeviceIoControl(*h, FSCTL_LOCK_VOLUME, 0, 0, 0, 0, &BytesHandled, 0)) {
			MiscStaticFuncsClass::GetErrorW(L"An error occurred while locking volume " + volpath, false);
			return;
		}

		if (!DeviceIoControl(*h, FSCTL_DISMOUNT_VOLUME, 0, 0, 0, 0, &BytesHandled, 0))  {
			MiscStaticFuncsClass::GetErrorW(L"An error occurred while dismounting volume " + volpath, false);
			return;
		}

		AddReportText(volpath + L" unmounted and locked.", false, BlueRGB, false);
	}



	//
	//	Skrivefunksjon.
	//
	void Write(TargetDisk td)
	{
		disk = CreateFileW(td.DeviceID.c_str(),					// Drive to open
			GENERIC_WRITE|GENERIC_READ,							// Access mode
			FILE_SHARE_READ | FILE_SHARE_WRITE,					// Share Mode
			0,													// Security Descriptor
			OPEN_EXISTING,										// How to create
			FILE_FLAG_WRITE_THROUGH|FILE_FLAG_NO_BUFFERING,		// File attributes
			0													// Handle to template
		);
		if (disk == INVALID_HANDLE_VALUE) 
			MiscStaticFuncsClass::GetErrorW(L"An error occurred while opening handle to disk.", true);

		// Sett timeouts på lesing og skriving.
		SetCommTimeouts(disk, &commts);

		//
		//	Finn volumer på disk og lagre de midlertidig i VolumeNameCollection.
		//
		wchar_t VolumeName[1024] = {0};
		std::vector<std::wstring> VolumeNameCollection;
		HANDLE ffvolhnd = FindFirstVolumeW(VolumeName, 1024);
		if (ffvolhnd != INVALID_HANDLE_VALUE) {
			
			//
			//	Først opprett handle til volumet som FindFirstVolumeW fant.
			//
			HANDLE volhnd = 0;
			VolumeName[wcslen(VolumeName)-1] = '\0';				// Fjern trailing slash.
			volhnd = CreateFileW(VolumeName,						// Volume to open
				GENERIC_WRITE|GENERIC_READ,							// Access mode
				FILE_SHARE_READ | FILE_SHARE_WRITE,					// Share Mode
				0,													// Security Descriptor
				OPEN_EXISTING,										// How to create
				0,													// File attributes
				0													// Handle to template
			);
			if (volhnd == INVALID_HANDLE_VALUE) 
				MiscStaticFuncsClass::GetErrorW(L"An error occurred while opening volhnd (1).", true);

			//
			//	Sjekk deretter om disknummer er riktig.
			//
			VOLUME_DISK_EXTENTS vde {0};
			if (!DeviceIoControl(volhnd, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, &vde, sizeof(vde), &BytesHandled, 0)) {
				MiscStaticFuncsClass::GetErrorW(L"An error occurred while scanning for volumes (1).", false);
				return;
			}
			if (vde.Extents[0].DiskNumber == td.Index) 
				VolumeNameCollection.push_back(VolumeName);
			CloseHandle(volhnd);

			//
			//	Fortsett å se etter andre volumer og lagre volumnavn hvis disknummer er riktig.
			//
			while (FindNextVolumeW(ffvolhnd, VolumeName, 1024)) {

				VolumeName[wcslen(VolumeName)-1] = '\0';				// Fjern trailing slash.
				volhnd = CreateFileW(VolumeName,						// Volume to open
					GENERIC_WRITE|GENERIC_READ,							// Access mode
					FILE_SHARE_READ | FILE_SHARE_WRITE,					// Share Mode
					0,													// Security Descriptor
					OPEN_EXISTING,										// How to create
					0,													// File attributes
					0													// Handle to template
				);
				if (volhnd == INVALID_HANDLE_VALUE) 
					MiscStaticFuncsClass::GetErrorW(L"An error occurred while opening volhnd (2).", true);

				if (!DeviceIoControl(volhnd, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, &vde, sizeof(vde), &BytesHandled, 0)) {
					// Tomme eller offline volumer kan generere feil her, da er det bare å hoppe over.
					CloseHandle(volhnd);
					continue;
				}

				if (vde.Extents[0].DiskNumber == td.Index) 
					VolumeNameCollection.push_back(VolumeName);
				CloseHandle(volhnd);
			}
			FindVolumeClose(ffvolhnd);
		}


		//
		//	Finn stasjonsbokstaver, dismount og lås dem før skriveoperasjon.
		//
		HANDLE VolumeHandles[10] = {0};
		//wchar_t* VolumePathNames = new wchar_t[256];
		//memset(VolumePathNames, 0x00, 256);
		for (size_t i=0; i < VolumeNameCollection.size(); i++) {
			AddReportText(L"Volume found: " + VolumeNameCollection.at(i), false, BlueRGB, false); 

			/* 
				Viser seg at jeg kan bare bruke volumnavn direkte...
				Beholder kode for referanse til å hente monteringspunkt/bokstav til evt. noe annet.

			VolumeNameCollection.at(i) += L"\\";
			if (GetVolumePathNamesForVolumeNameW(VolumeNameCollection.at(i).c_str(), VolumePathNames, 256, &BytesHandled) == 0) {
				delete[] VolumePathNames;
				GetError(L"Error during GetVolumePathNamesForVolumeNameW.");
				continue;
			}

			// Ikke prøv å dismount/lås partisjoner som ikke har bokstav/monteringspunkt.
			// Kan f.eks. være recovery partisjoner o.l.
			if (wcscmp(VolumePathNames, L"") == 0)
				continue;

			// Se kun etter den første bokstaven per volum, 
			// ettersom dette er tilfellet i 99% av disker.
			std::wstring LetterPath = L"\\\\.\\";
			VolumePathNames[wcslen(VolumePathNames)-1] = '\0';
			LetterPath.append(VolumePathNames);
			*/

			DismountVol(VolumeNameCollection.at(i).c_str(), &VolumeHandles[i]);
		}
		//delete[] VolumePathNames;


		//
		//	Vent litt før start. For å få med seg volumer og evt. angre.
		//
		int CountDownToBegin = 5;
		AddReportText(L"Starting in 5 seconds...",false,BlueRGB,false);
		while (CountDownToBegin > 0) {
			Sleep(1000);
			CountDownToBegin -= 1;
		}


		//
		//	Start skriving av alle sektorer.
		//
		unsigned char *writebuf = new unsigned char[td.BytesPerSector];
		memset(writebuf, 0x00, td.BytesPerSector);
		errorsdetected = L"";
		nonzerodetected = L"";

		for (unsigned long long SectorNum = 0; SectorNum <= td.TotalSectors; SectorNum++) {

			// Skal jobben stoppes?
			if (StopRunningTask) 
				break;

			// Sett skrivepunkt.
			ReadWriteJump.QuadPart = SectorNum * td.BytesPerSector;

			// Pass på overskriving på slutten.
			if (td.Size < (unsigned long long)ReadWriteJump.QuadPart)
				ReadWriteJump.QuadPart = td.Size - ReadWriteJump.QuadPart;

			// Skriv.
			SetFilePointerEx(disk, ReadWriteJump, 0, FILE_BEGIN);
			if (!WriteFile(disk, writebuf, td.BytesPerSector, &BytesHandled, 0)) 
				MiscStaticFuncsClass::GetErrorW(L"An error occurred while writing to disk.", true);

			// Les området som nettopp ble skrevet for 0x00 verifisering.
			if (ReadWriteJump.QuadPart >= td.BytesPerSector)
				ReadWriteJump.QuadPart -= td.BytesPerSector;
			SetFilePointerEx(disk, ReadWriteJump, 0, FILE_BEGIN);
			if (!ReadFile(disk, writebuf, td.BytesPerSector, &BytesHandled, 0)) {
				
				if (GetLastError() == 23) {
					errorsdetected += L"\nError 23 (CRC) verifying written block "+ std::to_wstring(SectorNum) +L".";
				} else {
					CloseHandle(disk);
					MiscStaticFuncsClass::GetErrorW(L"An error occurred while verifying written sector block.", true);
				}
			}

			// Se om noen bytes er non-zero.
			for (int i=0; i<td.BytesPerSector; i++) {
				if (writebuf[i] != 0x00) {
					nonzerodetected = L"\nnon-zero-byte(s) detected in written block "+ std::to_wstring(SectorNum) +L", bytenum "+ std::to_wstring(i) +L".";
					break;
				}
			}

			// Sett skrivebuffer tilbake til 0x00 etter at det ble lånt til lesetest.
			memset(writebuf, 0x00, td.BytesPerSector);

			// Prosent.
			PercentageDone = (int)((SectorNum * 100) / td.TotalSectors);
			
			// Fart.
			bps += BytesHandled;
			if (time(0) - SecondCounter >= 1) {
				KBps = (int)(bps / 1024);
				bps = 0;
				SecondCounter = time(0);
			}

			// Oppdater UI.
			AddReportText(L"Writing sector block " + 
			std::to_wstring(SectorNum) + L" of " + 
			std::to_wstring(td.TotalSectors) +
			L" (" + std::to_wstring(PercentageDone) + L" %) " +
			std::to_wstring(KBps) + L" KiB/s. " + 
			nonzerodetected + errorsdetected,
			false, BlueRGB, true);
		}
		delete[] writebuf;


		//
		//	Slipp alle potensielt låste volum handles.
		//
		for (int i=0; i<10; i++)
			CloseHandle(VolumeHandles[i]);

		CloseHandle(disk);
		EnableUI(1);
		StoppKnapp(false);

		if (!StopRunningTask)
			AddReportText(L"Task Complete.\n", true, BlueRGB, false);
		else
			AddReportText(L"Task stopped before completion.\n", true, BlueRGB, false);
		
		if (nonzerodetected == L"" && !StopRunningTask)
			AddReportText(L"This is a wiped disk. All written sectors are verified as zero (0x00).", false, BlueRGB, false);
		
		StopRunningTask = false;
	}
} rw;