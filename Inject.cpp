#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <tlhelp32.h>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s <pid> <dllpath>\n", argv[0]);
        return 1;
    }

    HANDLE ph; // process handle
    HANDLE rt; // remote thread
    LPVOID rb; // remote buffer

    // handle to kernel32 và truyền vào GetProcAddressA
    HMODULE hKernel32 = GetModuleHandleA("Kernel32");
    VOID* lb = GetProcAddress(hKernel32, "LoadLibraryA");

    // parse process ID
    int pid = atoi(argv[1]);
    if (pid == 0) {
        printf("Invalid PID\n");
        return 1;
    }
    printf("PID: %i\n", pid);

    // Lấy đường dẫn đầy đủ của DLL
    char fullPath[MAX_PATH];
    if (GetFullPathNameA(argv[2], MAX_PATH, fullPath, NULL) == 0) {
        printf("Error getting full path of DLL (%u)\n", GetLastError());
        return 1;
    }

    // Mở tiến trình mục tiêu
    ph = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (ph == NULL) {
        printf("Failed to open process: %u\n", GetLastError());
        return 1;
    }

    // Cấp phát bộ nhớ trong tiến trình mục tiêu
    rb = VirtualAllocEx(ph, NULL, strlen(fullPath) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (rb == NULL) {
        printf("Failed to allocate memory: %u\n", GetLastError());
        CloseHandle(ph);
        return 1;
    }

    // Ghi đường dẫn DLL vào bộ nhớ của tiến trình mục tiêu
    if (!WriteProcessMemory(ph, rb, fullPath, strlen(fullPath) + 1, NULL)) {
        printf("Failed to write memory: %u\n", GetLastError());
        VirtualFreeEx(ph, rb, 0, MEM_RELEASE);
        CloseHandle(ph);
        return 1;
    }

    // Tạo luồng từ xa để nạp DLL
    rt = CreateRemoteThread(ph, NULL, 0, (LPTHREAD_START_ROUTINE)lb, rb, 0, NULL);
    if (rt == NULL) {
        printf("Failed to create remote thread: %u\n", GetLastError());
        VirtualFreeEx(ph, rb, 0, MEM_RELEASE);
        CloseHandle(ph);
        return 1;
    }

    // Chờ luồng hoàn thành
    WaitForSingleObject(rt, INFINITE);
    CloseHandle(rt);
    VirtualFreeEx(ph, rb, 0, MEM_RELEASE);
    CloseHandle(ph);

    return 0;
}
