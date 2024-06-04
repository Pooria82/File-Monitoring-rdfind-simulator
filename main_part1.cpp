#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <stdbool.h>

#define MAX_THREADS 4
#define BUF_SIZE MAX_PATH
#define MAX_FILES 10000
#define MAX_FILE_TYPES 100

struct FileInfo {
    TCHAR path[MAX_PATH];
    DWORD size;
    char hash[MD5_DIGEST_LENGTH * 2 + 1];
};

struct Task {
    TCHAR path[MAX_PATH];
    bool isDirectory;
    struct Task* next;
};
