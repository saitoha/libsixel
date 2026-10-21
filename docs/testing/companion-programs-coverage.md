# Companion-program test coverage

This inventory describes the numbered Quick Look build, WIC package and installed-codec observations. The [WIC codec reference](../programs/wic-codec.md) owns public behavior, deployment and COM lifetime contracts; the [Quick Look extension reference](../programs/quicklook-extension.md) owns macOS deployment and its untested boundaries. Input-loader suites are separate and are not counted as companion-program coverage. General assertion rules and timing requirements belong to the [testing guide](guide.md).

## Execution and result model

Both WIC categories use [run-tests.ps1](../../tests/extension/wic-installer/run-tests.ps1). It selects `????_*.ps1`, runs one script block and emits one TAP observation per file. Exceptions produce FAIL; successful assertions produce PASS. A missing required payload/package, COM facility or native probe is a harness error/failure, not a successful skip. Windows with PowerShell, Windows Installer COM, a matching native DLL and a built MSI is required. These suites are registered only for native Windows packaging builds with tests enabled; absence on a Unix cross build is not Windows validation.

| Entry | Prerequisites and expected result |
| --- | --- |
| Package: `make -C wic check`, or `meson test -C builddir --suite extension` | Built MSI and payload; no system installation required. Cases normally PASS, FAIL on mismatch. Registration capture relays x86 payloads to x86 PowerShell; unsupported remaining architecture mismatches SKIP only case 0013. |
| Installed: `make -C wic installcheck`, or `meson compile -C builddir installcheck-wic` | Exact built MSI already installed, two compiled native probes, same process architecture. Absent product fails rather than accepting old manual registration. DESTDIR or SIXEL_INSTALLCHECK_DESTDIR skips this entire system suite before setup. |
| Selection | Autotools accepts `WIC_MSI_TEST=0005_*`. Direct native PowerShell uses the harness `-Test` basename wildcard, `-Mode Package` or `Installed`, `-Dll`, `-Package`, and for installed tests `-Probe` / `-LifetimeProbe`. Meson's registered suite runs the full category; selection must not silently redefine its coverage. |

