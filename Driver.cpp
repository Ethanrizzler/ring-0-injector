#include "Driver.h"

bool SendIoctl(HANDLE hDriver, DWORD code, KERNEL_RW_REQUEST* req) {
    DWORD bytesReturned;
    return DeviceIoControl(hDriver, code, req, sizeof(KERNEL_RW_REQUEST), nullptr, 0, &bytesReturned, nullptr);
}

bool DriverWriteMemory(HANDLE hDriver, ULONG pid, uintptr_t addr, void* buffer, SIZE_T size) {
    KERNEL_RW_REQUEST req = { pid, addr, (uintptr_t)buffer, size };
    return SendIoctl(hDriver, IOCTL_WRITE_MEM, &req);
}

bool DriverReadMemory(HANDLE hDriver, ULONG pid, uintptr_t addr, void* buffer, SIZE_T size) {
    KERNEL_RW_REQUEST req = { pid, addr, (uintptr_t)buffer, size };
    return SendIoctl(hDriver, IOCTL_READ_MEM, &req);
}

bool DriverPatchMemory(HANDLE hDriver, ULONG pid, uintptr_t addr, void* patch, SIZE_T size) {
    KERNEL_RW_REQUEST req = { pid, addr, (uintptr_t)patch, size };
    return SendIoctl(hDriver, IOCTL_PATCH_MEM, &req);
}
