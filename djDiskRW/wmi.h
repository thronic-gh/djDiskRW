#pragma once

#define _WI32_DCOM
#include <comdef.h>
#include <WbemIdl.h>


//
//	Struktur for valgt disk informasjon.
//
struct TargetDisk {
	int Index = -1;
	int BytesPerSector = 0;
	unsigned long long TotalSectors = 0;
	unsigned long long Size = 0;
	int GBsize = 0;
	std::wstring Model = L"";
	std::wstring DeviceID = L"";
} td;

// Se nedenfor.
struct DiskmodelIndexPair {
	std::wstring Model = L"";
	int Index = -1;
};

// Jeg samler disker her for å kunne slå opp index.
std::vector<DiskmodelIndexPair> FoundDiskCollection;


//
//	Klasse for henting av diskinfo via WMI.
//
class wmi 
{
	HRESULT hres = 0;
	IWbemLocator* pLoc = 0;
	IWbemServices* pSvc = 0;
	IEnumWbemClassObject* pEnumerator = 0;
	IWbemClassObject* pclsObj = 0;
	ULONG uReturn = 0;
	HRESULT hr = 0;

	public:
	wmi() 
	{
		hres = CoInitializeEx(0, COINIT_MULTITHREADED);
		if (FAILED(hres)) 
			MiscStaticFuncsClass::GetErrorW(L"Failed to initialize COM library.", true);

		hres = CoInitializeSecurity(0, -1, 0, 0, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE, 0);
		if (FAILED(hres))
			MiscStaticFuncsClass::GetErrorW(L"Failed to initialize DOM security.", true); 

		hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
		if (FAILED(hres)) 
			MiscStaticFuncsClass::GetErrorW(L"Failed to create locator object.", true);
		

		hres = pLoc->ConnectServer(_bstr_t("ROOT\\CIMV2"), 0, 0, 0, 0, 0, 0, &pSvc);
		if (FAILED(hres)) 
			MiscStaticFuncsClass::GetErrorW(L"Could not connect to ROOT\\CIMV2 WMI namespace.", true);

		hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, 0, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, 0, EOAC_NONE);
		if (FAILED(hres)) 
			MiscStaticFuncsClass::GetErrorW(L"Could not set proxy blanket.", true);
	}


	void GetDriveInfo()
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		FoundDiskCollection.clear();
		AddReportText(L"Looking for disks...\n", true, BlueRGB, true);

		hres = pSvc->ExecQuery(
			bstr_t("WQL"),
			bstr_t("SELECT Index,Model FROM Win32_DiskDrive"),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
			0,
			&pEnumerator
		);
		if (FAILED(hres)) 
			MiscStaticFuncsClass::GetErrorW(L"Query for getting disk information failed.", true);

		DiskmodelIndexPair dmip = {L"",-1};
		while (pEnumerator) {
			hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
			if (uReturn == 0)
				break;
			
			hr = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
			dmip.Model = vtProp.bstrVal;
			VariantClear(&vtProp);

			hr = pclsObj->Get(L"Index", 0, &vtProp, 0, 0);
			dmip.Index = vtProp.intVal;
			VariantClear(&vtProp);
			
			pclsObj->Release();
			
			if (dmip.Index == 0)
				AddReportText(dmip.Model +L" (NOTE: Disk 0 is usually the system drive.)", false, RedRGB, false);
			else
				AddReportText(dmip.Model, false, BlackRGB, false);
			
			SendMessage(DiskRulleMeny, CB_ADDSTRING, NULL, (LPARAM)dmip.Model.c_str());
			FoundDiskCollection.push_back(dmip);
			dmip = {L"",-1};
		}

		AddReportText(L"\nFound "+ std::to_wstring(FoundDiskCollection.size()) +L" disks.\n", true, BlueRGB, false);
	}


	int LoadTargetDiskData(int Index, TargetDisk &td)
	{
		VARIANT vtProp;
		VariantInit(&vtProp);
		int output = 0;

		hres = pSvc->ExecQuery(
			bstr_t("WQL"),
			bstr_t("SELECT BytesPerSector,Index,Model,DeviceID,TotalSectors,Size FROM Win32_DiskDrive"),
			WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
			0,
			&pEnumerator
		);
		if (FAILED(hres)) 
			MiscStaticFuncsClass::GetErrorW(L"Query for getting LoadTargetDiskData failed.", true);

		while (pEnumerator) {
			hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
			if (uReturn == 0)
				break;

			hr = pclsObj->Get(L"Index", 0, &vtProp, 0, 0);
			if (vtProp.intVal != Index) {
				VariantClear(&vtProp);
				continue;
			}
			
			hr = pclsObj->Get(L"Index", 0, &vtProp, 0, 0);
			td.Index = vtProp.intVal;
			VariantClear(&vtProp);

			hr = pclsObj->Get(L"BytesPerSector", 0, &vtProp, 0, 0);
			td.BytesPerSector = vtProp.intVal;
			VariantClear(&vtProp);

			hr = pclsObj->Get(L"TotalSectors", 0, &vtProp, 0, 0);
			td.TotalSectors = std::stoull(vtProp.bstrVal);
			VariantClear(&vtProp);

			hr = pclsObj->Get(L"Size", 0, &vtProp, 0, 0);
			td.Size = std::stoull(vtProp.bstrVal);
			VariantClear(&vtProp);

			hr = pclsObj->Get(L"Model", 0, &vtProp, 0, 0);
			td.Model = vtProp.bstrVal;
			VariantClear(&vtProp);

			hr = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
			td.DeviceID = vtProp.bstrVal;
			VariantClear(&vtProp);

			pclsObj->Release();
		}

		return output;
	}


	~wmi()
	{
		// Cleanup.
		pSvc->Release();
		pLoc->Release();
		pEnumerator->Release();
		CoUninitialize();
	}
} w;