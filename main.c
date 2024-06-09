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

struct TaskQueue {
    struct Task* head;
    struct Task* tail;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;
} taskQueue;

struct FileHashCount {
    char hash[MD5_DIGEST_LENGTH * 2 + 1];
    int count;
    struct FileHashCount* next;
};

struct FileTypeCount {
    TCHAR extension[BUF_SIZE];
    int count;
};

struct FileInfo filesMap[MAX_FILES];
struct FileHashCount* fileHashCountHead = NULL;
struct FileTypeCount fileTypeCounts[MAX_FILE_TYPES];
CRITICAL_SECTION cs;
int fileCount = 0;
int fileTypeCountSize = 0;
bool done = false;
DWORD totalSizeBefore = 0;
DWORD totalSizeAfter = 0;
int duplicateFileCount = 0;
TCHAR deletedFiles[MAX_FILES][MAX_PATH];
int deletedFileCount = 0;

unsigned __stdcall ProcessFile(void* data);
DWORD WINAPI WorkerThread(LPVOID lpParam);
void EnqueueTask(const TCHAR* path, bool isDirectory);
bool DequeueTask(struct Task* task);
void ProcessDirectory(const TCHAR* directoryPath);
char* CalculateFileHash(const TCHAR* filePath);
bool IsDuplicateFile(const char* hash, const TCHAR* filePath);
void MarkFileAsProcessed(const char* hash, const TCHAR* filePath);
void IncrementFileHashCount(const char* hash);
void DecrementFileHashCount(const char* hash);
void IncrementFileTypeCount(const TCHAR* extension);
void PrintSummary();
void WriteLog(const TCHAR* directoryPath, const TCHAR* message);

void InitializeTaskQueue(struct TaskQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    InitializeCriticalSection(&queue->cs);
    InitializeConditionVariable(&queue->cv);
}

