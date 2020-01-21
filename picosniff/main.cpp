#include <ntifs.h>
#include <ntddk.h>
#include "IrpHandlers.h"
#include "Header.h"
#include "filter.h"



PDEVICE_OBJECT g_MyDevObject = nullptr;
PPICOSNIFF_DEVICE_EXTENSION g_PicoDevExtension = nullptr;
_DeviceControlFilter g_DevControlFltOriginal = nullptr;
PDRIVER_OBJECT g_LxCoreDriverObject = nullptr;

UNICODE_STRING g_LxssDeviceName = RTL_CONSTANT_STRING(L"\\Device\\lxss");
UNICODE_STRING g_PicoSniffDevice = RTL_CONSTANT_STRING(L"\\Device\\picosniff");
UNICODE_STRING g_PicoSymbolicLink = RTL_CONSTANT_STRING(L"\\?\\picosniff");

static DRIVER_UNLOAD DriverUnload;
extern char* PsGetProcessImageFileName(PEPROCESS p);
NTSTATUS InstallProcessCallback();
NTSTATUS RemoveProcessCallback();
VOID CreatePicoProcessNotifyRoutine(PEPROCESS, HANDLE, PPS_CREATE_NOTIFY_INFO);

LPNTQUERYINFORMATIONPROCESS ZwQueryInformationProcess = NULL;

// borrowed code
PCSTR MajorFunctionToString(UCHAR major)
{
	static const char* strings[] = {
		"IRP_MJ_CREATE",
		"IRP_MJ_CREATE_NAMED_PIPE",
		"IRP_MJ_CLOSE",
		"IRP_MJ_READ",
		"IRP_MJ_WRITE",
		"IRP_MJ_QUERY_INFORMATION",
		"IRP_MJ_SET_INFORMATION",
		"IRP_MJ_QUERY_EA",
		"IRP_MJ_SET_EA",
		"IRP_MJ_FLUSH_BUFFERS",
		"IRP_MJ_QUERY_VOLUME_INFORMATION",
		"IRP_MJ_SET_VOLUME_INFORMATION",
		"IRP_MJ_DIRECTORY_CONTROL",
		"IRP_MJ_FILE_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CONTROL",
		"IRP_MJ_INTERNAL_DEVICE_CONTROL",
		"IRP_MJ_SHUTDOWN",
		"IRP_MJ_LOCK_CONTROL",
		"IRP_MJ_CLEANUP",
		"IRP_MJ_CREATE_MAILSLOT",
		"IRP_MJ_QUERY_SECURITY",
		"IRP_MJ_SET_SECURITY",
		"IRP_MJ_POWER",
		"IRP_MJ_SYSTEM_CONTROL",
		"IRP_MJ_DEVICE_CHANGE",
		"IRP_MJ_QUERY_QUOTA",
		"IRP_MJ_SET_QUOTA",
		"IRP_MJ_PNP",
	};
	NT_ASSERT(major <= IRP_MJ_MAXIMUM_FUNCTION);
	return strings[major];
}


