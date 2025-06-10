
#include <ntifs.h>   
#include <ntddk.h>      
#include <windef.h>    
#define DEVICE_NAME L"\\Device\\KernelComm"
#define SYMBOLIC_NAME L"\\DosDevices\\KernelComm"

#define IOCTL_READ_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_WRITE_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IOCTL_PATCH_MEM CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)


extern "C" NTSTATUS NTAPI MmCopyVirtualMemory(
    PEPROCESS FromProcess,
    PVOID FromAddress,
    PEPROCESS ToProcess,
    PVOID ToAddress,
    SIZE_T BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T NumberOfBytesCopied
);

typedef struct _KERNEL_RW_REQUEST {
    ULONG pid;
    UINT64 address;
    UINT64 buffer; // usermode pointer (buffer in system buffer)
    SIZE_T size;
} KERNEL_RW_REQUEST, * PKERNEL_RW_REQUEST;

PDEVICE_OBJECT gDeviceObject = NULL;

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

NTSTATUS DriverIoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

    if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(KERNEL_RW_REQUEST)) {
        Irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_BUFFER_TOO_SMALL;
    }

    PKERNEL_RW_REQUEST req = (PKERNEL_RW_REQUEST)Irp->AssociatedIrp.SystemBuffer;
    if (!req) {
        Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INVALID_PARAMETER;
    }

    PEPROCESS targetProcess = NULL;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->pid, &targetProcess);
    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    SIZE_T size = req->size;
    UINT8* buffer = (UINT8*)req->buffer;

    KAPC_STATE apcState;
    KeStackAttachProcess(targetProcess, &apcState);

    ULONG_PTR info = 0;

    switch (code) {
    case IOCTL_READ_MEM:
        status = MmCopyVirtualMemory(
            targetProcess,
            (PVOID)(ULONG_PTR)req->address,
            PsGetCurrentProcess(),
            buffer,
            size,
            KernelMode,
            &size);
        info = (ULONG_PTR)size;
        break;

    case IOCTL_WRITE_MEM:
    case IOCTL_PATCH_MEM:
        status = MmCopyVirtualMemory(
            PsGetCurrentProcess(),
            buffer,
            targetProcess,
            (PVOID)(ULONG_PTR)req->address,
            size,
            KernelMode,
            &size);
        info = (ULONG_PTR)size;
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(targetProcess);

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(SYMBOLIC_NAME);
    IoDeleteSymbolicLink(&symLink);
    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(DEVICE_NAME);
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(SYMBOLIC_NAME);

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &gDeviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = IoCreateSymbolicLink(&symLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(gDeviceObject);
        return status;
    }

    for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++) {
        DriverObject->MajorFunction[i] = DriverCreateClose;
    }
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverIoControl;

    DriverObject->DriverUnload = DriverUnload;

    return STATUS_SUCCESS;
}
