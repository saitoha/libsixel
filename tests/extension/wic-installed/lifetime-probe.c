/* SPDX-License-Identifier: MIT */
/* Exercise a real DLL; the caller supplies one observation per test file. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <oleauto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Some MinGW SDKs omit this interface declaration. */
static const IID transform_id = {
    0x3b16811b, 0x6a43, 0x4ec9,
    {0xa8, 0x13, 0x3d, 0x93, 0x0c, 0x13, 0xb9, 0x40}
};
static const CLSID decoder_id = {
    0x15b9b4da, 0xb155, 0x4977,
    {0x85, 0x71, 0xcf, 0x00, 0x58, 0x84, 0xbc, 0xb9}
};
static const CLSID automation_id = {
    0x1eaf6501, 0x96e9, 0x4a4e,
    {0x92, 0xa2, 0x2b, 0x15, 0xb9, 0x0d, 0xda, 0xde}
};
typedef HRESULT (STDAPICALLTYPE *GetClass)(REFCLSID, REFIID, void **);
typedef HRESULT (STDAPICALLTYPE *CanUnload)(void);
typedef LPVOID (WINAPI *TaskAlloc)(SIZE_T);

/* Fault only the loaded codec's import, never the host or system COM DLLs.
 * This leaves production lifetime code intact and makes partial allocation
 * rollback deterministic. Restore the import before releasing any objects. */
static TaskAlloc real_alloc;
static LONG fail_at;
static LPVOID WINAPI
fail_alloc(SIZE_T size)
{
    if (InterlockedDecrement(&fail_at) == 0) {
        return NULL;
    }
    return real_alloc(size);
}

static ULONG_PTR *
find_alloc_import(HMODULE module)
{
    BYTE *base;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_IMPORT_DESCRIPTOR *entry;
    IMAGE_THUNK_DATA *thunk;
    DWORD rva;

    base = (BYTE *)module;
    dos = (IMAGE_DOS_HEADER *)base;
    nt = (IMAGE_NT_HEADERS *)(base + dos->e_lfanew);
    rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
             .VirtualAddress;
    if (!rva) return NULL;
    entry = (IMAGE_IMPORT_DESCRIPTOR *)(base + rva);
    for (; entry->Name; ++entry) {
        thunk = (IMAGE_THUNK_DATA *)(base + entry->FirstThunk);
        for (; thunk->u1.Function; ++thunk) {
            if (thunk->u1.Function == (ULONG_PTR)real_alloc) {
                return &thunk->u1.Function;
            }
        }
    }
    return NULL;
}

static HRESULT
set_alloc_import(ULONG_PTR *slot, TaskAlloc alloc)
{
    DWORD protection;
    DWORD ignored;

    if (!slot || !VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE,
                                &protection)) return E_FAIL;
    *slot = (ULONG_PTR)alloc;
    if (!VirtualProtect(slot, sizeof(*slot), protection, &ignored)) {
        return E_FAIL;
    }
    return S_OK;
}

typedef struct {
    GetClass get_class;
    HANDLE ready;
    HANDLE finish;
    HRESULT hr;
} Worker;

static DWORD WINAPI
worker_main(void *argument)
{
    Worker *worker;
    IClassFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    HRESULT init;

    worker = (Worker *)argument;
    init = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    worker->hr = init;
    if (SUCCEEDED(init)) {
        worker->hr = worker->get_class(&decoder_id, &IID_IClassFactory,
                                      (void **)&factory);
        if (SUCCEEDED(worker->hr)) {
            worker->hr = IClassFactory_CreateInstance(factory, NULL,
                &IID_IWICBitmapDecoder, (void **)&decoder);
            IClassFactory_Release(factory);
        }
    }
    SetEvent(worker->ready);
    WaitForSingleObject(worker->finish, INFINITE);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (SUCCEEDED(init)) CoUninitialize();
    return 0;
}

