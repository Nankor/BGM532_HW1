// this is the packer of HW1 application
// it expects an argument form the cmment prompt (kullanicismi)
// what this packer do:
// 

#include <windows.h>
#include <tchar.h>
#include <Strsafe.h>
// my headers
#include "../WindowsServisDeneme/genel.h"
#include "../WindowsServisDeneme/SifreAlgs.h"
#include "../WindowsServisDeneme/Shared.h"

CHAR gszExeKatarSifre[] = STRKEY;

TCHAR szSrcExename_[] = TEXT("Ornek007.exe");
TCHAR szSrcExename[MAX_PATH] = TEXT("");
TCHAR szDscExename_[] = TEXT("Ornek007New.exe");
TCHAR szDscExename[MAX_PATH] = TEXT("");
CHAR szID[27];

PSTR szKatarListesi[] = {
	HEADERS,
	WEBSITEPAGE,
	WEBADRESS1,
	WEBADRESS2
};

// generates a random base64 string with size dwSize
// szData will store the data, must has at least dwSize free space
// dwSize should be multiple of 4 
// lpszID nin yeterince büyük olmasi lazim yoksa sorun olur
// dwSize 64 den fazla veya 8 den az olmamali
BOOL GetRandomCharSeq(PCHAR szData, DWORD dwSize)
{
	// Declare and initialize variables.
	HCRYPTPROV	hCryptProv;
	BYTE		pbData[64];
	BOOL		bResult = FALSE;

	// 4 ün kati olmasza tam bolunmez sorun olmasin...
	if (dwSize % 4 != 0)
		goto cleanup;
	dwSize = dwSize / 4 * 3; // base64 cevrimine hazirlik, ona göre size degisecek
	// dwSize'in cok küçük veya çok büyük olmamasi lazim
	if (dwSize > 64 || dwSize < 6) dwSize = 64;

	// Acquire a cryptographic provider context handle.
	if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, 0))
	{
		// Try to create a new key container
		if (!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET))
		{
			HATA("GetRandomCharSeq", "CryptAcquireContext failed.");
			goto cleanup;
		}
	}
	// Generate a random initialization vector.
	if (!CryptGenRandom(hCryptProv, dwSize, pbData))
	{
		HATA("GetRandomCharSeq", "CryptGenRandom failed.");
		goto cleanup;
	}

	// base64 ünü al (bu size i 4/3 oraninda buyutecek)
	Base64encode(szData, (const char*)pbData, dwSize);
	// buraya kadar bi sorun yoksa 
	bResult = TRUE;
	// Clean up
cleanup:
	if (hCryptProv) {
		if (!CryptReleaseContext(hCryptProv, 0)) {
			HATA("GetRandomCharSeq", "CryptReleaseContext failed");
		}
	}

	return bResult;
}

// read from file with PATH szFileName
// dont forget to free the result
PCHAR DosyaOku(PTSTR szFileName, PDWORD pdwFizeSize){
	DWORD dwStatus = 0;
	BOOL bResult = FALSE;
	HANDLE hFile = NULL;
	DWORD cbRead = 0;
	LARGE_INTEGER fileSize;
	PCHAR veri = NULL;
	// dosyayi ac
	hFile = CreateFile(szFileName,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);
	if (INVALID_HANDLE_VALUE == hFile)
	{
		dwStatus = GetLastError();
		_tprintf(_T("Error opening file %s\nError: %d\n"), szFileName,
			dwStatus);
		goto Temizle;
	}
	// boyutunu ogren
	if (!GetFileSizeEx(hFile, &fileSize)){
		dwStatus = GetLastError();
		_tprintf(_T("Error GetFileSizeEx %s\nError: %d\n"), szFileName, dwStatus);
		goto Temizle;
	}
	// veriyi koymak icin hafizada yer ayarla 
	*pdwFizeSize = fileSize.LowPart;
	veri = (PCHAR)malloc(fileSize.LowPart);
	// dosya icerigini hafizaya al
	if (!ReadFile(hFile, veri, fileSize.LowPart, &cbRead, NULL))
	{
		dwStatus = GetLastError();
		printf("ReadFile failed: %d\n", dwStatus);
		goto Temizle;
	}
	// temizleme islemi
Temizle:
	if (hFile)
		CloseHandle(hFile);

	return veri;
}

