Read this in other languages: <a href="https://github.com/Pooria82/File-Monitoring-rdfind-simulator/blob/main/README.md">English</a> , <a href="https://github.com/Pooria82/File-Monitoring-rdfind-simulator/blob/main/README.fa.md">Persian</a>

#### 1. کتابخانه‌ها

- **windows.h**: شامل توابع و ساختارهای مورد نیاز برای انجام عملیات‌های سیستمی ویندوز مانند مدیریت فایل‌ها، دایرکتوری‌ها، و رشته‌ها.
- **tchar.h**: برای استفاده از انواع داده‌های رشته‌ای مستقل از کاراکتر (TCHAR) که امکان استفاده از رشته‌های ANSI و Unicode را فراهم می‌کند.
- **strsafe.h**: شامل توابع امن برای عملیات‌های رشته‌ای که از بروز سرریز بافر جلوگیری می‌کند.
- **stdio.h** و **stdlib.h**: شامل توابع استاندارد ورودی/خروجی و مدیریت حافظه.
- **string.h**: برای عملیات‌های معمول رشته‌ای.
- **openssl/md5.h**: برای محاسبه هش MD5 فایل‌ها.
- **stdbool.h**: برای استفاده از نوع داده بولین (bool) در C.

#### 2. ساختارها

- **FileInfo**: شامل مسیر فایل، اندازه فایل و هش MD5 فایل.
  
  ```c
  struct FileInfo {
      TCHAR path[MAX_PATH];
      DWORD size;
      char hash[MD5_DIGEST_LENGTH * 2 + 1];
  };
  ```

- **Task**: برای نگهداری وظایف صف (مانند مسیر و نوع وظیفه).
  
  ```c
  struct Task {
      TCHAR path[MAX_PATH];
      bool isDirectory;
      struct Task* next;
  };
  ```

- **TaskQueue**: برای مدیریت صف وظایف با استفاده از لیست پیوندی، بخش‌های بحرانی و متغیرهای شرطی.
  
  ```c
  struct TaskQueue {
      struct Task* head;
      struct Task* tail;
      CRITICAL_SECTION cs;
      CONDITION_VARIABLE cv;
  } taskQueue;
  ```

- **FileHashCount**: برای نگهداری هش فایل‌ها و تعداد تکرار هر هش.
  
  ```c
  struct FileHashCount {
      char hash[MD5_DIGEST_LENGTH * 2 + 1];
      int count;
      struct FileHashCount* next;
  };
  ```

- **FileTypeCount**: برای نگهداری تعداد فایل‌ها بر اساس نوع (پسوند).
  
  ```c
  struct FileTypeCount {
      TCHAR extension[BUF_SIZE];
      int count;
  };
  ```

#### 3. متغیرهای سراسری

- **filesMap**: برای نگهداری اطلاعات تمامی فایل‌های بررسی شده.
- **fileHashCountHead**: اشاره‌گری به سر لیست پیوندی برای نگهداری هش‌های فایل‌ها و تعداد تکرار آن‌ها.
- **fileTypeCounts**: برای نگهداری تعداد فایل‌ها بر اساس نوع فایل.
- **cs**: بخش بحرانی برای همگام‌سازی دسترسی به داده‌ها.
- **متغیرهای دیگر**: شامل شمارشگرها و مقادیر مختلف برای نگهداری اطلاعات در طول اجرای برنامه.

#### 4. توابع اصلی

- **InitializeTaskQueue**: صف وظایف را مقداردهی اولیه می‌کند.
  
  ```c
  void InitializeTaskQueue(struct TaskQueue* queue) {
      queue->head = NULL;
      queue->tail = NULL;
      InitializeCriticalSection(&queue->cs);
      InitializeConditionVariable(&queue->cv);
  }
  ```

- **DestroyTaskQueue**: صف وظایف را از بین می‌برد و منابع آن را آزاد می‌کند.
  
  ```c
  void DestroyTaskQueue(struct TaskQueue* queue) {
      DeleteCriticalSection(&queue->cs);
  }
  ```

- **EnqueueTask**: وظایفی را به صف وظایف اضافه می‌کند.
  
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

- **DequeueTask**: وظایفی را از صف وظایف حذف و بازمی‌گرداند.
  
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

- **ProcessDirectory**: دایرکتوری‌ها را پردازش و وظایف جدیدی برای فایل‌ها و دایرکتوری‌های زیرمجموعه ایجاد می‌کند.
  
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

