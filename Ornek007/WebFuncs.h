#pragma once
#pragma warning(disable : 4995)

#include "genel.h"
#include "SifreAlgs.h"

#define DEFAULT_SIZE 128
#define MSG_SIZE 64
#define BUF_SIZE 1024

BOOL WebPing(LPSTR lpszUserName, LPSTR lpszWebAdres, LPSTR lpszMesaj);
BOOL InternetCheck();
