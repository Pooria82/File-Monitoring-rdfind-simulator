Read this in other languages: <a href="https://github.com/Pooria82/File-Monitoring-rdfind-simulator/blob/main/README.md">English</a> , <a href="https://github.com/Pooria82/File-Monitoring-rdfind-simulator/blob/main/README.fa.md">Persian</a>

### 1. Libraries

- **windows.h**: Includes functions and structures needed for performing Windows system operations such as file, directory, and string management.
- **tchar.h**: Used for character-independent string data types (TCHAR), enabling the use of both ANSI and Unicode strings.
- **strsafe.h**: Contains safe string-handling functions to prevent buffer overflows.
- **stdio.h** and **stdlib.h**: Includes standard input/output and memory management functions.
- **string.h**: Used for common string operations.
- **openssl/md5.h**: Used for computing the MD5 hash of files.
- **stdbool.h**: Enables the use of the boolean data type (bool) in C.

### 2. Structures

- **FileInfo**: Contains the file path, file size, and MD5 hash of the file.
  
  ```c
  struct FileInfo {
      TCHAR path[MAX_PATH];
      DWORD size;
      char hash[MD5_DIGEST_LENGTH * 2 + 1];
  };
  ```

- **Task**: Holds the task queue items (like the path and task type).
  
  ```c
  struct Task {
      TCHAR path[MAX_PATH];
      bool isDirectory;
      struct Task* next;
  };
  ```

- **TaskQueue**: Manages the task queue using a linked list, critical sections, and condition variables.
  
  ```c
  struct TaskQueue {
      struct Task* head;
      struct Task* tail;
      CRITICAL_SECTION cs;
      CONDITION_VARIABLE cv;
  } taskQueue;
  ```

- **FileHashCount**: Holds file hashes and their repetition counts.
  
  ```c
  struct FileHashCount {
      char hash[MD5_DIGEST_LENGTH * 2 + 1];
      int count;
      struct FileHashCount* next;
  };
  ```

- **FileTypeCount**: Keeps the count of files based on their type (extension).
  
  ```c
  struct FileTypeCount {
      TCHAR extension[BUF_SIZE];
      int count;
  };
  ```

### 3. Global Variables

- **filesMap**: Stores information about all checked files.
- **fileHashCountHead**: Pointer to the head of the linked list holding file hashes and their repetition counts.
- **fileTypeCounts**: Stores counts of files based on their type.
- **cs**: Critical section for synchronizing access to data.
- **Other Variables**: Includes counters and various values to store information during program execution.

### 4. Main Functions

- **InitializeTaskQueue**: Initializes the task queue.
  
  ```c
  void InitializeTaskQueue(struct TaskQueue* queue) {
      queue->head = NULL;
      queue->tail = NULL;
      InitializeCriticalSection(&queue->cs);
      InitializeConditionVariable(&queue->cv);
  }
  ```

- **DestroyTaskQueue**: Destroys the task queue and frees its resources.
  
  ```c
  void DestroyTaskQueue(struct TaskQueue* queue) {
      DeleteCriticalSection(&queue->cs);
  }
  ```

- **EnqueueTask**: Adds tasks to the task queue.
  
  ```c
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
  ```

- **DequeueTask**: Removes and returns tasks from the task queue.
  
  ```c
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
  ```

- **ProcessDirectory**: Processes directories and creates new tasks for subdirectory and file contents.
  
  ```c
  void ProcessDirectory(const TCHAR* directoryPath) {
      WIN32_FIND_DATA ffd;
      HANDLE hFind;
      TCHAR szDir[MAX_PATH];

      StringCchCopy(szDir, MAX_PATH, directoryPath);
      StringCchCat(szDir, MAX_PATH, _T("\\*"));

      _tprintf(_T("Searching in directory: %s\n"), szDir);

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
                  _tprintf(_T("Found directory: %s\n"), subdirectoryPath);
                  EnqueueTask(subdirectoryPath, true);
              }
          } else {
              TCHAR filePath[MAX_PATH];
              StringCchPrintf(filePath, MAX_PATH, _T("%s\\%s"), directoryPath, ffd.cFileName);
              _tprintf(_T("Found file: %s\n"), filePath);
              EnqueueTask(filePath, false);
          }
      } while (FindNextFile(hFind, &ffd) != 0);

      DWORD dwError = GetLastError();
      if (dwError != ERROR_NO_MORE_FILES) {
          printLastError("FindNextFile failed");
      }

      FindClose(hFind);

      EnterCriticalSection(&taskQueue.cs);
      if (taskQueue.head == NULL) {
          done = true;
          WakeAllConditionVariable(&taskQueue.cv);
      }
      LeaveCriticalSection(&taskQueue.cs);
  }
  ```

