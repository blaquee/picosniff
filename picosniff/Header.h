#pragma once
#include <ntifs.h>




typedef struct _PICOCONTEXT
{
	PVOID DeviceCode;
}PICOCONTEXT, *PPICOCONTEXT;

typedef struct _PICOSNIFF_DEVICE_EXTENSION
{
	PDEVICE_OBJECT AttachedToDeviceObject;
	PFILE_OBJECT AttachedFileObject;
} PICOSNIFF_DEVICE_EXTENSION, * PPICOSNIFF_DEVICE_EXTENSION;

typedef NTSTATUS(*_DeviceControlFilter)(PDEVICE_OBJECT, PIRP);

extern UNICODE_STRING g_LxssDeviceName;
extern UNICODE_STRING g_PicoSniffDevice;
extern UNICODE_STRING g_PicoSymbolicLink;
extern PDEVICE_OBJECT g_MyDevObject;
extern PPICOSNIFF_DEVICE_EXTENSION g_PicoDevExtension;
extern PDRIVER_OBJECT g_LxCoreDriverObject;
extern _DeviceControlFilter g_DevControlFltOriginal;


extern "C" {
	typedef NTSTATUS NTAPI NTQUERYINFORMATIONPROCESS(
		_In_		HANDLE ProcessHandle,
		_In_		PROCESSINFOCLASS ProcessInformationClass,
		_Out_		PVOID ProcessInformation,
		_In_		ULONG ProcessInformationLength,
		_Out_opt_	PULONG ReturnLength
	);

	typedef NTQUERYINFORMATIONPROCESS FAR* LPNTQUERYINFORMATIONPROCESS;

}

PCSTR MajorFunctionToString(UCHAR major);