// simple XOR cipher
void XorEncode(PBYTE pbVeri, DWORD dwSize, PBYTE bSifre, DWORD dwSifreSize){
	for (DWORD i = 0; i < dwSize; i++)
		if (pbVeri[i] != 0 && pbVeri[i] != bSifre[i % dwSifreSize])
			pbVeri[i] = pbVeri[i] ^ bSifre[i % dwSifreSize];
}

BOOL ChangeBytes(PBYTE pbVeri, DWORD dwSize, PBYTE szOld, PBYTE szNew, DWORD dwLen) {
	PVOID szTempName = NULL;

	for (DWORD i = 0; i < dwSize; i++) {
		for (DWORD m = 0; m < dwLen; m++) {
			if (*(pbVeri + i + m) != PBYTE(szOld)[m]) break;
			else if (m == dwLen - 1) {
				szTempName = (PBYTE(pbVeri) + i);
				goto found;
			}
		}
	}
	// if couldnt found the byte sequence
	return FALSE;

found:
	memcpy_s(szTempName, dwLen + 1, szNew, dwLen);
	return TRUE;
}

// change the szOld string with szNew string within pbVeri  
BOOL ChangeStr(PBYTE pbVeri, DWORD dwSize, PCHAR szOld, PCHAR szNew){
	DWORD dwLen = strlen(szOld);
	if (dwLen != strlen(szNew)) {
		printf("ChangeStr Error! Size mismatch, Old: %s/tNew: %s\n", szOld, szNew);
		return FALSE;
	}

	if (ChangeBytes(pbVeri, dwSize, (PBYTE)szOld, (PBYTE)szNew, dwLen)) {
		return TRUE;
	}

	printf("ChangeStr Error! Couldnt found string: %s\n", szOld);
	return FALSE;
}

// change the szOld unicode string with szNew unicode string within pbVeri 
BOOL ChangeStrW(PBYTE pbVeri, DWORD dwSize, PTCHAR szOld, PTCHAR szNew){
	PVOID szTempName = NULL;
	DWORD dwLen = lstrlen(szOld) * 2;

	if (dwLen != lstrlen(szNew) * 2){
		_tprintf(L"ChangeStrW Error! Size mismatch, Old: %s/tNew: %s\n", szOld, szNew);
		return FALSE;
	}

	if (ChangeBytes(pbVeri, dwSize, (PBYTE)szOld, (PBYTE)szNew, dwLen)) {
		return TRUE;
	}

	_tprintf(L"ChangeStrW Error! Couldnt found string: %s\n", szOld);
	return FALSE;
}

// write to file
BOOL DosyaYaz(PTSTR szFileName, LPVOID lpData, DWORD dwSize){
	HANDLE hLibFile = NULL;
	BOOL bSuccess = FALSE;
	//DWORD err = 0;

	//yeni dosyayý oluþtur //FILE_ATTRIBUTE_SYSTEM biraz daha saklý olmasý için
	hLibFile = CreateFile(szFileName, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hLibFile == INVALID_HANDLE_VALUE){
		//err = GetLastError();
		return FALSE;
	}

	DWORD dwWritten = 0;
	if (!WriteFile(hLibFile, lpData, dwSize, &dwWritten, NULL) || dwSize != dwWritten){
		//err = GetLastError();
		goto temizle;
	}

	bSuccess = TRUE;

temizle:
	if (hLibFile) CloseHandle(hLibFile);

	return bSuccess;
}

