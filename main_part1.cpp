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

void DestroyTaskQueue(struct TaskQueue* queue) {
    DeleteCriticalSection(&queue->cs);
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