NTSTATUS GetProcNameFromFileObject(PFILE_OBJECT FileObject, PUNICODE_STRING FileName)
{
	NTSTATUS Status;
	//PUNICODE_STRING FilePath;

	if (!FileObject)
	{
		return STATUS_INVALID_PARAMETER;
	}
	// no file name present
	if (&FileObject->FileName == nullptr)
	{
		// we can try to see if the section object contains a backing file
		// or the cache manager contains a pointer to another file object also.
		return STATUS_UNSUCCESSFUL;
	}

	RtlInitUnicodeString(FileName, FileObject->FileName.Buffer);

	return STATUS_SUCCESS;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS Status = STATUS_SUCCESS;
	
	PDEVICE_OBJECT pdLxssDeviceObject = nullptr;
	PFILE_OBJECT foLxssFileObject = nullptr;

	PAGED_CODE();

	if (ZwQueryInformationProcess == NULL)
	{
		UNICODE_STRING routinName;
		RtlInitUnicodeString(&routinName, L"ZwQueryInformationProcess");
		ZwQueryInformationProcess = (LPNTQUERYINFORMATIONPROCESS)MmGetSystemRoutineAddress(&routinName);
		if (NULL == ZwQueryInformationProcess)
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
					   "Could not resolve Export\n");
			return STATUS_UNSUCCESSFUL;
		}
	}
	
	// Create our Device
	Status = IoCreateDevice(DriverObject,
							sizeof(PICOSNIFF_DEVICE_EXTENSION), 
							&g_PicoSniffDevice,
							FILE_DEVICE_NETWORK_FILE_SYSTEM,
							0,
							TRUE,
							&g_MyDevObject);
	if(!NT_SUCCESS(Status))
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
				   "Could not create Device\n");
		return Status;
	}
	// save dev extension
	g_PicoDevExtension = (PPICOSNIFF_DEVICE_EXTENSION)g_MyDevObject->DeviceExtension;

	// lookup lxss device
	Status = IoGetDeviceObjectPointer(&g_LxssDeviceName,
									  FILE_READ_DATA,
									  &foLxssFileObject,
									  &pdLxssDeviceObject);

	if (foLxssFileObject != nullptr)
	{
		g_PicoDevExtension->AttachedFileObject = foLxssFileObject;
	}

	if(!NT_SUCCESS(Status))
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
				   "Could not get Pointer to Device %wZ\n",
				   g_LxssDeviceName);
		IoDeleteDevice(g_MyDevObject);
		return STATUS_UNSUCCESSFUL;
	}
	
	// attach to the Lxss Device Object
	Status = IoAttachDeviceToDeviceStackSafe(g_MyDevObject,
											 pdLxssDeviceObject,
											 &g_PicoDevExtension->AttachedToDeviceObject);
	if(!NT_SUCCESS(Status))
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
				   "Could not attach to DeviceObject %wZ\n",
				   g_LxssDeviceName);
	}
	// if we weren't attached then a lower device will not be present
	if(&g_PicoDevExtension->AttachedToDeviceObject == NULL)
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
				   "Could not attach to Device\n");
		IoDeleteDevice(g_MyDevObject);
		return STATUS_UNSUCCESSFUL;
		
	}
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
			   "Attached to Device Object: %X\n",
			   (PVOID)g_PicoDevExtension->AttachedToDeviceObject);
	
	Status = InstallProcessCallback();
	if (!NT_SUCCESS(Status))
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID,
				   DPFLTR_ERROR_LEVEL,
				   "Process Callback Install failed\n");
		if (g_MyDevObject)
		{
			IoDetachDevice(g_PicoDevExtension->AttachedToDeviceObject);
			IoDeleteDevice(g_MyDevObject);
		}
		return Status;
	}

	// ensure our device has the same flags/characteristics as the lower level driver/s
	if (FlagOn(g_PicoDevExtension->AttachedToDeviceObject->Flags, DO_BUFFERED_IO))
		SetFlag(g_MyDevObject->Flags, DO_BUFFERED_IO);
	if (FlagOn(g_PicoDevExtension->AttachedToDeviceObject->Flags, DO_DIRECT_IO))
		SetFlag(g_MyDevObject->Flags, DO_DIRECT_IO);
	if (FlagOn(g_PicoDevExtension->AttachedToDeviceObject->Characteristics, FILE_DEVICE_SECURE_OPEN))
		SetFlag(g_MyDevObject->Characteristics, FILE_DEVICE_SECURE_OPEN);
	if (FlagOn(g_PicoDevExtension->AttachedToDeviceObject->Flags, DO_DEVICE_HAS_NAME))
		SetFlag(g_MyDevObject->Flags, DO_DEVICE_HAS_NAME);
	
	g_MyDevObject->DeviceType = g_PicoDevExtension->AttachedToDeviceObject->DeviceType;


	
	// major functions
	for(ULONG i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = PicoPassthroughFilter;
	}

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PicoDeviceIOFilter;

	// We aren't receiving the device IO controls for some reason.
	// We'll overwrite the IRP_MJ_DEVICE_CONTROL and save the old one.
	/*
	g_LxCoreDriverObject = pdLxssDeviceObject->DriverObject;
	g_DevControlFltOriginal = g_LxCoreDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];

	DbgPrintEx(DPFLTR_IHVDRIVER_ID,
			   DPFLTR_ERROR_LEVEL, "Current Address of DeviceIO Dispatch: 0x08%X\n"
			   "PicoDriverDispatch: 0x08%X\n",
			   g_DevControlFltOriginal, PicoDeviceIOFilter);
	if (g_DevControlFltOriginal)
	{
		//__debugbreak();
		InterlockedExchange64((PLONG64)&g_LxCoreDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL],
			(LONG64)PicoDeviceIOFilter);

		DbgPrintEx(DPFLTR_IHVDRIVER_ID,
				   DPFLTR_ERROR_LEVEL, "Current Address of DeviceIO Dispatch: 0x08%X\n"
				   "PicoDriverDispatch: 0x08%X\n",
				   g_DevControlFltOriginal, PicoDeviceIOFilter);
	}
	*/
	DriverObject->DriverUnload = DriverUnload;
	
	// clear the initialization flag so we can receive IRPs
	ClearFlag(g_MyDevObject->Flags, DO_DEVICE_INITIALIZING);


	if(foLxssFileObject)
		ObDereferenceObject(foLxssFileObject);
	
	return Status;
}

