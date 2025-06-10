#pragma once
#include <Windows.h>

#define IOCTL_READ_MEM  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_WRITE_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_PATCH_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

struct KERNEL_RW_REQUEST {
    ULONG pid;
    uintptr_t address;
    uintptr_t buffer;
    SIZE_T size;
};

bool DriverWriteMemory(HANDLE hDriver, ULONG pid, uintptr_t addr, void* buffer, SIZE_T size);
bool DriverReadMemory(HANDLE hDriver, ULONG pid, uintptr_t addr, void* buffer, SIZE_T size);
bool DriverPatchMemory(HANDLE hDriver, ULONG pid, uintptr_t addr, void* patch, SIZE_T size);
