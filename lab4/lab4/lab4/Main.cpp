#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <ctime>

using namespace std;

// Структура для передачі даних першому потоку
struct ThreadData1 {
    vector<int> p1;
    vector<int> p2;
    HANDLE hWritePipe;
    HANDLE hSemaphore;
};

// Структура для передачі даних другому потоку
struct ThreadData2 {
    vector<int> p3;
    HANDLE hReadPipe;
    HANDLE hSemaphore;
};

// Генерація многочлена заданого степеня з випадковими коефіцієнтами
vector<int> GenerateLargePolynomial(int degree) {
    vector<int> poly(degree + 1);
    for (int i = 0; i <= degree; ++i) {
        int val = 0;
        while (val == 0) val = (rand() % 7) - 3;
        poly[i] = val;
    }
    return poly;
}

// Функція для множення двох многочленів
vector<int> MultiplyPolynomials(const vector<int>& A, const vector<int>& B) {
    if (A.empty() || B.empty()) return {};
    vector<int> C(A.size() + B.size() - 1, 0);
    for (size_t i = 0; i < A.size(); ++i) {
        for (size_t j = 0; j < B.size(); ++j) {
            C[i + j] += A[i] * B[j];
        }
    }
    return C;
}

// Форматування многочлена для виводу на екран
string PolynomialToString(const vector<int>& poly) {
    if (poly.empty()) return "0";
    string result = "";
    bool first = true;

    for (size_t i = 0; i < poly.size(); ++i) {
        int c = poly[i];
        if (c == 0) continue;

        if (!first) {
            if (c > 0) result += " + ";
            else { result += " - "; c = -c; }
        }
        else {
            if (c < 0) { result += "-"; c = -c; }
            first = false;
        }

        if (c != 1 || i == 0) result += to_string(c);
        if (i == 1) result += "x";
        else if (i > 1) result += "x^" + to_string(i);
    }

    if (result.empty()) return "0";
    return result;
}

// WORKER 1: Множить P1 та P2
DWORD WINAPI Worker1(LPVOID param) {
    ThreadData1* data = (ThreadData1*)param;

    printf("[Worker 1] Started multiplying P1 and P2 (Both Degree %zu)...\n", data->p1.size() - 1);

    Sleep(800);

	// Обчислення тимчасового результату P1 * P2
    vector<int> pTemp = MultiplyPolynomials(data->p1, data->p2);

    printf("[Worker 1] Multiplication finished. Temporary polynomial degree: %zu\n", pTemp.size() - 1);
    printf("[Worker 1] Signaling Worker 2 to START and sending data through Pipe...\n");

    // ФІНІШ-СТАРТ
    ReleaseSemaphore(data->hSemaphore, 1, NULL);

    // Записуємо результат в Anonymous Pipe
    DWORD bytesWritten;
    int size = pTemp.size();
    WriteFile(data->hWritePipe, &size, sizeof(int), &bytesWritten, NULL);
    WriteFile(data->hWritePipe, pTemp.data(), size * sizeof(int), &bytesWritten, NULL);

    return 0;
}

// WORKER 2: Множить результат на P3
DWORD WINAPI Worker2(LPVOID param) {
    ThreadData2* data = (ThreadData2*)param;

    printf("[Worker 2] Waiting for Worker 1 to finish...\n");

    // ФІНІШ-СТАРТ
    WaitForSingleObject(data->hSemaphore, INFINITE);

    printf("[Worker 2] Received START signal. Reading data from Pipe...\n");

    // Читає дані з Anonymous Pipe
    DWORD bytesRead;
    int size;
    ReadFile(data->hReadPipe, &size, sizeof(int), &bytesRead, NULL);

    vector<int> pTemp(size);
    ReadFile(data->hReadPipe, pTemp.data(), size * sizeof(int), &bytesRead, NULL);

    printf("[Worker 2] Data received. Multiplying temporary polynomial by P3 (Degree %zu)...\n", data->p3.size() - 1);

    Sleep(800);

	// Обчислення фінального результату (P1 * P2) * P3
    vector<int> pFinal = MultiplyPolynomials(pTemp, data->p3);

    // Вивід фінального многочлена
    printf("\n==========================================================================\n");
    printf("[Worker 2] SUCCESS! Final Polynomial Calculated (Degree %zu):\n\n", pFinal.size() - 1);
    printf("%s\n", PolynomialToString(pFinal).c_str());
    printf("\n==========================================================================\n\n");

    return 0;
}

int main() {
    srand((unsigned int)time(0));

    // Степінь многочленів
    int degree = 20;

    vector<int> P1 = GenerateLargePolynomial(degree);
    vector<int> P2 = GenerateLargePolynomial(degree);
    vector<int> P3 = GenerateLargePolynomial(degree);

    printf("==================== MANAGER LOG ====================\n");
    printf("Generated Polynomials of degree %d:\n\n", degree);
    printf("P1 = %s\n\n", PolynomialToString(P1).c_str());
    printf("P2 = %s\n\n", PolynomialToString(P2).c_str());
    printf("P3 = %s\n\n", PolynomialToString(P3).c_str());
    printf("Delegating multiplication (P1 * P2 * P3) to Workers...\n");
    printf("=====================================================\n\n");

    // Створення Anonymous Pipe
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        printf("Failed to create pipe!\n");
        return 1;
    }

	// Семафор для синхронізації
    HANDLE hSemaphore = CreateSemaphore(NULL, 0, 1, NULL);

	// Підготовка даних для потоків
    ThreadData1 tData1 = { P1, P2, hWritePipe, hSemaphore };
    ThreadData2 tData2 = { P3, hReadPipe, hSemaphore };

	// Запуск потоків
    HANDLE hThreads[2];
    hThreads[0] = CreateThread(NULL, 0, Worker1, &tData1, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, Worker2, &tData2, 0, NULL);

    WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);

    printf("[Manager] All workers finished. Closing resources.\n");

    CloseHandle(hThreads[0]);
    CloseHandle(hThreads[1]);
    CloseHandle(hReadPipe);
    CloseHandle(hWritePipe);
    CloseHandle(hSemaphore);

    system("pause");
    return 0;
}