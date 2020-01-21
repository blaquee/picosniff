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
	//PFILE_OBJECT pFileObject = nullptr;
	PULONG pIoctl = nullptr;
	PEPROCESS requestorProcess = nullptr;
	ULONG requestorProcessId;
	//char* requestorProcessName;

	PIO_STACK_LOCATION curStack = IoGetCurrentIrpStackLocation(Irp);
	requestorProcess = IoGetRequestorProcess(Irp);
	requestorProcessId = IoGetRequestorProcessId(Irp);

	// information is passed through the FsContext2 pointer for certain operations
	__try
	{
		if (curStack->FileObject->FsContext2)
		{
			pIoctl = (PULONG)(curStack->FileObject->FsContext2);
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,
					   DPFLTR_ERROR_LEVEL,
					   "FsContext Received\n");
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		// reset
		pIoctl = nullptr;
	}

	if (pIoctl)
	{
		switch (*pIoctl)
		{
		case 1:
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,
					   DPFLTR_ERROR_LEVEL,
					   "Pico FsContext IoCtl AdssBusRoot\n");
			break;
		}
		case 2:
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,
					   DPFLTR_ERROR_LEVEL,
					   "Pico FsContext IoCtl AdssBusInstance\n");
			break;
		}
		case 3:
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,
					   DPFLTR_ERROR_LEVEL,
					   "Pico FsContext IoCtl ServerPort\n");
			break;
		}

		case 4:
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,
					   DPFLTR_ERROR_LEVEL,
					   "Pico FsContext IoCtl MessagePort\n");
			break;
		}
		case 6:
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID,
					   DPFLTR_ERROR_LEVEL,
					   "Pico FsContext IoCtl LxProcess\n");
			break;
		}
		}
	}
	
	DbgPrintEx(DPFLTR_IHVDRIVER_ID,
			   DPFLTR_ERROR_LEVEL,
			   "DeviceControl Code Received: 0x%08X\n",
			   "From Process: %wZ\n",
			   curStack->Parameters.DeviceIoControl.IoControlCode,
			   curStack->FileObject->FileName);

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(g_PicoDevExtension->AttachedToDeviceObject, Irp);
	//return g_DevControlFltOriginal(DeviceObject, Irp);
}