static int
run_case(int argc, char **argv)
{
    WCHAR expected[32768];
    WCHAR loaded[32768];
    HMODULE module = NULL;
    GetClass get_class = NULL;
    CanUnload can_unload = NULL;
    FARPROC entry;
    ULONG_PTR *alloc_slot = NULL;
    IClassFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICImagingFactory *imaging = NULL;
    IWICStream *stream = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICMetadataQueryReader *reader = NULL;
    IEnumString *enumerator = NULL;
    IUnknown *object = NULL;
    BYTE sample[] = "\033Pq\"1;1;1;1#0;2;100;0;0#0@\033\\";
    HRESULT hr = E_FAIL;
    HRESULT actual = E_FAIL;
    HRESULT wanted;
    HRESULT init;
    const IID *iid;
    const CLSID *clsid;
    const char *target;
    const char *phase;
    BOOL locked = FALSE;
    BOOL allocation;
    BOOL unsupported;
    BOOL held;
    DWORD length;
    Worker workers[4] = {0};
    HANDLE threads[4] = {0};
    unsigned int i;
    int result = 1;
    LARGE_INTEGER frequency, start, setup, observation, end;

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&start);
    if (argc != 4) return 2;
    target = argv[1];
    phase = argv[2];
    allocation = strcmp(phase, "allocation") == 0;
    unsupported = strcmp(phase, "unsupported") == 0;
    held = strcmp(phase, "held") == 0;
    wanted = held ? S_FALSE : S_OK;
    length = GetEnvironmentVariableW(L"LIBSIXEL_WIC_INSTALLED_DLL",
                                    expected, 32768);
    if (!length || length >= 32768) return 2;
    init = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(init)) return 2;
    module = LoadLibraryW(expected);
    if (!module) goto cleanup;
    length = GetModuleFileNameW(module, loaded, 32768);
    if (!length || length >= 32768 || _wcsicmp(expected, loaded)) {
        fprintf(stderr, "Loaded module differs from MSI ComponentPath.\n");
        goto cleanup;
    }
    /* Windows exports and typed callbacks have the same pointer size. */
    entry = GetProcAddress(module, "DllGetClassObject");
    memcpy(&get_class, &entry, sizeof(get_class));
    entry = GetProcAddress(module, "DllCanUnloadNow");
    memcpy(&can_unload, &entry, sizeof(can_unload));
    if (!get_class || !can_unload) goto cleanup;
    /* A clean process has no codec COM objects or server locks yet. */
    if (can_unload() != S_OK) goto cleanup;
    real_alloc = (TaskAlloc)CoTaskMemAlloc;
    QueryPerformanceCounter(&setup);

    clsid = strstr(target, "automation") ? &automation_id : &decoder_id;
    iid = unsupported ? &IID_IStream : &IID_IClassFactory;
    if (!strcmp(target, "factory") ||
        !strcmp(target, "automation_factory")) {
        if (allocation) {
            alloc_slot = find_alloc_import(module);
            fail_at = atoi(argv[3]);
            hr = set_alloc_import(alloc_slot, fail_alloc);
            if (FAILED(hr)) goto cleanup;
        }
        hr = get_class(clsid, iid, (void **)&object);
        goto observe;
    }
    if (!strcmp(target, "concurrent")) {
        for (i = 0; i < 4; ++i) {
            workers[i].get_class = get_class;
            workers[i].ready = CreateEventW(NULL, TRUE, FALSE, NULL);
            workers[i].finish = CreateEventW(NULL, TRUE, FALSE, NULL);
            if (!workers[i].ready || !workers[i].finish) goto cleanup;
            threads[i] = CreateThread(NULL, 0, worker_main, &workers[i],
                                      0, NULL);
            if (!threads[i]) goto cleanup;
        }
        hr = S_OK;
        for (i = 0; i < 4; ++i) {
            WaitForSingleObject(workers[i].ready, INFINITE);
            if (FAILED(workers[i].hr)) hr = workers[i].hr;
        }
        goto observe;
    }
    hr = get_class(clsid, &IID_IClassFactory, (void **)&factory);
    if (FAILED(hr)) goto cleanup;
    if (!strcmp(target, "lock")) {
        hr = IClassFactory_LockServer(factory, TRUE);
        if (FAILED(hr)) goto cleanup;
        locked = TRUE;
        IClassFactory_Release(factory);
        factory = NULL;
        goto observe;
    }
    if (!strcmp(target, "decoder") || !strcmp(target, "automation")) {
        if (allocation) {
            alloc_slot = find_alloc_import(module);
            fail_at = atoi(argv[3]);
            hr = set_alloc_import(alloc_slot, fail_alloc);
            if (FAILED(hr)) goto cleanup;
        }
        iid = unsupported ? &IID_IStream : &IID_IUnknown;
        hr = IClassFactory_CreateInstance(factory, NULL, iid,
                                         (void **)&object);
        goto observe;
    }
    hr = IClassFactory_CreateInstance(factory, NULL, &IID_IWICBitmapDecoder,
                                     (void **)&decoder);
    if (FAILED(hr)) goto cleanup;
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL,
                         CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                         (void **)&imaging);
    if (FAILED(hr)) goto cleanup;
    hr = IWICImagingFactory_CreateStream(imaging, &stream);
    if (FAILED(hr)) goto cleanup;
    hr = IWICStream_InitializeFromMemory(stream, sample, sizeof(sample) - 1);
    if (FAILED(hr)) goto cleanup;
    hr = IWICBitmapDecoder_Initialize(decoder, (IStream *)stream,
                                     WICDecodeMetadataCacheOnDemand);
    if (FAILED(hr)) goto cleanup;
    if (!strcmp(target, "frame_reader") || !strcmp(target, "transform")) {
        hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
        if (FAILED(hr)) goto cleanup;
    }
    if (!strcmp(target, "writer") || !strcmp(target, "enumerator") ||
        !strcmp(target, "clone")) {
        hr = IWICBitmapDecoder_GetMetadataQueryReader(decoder, &reader);
        if (FAILED(hr)) goto cleanup;
    }
    if (!strcmp(target, "clone")) {
        hr = IWICMetadataQueryReader_GetEnumerator(reader, &enumerator);
        if (FAILED(hr)) goto cleanup;
    }
    if (allocation) {
        alloc_slot = find_alloc_import(module);
        fail_at = atoi(argv[3]);
        hr = set_alloc_import(alloc_slot, fail_alloc);
        if (FAILED(hr)) goto cleanup;
    }
    if (!strcmp(target, "frame")) {
        hr = IWICBitmapDecoder_GetFrame(decoder, 0,
                                       (IWICBitmapFrameDecode **)&object);
    } else if (!strcmp(target, "thumbnail")) {
        hr = IWICBitmapDecoder_GetThumbnail(decoder,
                                           (IWICBitmapSource **)&object);
    } else if (!strcmp(target, "transform")) {
        hr = IWICBitmapFrameDecode_QueryInterface(frame,
            &transform_id, (void **)&object);
    } else if (!strcmp(target, "frame_reader")) {
        hr = IWICBitmapFrameDecode_GetMetadataQueryReader(frame,
            (IWICMetadataQueryReader **)&object);
    } else if (!strcmp(target, "decoder_reader")) {
        hr = IWICBitmapDecoder_GetMetadataQueryReader(decoder,
            (IWICMetadataQueryReader **)&object);
    } else if (!strcmp(target, "writer")) {
        hr = IWICMetadataQueryReader_QueryInterface(reader,
            &IID_IWICMetadataQueryWriter, (void **)&object);
    } else if (!strcmp(target, "enumerator")) {
        hr = IWICMetadataQueryReader_GetEnumerator(reader,
                                                 (IEnumString **)&object);
    } else if (!strcmp(target, "clone")) {
        hr = IEnumString_Clone(enumerator, (IEnumString **)&object);
    } else {
        hr = E_INVALIDARG;
    }
