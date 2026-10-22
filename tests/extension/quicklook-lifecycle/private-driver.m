/* Compile-time OS boundaries for lifecycle regression controls. The caller
 * supplies only a private root; never use this binary on an installed tree. */
#import <Foundation/Foundation.h>
static NSString *test_root;
static NSString *test_journal;
static NSString *test_os;
static NSString *test_installer;
static NSString *test_pkgutil;
#define SIXEL_INSTALL_ROOT test_root
#define SIXEL_INSTALLER test_installer
#define SIXEL_PKGUTIL test_pkgutil
#define SIXEL_PLUGIN_TOOL test_os
#define SIXEL_INSTALL_UID 0
#define SIXEL_USER_PROBE "private-user.inc.h"
#define SIXEL_USER_MUTATOR "private-mutations.inc.h"
#define main package_tool_main
#include "../../../quicklook-extension/Sources/SixelPackageTool.m"
#undef main
int
main(int argc, const char *argv[])
{
    @autoreleasepool {
        test_root = @(getenv("QUICKLOOK_PRIVATE_ROOT") ?: "");
        test_journal = @(getenv("QUICKLOOK_PRIVATE_JOURNAL") ?: "");
        test_os = @(getenv("QUICKLOOK_PRIVATE_OS") ?: "");
        test_installer = @(getenv("QUICKLOOK_PRIVATE_INSTALLER") ?:
            test_os.UTF8String);
        test_pkgutil = @(getenv("QUICKLOOK_PRIVATE_PKGUTIL") ?:
            test_os.UTF8String);
        if (test_root.length == 0 || test_os.length == 0 ||
            [test_root isEqual:@"/"]) {
            return 90;
        }
        return package_tool_main(argc, argv);
    }
}
