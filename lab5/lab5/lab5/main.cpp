#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <string>

using namespace std;

#define SHARED_MEM L"Local\\SimpleDataRaceMem"
#define MUTEX_NAME L"Local\\SimpleConsoleMutex"

// Спільна функція для обох дочірніх процесів
int RunChild(bool isPositive) {
    // Відкриття спільної пам'яті, створеної батьківським процесом
    HANDLE hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM);
    int* sharedData = (int*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));

    // М'ютекс потрібен щоб текст у консолі не перемішувався
    HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);

    for (int i = 1; i <= 200; i++) {
        // Зберігаємо локально те, що збираємось записати
        int wroteValue = isPositive ? i : -i;

        // Запис даних у спільну пам'ять
        *sharedData = wroteValue;

        // Штучна затримка (щоб ОС встигла перемкнути процеси місцями)
        for (volatile int k = 0; k < 20000; k++);

        // Читаємо те, що ЗАРАЗ лежить у спільній пам'яті
        int readValue = *sharedData;

        // Вивід (порівнюємо локальне і прочитане)
        WaitForSingleObject(hMutex, INFINITE);
        if (isPositive) {
            printf("Process A - Wrote: %4d | Read: %4d\n", wroteValue, readValue);
        }
        else {
            printf("\t\t\t\tProcess B - Wrote: %4d | Read: %4d\n", wroteValue, readValue);
        }
        ReleaseMutex(hMutex);

        Sleep(5);
    }

    UnmapViewOfFile(sharedData);
    CloseHandle(hMap);
    CloseHandle(hMutex);
    return 0;
}

int main(int argc, char* argv[]) {
    // Якщо програма запущена з аргументом - вона працює як дочірній процес
    if (argc > 1) {
        return RunChild(string(argv[1]) == "pos");
    }

    printf("Process A (Positives)\t\t\tProcess B (Negatives)\n");
    printf("----------------------------------------------------------------------\n");

    // Створення спільної пам'яті та м'ютекса
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(int), SHARED_MEM);
    int* sharedData = (int*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
    *sharedData = 0;

    HANDLE hMutex = CreateMutex(NULL, FALSE, MUTEX_NAME);

    // Запуск двох дочірніх процесів
    STARTUPINFOA si[2] = { { sizeof(STARTUPINFOA) }, { sizeof(STARTUPINFOA) } };
    PROCESS_INFORMATION pi[2];
    HANDLE hProcs[2];

    string cmdA = string(argv[0]) + " pos";
    string cmdB = string(argv[0]) + " neg";
    char bufA[256], bufB[256];
    strcpy(bufA, cmdA.c_str());
    strcpy(bufB, cmdB.c_str());

    CreateProcessA(NULL, bufA, NULL, NULL, FALSE, 0, NULL, NULL, &si[0], &pi[0]);
    CreateProcessA(NULL, bufB, NULL, NULL, FALSE, 0, NULL, NULL, &si[1], &pi[1]);

    hProcs[0] = pi[0].hProcess;
    hProcs[1] = pi[1].hProcess;

    WaitForMultipleObjects(2, hProcs, TRUE, INFINITE);

    CloseHandle(pi[0].hProcess); CloseHandle(pi[0].hThread);
    CloseHandle(pi[1].hProcess); CloseHandle(pi[1].hThread);
    CloseHandle(hMutex);
    UnmapViewOfFile(sharedData);
    CloseHandle(hMap);

    system("pause");
    return 0;
}