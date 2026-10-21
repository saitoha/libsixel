/* Private pkgutil/PlugInKit endpoint. Unknown argv is rejected. */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
static void
save(id value, NSString *path)
{
    NSData *data;
    data = [NSJSONSerialization dataWithJSONObject:value options:0 error:nil];
    if (![data writeToFile:path atomically:YES]) {
        exit(88);
    }
}
int
main(int argc, const char *argv[])
{
    NSString *root;
    NSString *state_path;
    NSString *journal_path;
    NSString *command;
    NSString *name;
    NSString *path;
    NSMutableDictionary *state;
    NSMutableDictionary *plugins;
    NSMutableArray *calls;
    NSDictionary *record;
    NSDictionary *journal;
    NSData *data;
    id value;
    @autoreleasepool {
        root = @(getenv("QUICKLOOK_PRIVATE_ROOT") ?: "");
        if (root.length == 0 || [root isEqual:@"/"]) {
            return 90;
        }
        state_path = [root stringByAppendingPathComponent:@"live.json"];
        data = [NSData dataWithContentsOfFile:state_path];
        state = [NSJSONSerialization JSONObjectWithData:data
            options:NSJSONReadingMutableContainers error:nil];
        if (state == nil || argc < 2) {
            return 90;
        }
        command = @(argv[1]);
        value = nil;
        if (argc == 2 && [command isEqual:@"--pkgs-plist"]) {
            value = state[@"receipt"] == nil ? @[] :
                @[@"com.saitoha.libsixel.quicklook.pkg"];
        } else if (argc == 3 && [command isEqual:@"--pkg-info-plist"] &&
            [@(argv[2]) isEqual:@"com.saitoha.libsixel.quicklook.pkg"]) {
            value = state[@"receipt"];
        } else if (argc == 3 && [command isEqual:@"--forget"] &&
            [@(argv[2]) isEqual:@"com.saitoha.libsixel.quicklook.pkg"]) {
            [state removeObjectForKey:@"receipt"];
            save(state, state_path);
            return 0;
        } else if (argc == 7 && [command isEqual:@"-m"] &&
            [@(argv[2]) isEqual:@"-A"] && [@(argv[3]) isEqual:@"-D"] &&
            [@(argv[4]) isEqual:@"-v"] && [@(argv[5]) isEqual:@"-i"]) {
            for (record in state[@"plugins"][@(argv[6])]) {
                printf("%s    %s(%s)\t%s\t2026-10-21\t%s\n",
                    [record[@"election"] UTF8String],
                    [record[@"identifier"] UTF8String],
                    [record[@"version"] UTF8String],
                    [record[@"uuid"] UTF8String],
                    [record[@"path"] UTF8String]);
            }
            return 0;
        } else if (argc == 3 &&
            [@[@"-a", @"-r"] containsObject:command]) {
            path = @(argv[2]);
            name = [path.lastPathComponent isEqual:@"SixelPreview.appex"] ?
                @"preview" : [path.lastPathComponent
                isEqual:@"SixelThumbnail.appex"] ? @"thumbnail" : nil;
            if (name == nil || ![path hasPrefix:
                [root stringByAppendingString:@"/"]]) {
                return 89;
            }
            journal_path = [@(getenv("QUICKLOOK_PRIVATE_JOURNAL") ?: "")
                stringByAppendingPathComponent:@"journal.json"];
            data = [NSData dataWithContentsOfFile:journal_path];
            journal = [NSJSONSerialization JSONObjectWithData:data
                options:0 error:nil];
            if (![@[@"pending", @"remove-pending"]
                containsObject:journal[@"state"]]) {
                return 91;
            }
            calls = state[@"calls"];
            [calls addObject:command];
            save(state, state_path);
            if ([state[@"fail"] isEqual:command]) {
                return 43;
            }
            plugins = state[@"plugins"];
            record = state[@"providerRecords"][name];
            if (state[@"providerRecordsByPath"][path] != nil) {
                record = state[@"providerRecordsByPath"][path];
            }
            if ([command isEqual:@"-a"]) {
                plugins[record[@"identifier"]] = @[record];
            } else {
                plugins[record[@"identifier"]] =
                    [plugins[record[@"identifier"]] filteredArrayUsingPredicate:
                    [NSPredicate predicateWithFormat:@"path != %@", path]]
                    ?: @[];
            }
            save(state, state_path);
            return 0;
        } else {
            return 89;
        }
        if (value == nil) {
            return 87;
        }
        data = [NSPropertyListSerialization dataWithPropertyList:value
            format:NSPropertyListXMLFormat_v1_0 options:0 error:nil];
        fwrite(data.bytes, 1, data.length, stdout);
    }
    return 0;
}