observe:
    if (alloc_slot) {
        if (FAILED(set_alloc_import(alloc_slot, real_alloc))) goto cleanup;
        alloc_slot = NULL;
        if (fail_at > 0) goto cleanup;
    }
    if (allocation ? hr != E_OUTOFMEMORY :
        unsupported ? hr != E_NOINTERFACE : FAILED(hr)) goto cleanup;
    /* Drop every parent and alternate interface before the observation. */
    if (enumerator) IEnumString_Release(enumerator);
    enumerator = NULL;
    if (reader) IWICMetadataQueryReader_Release(reader);
    reader = NULL;
    if (frame) IWICBitmapFrameDecode_Release(frame);
    frame = NULL;
    if (decoder) IWICBitmapDecoder_Release(decoder);
    decoder = NULL;
    if (factory) IClassFactory_Release(factory);
    factory = NULL;
    if (!held) {
        if (object) IUnknown_Release(object);
        object = NULL;
        if (locked) {
            hr = get_class(&decoder_id, &IID_IClassFactory,
                           (void **)&factory);
            if (FAILED(hr)) goto cleanup;
            hr = IClassFactory_LockServer(factory, FALSE);
            if (FAILED(hr)) goto cleanup;
            locked = FALSE;
            IClassFactory_Release(factory);
            factory = NULL;
        }
        for (i = 0; i < 4; ++i) {
            if (threads[i]) {
                SetEvent(workers[i].finish);
                WaitForSingleObject(threads[i], INFINITE);
            }
        }
    }
    actual = can_unload();
    result = actual != wanted;
    QueryPerformanceCounter(&observation);
    printf("%s/%s: DllCanUnloadNow=0x%08lx expected=0x%08lx\n",
           target, phase, (unsigned long)actual, (unsigned long)wanted);
    printf("native setup=%.3f ms observation=%.3f ms\n",
        (setup.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart,
        (observation.QuadPart - setup.QuadPart) * 1000.0 / frequency.QuadPart);
cleanup:
    if (alloc_slot) set_alloc_import(alloc_slot, real_alloc);
    if (object) IUnknown_Release(object);
    if (enumerator) IEnumString_Release(enumerator);
    if (reader) IWICMetadataQueryReader_Release(reader);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (locked) {
        if (!factory) get_class(&decoder_id, &IID_IClassFactory,
                                (void **)&factory);
        if (factory) IClassFactory_LockServer(factory, FALSE);
    }
    if (factory) IClassFactory_Release(factory);
    for (i = 0; i < 4; ++i) {
        if (threads[i]) {
            SetEvent(workers[i].finish);
            WaitForSingleObject(threads[i], INFINITE);
            CloseHandle(threads[i]);
        }
        if (workers[i].ready) CloseHandle(workers[i].ready);
        if (workers[i].finish) CloseHandle(workers[i].finish);
    }
    if (stream) IWICStream_Release(stream);
    if (imaging) IWICImagingFactory_Release(imaging);
    /* Never unload while a test object or worker can still enter DLL code. */
    if (module) FreeLibrary(module);
    CoUninitialize();
    QueryPerformanceCounter(&end);
    printf("native total=%.3f ms (main entry through cleanup)\n",
           (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart);
    if (result) fprintf(stderr, "Lifetime probe failed: hr=0x%08lx\n",
                        (unsigned long)hr);
    return result;
}

/* Reuse process startup, not COM fixtures: each request still initializes,
 * loads, constructs, observes, releases and unloads inside its own timer. */
int
main(int argc, char **argv)
{
    char line[128];
    char target[32];
    char phase[32];
    char fail[16];
    char *args[4];
    int result;

    if (argc != 2 || strcmp(argv[1], "--server")) {
        return run_case(argc, argv);
    }
    puts("READY");
    fflush(stdout);
    while (fgets(line, sizeof(line), stdin)) {
        if (sscanf(line, "%31s %31s %15s", target, phase, fail) != 3) {
            return 2;
        }
        args[0] = argv[0];
        args[1] = target;
        args[2] = phase;
        args[3] = fail;
        result = run_case(4, args);
        printf("RESULT %d\n", result);
        fflush(stdout);
    }
    return ferror(stdin) ? 2 : 0;
}
