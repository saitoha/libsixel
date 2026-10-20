# SIXEL Windows Imaging Component codec

The optional WIC codec lets Windows imaging clients discover and decode SIXEL files through `IWICBitmapDecoder`. It also exposes a separate Automation decoder for COM clients. This companion DLL is disabled by default. The [WIC input loader](../loader/wic.md) instead uses already installed Windows codecs to feed images into libsixel; its configuration is independent of this SIXEL codec.

## Build, dependencies and deployment

The target is native Windows with WIC/COM headers and libraries. Build with `--enable-wiccodec` (Autotools, default `no`) or `-Dwiccodec=enabled` (Meson, default `disabled`). [configure.ac](../../configure.ac), [meson_options.txt](../../meson_options.txt) and the [WIC build definitions](../../wic/) own the available options and artifacts. Native x86, x64 and ARM64 payloads have architecture-specific MSI metadata; a client must use a codec of its own process architecture. An x64 test result does not validate x86, ARM64 or a different compiler ABI.

On Windows, the build produces `wic/libwicsixel.dll` (Autotools keeps the DLL in `wic/.libs/`) and `wic/libsixel-wic.msi`. Windows PowerShell 5.1, Windows Installer's COM API and `makecab.exe` generate the package. No separate installer toolchain is required. The SIXEL core and compiler runtime are linked into the DLL; MSVC uses the static CRT and GNU-style builds request static compiler/runtime dependencies. System Windows DLLs remain runtime dependencies. [installer.ps1](../../wic/installer.ps1) reads PE metadata without executing the DLL and rejects missing COM exports and non-system imports, including external developer-toolchain DLLs. Custom link options must still satisfy that packaging check. The MSI embeds its cabinet and license and is unsigned; release signing is a separate step.

```sh
# Native Windows shell, Autotools
./configure --enable-wiccodec
make
make install
make -C wic installcheck
```

```sh
# Native Windows shell, Meson
meson setup builddir -Dwiccodec=enabled
meson compile -C builddir
meson install -C builddir
meson compile -C builddir installcheck-wic
```

Installation copies the MSI to `${datadir}/libsixel/installers` and launches Windows Installer with elevation. It installs per machine under the architecture-specific `Program Files/libsixel/WIC-*` directory. MSI owns the DLL and the typed registry values declared in [registry.tsv](../../wic/registry.tsv); there are no self-registration or custom-action tables. `--enable-register-dll` and `-Dregister_dll=true` are obsolete compatibility switches, not prerequisites for MSI registration. Avoid manually calling `regsvr32` on an MSI-owned installation: direct DLL registration is a separate compatibility entry point and does not establish Windows Installer ownership.

