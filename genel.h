#pragma once

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
// katarlar encode edildi mi?
#define ENCODED_STRINGS
#define HATASIZE 256
// debug mode deyilse string leri sakla
#ifdef _DEBUG
#define IFDBG(kod) (kod)
#define PRINTF(str) (_tprintf(TEXT(str)))
#define PRINTF2(str, extra)	(_tprintf(TEXT(str), extra))
#define HATA(veri1, veri2) (HataGoster(TEXT(veri1), TEXT(veri2)))
#define HATA2(veri1, veri2, veri3) (HataGoster2(TEXT(veri1), TEXT(veri2), veri3))
#else
#define IFDBG(kod)
#define PRINTF(str)
#define PRINTF2(str, extra)
#define HATA(veri1, veri2)
#define HATA2(veri1, veri2, veri3)
#endif

// hata gosterme methodlari
inline DWORD HataGoster(PTSTR szHata1, PTSTR szHata2) {
	DWORD dwHataKodu = GetLastError();
#ifdef _DEBUG
	TCHAR szHata[HATASIZE];

#ifdef CONSOLEAPP
	swprintf(szHata, HATASIZE, L"%s: %s. Hata Kodu: %#x\n", szHata1, szHata2, dwHataKodu);
	_tprintf(szHata);
#else
	StringCchPrintf(szHata, HATASIZE, L"%s. Hata: %#x\n", szHata2, dwHataKodu);
	MessageBox(
		NULL,
		szHata,
		szHata1,
		MB_ICONINFORMATION | MB_OK
	);
#endif
#endif
	return dwHataKodu;
}

inline VOID HataGoster2(PTSTR szHata1, PTSTR szHata2, DWORD dwErrorCode) {
#ifdef _DEBUG
	TCHAR szHata[HATASIZE];

#ifdef CONSOLEAPP
	swprintf(szHata, HATASIZE, L"%s: %s. Hata Kodu: %#x\n", szHata1, szHata2, dwHataKodu);
	_tprintf(szHata);
#else
	StringCchPrintf(szHata, HATASIZE, L"%s. Hata: %#x\n", szHata2, dwErrorCode);
	MessageBox(
		NULL,
		szHata,
		szHata1,
		MB_ICONINFORMATION | MB_OK
	);
#endif
#endif
}