The [ordered lifecycle targets](../ci/design.md#wic-installer-validation) install the system codec explicitly. Test execution itself does not install/repair it. The timing report is advisory for slow tests; functional mismatches still fail. [Test timing](guide.md#test-timing) defines shared setup, individual wall time, enclosing batch time and invalid-evidence handling.

`msi-query.ps1` and `registration-capture.ps1` are shared helpers. `decode-red-pixel.c` and `lifetime-probe.c` are native assertion helpers invoked by numbered observations, not extra test cases. The lifetime process is shared, but each request initializes COM, loads the exact ComponentPath DLL, checks the loaded path, constructs its fixture, observes unload eligibility, releases resources, unloads and uninitializes COM. Allocation injection changes only the actual DLL's CoTaskMemAlloc import temporarily and restores it before cleanup. Native elapsed values are overlapping diagnostic breakdowns inside the outer PowerShell wall time, not replacements for it.

## Package observations

All rows use the package entry and prerequisites above. Only 0013 has the architecture SKIP condition; every other completed row expects PASS and reports FAIL when its assertion is violated. Structural assertions do not execute repair, upgrade, downgrade, rollback or uninstall transactions.

<!-- test-plan: tests/extension/wic-installer/????_*.ps1 -->

| Test | Observation | Evidence limit |
| --- | --- | --- |
| [tests/extension/wic-installer/0001_per_machine.ps1](../../tests/extension/wic-installer/0001_per_machine.ps1) | ALLUSERS equals 1. | Per-machine authoring; does not execute elevation or installation. |
| [tests/extension/wic-installer/0002_embedded_cabinet_reference.ps1](../../tests/extension/wic-installer/0002_embedded_cabinet_reference.ps1) | Media.Cabinet equals #payload.cab. | Cabinet reference only; not an extraction test. |
| [tests/extension/wic-installer/0003_embedded_cabinet_stream.ps1](../../tests/extension/wic-installer/0003_embedded_cabinet_stream.ps1) | _Streams contains payload.cab. | Stream existence only; not cabinet contents or extraction. |
| [tests/extension/wic-installer/0004_payload_file_size.ps1](../../tests/extension/wic-installer/0004_payload_file_size.ps1) | CodecDll FileSize equals the built DLL length. | Size consistency only; equal sizes do not prove equal bytes. |
| [tests/extension/wic-installer/0005_upgrade_transaction.ps1](../../tests/extension/wic-installer/0005_upgrade_transaction.ps1) | RemoveExistingProducts has sequence 1510. | Transaction scheduling metadata only; not an executed upgrade or rollback. |
| [tests/extension/wic-installer/0006_downgrade_detection.ps1](../../tests/extension/wic-installer/0006_downgrade_detection.ps1) | Upgrade attributes 2 select WIC_NEWER. | Newer-version detection row only; not an attempted downgrade or LaunchCondition execution. |
| [tests/extension/wic-installer/0007_same_version_replacement.ps1](../../tests/extension/wic-installer/0007_same_version_replacement.ps1) | Upgrade attributes 512 select WIC_OLDER. | Inclusive version-range metadata only; not an executed same-version replacement. |
| [tests/extension/wic-installer/0008_com_registration_path.ps1](../../tests/extension/wic-installer/0008_com_registration_path.ps1) | The WIC decoder InprocServer32 row uses [#CodecDll]. | MSI file reference only; not a live registry lookup. |
| [tests/extension/wic-installer/0009_component_architecture.ps1](../../tests/extension/wic-installer/0009_component_architecture.ps1) | Codec Component attributes are 0 for x86, 256 otherwise. | Registry-view authoring only; does not exercise every target architecture. |
| [tests/extension/wic-installer/0010_no_self_registration.ps1](../../tests/extension/wic-installer/0010_no_self_registration.ps1) | No SelfReg table exists. | Declarative authoring boundary; does not execute uninstall. |
| [tests/extension/wic-installer/0011_no_custom_actions.ps1](../../tests/extension/wic-installer/0011_no_custom_actions.ps1) | No CustomAction table exists. | No MSI custom actions; does not prove every standard action succeeds. |
| [tests/extension/wic-installer/0012_destdir_skips_install.ps1](../../tests/extension/wic-installer/0012_destdir_skips_install.ps1) | Install-WicMsi returns without opening a nonexistent MSI while DESTDIR is set; the test restores its environment. | Staging early return; no MSI is installed, and complete staged file contents are not checked. |
| [tests/extension/wic-installer/0013_registration.ps1](../../tests/extension/wic-installer/0013_registration.ps1) | Redirect HKCR/HKLM for this process, call DLL registration, compare registry.tsv values/types and reject extra values, then remove the private HKCU tree. | DLL-to-declarative-source consistency, not reading every MSI Registry row or installing a system codec. |
| [tests/extension/wic-installer/0014_dependencies.ps1](../../tests/extension/wic-installer/0014_dependencies.ps1) | Replace a KERNEL32 import spelling with external.dll and require the exact static-dependency rejection from Get-WicPeInfo. | Defensive packaging rejection; not a complete audit of every external dependency or malformed PE. |

<!-- test-plan-end -->

## Installed observations

All rows use the installed entry and prerequisites above. Once setup succeeds, each row expects PASS and has no per-case capability SKIP; unmet assertions FAIL. The whole-suite staging SKIP is described above. Held and released states are separate test files. Allocations are numbered within the selected acquisition call, after supporting parents have been created; each failure case checks that injection was actually consumed.

<!-- test-plan: tests/extension/wic-installed/????_*.ps1 -->

| Test | Observation | Evidence limit |
| --- | --- | --- |
| [tests/extension/wic-installed/0001_product_installed.ps1](../../tests/extension/wic-installed/0001_product_installed.ps1) | Windows Installer reports state 5 for the exact built ProductCode. | Installed-state assertion; does not independently check bytes, registry or pixels. |
| [tests/extension/wic-installed/0002_installed_payload.ps1](../../tests/extension/wic-installed/0002_installed_payload.ps1) | SHA-256 of MSI ComponentPath equals the DLL used to build the package. | Payload equality; does not establish decoded image semantics. |
| [tests/extension/wic-installed/0003_installed_registration.ps1](../../tests/extension/wic-installed/0003_installed_registration.ps1) | All declared registry.tsv values and types match in the payload architecture registry view, with the installed DLL substituted. | Declared registration values; does not reject every unrelated extra machine value or prove UI discovery. |
| [tests/extension/wic-installed/0004_decode_red_pixel.ps1](../../tests/extension/wic-installed/0004_decode_red_pixel.ps1) | WIC discovers a literal in-memory SIXEL sample, returns the expected 1x1 red pixel and loads the MSI ComponentPath DLL. | One exact pixel plus discovery/path; not Explorer UI, general image quality or every WIC method. |
| [tests/extension/wic-installed/0005_factory_held.ps1](../../tests/extension/wic-installed/0005_factory_held.ps1) | DllCanUnloadNow is S_FALSE with only the WIC class factory retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0006_factory_released.ps1](../../tests/extension/wic-installed/0006_factory_released.ps1) | DllCanUnloadNow is S_OK after the WIC class factory and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0007_automation_factory_held.ps1](../../tests/extension/wic-installed/0007_automation_factory_held.ps1) | DllCanUnloadNow is S_FALSE with only the Automation class factory retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0008_automation_factory_released.ps1](../../tests/extension/wic-installed/0008_automation_factory_released.ps1) | DllCanUnloadNow is S_OK after the Automation class factory and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0009_decoder_held.ps1](../../tests/extension/wic-installed/0009_decoder_held.ps1) | DllCanUnloadNow is S_FALSE with only the WIC decoder retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0010_decoder_released.ps1](../../tests/extension/wic-installed/0010_decoder_released.ps1) | DllCanUnloadNow is S_OK after the WIC decoder and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0011_automation_held.ps1](../../tests/extension/wic-installed/0011_automation_held.ps1) | DllCanUnloadNow is S_FALSE with only the Automation decoder retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0012_automation_released.ps1](../../tests/extension/wic-installed/0012_automation_released.ps1) | DllCanUnloadNow is S_OK after the Automation decoder and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0013_frame_held.ps1](../../tests/extension/wic-installed/0013_frame_held.ps1) | DllCanUnloadNow is S_FALSE with only the frame retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0014_frame_released.ps1](../../tests/extension/wic-installed/0014_frame_released.ps1) | DllCanUnloadNow is S_OK after the frame and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0015_thumbnail_held.ps1](../../tests/extension/wic-installed/0015_thumbnail_held.ps1) | DllCanUnloadNow is S_FALSE with only the thumbnail retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0016_thumbnail_released.ps1](../../tests/extension/wic-installed/0016_thumbnail_released.ps1) | DllCanUnloadNow is S_OK after the thumbnail and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0017_transform_held.ps1](../../tests/extension/wic-installed/0017_transform_held.ps1) | DllCanUnloadNow is S_FALSE with only the frame source-transform interface retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0018_transform_released.ps1](../../tests/extension/wic-installed/0018_transform_released.ps1) | DllCanUnloadNow is S_OK after the frame source-transform interface and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0019_decoder_reader_held.ps1](../../tests/extension/wic-installed/0019_decoder_reader_held.ps1) | DllCanUnloadNow is S_FALSE with only the decoder metadata query reader retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0020_decoder_reader_released.ps1](../../tests/extension/wic-installed/0020_decoder_reader_released.ps1) | DllCanUnloadNow is S_OK after the decoder metadata query reader and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0021_frame_reader_held.ps1](../../tests/extension/wic-installed/0021_frame_reader_held.ps1) | DllCanUnloadNow is S_FALSE with only the frame metadata query reader retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0022_frame_reader_released.ps1](../../tests/extension/wic-installed/0022_frame_reader_released.ps1) | DllCanUnloadNow is S_OK after the frame metadata query reader and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0023_writer_held.ps1](../../tests/extension/wic-installed/0023_writer_held.ps1) | DllCanUnloadNow is S_FALSE with only the metadata query writer retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0024_writer_released.ps1](../../tests/extension/wic-installed/0024_writer_released.ps1) | DllCanUnloadNow is S_OK after the metadata query writer and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0025_enumerator_held.ps1](../../tests/extension/wic-installed/0025_enumerator_held.ps1) | DllCanUnloadNow is S_FALSE with only the string enumerator retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0026_enumerator_released.ps1](../../tests/extension/wic-installed/0026_enumerator_released.ps1) | DllCanUnloadNow is S_OK after the string enumerator and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0027_clone_held.ps1](../../tests/extension/wic-installed/0027_clone_held.ps1) | DllCanUnloadNow is S_FALSE with only the enumerator clone retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0028_clone_released.ps1](../../tests/extension/wic-installed/0028_clone_released.ps1) | DllCanUnloadNow is S_OK after the enumerator clone and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0029_factory_unsupported_iid.ps1](../../tests/extension/wic-installed/0029_factory_unsupported_iid.ps1) | Request IID_IStream from the WIC class factory; require E_NOINTERFACE and S_OK unload eligibility after cleanup. | One unsupported IID rollback path, not all possible QueryInterface requests. |
| [tests/extension/wic-installed/0030_decoder_unsupported_iid.ps1](../../tests/extension/wic-installed/0030_decoder_unsupported_iid.ps1) | Request IID_IStream from the WIC decoder; require E_NOINTERFACE and S_OK unload eligibility after cleanup. | One unsupported IID rollback path, not all possible QueryInterface requests. |
| [tests/extension/wic-installed/0031_automation_unsupported_iid.ps1](../../tests/extension/wic-installed/0031_automation_unsupported_iid.ps1) | Request IID_IStream from the Automation decoder; require E_NOINTERFACE and S_OK unload eligibility after cleanup. | One unsupported IID rollback path, not all possible QueryInterface requests. |
| [tests/extension/wic-installed/0032_factory_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0032_factory_allocation_1_rollback.ps1) | Fail allocation 1 (object) during WIC class factory construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0033_decoder_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0033_decoder_allocation_1_rollback.ps1) | Fail allocation 1 (object) during WIC decoder construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0034_automation_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0034_automation_allocation_1_rollback.ps1) | Fail allocation 1 (object) during Automation decoder construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0035_frame_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0035_frame_allocation_1_rollback.ps1) | Fail allocation 1 (pixel buffer) during frame construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0036_frame_allocation_2_rollback.ps1](../../tests/extension/wic-installed/0036_frame_allocation_2_rollback.ps1) | Fail allocation 2 (palette buffer) during frame construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0037_frame_allocation_3_rollback.ps1](../../tests/extension/wic-installed/0037_frame_allocation_3_rollback.ps1) | Fail allocation 3 (frame object) during frame construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0038_thumbnail_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0038_thumbnail_allocation_1_rollback.ps1) | Fail allocation 1 (pixel buffer) during thumbnail construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0039_thumbnail_allocation_2_rollback.ps1](../../tests/extension/wic-installed/0039_thumbnail_allocation_2_rollback.ps1) | Fail allocation 2 (palette buffer) during thumbnail construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0040_thumbnail_allocation_3_rollback.ps1](../../tests/extension/wic-installed/0040_thumbnail_allocation_3_rollback.ps1) | Fail allocation 3 (frame object) during thumbnail construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0041_decoder_reader_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0041_decoder_reader_allocation_1_rollback.ps1) | Fail allocation 1 (object) during decoder metadata query reader construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0042_enumerator_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0042_enumerator_allocation_1_rollback.ps1) | Fail allocation 1 (object) during string enumerator construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0043_clone_allocation_1_rollback.ps1](../../tests/extension/wic-installed/0043_clone_allocation_1_rollback.ps1) | Fail allocation 1 (object) during enumerator clone construction; require E_OUTOFMEMORY, injection reached, and S_OK after cleanup. | Selected CoTaskMemAlloc import failure in the real DLL; not every allocator or OS-internal failure. |
| [tests/extension/wic-installed/0044_server_lock_held.ps1](../../tests/extension/wic-installed/0044_server_lock_held.ps1) | DllCanUnloadNow is S_FALSE with only the balanced server lock retained; other parent/alternate interfaces are released. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0045_concurrent_objects_held.ps1](../../tests/extension/wic-installed/0045_concurrent_objects_held.ps1) | DllCanUnloadNow is S_FALSE after all four independent workers signal that their factories/decoders are live. | Unload eligibility only; cleanup follows the observation, and no unsafe unload is attempted. |
| [tests/extension/wic-installed/0046_server_lock_released.ps1](../../tests/extension/wic-installed/0046_server_lock_released.ps1) | DllCanUnloadNow is S_OK after the balanced server lock and all supporting codec objects are released. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |
| [tests/extension/wic-installed/0047_concurrent_objects_released.ps1](../../tests/extension/wic-installed/0047_concurrent_objects_released.ps1) | DllCanUnloadNow is S_OK after all four independent workers finish and are joined. | Final unload eligibility only; not a general heap/GDI leak or shared-object thread-safety test. |

