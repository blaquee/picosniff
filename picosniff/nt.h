#pragma once
#include <ntifs.h>

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