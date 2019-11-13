#include "Header.h"
#include "IrpHandlers.h"


NTSTATUS PicoPassthroughFilter(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	//PPICOSNIFF_DEVICE_EXTENSION pDevExt = (PPICOSNIFF_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PIO_STACK_LOCATION curStack = IoGetCurrentIrpStackLocation(Irp);

	DbgPrintEx(
		DPFLTR_IHVDRIVER_ID,
		DPFLTR_ERROR_LEVEL,
		"Current Major Function: %s\n",
		MajorFunctionToString(curStack->MajorFunction));

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(g_PicoDevExtension->AttachedToDeviceObject, Irp);
}

NTSTATUS PicoDeviceIOFilter(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	PIO_STACK_LOCATION curStack = IoGetCurrentIrpStackLocation(Irp);

	// get current minor/ioctl
	// Get Device IO
	DbgPrintEx(DPFLTR_IHVDRIVER_ID,
			   DPFLTR_ERROR_LEVEL,
			   "DeviceControl Code Received: 0x%08x\n",
			   curStack->Parameters.DeviceIoControl.IoControlCode);
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(g_PicoDevExtension->AttachedToDeviceObject, Irp);
}