void printLastError(const char* msg) {
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();

    FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

    _tprintf(_T("%s: %s\n"), msg, (TCHAR*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

void DestroyTaskQueue(struct TaskQueue* queue) {
    DeleteCriticalSection(&queue->cs);
}

int main() {
    InitializeCriticalSection(&cs);
    InitializeTaskQueue(&taskQueue);

    HANDLE hThreads[MAX_THREADS];
    for (int i = 0; i < MAX_THREADS; ++i) {
        hThreads[i] = CreateThread(NULL, 0, WorkerThread, NULL, 0, NULL);
    }

    TCHAR directoryPath[BUF_SIZE];
    _tprintf(_T("Enter the root directory path: "));
    _fgetts(directoryPath, BUF_SIZE, stdin);

    size_t len = _tcslen(directoryPath);
    if (directoryPath[len - 1] == _T('\n')) {
        directoryPath[len - 1] = _T('\0');
    }

    _tprintf(_T("Root directory path: %s\n"), directoryPath); 

    EnqueueTask(directoryPath, true);

    for (int i = 0; i < MAX_THREADS; ++i) {
        WaitForSingleObject(hThreads[i], INFINITE);
        CloseHandle(hThreads[i]);
    }

    DestroyTaskQueue(&taskQueue);
    DeleteCriticalSection(&cs);

    PrintSummary();
    return 0;
}

void EnqueueTask(const TCHAR* path, bool isDirectory) {
    struct Task* newTask = (struct Task*)malloc(sizeof(struct Task));
    StringCchCopy(newTask->path, MAX_PATH, path);
    newTask->isDirectory = isDirectory;
    newTask->next = NULL;

    EnterCriticalSection(&taskQueue.cs);
    if (taskQueue.tail == NULL) {
        taskQueue.head = newTask;
        taskQueue.tail = newTask;
    } else {
        taskQueue.tail->next = newTask;
        taskQueue.tail = newTask;
    }
    LeaveCriticalSection(&taskQueue.cs);
    WakeConditionVariable(&taskQueue.cv);
}
bool DequeueTask(struct Task* task) {
    EnterCriticalSection(&taskQueue.cs);
    while (taskQueue.head == NULL && !done) {
        SleepConditionVariableCS(&taskQueue.cv, &taskQueue.cs, INFINITE);
    }
    if (done && taskQueue.head == NULL) {
        LeaveCriticalSection(&taskQueue.cs);
        return false;
    }

    struct Task* temp = taskQueue.head;
    *task = *temp;
    taskQueue.head = taskQueue.head->next;
    if (taskQueue.head == NULL) {
        taskQueue.tail = NULL;
    }
    LeaveCriticalSection(&taskQueue.cs);

    free(temp);
    return true;
}

DWORD WINAPI WorkerThread(LPVOID lpParam) {
    struct Task task;

    while (DequeueTask(&task)) {
        if (task.isDirectory) {
            ProcessDirectory(task.path);
        } else {
            ProcessFile(task.path);
        }
    }

    return 0;
}

void ProcessDirectory(const TCHAR* directoryPath) {
    WIN32_FIND_DATA ffd;
    HANDLE hFind;
    TCHAR szDir[MAX_PATH];

    StringCchCopy(szDir, MAX_PATH, directoryPath);
    StringCchCat(szDir, MAX_PATH, _T("\\*"));

    _tprintf(_T("Searching in directory: %s\n"), szDir); // Debugging line to ensure the search directory is correct

    hFind = FindFirstFile(szDir, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        printLastError("FindFirstFile failed");
        return;
    }

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (_tcscmp(ffd.cFileName, _T(".")) != 0 && _tcscmp(ffd.cFileName, _T("..")) != 0) {
                TCHAR subdirectoryPath[MAX_PATH];
                StringCchPrintf(subdirectoryPath, MAX_PATH, _T("%s\\%s"), directoryPath, ffd.cFileName);
                _tprintf(_T("Found directory: %s\n"), subdirectoryPath); // Debugging line to ensure directories are found
                EnqueueTask(subdirectoryPath, true);
            }
        } else {
            TCHAR filePath[MAX_PATH];
            StringCchPrintf(filePath, MAX_PATH, _T("%s\\%s"), directoryPath, ffd.cFileName);
            _tprintf(_T("Found file: %s\n"), filePath); // Debugging line to ensure files are found
            EnqueueTask(filePath, false);
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    DWORD dwError = GetLastError();
    if (dwError != ERROR_NO_MORE_FILES) {
        printLastError("FindNextFile failed");
    }

    FindClose(hFind);

    // After finishing the directory processing, check if we are done
    EnterCriticalSection(&taskQueue.cs);
    if (taskQueue.head == NULL) {
        done = true;
        WakeAllConditionVariable(&taskQueue.cv);
    }
    LeaveCriticalSection(&taskQueue.cs);
}

unsigned __stdcall ProcessFile(void* data) {
    TCHAR* filePath = (TCHAR*)data;
    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();
    _tprintf(_T("Processing file: %s\n"), filePath); // Debugging line to ensure file processing starts
    char* hash = CalculateFileHash(filePath);

    if (hash == NULL) {
        _tprintf(_T("Hash calculation failed for file: %s\n"), filePath); // Debugging line for hash calculation failure
        return 1; // If hash calculation fails, exit thread
    }

    _tprintf(_T("File: %s, MD5 hash: %s\n"), filePath, hash); // Display the file path and hash

    TCHAR logMessage[MAX_PATH + 100];
    StringCchPrintf(logMessage, sizeof(logMessage)/sizeof(logMessage[0]), _T("PID: %lu, TID: %lu, %s"), processId, threadId, filePath);

    EnterCriticalSection(&cs);

    // Check if the file is a duplicate and update the hash count
    bool isDuplicate = IsDuplicateFile(hash, filePath);
    TCHAR directoryPath[MAX_PATH];
    StringCchCopy(directoryPath, MAX_PATH, filePath);

    // Manually remove the file part to get the directory path
    TCHAR* lastSlash = _tcsrchr(directoryPath, _T('\\'));
    if (lastSlash != NULL) {
        *lastSlash = _T('\0');
    }

    if (isDuplicate) {
        IncrementFileHashCount(hash);
        for (struct FileHashCount* current = fileHashCountHead; current != NULL; current = current->next) {
            if (strcmp(current->hash, hash) == 0) {
                if (current->count > 1) {
                    _tprintf(_T("Deleting duplicate file: %s\n"), filePath); // Debugging line to indicate file deletion
                    TCHAR deleteMessage[MAX_PATH + 150];
                    StringCchPrintf(deleteMessage, sizeof(deleteMessage)/sizeof(deleteMessage[0]), _T("Directory: %s\nPID: %lu, TID: %lu, %s\n%s has duplicate"), directoryPath, processId, threadId, filePath, filePath);
                    WriteLog(directoryPath, deleteMessage);

                    HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        DWORD fileSize = GetFileSize(hFile, NULL);
                        totalSizeAfter -= fileSize; // Update total size after deletion
                        CloseHandle(hFile);
                    }

                    if (!DeleteFile(filePath)) {
                        printLastError("Failed to delete duplicate file");
                    } else {
                        StringCchCopy(deletedFiles[deletedFileCount++], MAX_PATH, filePath); // Keep track of deleted files
                        duplicateFileCount++;
                        DecrementFileHashCount(hash);
                        StringCchPrintf(deleteMessage, sizeof(deleteMessage)/sizeof(deleteMessage[0]), _T("PID: %lu, TID: %lu, %s removed"), processId, threadId, filePath);
                        WriteLog(directoryPath, deleteMessage);
                    }
                }
                break;
            }
        }
    } else {
        IncrementFileHashCount(hash);
        MarkFileAsProcessed(hash, filePath);

        // Update file type count
        TCHAR* ext = _tcsrchr(filePath, _T('.'));
        if (ext != NULL) {
            IncrementFileTypeCount(ext);
        }

        HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, NULL);
            totalSizeBefore += fileSize; // Update total size before deletion
            totalSizeAfter += fileSize; // Initially, total size after deletion is the same
            CloseHandle(hFile);
        }
    }

    LeaveCriticalSection(&cs);

    free(hash);
    return 0;
}
bool IsDuplicateFile(const char* hash, const TCHAR* filePath) {
    for (int i = 0; i < fileCount; i++) {
        if (strcmp(filesMap[i].hash, hash) == 0 && _tcscmp(filesMap[i].path, filePath) != 0) {
            return true;
        }
    }
    return false;
}