- **ProcessFile**: Processes files, computes their hashes, and checks for duplicates. If a file is a duplicate, it is deleted and a log entry is written.
  
  ```c
  unsigned __stdcall ProcessFile(void* data) {
      TCHAR* filePath = (TCHAR*)data;
      DWORD processId = GetCurrentProcessId();
      DWORD threadId = GetCurrentThreadId();
      _tprintf(_T("Processing file: %s\n"), filePath);
      char* hash = CalculateFileHash(filePath);

      if (hash == NULL) {
          _tprintf(_T("Hash calculation failed for file: %s\n"), filePath);
          return 1;
      }

      _tprintf(_T("File: %s, MD5 hash: %s\n"), filePath, hash);

      TCHAR logMessage[MAX_PATH + 100];
      StringCchPrintf(logMessage, sizeof(logMessage)/sizeof(logMessage[0]), _T("PID: %lu, TID: %lu, %s"), processId, threadId, filePath);

      EnterCriticalSection(&cs);

      bool isDuplicate = IsDuplicateFile(hash, filePath);
      TCHAR directoryPath[MAX_PATH];
      StringCchCopy(directoryPath, MAX_PATH, filePath);

      TCHAR* lastSlash = _tcsrchr(directoryPath, _T('\\'));
      if (lastSlash != NULL) {
          *lastSlash = _T('\0');
      }

      if (isDuplicate) {
          IncrementFileHashCount(hash);
          for (struct FileHashCount* current = fileHashCountHead; current != NULL; current = current->next) {
              if (strcmp(current->hash, hash) == 0) {
                  if (current->count > 1) {
                      _tprintf(_T("Deleting duplicate file: %s\n"), filePath);
                      TCHAR deleteMessage[MAX_PATH + 150];
                      StringCchPrintf(deleteMessage, sizeof(deleteMessage)/sizeof(deleteMessage[0]), _T("Directory: %s\nPID: %lu, TID: %lu, %s\n%s has duplicate"), directoryPath, processId, threadId, filePath, filePath);
                      WriteLog(directoryPath, deleteMessage);

                      HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                      if (hFile != INVALID_HANDLE_VALUE) {
                          DWORD fileSize = GetFileSize(hFile, NULL);
                          totalSizeAfter -= fileSize;
                          CloseHandle(hFile);
                      }

                      if (!DeleteFile(filePath)) {
                          printLastError("Failed to delete duplicate file");
                      } else {
                          StringCchCopy(deletedFiles[deletedFileCount++], MAX_PATH, filePath);
                          duplicateFileCount++;
                          DecrementFileHashCount(hash);
                          StringCchPrintf(deleteMessage, sizeof(deleteMessage)/sizeof(deleteMessage[0]), _T("PID: %lu, TID: %lu, %s was deleted as a duplicate"), processId, threadId, filePath);
                          WriteLog(directoryPath, deleteMessage);
                      }
                  }
                  break;
              }
          }
      } else {
          InsertFileHash(hash, filePath);
          IncrementFileHashCount(hash);
          TCHAR addMessage[MAX_PATH + 100];
          StringCchPrintf(addMessage, sizeof(addMessage)/sizeof(addMessage[0]), _T("PID: %lu, TID: %lu, %s"), processId, threadId, filePath);
          WriteLog(directoryPath, addMessage);

          HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
          if (hFile != INVALID_HANDLE_VALUE) {
              DWORD fileSize = GetFileSize(hFile, NULL);
              totalSizeAfter += fileSize;
              CloseHandle(hFile);
          }
      }

      LeaveCriticalSection(&cs);

      free(hash);

      return 0;
  }
  ```

These structures and functions outline the overall framework for processing directories and files in parallel, computing their hashes, identifying duplicates, deleting duplicates, and logging the results. The code also handles synchronization across multiple threads and updates shared data structures safely.