<!-- test-plan-end -->

## Uncovered behavior and maintenance

No numbered tests here establish Quick Look release signing/notarization, actual Installer lifecycle transitions, Finder previews/thumbnails, Explorer display, WPF rendering, typed C#/Python/WSH invocation, dispatch method argument binding, runtime finalization, general heap/GDI leakage, or the full MSI repair/upgrade/downgrade lifecycle. A build, registration table, literal WIC pixel, COM unload result and visible application rendering establish different properties. The functional reference marks source-inspected implementation separately from automated evidence.

Both reciprocal static checks collect PowerShell `# Policy:` and `# Test-plan:` comments. The numbered-file glob excludes helpers and is nonrecursive. Add, rename or remove an observation together with this inventory and its applicable behavioral contract row; run the [documentation checks](staticcheck.md#adding-or-changing-a-staticcheck). Syntax/set checks do not prove that prose correctly describes an assertion. Keep ownership decisions and evidence limits under source review.

## Quick Look build outputs

These cases observe completed build artifacts without installing or registering them. A clean build and subsequent dependency-triggered rebuild are separate build validation phases. QL-01 reads `QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR`, an absolute path to a separate completed build configured from an empty directory without the extension option. The caller owns this freshness and configuration precondition; the test checks that `config.h` and the built `img2sixel` exist before asserting host-app absence. Missing input is SKIP, explicitly supplied but incomplete input is FAIL, and an unexpected host app is FAIL. The normal build directory is not used as a substitute: disabling an enabled build can retain its apps. Tests never remove these artifacts. The owning contracts are in the [Quick Look extension reference](../programs/quicklook-extension.md#test-coverage).

<!-- test-plan: tests/extension/quicklook-build/????_*.t -->

| Test | Observation |
| --- | --- |
| [tests/extension/quicklook-build/0001_fresh_default_off_host_absent.t](../../tests/extension/quicklook-build/0001_fresh_default_off_host_absent.t) | A separately prepared fresh default-off build has no host app. |
| [tests/extension/quicklook-build/0002_host_executable.t](../../tests/extension/quicklook-build/0002_host_executable.t) | Host bundle contains the completed executable. |
| [tests/extension/quicklook-build/0003_host_plist.t](../../tests/extension/quicklook-build/0003_host_plist.t) | Host bundle contains its generated plist. |
| [tests/extension/quicklook-build/0004_preview_executable.t](../../tests/extension/quicklook-build/0004_preview_executable.t) | Preview bundle contains the completed executable. |
| [tests/extension/quicklook-build/0005_preview_plist.t](../../tests/extension/quicklook-build/0005_preview_plist.t) | Preview bundle contains its generated plist. |
| [tests/extension/quicklook-build/0006_thumbnail_executable.t](../../tests/extension/quicklook-build/0006_thumbnail_executable.t) | Thumbnail bundle contains the completed executable. |
| [tests/extension/quicklook-build/0007_thumbnail_plist.t](../../tests/extension/quicklook-build/0007_thumbnail_plist.t) | Thumbnail bundle contains its generated plist. |
| [tests/extension/quicklook-build/0008_bridge_executable.t](../../tests/extension/quicklook-build/0008_bridge_executable.t) | Bridge bundle contains the completed executable. |
| [tests/extension/quicklook-build/0009_bridge_plist.t](../../tests/extension/quicklook-build/0009_bridge_plist.t) | Bridge bundle contains its generated plist. |
| [tests/extension/quicklook-build/0010_register_tool_built.t](../../tests/extension/quicklook-build/0010_register_tool_built.t) | Registration helper is built without executing it. |
<!-- test-plan-end -->

### Fresh default-off input and reconfiguration

Prepare each default-off input once, outside the observations. Use new absolute build paths and an unconfigured source tree. `mkdir` must fail if either path already exists; do not clean or reuse an enabled build as this fixture. For example, on macOS:

```sh
set -e
src=/absolute/path/to/libsixel
auto_off=/absolute/path/to/new-auto-default-off
meson_off=/absolute/path/to/new-meson-default-off
export PATH="$src/.local/bin:$PATH"
mkdir "$auto_off"
(cd "$auto_off" && time "$src/configure")
time make -C "$auto_off" -j4
mkdir "$meson_off"
time meson setup "$meson_off" "$src"
time meson compile -C "$meson_off" -j4
```

Record configure/build commands, exit status and wall time as shared preparation, then run the observations with the corresponding input. The harness inherits this environment variable; it does not save freshness in a reusable configure flag. Keep the input unchanged until observations finish.

```sh
QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR="$auto_off" \
    make -C "$auto_off/tests" check \
    TESTS=extension/quicklook-build/0001_fresh_default_off_host_absent.t
QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR="$meson_off" \
    meson test -C "$meson_off" --num-processes 1 \
    extension/quicklook-build/0001_fresh_default_off_host_absent.t
```

Validate enabled-to-disabled reconfiguration separately: start another empty directory with `--enable-quicklook-extension` or `-Dquicklook_extension=enabled`, build and run the enabled observations, then run configure with `--disable-quicklook-extension` in the same Autotools directory or `meson configure BUILD -Dquicklook_extension=disabled` followed by `meson setup --reconfigure BUILD SOURCE`. Complete the normal build again and run the Quick Look build observations. With no fresh input supplied, all ten observations skip in the disabled configuration; with the preserved default-off input supplied, QL-01 passes and the nine enabled observations skip. Record the effective disabled setting and retained app hashes separately; neither SKIP nor fresh-fixture absence alone proves reconfiguration. Keep the initial default-off input separate from this reconfigured directory. From a disabled test configuration, supply a directory retaining an app as the purported fresh input for a negative control: QL-01 must report `not ok` without deleting the unexpected app. Shared setup/build timing and individual observation timing remain separate under the [timing policy](guide.md#test-timing).

## Quick Look package outputs

The [package construction reference](../programs/quicklook-extension.md#installer-package-construction) owns the numeric distribution version, fixed locations, signing order, self-contained runtime dependencies and preflight boundaries. Normal enabled builds produce and expand the archive once. These observations consume that completed output and never install it. The default-off case uses `QUICKLOOK_FRESH_DEFAULT_OFF_BUILDDIR` under the same shared preparation contract as the assembly case above; missing fixture input is SKIP, not absence evidence. Tests of signatures call the native Security API inside the observation, and the payload equality test hashes the actual expanded files. The payload observation includes manifest parsing and validation, full directory enumeration, ancestor and node `lstat` checks, file reads/hashes and inventory comparison; measure process startup and termination too. Record its first normal harness run and all repeats separately from instrumented diagnostics, together with concurrent work and host load, under the [timing policy](guide.md#test-timing). Archive expansion is a separately reported build operation, not a hidden test warmup. The publication correspondence case includes current archive and manifest reads/hashes plus a complete expanded-tree inventory and hashes inside its measured invocation. The evidence stores provenance, never a cached successful assertion. Run it together with the payload, metadata and signature cases; a cached expansion alone is not sufficient package evidence. Each bundle identity/content-type/version/executable test selects exactly one bundle. Each code architecture/dependency test selects exactly one executable; the helper rejects a missing or unknown subject. Read failures in an unrelated bundle must not fail that subject. Corrupt each subject independently and run both its owning test and the sibling-subject tests to prove failure isolation. The package-wide maximum deployment floor, Distribution/manifest architecture declaration and complete component policy sets remain relational observations. Mach-O observations parse all actual slices of their selected executable in process and independently compare the package architecture/minimum OS and system dependency closure, including the dynamic-loader path. Entitlement observations validate signatures and compare the actual entitlement dictionaries against the fixed sandbox contract. The non-system-volume test stops before any real receipt query.

<!-- test-plan: tests/extension/quicklook-package/????_*.t -->

| Test | Observation |
| --- | --- |
| [tests/extension/quicklook-package/0001_fresh_default_off_package_absent.t](../../tests/extension/quicklook-package/0001_fresh_default_off_package_absent.t) | Fresh default-off build has no package. |
| [tests/extension/quicklook-package/0002_package_built.t](../../tests/extension/quicklook-package/0002_package_built.t) | Normal enabled build produces a package. |
| [tests/extension/quicklook-package/0003_payload_matches_manifest.t](../../tests/extension/quicklook-package/0003_payload_matches_manifest.t) | Expanded package payload matches the final manifest. |
| [tests/extension/quicklook-package/0004_packaged_manifest.t](../../tests/extension/quicklook-package/0004_packaged_manifest.t) | Packaged manifest equals the external distribution manifest. |
| [tests/extension/quicklook-package/0005_host_signature.t](../../tests/extension/quicklook-package/0005_host_signature.t) | Packaged host code signature is valid. |
| [tests/extension/quicklook-package/0006_preview_signature.t](../../tests/extension/quicklook-package/0006_preview_signature.t) | Packaged preview code signature is valid. |
| [tests/extension/quicklook-package/0007_thumbnail_signature.t](../../tests/extension/quicklook-package/0007_thumbnail_signature.t) | Packaged thumbnail code signature is valid. |
| [tests/extension/quicklook-package/0008_bridge_signature.t](../../tests/extension/quicklook-package/0008_bridge_signature.t) | Packaged bridge code signature is valid. |
| [tests/extension/quicklook-package/0009_package_identifier.t](../../tests/extension/quicklook-package/0009_package_identifier.t) | Package has the fixed receipt identifier. |
| [tests/extension/quicklook-package/0010_registration_helper_signature.t](../../tests/extension/quicklook-package/0010_registration_helper_signature.t) | Packaged registration helper signature is valid. |
| [tests/extension/quicklook-package/0011_preflight_helper_signature.t](../../tests/extension/quicklook-package/0011_preflight_helper_signature.t) | Packaged preflight helper signature is valid. |
| [tests/extension/quicklook-package/0012_non_system_target_rejected.t](../../tests/extension/quicklook-package/0012_non_system_target_rejected.t) | Package preflight rejects non-system target volumes. |
| [tests/extension/quicklook-package/0013_preinstall_script.t](../../tests/extension/quicklook-package/0013_preinstall_script.t) | Package contains the declared preinstall script. |
| [tests/extension/quicklook-package/0014_distribution_version.t](../../tests/extension/quicklook-package/0014_distribution_version.t) | Distribution version equals the configured build number. |
| [tests/extension/quicklook-package/0015_package_version.t](../../tests/extension/quicklook-package/0015_package_version.t) | Package version equals the configured distribution build. |
| [tests/extension/quicklook-package/0016_install_location.t](../../tests/extension/quicklook-package/0016_install_location.t) | Package uses the fixed system install location. |
| [tests/extension/quicklook-package/0017_relocation_disabled.t](../../tests/extension/quicklook-package/0017_relocation_disabled.t) | Package relocation is disabled. |
| [tests/extension/quicklook-package/0018_existing_directory_permissions.t](../../tests/extension/quicklook-package/0018_existing_directory_permissions.t) | Package declares preservation of existing directory attributes. |
| [tests/extension/quicklook-package/0019_bom_payload_attributes.t](../../tests/extension/quicklook-package/0019_bom_payload_attributes.t) | Package Bom declares payload attributes, including the independent 0644/root:wheel contract for all four signature resources. |
| [tests/extension/quicklook-package/0020_postinstall_script.t](../../tests/extension/quicklook-package/0020_postinstall_script.t) | Package contains the declared postinstall script. |
| [tests/extension/quicklook-package/0021_publication_correspondence.t](../../tests/extension/quicklook-package/0021_publication_correspondence.t) | Published archive, manifest, evidence and complete expansion correspond. |
| [tests/extension/quicklook-package/0022_host_identity.t](../../tests/extension/quicklook-package/0022_host_identity.t) | Host has the required fixed bundle identifier. |
| [tests/extension/quicklook-package/0023_host_content_type.t](../../tests/extension/quicklook-package/0023_host_content_type.t) | Host has the required SIXEL content type and associations. |
| [tests/extension/quicklook-package/0024_preview_extension.t](../../tests/extension/quicklook-package/0024_preview_extension.t) | Preview declares its extension point, provider and supported content type. |
| [tests/extension/quicklook-package/0025_thumbnail_extension.t](../../tests/extension/quicklook-package/0025_thumbnail_extension.t) | Thumbnail declares its extension point, provider and supported content type. |
| [tests/extension/quicklook-package/0026_host_numeric_versions.t](../../tests/extension/quicklook-package/0026_host_numeric_versions.t) | Host has the required numeric bundle versions matching the distribution. |
| [tests/extension/quicklook-package/0027_minimum_os.t](../../tests/extension/quicklook-package/0027_minimum_os.t) | Declared minimum OS equals the actual Mach-O maximum with the supported floor. |
| [tests/extension/quicklook-package/0028_host_architectures.t](../../tests/extension/quicklook-package/0028_host_architectures.t) | Host has the required actual Mach-O architecture set matching the manifest. |
| [tests/extension/quicklook-package/0029_host_dependencies.t](../../tests/extension/quicklook-package/0029_host_dependencies.t) | Host has the required system-only dylib, dynamic-loader and rpath references. |
| [tests/extension/quicklook-package/0030_preview_entitlements.t](../../tests/extension/quicklook-package/0030_preview_entitlements.t) | Preview has exactly the required sandbox entitlements. |
| [tests/extension/quicklook-package/0031_thumbnail_entitlements.t](../../tests/extension/quicklook-package/0031_thumbnail_entitlements.t) | Thumbnail has exactly the required sandbox entitlements. |
| [tests/extension/quicklook-package/0032_component_strict_identifiers.t](../../tests/extension/quicklook-package/0032_component_strict_identifiers.t) | Component strict identifier policy names all fixed bundles. |
| [tests/extension/quicklook-package/0033_component_version_guards.t](../../tests/extension/quicklook-package/0033_component_version_guards.t) | Component version guards name all fixed bundles. |
| [tests/extension/quicklook-package/0034_component_overwrite_policy.t](../../tests/extension/quicklook-package/0034_component_overwrite_policy.t) | Components use whole-bundle upgrade without conflicting overwrite policies. |
| [tests/extension/quicklook-package/0035_host_executable.t](../../tests/extension/quicklook-package/0035_host_executable.t) | Host has the required declared regular executable payload file. |
| [tests/extension/quicklook-package/0036_system_domain.t](../../tests/extension/quicklook-package/0036_system_domain.t) | Distribution permits only the system installation domain. |
| [tests/extension/quicklook-package/0037_preview_identity.t](../../tests/extension/quicklook-package/0037_preview_identity.t) | Preview has the required fixed bundle identifier. |
| [tests/extension/quicklook-package/0038_thumbnail_identity.t](../../tests/extension/quicklook-package/0038_thumbnail_identity.t) | Thumbnail has the required fixed bundle identifier. |
| [tests/extension/quicklook-package/0039_bridge_identity.t](../../tests/extension/quicklook-package/0039_bridge_identity.t) | Bridge has the required fixed bundle identifier. |
| [tests/extension/quicklook-package/0040_preview_content_type.t](../../tests/extension/quicklook-package/0040_preview_content_type.t) | Preview has the required SIXEL content type and associations. |
| [tests/extension/quicklook-package/0041_thumbnail_content_type.t](../../tests/extension/quicklook-package/0041_thumbnail_content_type.t) | Thumbnail has the required SIXEL content type and associations. |
| [tests/extension/quicklook-package/0042_bridge_content_type.t](../../tests/extension/quicklook-package/0042_bridge_content_type.t) | Bridge has the required SIXEL content type and associations. |
| [tests/extension/quicklook-package/0043_preview_numeric_versions.t](../../tests/extension/quicklook-package/0043_preview_numeric_versions.t) | Preview has the required numeric bundle versions matching the distribution. |
| [tests/extension/quicklook-package/0044_thumbnail_numeric_versions.t](../../tests/extension/quicklook-package/0044_thumbnail_numeric_versions.t) | Thumbnail has the required numeric bundle versions matching the distribution. |
| [tests/extension/quicklook-package/0045_bridge_numeric_versions.t](../../tests/extension/quicklook-package/0045_bridge_numeric_versions.t) | Bridge has the required numeric bundle versions matching the distribution. |
| [tests/extension/quicklook-package/0046_preview_executable.t](../../tests/extension/quicklook-package/0046_preview_executable.t) | Preview has the required declared regular executable payload file. |
| [tests/extension/quicklook-package/0047_thumbnail_executable.t](../../tests/extension/quicklook-package/0047_thumbnail_executable.t) | Thumbnail has the required declared regular executable payload file. |
| [tests/extension/quicklook-package/0048_bridge_executable.t](../../tests/extension/quicklook-package/0048_bridge_executable.t) | Bridge has the required declared regular executable payload file. |
| [tests/extension/quicklook-package/0049_preview_architectures.t](../../tests/extension/quicklook-package/0049_preview_architectures.t) | Preview has the required actual Mach-O architecture set matching the manifest. |
| [tests/extension/quicklook-package/0050_thumbnail_architectures.t](../../tests/extension/quicklook-package/0050_thumbnail_architectures.t) | Thumbnail has the required actual Mach-O architecture set matching the manifest. |
| [tests/extension/quicklook-package/0051_bridge_architectures.t](../../tests/extension/quicklook-package/0051_bridge_architectures.t) | Bridge has the required actual Mach-O architecture set matching the manifest. |
| [tests/extension/quicklook-package/0052_registration_helper_architectures.t](../../tests/extension/quicklook-package/0052_registration_helper_architectures.t) | Registration helper has the required actual Mach-O architecture set matching the manifest. |
| [tests/extension/quicklook-package/0053_package_helper_architectures.t](../../tests/extension/quicklook-package/0053_package_helper_architectures.t) | Package helper has the required actual Mach-O architecture set matching the manifest. |
| [tests/extension/quicklook-package/0054_preview_dependencies.t](../../tests/extension/quicklook-package/0054_preview_dependencies.t) | Preview has the required system-only dylib, dynamic-loader and rpath references. |
| [tests/extension/quicklook-package/0055_thumbnail_dependencies.t](../../tests/extension/quicklook-package/0055_thumbnail_dependencies.t) | Thumbnail has the required system-only dylib, dynamic-loader and rpath references. |
| [tests/extension/quicklook-package/0056_bridge_dependencies.t](../../tests/extension/quicklook-package/0056_bridge_dependencies.t) | Bridge has the required system-only dylib, dynamic-loader and rpath references. |
| [tests/extension/quicklook-package/0057_registration_helper_dependencies.t](../../tests/extension/quicklook-package/0057_registration_helper_dependencies.t) | Registration helper has the required system-only dylib, dynamic-loader and rpath references. |
| [tests/extension/quicklook-package/0058_package_helper_dependencies.t](../../tests/extension/quicklook-package/0058_package_helper_dependencies.t) | Package helper has the required system-only dylib, dynamic-loader and rpath references. |
| [tests/extension/quicklook-package/0059_distribution_architectures.t](../../tests/extension/quicklook-package/0059_distribution_architectures.t) | Distribution architecture declaration matches the payload manifest. |
<!-- test-plan-end -->

For corruption controls, preserve the original public artifacts and independently remove/replace the public archive, retain an older expansion with newer evidence, alter the external manifest, and alter each observed plist/Mach-O/entitlement/component field. Run existing-artifact checks without automatic rebuild and require the corresponding TAP to fail; record subsequent producer regeneration as a separate repair observation. An independent `pkgutil --expand-full` extraction can be compared with the published expansion evidence without replacing producer evidence.

PackageInfo and Bom observations verify the authored directory-preservation policy and the recorded payload attributes. They do not establish that a real Installer preserved customized shared parent attributes. Re-expand the published archive independently for review and run negative controls with `overwrite-permissions=true`, missing policy, and changed Bom ownership/modes; preserve the original archive. Include the expand/flatten and fixture preparation time separately from native harness observation times.

For caller-umask regression coverage, prepare packages from the same completed assembly inputs with umasks 022, 077 and 002 through both native producers. Check the real archive's manifest mode 0644 and all four signature resources at 0644/root:wheel with the existing Bom observation, alongside payload/manifest identity and signatures. Include a negative archive whose manifest and Bom consistently record CodeResources at 0600: the Bom observation must reject it even when owner-executed signature checks pass. Preserve input assembly hashes, modes and ownership, and verify that signing's child-only mask does not change the parent's later file creation. Exercise the normal build entry points as well. Package construction and expansion remain separately timed shared preparation; do not put them inside the individual TAP files or relax the expected mode to match the caller's umask.

Private-root preflight scenarios and archive corruption controls must preserve the original package and report their shared preparation and per-invocation timings. A private root and supplied receipt exercise the refusal logic; they do not establish Installer's real receipt, script ordering, locking, rollback, registration or GUI behavior. The complete installed-state and lifecycle suites remain uncovered here.

## Quick Look install routing

The `quicklook-install` category checks the package management boundary without installing a system app. The dry-run test uses a nonexistent tool to detect accidental execution. Argument refusal tests exercise the actual wrapper/native parser. A staging installcheck must explicitly skip before receipt inspection. These tests do not use the old user-copy hook.

Prepare staging once through each native build's normal install entry with an absolute private DESTDIR and a datadir containing spaces. Record that shared operation, host load, elapsed time and a before/after system app/receipt/user-state snapshot separately. Supply `QUICKLOOK_STAGE_DIR` (the resulting installers directory) and `QUICKLOOK_STAGE_SOURCE` (that build's package directory) to the native harness. The three byte comparisons remain inside their individual observations. Missing fixture variables skip; preparation failures must not be presented as successful staging. No stage operation is hidden inside each numbered observation.

<!-- test-plan: tests/extension/quicklook-install/????_*.t -->

| Test | Observation |
| --- | --- |
| [tests/extension/quicklook-install/0001_dry_run_has_no_tool_execution.t](../../tests/extension/quicklook-install/0001_dry_run_has_no_tool_execution.t) | Dry run has no tool execution. |
| [tests/extension/quicklook-install/0002_empty_package_path_rejected.t](../../tests/extension/quicklook-install/0002_empty_package_path_rejected.t) | Empty package path rejected. |
| [tests/extension/quicklook-install/0003_legacy_user_copy_arguments_rejected.t](../../tests/extension/quicklook-install/0003_legacy_user_copy_arguments_rejected.t) | Legacy user copy arguments rejected. |
| [tests/extension/quicklook-install/0004_staging_installcheck_skips_system.t](../../tests/extension/quicklook-install/0004_staging_installcheck_skips_system.t) | Staging installcheck skips system. |
| [tests/extension/quicklook-install/0005_relative_destdir_rejected.t](../../tests/extension/quicklook-install/0005_relative_destdir_rejected.t) | Relative destdir rejected. |
| [tests/extension/quicklook-install/0006_staged_package_matches_distribution.t](../../tests/extension/quicklook-install/0006_staged_package_matches_distribution.t) | Staged package matches distribution. |
| [tests/extension/quicklook-install/0007_staged_manifest_matches_distribution.t](../../tests/extension/quicklook-install/0007_staged_manifest_matches_distribution.t) | Staged manifest matches distribution. |
| [tests/extension/quicklook-install/0008_staged_management_tool_matches.t](../../tests/extension/quicklook-install/0008_staged_management_tool_matches.t) | Staged management tool matches. |
| [tests/extension/quicklook-install/0009_wrapper_preserves_failure_status.t](../../tests/extension/quicklook-install/0009_wrapper_preserves_failure_status.t) | Wrapper preserves failure status. |
| [tests/extension/quicklook-install/0010_nonroot_install_refused.t](../../tests/extension/quicklook-install/0010_nonroot_install_refused.t) | Nonroot install refused. |
<!-- test-plan-end -->

For operation-level controls, use private roots and a separately compiled test executable whose Installer/receipt/root/UID boundaries are substituted at compile time. The distributed executable accepts no runtime override for those boundaries. Record each substitution and verify arguments, original error status, managed exclusion through child exit and final receipt validation, standalone success without manual completion, lock contention/stale refusal, complete payload/receipt ownership, removal, shared-parent preservation and stage tampering refusal. Substitute-command success is not real Installer evidence. Compile, build, staging and restoration are shared operations, with all first and repeated normal harness samples retained under the [timing policy](guide.md#test-timing). Check dry-run and the actual nonroot refusal on the real host without elevating or installing. Real successful install/receipt transitions and general-user launch/activation require a managed machine and restoration plan; they remain separate lifecycle evidence.

## Quick Look installed observations

The build-tree observer consumes independent distribution expectations. These are real installed checks when explicitly selected through installcheck; normal check skips them. An absent enabled installation fails. A missing explicit user context skips only user observations and does not establish activation. Private roots and substituted OS queries are validation controls, not real Installer or GUI proof.

<!-- test-plan: tests/extension/quicklook-installed/????_*.t -->

The installed stage is independent of CLI selection, runs once, and uses already-built observers. No-context user cases are zero-plan SKIP in both drivers; supplied invalid contexts fail.

| Test | Observation |
| --- | --- |
| [tests/extension/quicklook-installed/0001_receipt_identity.t](../../tests/extension/quicklook-installed/0001_receipt_identity.t) | Exact package receipt exists. |
| [tests/extension/quicklook-installed/0002_receipt_version.t](../../tests/extension/quicklook-installed/0002_receipt_version.t) | Receipt version matches distribution. |
| [tests/extension/quicklook-installed/0003_receipt_location.t](../../tests/extension/quicklook-installed/0003_receipt_location.t) | Receipt system location matches. |
| [tests/extension/quicklook-installed/0004_path_host.t](../../tests/extension/quicklook-installed/0004_path_host.t) | Installed bundle path exists host. |
| [tests/extension/quicklook-installed/0005_path_bridge.t](../../tests/extension/quicklook-installed/0005_path_bridge.t) | Installed bundle path exists bridge. |
| [tests/extension/quicklook-installed/0006_path_preview.t](../../tests/extension/quicklook-installed/0006_path_preview.t) | Installed bundle path exists preview. |
| [tests/extension/quicklook-installed/0007_path_thumbnail.t](../../tests/extension/quicklook-installed/0007_path_thumbnail.t) | Installed bundle path exists thumbnail. |
| [tests/extension/quicklook-installed/0008_payload_host.t](../../tests/extension/quicklook-installed/0008_payload_host.t) | Installed tree matches distribution host. |
| [tests/extension/quicklook-installed/0009_payload_bridge.t](../../tests/extension/quicklook-installed/0009_payload_bridge.t) | Installed tree matches distribution bridge. |
| [tests/extension/quicklook-installed/0010_payload_preview.t](../../tests/extension/quicklook-installed/0010_payload_preview.t) | Installed tree matches distribution preview. |
| [tests/extension/quicklook-installed/0011_payload_thumbnail.t](../../tests/extension/quicklook-installed/0011_payload_thumbnail.t) | Installed tree matches distribution thumbnail. |
| [tests/extension/quicklook-installed/0012_payload_support.t](../../tests/extension/quicklook-installed/0012_payload_support.t) | Installed tree matches distribution support. |
| [tests/extension/quicklook-installed/0013_signature_host.t](../../tests/extension/quicklook-installed/0013_signature_host.t) | Installed code signature matches distribution host. |
| [tests/extension/quicklook-installed/0014_signature_bridge.t](../../tests/extension/quicklook-installed/0014_signature_bridge.t) | Installed code signature matches distribution bridge. |
| [tests/extension/quicklook-installed/0015_signature_preview.t](../../tests/extension/quicklook-installed/0015_signature_preview.t) | Installed code signature matches distribution preview. |
| [tests/extension/quicklook-installed/0016_signature_thumbnail.t](../../tests/extension/quicklook-installed/0016_signature_thumbnail.t) | Installed code signature matches distribution thumbnail. |
| [tests/extension/quicklook-installed/0017_signature_registration_helper.t](../../tests/extension/quicklook-installed/0017_signature_registration_helper.t) | Installed code signature matches distribution registration-helper. |
| [tests/extension/quicklook-installed/0018_signature_management_helper.t](../../tests/extension/quicklook-installed/0018_signature_management_helper.t) | Installed code signature matches distribution management-helper. |
| [tests/extension/quicklook-installed/0019_manifest_bytes.t](../../tests/extension/quicklook-installed/0019_manifest_bytes.t) | Installed manifest bytes and attributes match. |
| [tests/extension/quicklook-installed/0020_session_identity.t](../../tests/extension/quicklook-installed/0020_session_identity.t) | Explicit user and GUI session match. |
| [tests/extension/quicklook-installed/0021_ls_host.t](../../tests/extension/quicklook-installed/0021_ls_host.t) | Launch Services installed instance matches host. |
| [tests/extension/quicklook-installed/0022_ls_bridge.t](../../tests/extension/quicklook-installed/0022_ls_bridge.t) | Launch Services installed instance matches bridge. |
| [tests/extension/quicklook-installed/0023_plugin_preview.t](../../tests/extension/quicklook-installed/0023_plugin_preview.t) | PlugInKit installed instance matches preview. |
| [tests/extension/quicklook-installed/0024_plugin_thumbnail.t](../../tests/extension/quicklook-installed/0024_plugin_thumbnail.t) | PlugInKit installed instance matches thumbnail. |
| [tests/extension/quicklook-installed/0025_duplicates_host.t](../../tests/extension/quicklook-installed/0025_duplicates_host.t) | Registration has exactly one instance host. |
| [tests/extension/quicklook-installed/0026_duplicates_bridge.t](../../tests/extension/quicklook-installed/0026_duplicates_bridge.t) | Registration has exactly one instance bridge. |
| [tests/extension/quicklook-installed/0027_duplicates_preview.t](../../tests/extension/quicklook-installed/0027_duplicates_preview.t) | Registration has exactly one instance preview. |
| [tests/extension/quicklook-installed/0028_duplicates_thumbnail.t](../../tests/extension/quicklook-installed/0028_duplicates_thumbnail.t) | Registration has exactly one instance thumbnail. |
| [tests/extension/quicklook-installed/0029_election_preview.t](../../tests/extension/quicklook-installed/0029_election_preview.t) | Election current value matches journal owned value preview. |
| [tests/extension/quicklook-installed/0030_election_thumbnail.t](../../tests/extension/quicklook-installed/0030_election_thumbnail.t) | Election current value matches journal owned value thumbnail. |
| [tests/extension/quicklook-installed/0031_default_six.t](../../tests/extension/quicklook-installed/0031_default_six.t) | Default current value matches journal owned value six. |
| [tests/extension/quicklook-installed/0032_default_sixel.t](../../tests/extension/quicklook-installed/0032_default_sixel.t) | Default current value matches journal owned value sixel. |

<!-- test-plan-end -->

## Quick Look private lifecycle observations

Shared preparation and mutations are explicit and timed. These tests read independent live residual state or preserved process-boundary evidence, and do not execute real Installer or user operations. Missing prepared fixtures are SKIP, not proof of state transitions.

<!-- test-plan: tests/extension/quicklook-lifecycle/????_*.t -->

| Test | Observation |
| --- | --- |
| [tests/extension/quicklook-lifecycle/0001_activation_preserves_default.t](../../tests/extension/quicklook-lifecycle/0001_activation_preserves_default.t) | Activation preserves default. |
| [tests/extension/quicklook-lifecycle/0002_default_identity.t](../../tests/extension/quicklook-lifecycle/0002_default_identity.t) | Default identity. |
| [tests/extension/quicklook-lifecycle/0003_default_restored.t](../../tests/extension/quicklook-lifecycle/0003_default_restored.t) | Default restored. |
| [tests/extension/quicklook-lifecycle/0004_providers_removed.t](../../tests/extension/quicklook-lifecycle/0004_providers_removed.t) | Providers removed. |
| [tests/extension/quicklook-lifecycle/0005_journal_deactivated.t](../../tests/extension/quicklook-lifecycle/0005_journal_deactivated.t) | Journal deactivated. |
| [tests/extension/quicklook-lifecycle/0006_wrong_user_refused.t](../../tests/extension/quicklook-lifecycle/0006_wrong_user_refused.t) | Wrong user refused. |
| [tests/extension/quicklook-lifecycle/0007_nongui_refused.t](../../tests/extension/quicklook-lifecycle/0007_nongui_refused.t) | Nongui refused. |
| [tests/extension/quicklook-lifecycle/0008_duplicate_refused.t](../../tests/extension/quicklook-lifecycle/0008_duplicate_refused.t) | Duplicate refused. |
| [tests/extension/quicklook-lifecycle/0009_changed_payload_refused.t](../../tests/extension/quicklook-lifecycle/0009_changed_payload_refused.t) | Changed payload refused. |
| [tests/extension/quicklook-lifecycle/0010_register_error_preserved.t](../../tests/extension/quicklook-lifecycle/0010_register_error_preserved.t) | Register error preserved. |
| [tests/extension/quicklook-lifecycle/0011_provider_error_preserved.t](../../tests/extension/quicklook-lifecycle/0011_provider_error_preserved.t) | Provider error preserved. |
| [tests/extension/quicklook-lifecycle/0012_default_error_preserved.t](../../tests/extension/quicklook-lifecycle/0012_default_error_preserved.t) | Default error preserved. |
| [tests/extension/quicklook-lifecycle/0013_default_readback_refused.t](../../tests/extension/quicklook-lifecycle/0013_default_readback_refused.t) | Default readback refused. |
| [tests/extension/quicklook-lifecycle/0014_missing_original_refused.t](../../tests/extension/quicklook-lifecycle/0014_missing_original_refused.t) | Missing original refused. |
| [tests/extension/quicklook-lifecycle/0015_later_choice_preserved.t](../../tests/extension/quicklook-lifecycle/0015_later_choice_preserved.t) | Later choice preserved. |
| [tests/extension/quicklook-lifecycle/0016_changed_provider_preserved.t](../../tests/extension/quicklook-lifecycle/0016_changed_provider_preserved.t) | Changed provider preserved. |
| [tests/extension/quicklook-lifecycle/0017_pending_journal_refused.t](../../tests/extension/quicklook-lifecycle/0017_pending_journal_refused.t) | Pending journal refused. |
| [tests/extension/quicklook-lifecycle/0018_guard_before_receipt_uninstall.t](../../tests/extension/quicklook-lifecycle/0018_guard_before_receipt_uninstall.t) | Uninstall refused at before receipt. |
| [tests/extension/quicklook-lifecycle/0019_guard_before_child_return_uninstall.t](../../tests/extension/quicklook-lifecycle/0019_guard_before_child_return_uninstall.t) | Uninstall refused at before child return. |
| [tests/extension/quicklook-lifecycle/0020_guard_final_validation_uninstall.t](../../tests/extension/quicklook-lifecycle/0020_guard_final_validation_uninstall.t) | Uninstall refused at final validation. |
| [tests/extension/quicklook-lifecycle/0021_standalone_completion.t](../../tests/extension/quicklook-lifecycle/0021_standalone_completion.t) | Standalone completion. |
| [tests/extension/quicklook-lifecycle/0022_legacy_restore_record.t](../../tests/extension/quicklook-lifecycle/0022_legacy_restore_record.t) | Legacy restore record. |
| [tests/extension/quicklook-lifecycle/0023_legacy_foreign_preserved.t](../../tests/extension/quicklook-lifecycle/0023_legacy_foreign_preserved.t) | Legacy foreign preserved. |
| [tests/extension/quicklook-lifecycle/0024_legacy_occupied_preserved.t](../../tests/extension/quicklook-lifecycle/0024_legacy_occupied_preserved.t) | Legacy occupied preserved. |
| [tests/extension/quicklook-lifecycle/0025_lifecycle_order.t](../../tests/extension/quicklook-lifecycle/0025_lifecycle_order.t) | Lifecycle order. |
| [tests/extension/quicklook-lifecycle/0026_lifecycle_errors_separate.t](../../tests/extension/quicklook-lifecycle/0026_lifecycle_errors_separate.t) | Lifecycle errors separate. |
| [tests/extension/quicklook-lifecycle/0027_lifecycle_install_failure.t](../../tests/extension/quicklook-lifecycle/0027_lifecycle_install_failure.t) | Lifecycle install failure. |
| [tests/extension/quicklook-lifecycle/0028_reactivation_preserves_default.t](../../tests/extension/quicklook-lifecycle/0028_reactivation_preserves_default.t) | Reactivation preserves later default choice and prior ownership. |
| [tests/extension/quicklook-lifecycle/0029_reactivation_preserves_provider.t](../../tests/extension/quicklook-lifecycle/0029_reactivation_preserves_provider.t) | Reactivation preserves later provider choice and prior ownership. |
| [tests/extension/quicklook-lifecycle/0030_command_status_preserved.t](../../tests/extension/quicklook-lifecycle/0030_command_status_preserved.t) | Ordinary command failure and successful cleanup retain the original status. |
| [tests/extension/quicklook-lifecycle/0031_command_status_after_record_failure.t](../../tests/extension/quicklook-lifecycle/0031_command_status_after_record_failure.t) | Command failure survives completion-journal failure; cleanup remains unattempted. |

| [tests/extension/quicklook-lifecycle/0032_legacy_restore_types.t](../../tests/extension/quicklook-lifecycle/0032_legacy_restore_types.t) | Restored legacy types match original evidence. |
| [tests/extension/quicklook-lifecycle/0033_legacy_restore_bytes.t](../../tests/extension/quicklook-lifecycle/0033_legacy_restore_bytes.t) | Restored legacy bytes match original evidence. |
| [tests/extension/quicklook-lifecycle/0034_legacy_restore_modes.t](../../tests/extension/quicklook-lifecycle/0034_legacy_restore_modes.t) | Restored legacy modes match original evidence. |
| [tests/extension/quicklook-lifecycle/0035_legacy_restore_xattrs.t](../../tests/extension/quicklook-lifecycle/0035_legacy_restore_xattrs.t) | Restored legacy xattrs match original evidence. |
| [tests/extension/quicklook-lifecycle/0036_legacy_restore_signature.t](../../tests/extension/quicklook-lifecycle/0036_legacy_restore_signature.t) | Restored legacy signature match original evidence. |
| [tests/extension/quicklook-lifecycle/0037_guard_before_receipt_install.t](../../tests/extension/quicklook-lifecycle/0037_guard_before_receipt_install.t) | Install refused at before receipt. |
| [tests/extension/quicklook-lifecycle/0038_guard_before_receipt_installcheck.t](../../tests/extension/quicklook-lifecycle/0038_guard_before_receipt_installcheck.t) | Installcheck refused at before receipt. |
| [tests/extension/quicklook-lifecycle/0039_guard_before_child_return_install.t](../../tests/extension/quicklook-lifecycle/0039_guard_before_child_return_install.t) | Install refused at before child return. |
| [tests/extension/quicklook-lifecycle/0040_guard_before_child_return_installcheck.t](../../tests/extension/quicklook-lifecycle/0040_guard_before_child_return_installcheck.t) | Installcheck refused at before child return. |
| [tests/extension/quicklook-lifecycle/0041_guard_final_validation_install.t](../../tests/extension/quicklook-lifecycle/0041_guard_final_validation_install.t) | Install refused at final validation. |
| [tests/extension/quicklook-lifecycle/0042_guard_final_validation_installcheck.t](../../tests/extension/quicklook-lifecycle/0042_guard_final_validation_installcheck.t) | Installcheck refused at final validation. |
| [tests/extension/quicklook-lifecycle/0043_guard_before_receipt_managed_lock.t](../../tests/extension/quicklook-lifecycle/0043_guard_before_receipt_managed_lock.t) | Guard managed-lock invariant at before receipt. |
| [tests/extension/quicklook-lifecycle/0044_guard_before_child_return_managed_lock.t](../../tests/extension/quicklook-lifecycle/0044_guard_before_child_return_managed_lock.t) | Guard managed-lock invariant at before child return. |
| [tests/extension/quicklook-lifecycle/0045_guard_final_validation_managed_lock.t](../../tests/extension/quicklook-lifecycle/0045_guard_final_validation_managed_lock.t) | Guard managed-lock invariant at final validation. |
| [tests/extension/quicklook-lifecycle/0046_guard_before_receipt_package_lock.t](../../tests/extension/quicklook-lifecycle/0046_guard_before_receipt_package_lock.t) | Guard package-lock invariant at before receipt. |
| [tests/extension/quicklook-lifecycle/0047_guard_before_child_return_package_lock.t](../../tests/extension/quicklook-lifecycle/0047_guard_before_child_return_package_lock.t) | Guard package-lock invariant at before child return. |
| [tests/extension/quicklook-lifecycle/0048_guard_final_validation_package_lock.t](../../tests/extension/quicklook-lifecycle/0048_guard_final_validation_package_lock.t) | Guard package-lock invariant at final validation. |
| [tests/extension/quicklook-lifecycle/0049_guard_before_receipt_host.t](../../tests/extension/quicklook-lifecycle/0049_guard_before_receipt_host.t) | Guard host invariant at before receipt. |
| [tests/extension/quicklook-lifecycle/0050_guard_before_child_return_host.t](../../tests/extension/quicklook-lifecycle/0050_guard_before_child_return_host.t) | Guard host invariant at before child return. |
| [tests/extension/quicklook-lifecycle/0051_guard_final_validation_host.t](../../tests/extension/quicklook-lifecycle/0051_guard_final_validation_host.t) | Guard host invariant at final validation. |
| [tests/extension/quicklook-lifecycle/0052_guard_before_receipt_bridge.t](../../tests/extension/quicklook-lifecycle/0052_guard_before_receipt_bridge.t) | Guard bridge invariant at before receipt. |
| [tests/extension/quicklook-lifecycle/0053_guard_before_child_return_bridge.t](../../tests/extension/quicklook-lifecycle/0053_guard_before_child_return_bridge.t) | Guard bridge invariant at before child return. |
| [tests/extension/quicklook-lifecycle/0054_guard_final_validation_bridge.t](../../tests/extension/quicklook-lifecycle/0054_guard_final_validation_bridge.t) | Guard bridge invariant at final validation. |
| [tests/extension/quicklook-lifecycle/0055_guard_before_receipt_support.t](../../tests/extension/quicklook-lifecycle/0055_guard_before_receipt_support.t) | Guard support invariant at before receipt. |
| [tests/extension/quicklook-lifecycle/0056_guard_before_child_return_support.t](../../tests/extension/quicklook-lifecycle/0056_guard_before_child_return_support.t) | Guard support invariant at before child return. |
| [tests/extension/quicklook-lifecycle/0057_guard_final_validation_support.t](../../tests/extension/quicklook-lifecycle/0057_guard_final_validation_support.t) | Guard support invariant at final validation. |
| [tests/extension/quicklook-lifecycle/0058_guard_before_receipt_receipt.t](../../tests/extension/quicklook-lifecycle/0058_guard_before_receipt_receipt.t) | Guard receipt invariant at before receipt. |
| [tests/extension/quicklook-lifecycle/0059_guard_before_child_return_receipt.t](../../tests/extension/quicklook-lifecycle/0059_guard_before_child_return_receipt.t) | Guard receipt invariant at before child return. |
| [tests/extension/quicklook-lifecycle/0060_guard_final_validation_receipt.t](../../tests/extension/quicklook-lifecycle/0060_guard_final_validation_receipt.t) | Guard receipt invariant at final validation. |
| [tests/extension/quicklook-lifecycle/0061_standalone_host_absent.t](../../tests/extension/quicklook-lifecycle/0061_standalone_host_absent.t) | Standalone host absent after removal. |
| [tests/extension/quicklook-lifecycle/0062_standalone_bridge_absent.t](../../tests/extension/quicklook-lifecycle/0062_standalone_bridge_absent.t) | Standalone bridge absent after removal. |
| [tests/extension/quicklook-lifecycle/0063_standalone_support_absent.t](../../tests/extension/quicklook-lifecycle/0063_standalone_support_absent.t) | Standalone support absent after removal. |
| [tests/extension/quicklook-lifecycle/0064_standalone_receipt_absent.t](../../tests/extension/quicklook-lifecycle/0064_standalone_receipt_absent.t) | Standalone receipt absent after removal. |
| [tests/extension/quicklook-lifecycle/0065_standalone_managed_lock_absent.t](../../tests/extension/quicklook-lifecycle/0065_standalone_managed_lock_absent.t) | Standalone managed lock absent after removal. |
| [tests/extension/quicklook-lifecycle/0066_standalone_package_lock_absent.t](../../tests/extension/quicklook-lifecycle/0066_standalone_package_lock_absent.t) | Standalone package lock absent after removal. |
| [tests/extension/quicklook-lifecycle/0067_unchanged_activation_retry.t](../../tests/extension/quicklook-lifecycle/0067_unchanged_activation_retry.t) | Unchanged activation retry preserves the observed journal and makes no new user calls. |
| [tests/extension/quicklook-lifecycle/0068_migration_host_identity.t](../../tests/extension/quicklook-lifecycle/0068_migration_host_identity.t) | Restored legacy Host matches its original artifact identity. |
| [tests/extension/quicklook-lifecycle/0069_migration_bridge_identity.t](../../tests/extension/quicklook-lifecycle/0069_migration_bridge_identity.t) | Restored legacy Bridge matches its original artifact identity. |
| [tests/extension/quicklook-lifecycle/0070_migration_host_absent.t](../../tests/extension/quicklook-lifecycle/0070_migration_host_absent.t) | Migration removes the owned system host root. |
| [tests/extension/quicklook-lifecycle/0071_migration_bridge_absent.t](../../tests/extension/quicklook-lifecycle/0071_migration_bridge_absent.t) | Migration removes the owned system bridge root. |
| [tests/extension/quicklook-lifecycle/0072_migration_support_absent.t](../../tests/extension/quicklook-lifecycle/0072_migration_support_absent.t) | Migration removes the owned system support root. |
| [tests/extension/quicklook-lifecycle/0073_migration_receipt_absent.t](../../tests/extension/quicklook-lifecycle/0073_migration_receipt_absent.t) | Migration removes the exact system receipt. |
| [tests/extension/quicklook-lifecycle/0074_migration_preview_restored.t](../../tests/extension/quicklook-lifecycle/0074_migration_preview_restored.t) | Migration observes the original preview instance and election. |
| [tests/extension/quicklook-lifecycle/0075_migration_thumbnail_restored.t](../../tests/extension/quicklook-lifecycle/0075_migration_thumbnail_restored.t) | Migration observes the original thumbnail instance and election. |
| [tests/extension/quicklook-lifecycle/0076_migration_six_default.t](../../tests/extension/quicklook-lifecycle/0076_migration_six_default.t) | Migration observes the original six default without a setter. |
| [tests/extension/quicklook-lifecycle/0077_migration_sixel_default.t](../../tests/extension/quicklook-lifecycle/0077_migration_sixel_default.t) | Migration observes the original sixel default without a setter. |
| [tests/extension/quicklook-lifecycle/0078_migration_final_record.t](../../tests/extension/quicklook-lifecycle/0078_migration_final_record.t) | Migration distinguishes observed restoration from removal readiness. |
| [tests/extension/quicklook-lifecycle/0079_migration_later_default.t](../../tests/extension/quicklook-lifecycle/0079_migration_later_default.t) | Later default choice stops migration preparation. |
| [tests/extension/quicklook-lifecycle/0080_migration_later_provider.t](../../tests/extension/quicklook-lifecycle/0080_migration_later_provider.t) | Later provider choice stops migration preparation. |
| [tests/extension/quicklook-lifecycle/0081_migration_wrong_archive.t](../../tests/extension/quicklook-lifecycle/0081_migration_wrong_archive.t) | Changed archive refuses migration activation. |
| [tests/extension/quicklook-lifecycle/0082_migration_occupied.t](../../tests/extension/quicklook-lifecycle/0082_migration_occupied.t) | Occupied original path refuses migration restoration. |
| [tests/extension/quicklook-lifecycle/0083_migration_wrong_session.t](../../tests/extension/quicklook-lifecycle/0083_migration_wrong_session.t) | Wrong GUI session refuses migration activation. |
| [tests/extension/quicklook-lifecycle/0084_migration_unresolved_default.t](../../tests/extension/quicklook-lifecycle/0084_migration_unresolved_default.t) | Unresolved original default remains pending and nonzero. |
| [tests/extension/quicklook-lifecycle/0085_archive_roundtrip_required.t](../../tests/extension/quicklook-lifecycle/0085_archive_roundtrip_required.t) | Failed archive verification cannot authorize original copy deletion. |
<!-- test-plan-end -->

## Quick Look rendering diagnostic readers

These offline observations validate independent literal 16x12 red/blue PNG content and narrow rejection controls. They exercise the same output readers used after explicit OS generation, without invoking Quick Look, changing a cache or proving provider selection. Live preview and thumbnail remain separate manual observations with the attribution limits documented in the [extension policy](../programs/quicklook-extension.md#explicit-rendering-diagnostics).

<!-- test-plan: tests/extension/quicklook-render/*.t -->

| Test | Observation |
| --- | --- |
| [tests/extension/quicklook-render/0001_preview_reference.t](../../tests/extension/quicklook-render/0001_preview_reference.t) | Preview output content matches independent dimensions and colors. |
| [tests/extension/quicklook-render/0002_thumbnail_reference.t](../../tests/extension/quicklook-render/0002_thumbnail_reference.t) | Thumbnail content matches independent dimensions and colors. |
| [tests/extension/quicklook-render/0003_preview_changed_color.t](../../tests/extension/quicklook-render/0003_preview_changed_color.t) | Preview output rejects changed pixels. |
| [tests/extension/quicklook-render/0004_thumbnail_changed_dimensions.t](../../tests/extension/quicklook-render/0004_thumbnail_changed_dimensions.t) | Thumbnail output rejects changed dimensions. |

<!-- test-plan-end -->