BOOL EncodeStr(PBYTE pbVeri, DWORD dwSize, PBYTE pbStr, DWORD dwStrLen, PBYTE pbSifre, DWORD dwSifreSize){
	PBYTE pFoundStrAdr = NULL;//strstr(PCHAR(pbVeri), szOld);

	for (DWORD i = 0; i < dwSize; i++){
		//bFound = FALSE;
		for (DWORD m = 0; m < dwStrLen; m++){
			if (*(pbVeri + i + m) != pbStr[m]) break;
			else if (m == dwStrLen - 1) {
				//bFound = TRUE;
				pFoundStrAdr = pbVeri + i;
				goto found;
			}
		}
	}

	_tprintf(L"EncodeStr HATA!: Eski katar bulunamadi: %s.\n", pbStr);
	return FALSE;

found:
	PBYTE pTemp = (PBYTE)malloc(dwStrLen + 2);
	memcpy_s(pTemp, dwStrLen, pbStr, dwStrLen);

	XorEncode(pTemp, dwStrLen, pbSifre, dwSifreSize);
	memcpy_s(pFoundStrAdr, dwStrLen, pTemp, dwStrLen);

	return TRUE;
}

void main(int argc, CHAR *argv[])
{
	PVOID pVeri1 = NULL;

	// bu exe'nin calistirildigi PATH i bulma
	TCHAR szPath[MAX_PATH];
	GetModuleFileName(NULL, szPath, MAX_PATH);
	PTCHAR szProcessName = wcsrchr(szPath, L'\\') + 1;
	*szProcessName = 0; // dosya ismini path'den ayirmak icin

	// paketlenek olan kaynak exe dosyasinin FULL path'ini bul
	StringCbCopy(szSrcExename, MAX_PATH, szPath);
	StringCbCat(szSrcExename, MAX_PATH, szSrcExename_);
	// paketlenmis hali hangi kopyalanacak hedef exe dosyasinin full path'i ayarla
	StringCbCopy(szDscExename, MAX_PATH, szPath);
	StringCbCat(szDscExename, MAX_PATH, szDscExename_);

	// Kullanim sekli kontrolü, en az bir argumani olmasi lazim
	if (argc < 2){
		// _tprintf(_T("Kullanim Sekli:\n\t %s Kullanicismi\n"), argv[0]);
		return; // yoksa cik
	}

	// random 8 karakter uzunluguna bir ID degeri üretelim
	if (!GetRandomCharSeq(szID, 8)) {
		_tprintf(_T("GetRandomCharSeq basarisiz oldu.\n"));
		goto temizle;
	}

	// Bu kisimda bu random veriye bir kullanicinin ismini ekleyelim -> ID_KULLANICIISMI
	StringCbCatA(szID, 26, "_");
	StringCbCatA(szID, 26, argv[1]);

	// kaynak exe dosyasini oku
	DWORD dwSize1 = 0;
	pVeri1 = DosyaOku(szSrcExename, &dwSize1);
	if (dwSize1 == 0) {
		_tprintf(_T("Kaynak exe dosyadan (%s) okuma basarisiz oldu.\n"), szSrcExename);
		goto temizle;
	}

	// katar sifresini exe ye yaz
	ChangeStr((PBYTE)pVeri1, dwSize1, gszExeKatarSifre, szID);

	// ID bilgisini degistir
	ChangeStr((PBYTE)pVeri1, dwSize1, ID, szID);

	// exe deki katarlarin hepsini sifrele
	for each (PSTR szKatar in szKatarListesi) {
		EncodeStr((PBYTE)pVeri1, dwSize1, (PBYTE)szKatar, strlen(szKatar), (PBYTE)szID, SIFRE_SIZE);
	}
	// paketlenmis hali hedef dosyaya yaz
	if (!DosyaYaz(szDscExename, pVeri1, dwSize1)) {
		_tprintf(_T("Hedef exe dosya (%s) yazma basarisiz oldu.\n"), szDscExename);
		goto temizle;
	}

	printf("%s", szID);

temizle:
	if (pVeri1 != NULL) free(pVeri1);

	return;
}