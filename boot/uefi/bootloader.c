#include <efi.h>
#include <efilib.h>

#define KERNEL_PATH L"\\EFI\\AUKOS\\KERNEL.ELF"

static EFI_FILE_HANDLE open_volume(EFI_HANDLE image) {
    EFI_STATUS status;
    EFI_LOADED_IMAGE *loaded_image;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
    EFI_FILE_HANDLE root;

    status = uefi_call_wrapper(BS->HandleProtocol, 3,
        image, &gEfiLoadedImageProtocolGuid, (void **)&loaded_image);
    if (EFI_ERROR(status))
        return NULL;

    status = uefi_call_wrapper(BS->HandleProtocol, 3,
        loaded_image->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&fs);
    if (EFI_ERROR(status))
        return NULL;

    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status))
        return NULL;

    return root;
}

static EFI_STATUS read_file(EFI_FILE_HANDLE root, CHAR16 *path, void **buffer, UINTN *size) {
    EFI_STATUS status;
    EFI_FILE_HANDLE file;
    EFI_FILE_INFO *info;
    UINTN info_size = SIZE_OF_EFI_FILE_INFO + 200;

    status = uefi_call_wrapper(root->Open, 5,
        root, &file, path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status))
        return status;

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(file->Close, 1, file);
        return status;
    }

    status = uefi_call_wrapper(file->GetInfo, 4,
        file, &gEfiFileInfoGuid, &info_size, info);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, info);
        uefi_call_wrapper(file->Close, 1, file);
        return status;
    }

    *size = info->FileSize;
    uefi_call_wrapper(BS->FreePool, 1, info);

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, *size, buffer);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(file->Close, 1, file);
        return status;
    }

    status = uefi_call_wrapper(file->Read, 3, file, size, *buffer);
    if (EFI_ERROR(status)) {
        uefi_call_wrapper(BS->FreePool, 1, *buffer);
        *buffer = NULL;
    }

    uefi_call_wrapper(file->Close, 1, file);
    return status;
}

#define ELF_MAGIC 0x464C457F
#define PT_LOAD 1

typedef struct {
    UINT8  e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
    UINT16 e_shentsize;
    UINT16 e_shnum;
    UINT16 e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} Elf64_Phdr;

static EFI_STATUS load_elf(void *elf_buf, UINTN elf_size, void **entry) {
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_buf;
    Elf64_Phdr *phdr;
    UINT16 i;

    if (elf_size < sizeof(Elf64_Ehdr))
        return EFI_LOAD_ERROR;

    if (*(UINT32 *)ehdr->e_ident != ELF_MAGIC)
        return EFI_LOAD_ERROR;

    if (ehdr->e_ident[4] != 2 || ehdr->e_machine != 0x3E)
        return EFI_LOAD_ERROR;

    phdr = (Elf64_Phdr *)((UINT8 *)elf_buf + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        if (phdr[i].p_filesz > 0) {
            uefi_call_wrapper(BS->CopyMem, 3,
                (void *)phdr[i].p_paddr,
                (UINT8 *)elf_buf + phdr[i].p_offset,
                phdr[i].p_filesz);
        }
        if (phdr[i].p_memsz > phdr[i].p_filesz) {
            uefi_call_wrapper(BS->SetMem, 3,
                (void *)(phdr[i].p_paddr + phdr[i].p_filesz),
                phdr[i].p_memsz - phdr[i].p_filesz, 0);
        }
    }

    *entry = (void *)ehdr->e_entry;
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    EFI_STATUS status;
    EFI_FILE_HANDLE root;
    void *elf_buf = NULL;
    UINTN elf_size = 0;
    void *kernel_entry = NULL;
    EFI_MEMORY_DESCRIPTOR *mmap = NULL;
    UINTN mmap_size = 0;
    UINTN mmap_capacity;
    UINTN map_key;
    UINTN desc_size;
    UINT32 desc_ver;

    InitializeLib(image, st);
    Print(L"AukOS UEFI Bootloader\n");

    root = open_volume(image);
    if (!root) {
        Print(L"Failed to open filesystem\n");
        return EFI_LOAD_ERROR;
    }

    status = read_file(root, KERNEL_PATH, &elf_buf, &elf_size);
    if (EFI_ERROR(status)) {
        Print(L"Failed to read kernel: %r\n", status);
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }

    Print(L"Kernel ELF loaded (%d bytes)\n", elf_size);

    status = load_elf(elf_buf, elf_size, &kernel_entry);
    uefi_call_wrapper(BS->FreePool, 1, elf_buf);
    if (EFI_ERROR(status)) {
        Print(L"Failed to load ELF: %r\n", status);
        uefi_call_wrapper(root->Close, 1, root);
        return status;
    }

    Print(L"Kernel entry: %p\n", kernel_entry);

    uefi_call_wrapper(root->Close, 1, root);

    mmap_size = 0;
    status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmap_size, NULL, &map_key, &desc_size, &desc_ver);
    mmap_size += 2 * desc_size;
    mmap_capacity = mmap_size;

    status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mmap_size, (void **)&mmap);
    if (EFI_ERROR(status)) {
        Print(L"Failed to alloc mmap: %r\n", status);
        return status;
    }

    Print(L"Exiting boot services (desc_size=%d)\n", desc_size);
    for (UINTN attempt = 0; attempt < 2; attempt++) {
        mmap_size = mmap_capacity;
        status = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmap_size, mmap,
                                   &map_key, &desc_size, &desc_ver);
        if (EFI_ERROR(status)) {
            Print(L"Failed to get mmap: %r\n", status);
            return status;
        }
        status = uefi_call_wrapper(BS->ExitBootServices, 2, image, map_key);
        if (!EFI_ERROR(status)) {
            break;
        }
    }
    if (EFI_ERROR(status)) {
        Print(L"Failed to exit boot services: %r\n", status);
        return status;
    }

    ((void (*)(UINT64, UINT64, UINT64))kernel_entry)((UINT64)mmap, (UINT64)mmap_size, (UINT64)desc_size);

    return EFI_SUCCESS;
}
