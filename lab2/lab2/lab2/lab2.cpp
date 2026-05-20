#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <iostream>
#include <cstdio>
#include <vector>

using namespace std;

enum SyncMode { MODE_NO_SYNC = 1, MODE_EVENT = 2, MODE_CRITICAL = 3 };

struct ThreadData {
    int pairId;
    SyncMode mode;
    bool isPositive;
    HANDLE hSemaphore;
    HANDLE hEvent;
    CRITICAL_SECTION* cs;
    int* sharedMemory;
    HANDLE hHeap;
    FILE* logFile;
    CRITICAL_SECTION* fileCS;
};

DWORD WINAPI ThreadFunction(LPVOID lpParam) {
    ThreadData* data = (ThreadData*)lpParam;

    // Семафор
    WaitForSingleObject(data->hSemaphore, INFINITE);

    int* localValue = (int*)HeapAlloc(data->hHeap, HEAP_ZERO_MEMORY, sizeof(int));
    if (!localValue) {
        ReleaseSemaphore(data->hSemaphore, 1, NULL);
        return 1;
    }

    if (data->mode == MODE_CRITICAL) {
        EnterCriticalSection(data->cs);
    }
    else if (data->mode == MODE_EVENT) {
        WaitForSingleObject(data->hEvent, INFINITE);
    }

    for (int i = 1; i <= 500; i++) {
        if (data->mode == MODE_NO_SYNC) {
            for (volatile int k = 0; k < 10000; k++);
        }

        // Запис у спільну пам'ять
        if (data->isPositive)
            *data->sharedMemory = i;
        else
            *data->sharedMemory = -i;

        *localValue = *data->sharedMemory;

        const char* padding = "";
        const char* modeStr = "NONE";
        if (data->mode == MODE_EVENT) { padding = "\t\t\t"; modeStr = "EVENT"; }
        else if (data->mode == MODE_CRITICAL) { padding = "\t\t\t\t\t\t"; modeStr = "C.S."; }

        char buffer[256];
        sprintf(buffer, "%s[%s] P%d: %4d\n", padding, modeStr, data->pairId, *localValue);

        // Безпечний вивід у консоль
        EnterCriticalSection(data->fileCS);
        printf("%s", buffer);
        if (data->logFile) {
            fprintf(data->logFile, "%s", buffer);
            fflush(data->logFile);
        }
        LeaveCriticalSection(data->fileCS);

        Sleep(1);
    }

    if (data->mode == MODE_CRITICAL) {
        LeaveCriticalSection(data->cs);
    }
    else if (data->mode == MODE_EVENT) {

        SetEvent(data->hEvent);
    }

    HeapFree(data->hHeap, 0, localValue);
    ReleaseSemaphore(data->hSemaphore, 1, NULL);
    return 0;
}

int main() {
    FILE* file = fopen("log_output.txt", "w");
    if (!file) return 1;

    HANDLE hSemaphore = CreateSemaphore(NULL, 2, 2, NULL);

    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(int), L"Local\\MySharedMem");
    int* sharedMemory = (int*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
    *sharedMemory = 0;

    CRITICAL_SECTION pairCS, fileCS;
    InitializeCriticalSectionAndSpinCount(&pairCS, 4000);
    InitializeCriticalSection(&fileCS);

    HANDLE hPairEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

    HANDLE threads[6];
    ThreadData data[6];
    HANDLE heaps[6];

    printf("NONE (Pair 1)       EVENT (Pair 2)       C.S. (Pair 3)\n");
    printf("------------------------------------------------------------\n");

    // Створення 6 потоків у стані CREATE_SUSPENDED
    for (int i = 0; i < 3; i++) {
        SyncMode currentMode = (SyncMode)(i + 1);
        for (int j = 0; j < 2; j++) {
            int idx = i * 2 + j;
            heaps[idx] = HeapCreate(0, 0, 0);

            data[idx].pairId = i + 1;
            data[idx].mode = currentMode;
            data[idx].isPositive = (j == 0);
            data[idx].hSemaphore = hSemaphore;

            // Передаємо одну подію
            data[idx].hEvent = hPairEvent;

            data[idx].cs = &pairCS;
            data[idx].sharedMemory = sharedMemory;
            data[idx].hHeap = heaps[idx];
            data[idx].logFile = file;
            data[idx].fileCS = &fileCS;

            threads[idx] = CreateThread(NULL, 0, ThreadFunction, &data[idx], CREATE_SUSPENDED, NULL);
            SetThreadPriority(threads[idx], (j == 0) ? THREAD_PRIORITY_HIGHEST : THREAD_PRIORITY_LOWEST);
        }
    }

    // Запуск по парах
    for (int i = 0; i < 3; i++) {
        ResumeThread(threads[i * 2]);
        Sleep(10);
        ResumeThread(threads[i * 2 + 1]);

        WaitForMultipleObjects(2, &threads[i * 2], TRUE, INFINITE);
    }

    // Прибирання пам'яті
    for (int i = 0; i < 6; i++) {
        CloseHandle(threads[i]);
        HeapDestroy(heaps[i]);
    }
    DeleteCriticalSection(&pairCS);
    DeleteCriticalSection(&fileCS);
    UnmapViewOfFile(sharedMemory);
    CloseHandle(hMapFile);
    CloseHandle(hSemaphore);
    CloseHandle(hPairEvent);
    fclose(file);

    system("pause");
    return 0;
}