- **ProcessFile**: فایل‌ها را پردازش می‌کند، هش آن‌ها را محاسبه می‌کند، و بررسی می‌کند که آیا فایل تکراری است یا خیر. در صورت تکراری بودن، فایل حذف می‌شود و لاگ مربوطه نوشته می‌شود.
  
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

          TCHAR* ext = _tcsrchr(filePath, _T('.'));
          if (ext != NULL) {
              IncrementFileTypeCount(ext);
          }

          HANDLE hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
          if (hFile != INVALID_HANDLE_VALUE) {
              DWORD fileSize = GetFileSize(hFile, NULL);
              totalSizeBefore += fileSize;
              totalSizeAfter += fileSize;
              CloseHandle(hFile);
          }
      }

      LeaveCriticalSection(&cs);

      free(hash);
      return 0;
  }
  ```

- **CalculateFileHash**: هش MD5 یک فایل را محاسبه و بازمی‌گرداند.
  
  ```c
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
  ```

- **WriteLog**: پیام‌ها را در فایل‌های لاگ می‌نویسد.
  
  ```c
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
  ```

- **PrintSummary**: خلاصه‌ای از نتایج پردازش را چاپ می‌کند.
  
  ```c
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
  ```

### نحوه عملکرد کد

1. **شروع و مقداردهی اولیه:**
   - کد با مقداردهی اولیه بخش‌های بحرانی (critical sections) و صف وظایف (task queue) آغاز می‌شود. این کار تضمین می‌کند که منابع مورد نیاز برای همگام‌سازی و مدیریت وظایف به درستی مقداردهی اولیه شده‌اند.

2. **ایجاد و اجرای رشته‌های کاری:**
   - تعدادی رشته کاری (threads) ایجاد می‌شود تا وظایف را به صورت همزمان پردازش کنند. این رشته‌ها با استفاده از توابع `CreateThread` ایجاد می‌شوند و هر کدام به طور مستقل وظایف موجود در صف وظایف را پردازش می‌کنند.

3. **اضافه کردن وظیفه اصلی:**
   - مسیر دایرکتوری ریشه از کاربر درخواست می‌شود و به عنوان اولین وظیفه به صف وظایف اضافه می‌شود. این کار با استفاده از تابع `EnqueueTask` انجام می‌شود.

4. **پردازش وظایف:**
   - هر رشته کاری وظایف را از صف وظایف برداشته و پردازش می‌کند. اگر وظیفه یک دایرکتوری باشد، محتویات آن دایرکتوری اسکن شده و وظایف جدید برای هر فایل و دایرکتوری زیرمجموعه ایجاد می‌شود. اگر وظیفه یک فایل باشد، هش آن محاسبه شده و بررسی می‌شود که آیا فایل تکراری است یا خیر. اگر تکراری باشد، فایل حذف شده و لاگ مربوطه نوشته می‌شود.

5. **محاسبه هش فایل‌ها:**
   - با استفاده از تابع `CalculateFileHash`، هش MD5 هر فایل محاسبه می‌شود. این تابع فایل را به صورت باینری باز کرده و داده‌های آن را در بخش‌های کوچک خوانده و هش MD5 را به روز رسانی می‌کند.

6. **مدیریت همزمانی:**
   - برای اطمینان از درست بودن دسترسی به داده‌ها در محیط‌های چند رشته‌ای، از بخش‌های بحرانی (critical sections) و متغیرهای شرطی (condition variables) استفاده می‌شود. این کار تضمین می‌کند که رشته‌ها به صورت همزمان به داده‌های مشترک دسترسی ندارند و در صورت نیاز به همگام‌سازی، به درستی منتظر می‌مانند.

7. **نوشتن لاگ‌ها و نمایش خلاصه:**
   - برای هر فایل بررسی شده و هر فایل حذف شده، لاگ مناسبی نوشته می‌شود که شامل اطلاعاتی نظیر شناسه پردازش (PID) و شناسه رشته کاری (TID) است. این اطلاعات به همراه پیام‌های مناسب در فایل‌های لاگ مربوطه نوشته می‌شوند.
   - در انتها، خلاصه‌ای از نتایج پردازش، شامل تعداد فایل‌های پردازش شده، تعداد فایل‌های تکراری حذف شده و حجم کلی فایل‌ها قبل و بعد از حذف نمایش داده می‌شود.