With `DESTDIR`, installation only stages files; it does not register a system codec. `SIXEL_WIC_MSI_QUIET=1` requests unattended installation and requires an already elevated process. `SIXEL_WIC_MSI_LOG` names an optional native Windows log path. `installcheck` verifies an existing installation and never installs or repairs it. The ordered lifecycle targets and their staging boundaries are specified in [CI design](../ci/design.md#wic-installer-validation).

Remove the codec through Windows Installed Apps or `msiexec /x path\to\libsixel-wic.msi`; use `msiexec /famus path\to\libsixel-wic.msi` for repair. `make uninstall` and Meson's uninstall remove their copied files, including the MSI copy, but do not remove the Windows Installer product. Retain the release package for maintenance. MSI authoring schedules upgrades transactionally, detects newer installed versions for downgrade rejection, and includes equal versions in replacement. Ordering uses numeric `major.minor.patch`, not prerelease suffixes. Table checks establish these authoring choices; actual repair, upgrade, downgrade and uninstall behavior requires lifecycle execution evidence.

Unix Windows cross builds produce the DLL without PowerShell or Wine. Installation stages a packaging kit under `${datadir}/libsixel/installers/wic-package` containing the DLL, `installer.ps1`, `registry.tsv` and `LICENSE`. Copy the kit to Windows and run, substituting the built version:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\installer.ps1 `
    -Dll .\libwicsixel.dll -Version 1.11.1-pre1 -License .\LICENSE `
    -Output .\libsixel-wic.msi
```

Package generation can inspect a supported foreign-architecture DLL without loading it; installed runtime validation must use the matching architecture. See the [build guide](../../build.md) for general toolchain setup and the [platform matrix](../platform-support.md) for the project's broader support tiers.

## Public COM entry points

[wic.c](../../wic/wic.c) supplies two classes from `DllGetClassObject`:

| Route | Class and interface | Use |
| --- | --- | --- |
| WIC | CLSID `{15B9B4DA-B155-4977-8571-CF005884BCB9}`, `IWICBitmapDecoder` | Use `IWICImagingFactory` discovery from an `IStream` or filename, get frame 0 and copy pixels. The codec exposes a still SIXEL image, metadata, preview/thumbnail and a frame source-transform interface. |
| Automation | CLSID `{1EAF6501-96E9-4A4E-92A2-2B15B90DDADE}`, ProgIDs `Libsixel.Decoder` / `Libsixel.Decoder.1` | Request `IUnknown`, `IDispatch` or `ISixelDecoder` (IID `{7AEB0D77-EF5C-45AD-BB82-9C7BC51C0AD2}`). These are one object identity, distinct from the WIC decoder class. |

Automation builds type information at runtime with `CreateDispTypeInfo` and `CreateStdDispatch`; no separate TLB is shipped. `LoadFromStream` takes an `IStream`, `LoadFromString` takes a BSTR **file path**, and `LoadFromByteArray` takes a one-dimensional byte SAFEARRAY. Each returns an `IPictureDisp` through an output parameter. `LoadFromString` does not treat its argument as literal SIXEL escape text. Native clients can call the `ISixelDecoder` vtable; dispatch clients use the declared method signatures and output argument. `GetIDsOfNames` delegates to the standard `IDispatch` implementation and resolves names case-insensitively; this is not `IDispatchEx` and has no case-sensitivity flag. These source-visible invocation routes are not yet covered by typed-language/dispatch-method integration tests in this suite.

Installing the codec makes it discoverable to compatible WIC clients. It does not prove that Explorer, WPF, a particular Photo Viewer configuration or a scripting runtime displays the file. The automated WIC pixel probe checks an in-memory one-pixel red sample and the loaded DLL path; UI rendering and broader image semantics are separate evidence.

## COM ownership and unload contract

Every live codec COM identity keeps the DLL ineligible for unloading. Balanced `IClassFactory::LockServer(TRUE)` calls hold it independently of object references until matching `FALSE` calls. `DllCanUnloadNow` returns `S_FALSE` while either kind of reference remains and `S_OK` after all are released. A caller must obey COM lifetime rules; the counter does not make concurrent calls through released pointers valid or make mutable objects generally thread-safe.

| Identity | Owned state and independent lifetime |
| --- | --- |
| Class factory, for either CLSID | Own reference even before it creates an object. Created decoders do not retain the factory. |
| WIC decoder | Own reference; parent of acquisition paths, not a required live COM owner of returned children. |
| Automation decoder | `IUnknown`, `IDispatch` and `ISixelDecoder` share one reference count; its system dispatcher and type information are released before its module reference. |
| Frame / thumbnail | Each acquisition creates an independent frame owning copied pixels/palette and shared private metadata. Releasing the decoder does not end this identity. |
| Frame source transform | Shares the frame identity. A remaining `IWICBitmapSourceTransform` reference keeps that frame alive after the frame interface is released. |
| Metadata query reader / writer | Each acquisition is independent of its decoder/frame parent. Reader and writer interfaces on the same query object share one identity; private metadata remains owned through it. |
| String enumerator / clone | Each enumerator and clone has its own identity; neither requires its query object or original enumerator to stay alive. |

The implementation uses a single interlocked count for live identities plus server locks, so the unload query takes one atomic snapshot. Construction increments after success and before returning an interface or querying it; final release decrements after resource cleanup. Construction failures before publication leave no module reference, and a failed `QueryInterface` rolls back through ordinary final release. Failed factories/instances for unsupported IIDs return `E_NOINTERFACE`; the selected injected allocation failures return `E_OUTOFMEMORY` without a retained module reference. Private metadata has no external codec vtable and is owned by counted objects. System-owned `ITypeInfo` and returned `IPictureDisp` do not execute codec vtables and are not extra codec identities. `GetPreview` delegates to `GetFrame`, but has no dedicated runtime observation in this inventory.

The unload result proves unload eligibility at the sampled boundary, not absence of every heap/GDI leak. The four-thread tests use independent factories/decoders and test the shared count; they do not establish concurrent mutation safety for a single decoder or metadata object. OS-internal `CreateDispTypeInfo` / `CreateStdDispatch` failures, arbitrary allocation sites, client-runtime finalization and repeated heap/GDI stress remain outside these tests.

## Test coverage

<!-- test-coverage: enforced -->

### Behavioral contract tests

The rows distinguish authoring assertions from actual installed runtime observations. The [complete inventory](../testing/companion-programs-coverage.md) owns all case descriptions, execution prerequisites and evidence limits.

| ID | Contract and observation boundary | Owning tests |
| --- | --- | --- |
| WIC-01 | ALLUSERS equals 1. Per-machine authoring; does not execute elevation or installation. | [tests/extension/wic-installer/0001_per_machine.ps1](../../tests/extension/wic-installer/0001_per_machine.ps1) |
| WIC-02 | Media.Cabinet equals #payload.cab. Cabinet reference only; not an extraction test. | [tests/extension/wic-installer/0002_embedded_cabinet_reference.ps1](../../tests/extension/wic-installer/0002_embedded_cabinet_reference.ps1) |
| WIC-03 | _Streams contains payload.cab. Stream existence only; not cabinet contents or extraction. | [tests/extension/wic-installer/0003_embedded_cabinet_stream.ps1](../../tests/extension/wic-installer/0003_embedded_cabinet_stream.ps1) |
| WIC-04 | RemoveExistingProducts has sequence 1510. Transaction scheduling metadata only; not an executed upgrade or rollback. | [tests/extension/wic-installer/0005_upgrade_transaction.ps1](../../tests/extension/wic-installer/0005_upgrade_transaction.ps1) |
| WIC-05 | Upgrade attributes 2 select WIC_NEWER. Newer-version detection row only; not an attempted downgrade or LaunchCondition execution. | [tests/extension/wic-installer/0006_downgrade_detection.ps1](../../tests/extension/wic-installer/0006_downgrade_detection.ps1) |
| WIC-06 | Upgrade attributes 512 select WIC_OLDER. Inclusive version-range metadata only; not an executed same-version replacement. | [tests/extension/wic-installer/0007_same_version_replacement.ps1](../../tests/extension/wic-installer/0007_same_version_replacement.ps1) |
| WIC-07 | The WIC decoder InprocServer32 row uses [#CodecDll]. MSI file reference only; not a live registry lookup. | [tests/extension/wic-installer/0008_com_registration_path.ps1](../../tests/extension/wic-installer/0008_com_registration_path.ps1) |
| WIC-08 | Codec Component attributes are 0 for x86, 256 otherwise. Registry-view authoring only; does not exercise every target architecture. | [tests/extension/wic-installer/0009_component_architecture.ps1](../../tests/extension/wic-installer/0009_component_architecture.ps1) |
| WIC-09 | No SelfReg table exists. Declarative authoring boundary; does not execute uninstall. | [tests/extension/wic-installer/0010_no_self_registration.ps1](../../tests/extension/wic-installer/0010_no_self_registration.ps1) |
| WIC-10 | No CustomAction table exists. No MSI custom actions; does not prove every standard action succeeds. | [tests/extension/wic-installer/0011_no_custom_actions.ps1](../../tests/extension/wic-installer/0011_no_custom_actions.ps1) |
| WIC-11 | Install-WicMsi returns without opening a nonexistent MSI while DESTDIR is set; the test restores its environment. Staging early return; no MSI is installed, and complete staged file contents are not checked. | [tests/extension/wic-installer/0012_destdir_skips_install.ps1](../../tests/extension/wic-installer/0012_destdir_skips_install.ps1) |
| WIC-12 | Replace a KERNEL32 import spelling with external.dll and require the exact static-dependency rejection from Get-WicPeInfo. Defensive packaging rejection; not a complete audit of every external dependency or malformed PE. | [tests/extension/wic-installer/0014_dependencies.ps1](../../tests/extension/wic-installer/0014_dependencies.ps1) |
| WIC-13 | Windows Installer reports state 5 for the exact built ProductCode. | [tests/extension/wic-installed/0001_product_installed.ps1](../../tests/extension/wic-installed/0001_product_installed.ps1) |
| WIC-14 | SHA-256 of MSI ComponentPath equals the DLL used to build the package. | [tests/extension/wic-installed/0002_installed_payload.ps1](../../tests/extension/wic-installed/0002_installed_payload.ps1) |
| WIC-15 | All declared registry.tsv values and types match in the payload architecture registry view, with the installed DLL substituted. | [tests/extension/wic-installed/0003_installed_registration.ps1](../../tests/extension/wic-installed/0003_installed_registration.ps1) |
| WIC-16 | WIC discovers a literal in-memory SIXEL sample, returns the expected 1x1 red pixel and loads the MSI ComponentPath DLL. | [tests/extension/wic-installed/0004_decode_red_pixel.ps1](../../tests/extension/wic-installed/0004_decode_red_pixel.ps1) |
| WIC-17 | The WIC class factory keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0005_factory_held.ps1](../../tests/extension/wic-installed/0005_factory_held.ps1), [tests/extension/wic-installed/0006_factory_released.ps1](../../tests/extension/wic-installed/0006_factory_released.ps1) |
| WIC-18 | The Automation class factory keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0007_automation_factory_held.ps1](../../tests/extension/wic-installed/0007_automation_factory_held.ps1), [tests/extension/wic-installed/0008_automation_factory_released.ps1](../../tests/extension/wic-installed/0008_automation_factory_released.ps1) |
| WIC-19 | The WIC decoder keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0009_decoder_held.ps1](../../tests/extension/wic-installed/0009_decoder_held.ps1), [tests/extension/wic-installed/0010_decoder_released.ps1](../../tests/extension/wic-installed/0010_decoder_released.ps1) |
| WIC-20 | The Automation decoder keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0011_automation_held.ps1](../../tests/extension/wic-installed/0011_automation_held.ps1), [tests/extension/wic-installed/0012_automation_released.ps1](../../tests/extension/wic-installed/0012_automation_released.ps1) |
| WIC-21 | The frame keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0013_frame_held.ps1](../../tests/extension/wic-installed/0013_frame_held.ps1), [tests/extension/wic-installed/0014_frame_released.ps1](../../tests/extension/wic-installed/0014_frame_released.ps1) |
| WIC-22 | The thumbnail keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0015_thumbnail_held.ps1](../../tests/extension/wic-installed/0015_thumbnail_held.ps1), [tests/extension/wic-installed/0016_thumbnail_released.ps1](../../tests/extension/wic-installed/0016_thumbnail_released.ps1) |
| WIC-23 | The frame source-transform interface keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0017_transform_held.ps1](../../tests/extension/wic-installed/0017_transform_held.ps1), [tests/extension/wic-installed/0018_transform_released.ps1](../../tests/extension/wic-installed/0018_transform_released.ps1) |
| WIC-24 | The decoder metadata query reader keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0019_decoder_reader_held.ps1](../../tests/extension/wic-installed/0019_decoder_reader_held.ps1), [tests/extension/wic-installed/0020_decoder_reader_released.ps1](../../tests/extension/wic-installed/0020_decoder_reader_released.ps1) |
| WIC-25 | The frame metadata query reader keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0021_frame_reader_held.ps1](../../tests/extension/wic-installed/0021_frame_reader_held.ps1), [tests/extension/wic-installed/0022_frame_reader_released.ps1](../../tests/extension/wic-installed/0022_frame_reader_released.ps1) |
| WIC-26 | The metadata query writer keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0023_writer_held.ps1](../../tests/extension/wic-installed/0023_writer_held.ps1), [tests/extension/wic-installed/0024_writer_released.ps1](../../tests/extension/wic-installed/0024_writer_released.ps1) |
| WIC-27 | The string enumerator keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0025_enumerator_held.ps1](../../tests/extension/wic-installed/0025_enumerator_held.ps1), [tests/extension/wic-installed/0026_enumerator_released.ps1](../../tests/extension/wic-installed/0026_enumerator_released.ps1) |
| WIC-28 | The enumerator clone keeps unload ineligible when retained alone and permits unloading after final release. | [tests/extension/wic-installed/0027_clone_held.ps1](../../tests/extension/wic-installed/0027_clone_held.ps1), [tests/extension/wic-installed/0028_clone_released.ps1](../../tests/extension/wic-installed/0028_clone_released.ps1) |
| WIC-29 | Request IID_IStream from the WIC class factory; require E_NOINTERFACE and S_OK unload eligibility after cleanup. | [tests/extension/wic-installed/0029_factory_unsupported_iid.ps1](../../tests/extension/wic-installed/0029_factory_unsupported_iid.ps1) |
| WIC-30 | Request IID_IStream from the WIC decoder; require E_NOINTERFACE and S_OK unload eligibility after cleanup. | [tests/extension/wic-installed/0030_decoder_unsupported_iid.ps1](../../tests/extension/wic-installed/0030_decoder_unsupported_iid.ps1) |
| WIC-31 | Request IID_IStream from the Automation decoder; require E_NOINTERFACE and S_OK unload eligibility after cleanup. | [tests/extension/wic-installed/0031_automation_unsupported_iid.ps1](../../tests/extension/wic-installed/0031_automation_unsupported_iid.ps1) |
| WIC-32 | Fail allocation 1 (object) during WIC class factory construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0032_factory_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0032_factory_allocation_1_rollback.ps1) |
| WIC-33 | Fail allocation 1 (object) during WIC decoder construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0033_decoder_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0033_decoder_allocation_1_rollback.ps1) |
| WIC-34 | Fail allocation 1 (object) during Automation decoder construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0034_automation_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0034_automation_allocation_1_rollback.ps1) |
| WIC-35 | Fail allocation 1 (pixel buffer) during frame construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0035_frame_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0035_frame_allocation_1_rollback.ps1) |
| WIC-36 | Fail allocation 2 (palette buffer) during frame construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0036_frame_allocation_2_rollback.ps1](../../tests/extension/wic-installed/0036_frame_allocation_2_rollback.ps1) |
| WIC-37 | Fail allocation 3 (frame object) during frame construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0037_frame_allocation_3_rollback.ps1](../../tests/extension/wic-installed/0037_frame_allocation_3_rollback.ps1) |
| WIC-38 | Fail allocation 1 (pixel buffer) during thumbnail construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0038_thumbnail_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0038_thumbnail_allocation_1_rollback.ps1) |
| WIC-39 | Fail allocation 2 (palette buffer) during thumbnail construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0039_thumbnail_allocation_2_rollback.ps1](../../tests/extension/wic-installed/0039_thumbnail_allocation_2_rollback.ps1) |
| WIC-40 | Fail allocation 3 (frame object) during thumbnail construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0040_thumbnail_allocation_3_rollback.ps1](../../tests/extension/wic-installed/0040_thumbnail_allocation_3_rollback.ps1) |
| WIC-41 | Fail allocation 1 (object) during decoder metadata query reader construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0041_decoder_reader_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0041_decoder_reader_allocation_1_rollback.ps1) |
| WIC-42 | Fail allocation 1 (object) during string enumerator construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0042_enumerator_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0042_enumerator_allocation_1_rollback.ps1) |
| WIC-43 | Fail allocation 1 (object) during enumerator clone construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | [tests/extension/wic-installed/0043_clone_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0043_clone_allocation_1_rollback.ps1) |
| WIC-44 | A balanced server lock survives factory release and permits unloading only after its matching unlock and object cleanup. | [tests/extension/wic-installed/0044_server_lock_held.ps1](../../tests/extension/wic-installed/0044_server_lock_held.ps1), [tests/extension/wic-installed/0046_server_lock_released.ps1](../../tests/extension/wic-installed/0046_server_lock_released.ps1) |
| WIC-45 | The module count retains concurrent independent objects and returns to zero after all four workers finish. | [tests/extension/wic-installed/0045_concurrent_objects_held.ps1](../../tests/extension/wic-installed/0045_concurrent_objects_held.ps1), [tests/extension/wic-installed/0047_concurrent_objects_released.ps1](../../tests/extension/wic-installed/0047_concurrent_objects_released.ps1) |

### Quality regression tests

No perceptual quality suite is claimed here. The installed pixel assertion is an exact value contract for one literal sample.

### Defensive and malformed-input tests

The selected unsupported-interface and allocation failures above directly own the stated rollback contracts. Other failure paths and heap/GDI stress have no exhaustive coverage claim. Package-size consistency and private-registry parity are supplementary probes listed in the [full inventory](../testing/companion-programs-coverage.md); they are not promoted to independent public guarantees.
