#pragma once
#include <ntifs.h>
#include <ntddk.h>



NTSTATUS PicoPassthroughFilter(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS PicoDeviceIOFilter(PDEVICE_OBJECT DeviceObject, PIRP Irp);