VOID CreatePicoProcessNotifyRoutine(PEPROCESS Process, HANDLE ProcessId,
									PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	NTSTATUS status = STATUS_SUCCESS;
	ULONG ulSize = 0;
	SUBSYSTEM_INFORMATION_TYPE SubSystemType;
	PUNICODE_STRING sProcessImageName = { 0 };
	HANDLE hProcessHandle = nullptr;
	CLIENT_ID clientId = { 0,0 };
	OBJECT_ATTRIBUTES objAttr;

	if (!CreateInfo)
		return;

	if (CreateInfo->IsSubsystemProcess)
	{
		/*DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
				   "Found Subsystem Process\n");*/

		//retrieve a pseudo handle
		InitializeObjectAttributes(&objAttr, 0,
								   OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
								   0, 0);
		clientId.UniqueProcess = ProcessId;
		clientId.UniqueThread = 0;

		status = ZwOpenProcess(&hProcessHandle, PROCESS_ALL_ACCESS,
							   &objAttr, &clientId);
		if (!NT_SUCCESS(status))
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
					   "Failed to Open Process Handle\n");
		}
		status = ZwQueryInformationProcess(
			hProcessHandle,
			ProcessSubsystemInformation,
			&SubSystemType,
			sizeof(SUBSYSTEM_INFORMATION_TYPE),
			&ulSize);

		if (!NT_SUCCESS(status))
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL,
					   "Failed to Query Process Information\n");
		}

		if (SubSystemType == SubsystemInformationTypeWSL)
		{
			// we have a WSL process
			// Get the name
			status = SeLocateProcessImageName(
				Process,
				&sProcessImageName);

			DbgPrintEx(DPFLTR_IHVDRIVER_ID,
					   DPFLTR_ERROR_LEVEL,
					   "Pico Process Started: %wZ\n",
					   "\tPico Pid: %d\n",
					   *sProcessImageName,
					   HandleToULong(ProcessId)
					   );
		}

	}

	// release the handle
	if (hProcessHandle)
		ZwClose(hProcessHandle);
}


VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	if (g_DevControlFltOriginal)
	{
		// atomic?
		g_LxCoreDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = g_DevControlFltOriginal;
	}

	RemoveProcessCallback();
	
	if (g_MyDevObject)
	{
		// detach from attached device
		IoDetachDevice(g_PicoDevExtension->AttachedToDeviceObject);
		IoDeleteDevice(g_MyDevObject);
	}
}

NTSTATUS InstallProcessCallback()
{
	NTSTATUS status = STATUS_SUCCESS;

	DbgPrintEx(
		DPFLTR_IHVDRIVER_ID,
		DPFLTR_ERROR_LEVEL,
		"Starting PSCallbacks...\n");
	status = PsSetCreateProcessNotifyRoutineEx2(
		PsCreateProcessNotifySubsystems,
		CreatePicoProcessNotifyRoutine,
		FALSE);
	if (!NT_SUCCESS(status))
	{
		DbgPrintEx(
			DPFLTR_IHVDRIVER_ID,
			DPFLTR_ERROR_LEVEL,
			"PSCallback failed!...%08X\n",
			status);
		return status;
	}
	return status;
}

NTSTATUS RemoveProcessCallback()
{
	return (
		PsSetCreateProcessNotifyRoutineEx2(
			PsCreateProcessNotifySubsystems,
			CreatePicoProcessNotifyRoutine,
			TRUE
		));
}