void MarkFileAsProcessed(const char* hash, const TCHAR* filePath) {
    struct FileInfo info;
    StringCchCopy(info.path, MAX_PATH, filePath);
    HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        info.size = GetFileSize(hFile, NULL);
        CloseHandle(hFile);
    }
    strcpy_s(info.hash, MD5_DIGEST_LENGTH * 2 + 1, hash);
    filesMap[fileCount++] = info;
}

void IncrementFileHashCount(const char* hash) {
    struct FileHashCount* current = fileHashCountHead;
    while (current != NULL) {
        if (strcmp(current->hash, hash) == 0) {
            current->count++;
            return;
        }
        current = current->next;
    }
    struct FileHashCount* newCount = (struct FileHashCount*)malloc(sizeof(struct FileHashCount));
    strcpy_s(newCount->hash, MD5_DIGEST_LENGTH * 2 + 1, hash);
    newCount->count = 1;
    newCount->next = fileHashCountHead;
    fileHashCountHead = newCount;
}

void DecrementFileHashCount(const char* hash) {
    struct FileHashCount** current = &fileHashCountHead;
    while (*current != NULL) {
        if (strcmp((*current)->hash, hash) == 0) {
            if (--((*current)->count) == 0) {
                struct FileHashCount* temp = *current;
                *current = (*current)->next;
                free(temp);
                return;
            }
        }
        current = &((*current)->next);
    }
}

void IncrementFileTypeCount(const TCHAR* extension) {
    for (int i = 0; i < fileTypeCountSize; i++) {
        if (_tcscmp(fileTypeCounts[i].extension, extension) == 0) {
            fileTypeCounts[i].count++;
            return;
        }
    }
    StringCchCopy(fileTypeCounts[fileTypeCountSize].extension, BUF_SIZE, extension);
    fileTypeCounts[fileTypeCountSize].count = 1;
    fileTypeCountSize++;
}

char* CalculateFileHash(const TCHAR* filePath) {
    FILE* file = _tfopen(filePath, _T("rb"));
    if (!file) {
        printLastError("Failed to open file");
        return NULL;
    }

    MD5_CTX md5;
    MD5_Init(&md5);

    unsigned char buffer[BUFSIZ];
    size_t bytesRead = 0;
    while ((bytesRead = fread(buffer, 1, BUFSIZ, file)) > 0) {
        MD5_Update(&md5, buffer, bytesRead);
    }

    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_Final(hash, &md5);

    char* hashString = (char*)malloc(MD5_DIGEST_LENGTH * 2 + 1);
    if (hashString == NULL) {
        fclose(file);
        _tprintf(_T("Memory allocation error.\n"));
        return NULL;
    }

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf_s(hashString + (i * 2), 3, "%02x", hash[i]);
    }
    hashString[MD5_DIGEST_LENGTH * 2] = '\0';

    fclose(file);
    return hashString;
}

void WriteLog(const TCHAR* directoryPath, const TCHAR* message) {
    TCHAR logFilePath[MAX_PATH];
    StringCchPrintf(logFilePath, MAX_PATH, _T("%s\\log.txt"), directoryPath);

    FILE* logFile = _tfopen(logFilePath, _T("a"));
    if (logFile == NULL) {
        printLastError("Failed to open log file");
        return;
    }

    _ftprintf(logFile, _T("%s\n"), message);
    fclose(logFile);
}

void PrintSummary() {
    _tprintf(_T("Total number of files: %d\n"), fileCount);
    _tprintf(_T("Number of each file type:\n"));
    for (int i = 0; i < fileTypeCountSize; i++) {
        _tprintf(_T("- %s: %d\n"), fileTypeCounts[i].extension, fileTypeCounts[i].count);
    }

    _tprintf(_T("Duplicate file found & removed: %d\n"), duplicateFileCount);
    for (int i = 0; i < deletedFileCount; i++) {
        _tprintf(_T("- %s\n"), deletedFiles[i]);
    }

    _tprintf(_T("Path size before removing: %lu KB\n"), totalSizeBefore / 1024);
    _tprintf(_T("Path size after removing: %lu KB\n"), totalSizeAfter / 1024);
}

