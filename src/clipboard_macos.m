/*
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

#if defined(HAVE_CLIPBOARD_MACOS)

#import <AppKit/AppKit.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "clipboard.h"

static SIXELSTATUS
clipboard_duplicate_payload(NSData *payload,
                            unsigned char **buffer_out,
                            size_t *length_out)
{
    SIXELSTATUS status;
    size_t length;
    unsigned char *buffer;

    status = SIXEL_FALSE;
    length = 0u;
    buffer = NULL;

    if (payload == nil || buffer_out == NULL || length_out == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: internal payload duplication failure.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    length = (size_t)[payload length];
    if (length == 0u) {
        *buffer_out = NULL;
        *length_out = 0u;
        status = SIXEL_OK;
        goto end;
    }

    buffer = (unsigned char *)malloc(length);
    if (buffer == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    memcpy(buffer, [payload bytes], length);

    *buffer_out = buffer;
    *length_out = length;
    buffer = NULL;
    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status) && buffer != NULL) {
        free(buffer);
    }

    return status;
}

SIXELSTATUS
sixel_clipboard_read_macos(char const *format,
                           unsigned char **data,
                           size_t *size,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    NSPasteboard *pasteboard;
    NSPasteboardType binary_order[2];
    NSUInteger type_index;
    NSData *payload;
    NSString *string_payload;
    NSData *utf8_payload;
    unsigned char *buffer;
    size_t length;
    (void) allocator;

    status = SIXEL_FALSE;
    pasteboard = nil;
    binary_order[0] = NSPasteboardTypePNG;
    binary_order[1] = NSPasteboardTypeTIFF;
    type_index = 0u;
    payload = nil;
    string_payload = nil;
    utf8_payload = nil;
    buffer = NULL;
    length = 0u;

    if (data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: destination pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    (void)format;
    @autoreleasepool {
        pasteboard = [NSPasteboard generalPasteboard];
        if (pasteboard == nil) {
            sixel_helper_set_additional_message(
                "clipboard: NSPasteboard generalPasteboard returned nil.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }

        /*
         * Probe for image snapshots in descending fidelity order.  PNG
         * delivers lossless data, TIFF acts as a fallback, and UTF-8 SIXEL
         * text is the final option when no binary payloads are available.
         */
        for (type_index = 0u;
                type_index < sizeof(binary_order) / sizeof(binary_order[0]);
                ++type_index) {
            payload = [pasteboard dataForType:binary_order[type_index]];
            if (payload != nil) {
                status = clipboard_duplicate_payload(payload,
                                                     &buffer,
                                                     &length);
                if (SIXEL_FAILED(status)) {
                    goto cleanup;
                }
                status = SIXEL_OK;
                goto cleanup;
            }
        }

        string_payload = [pasteboard stringForType:NSPasteboardTypeString];
        if (string_payload != nil) {
            utf8_payload = [string_payload
                dataUsingEncoding:NSUTF8StringEncoding
             allowLossyConversion:NO];
            if (utf8_payload == nil) {
                sixel_helper_set_additional_message(
                    "clipboard: failed to encode string as UTF-8.");
                status = SIXEL_RUNTIME_ERROR;
                goto cleanup;
            }
            status = clipboard_duplicate_payload(utf8_payload,
                                                 &buffer,
                                                 &length);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            status = SIXEL_OK;
            goto cleanup;
        }

        sixel_helper_set_additional_message(
            "clipboard: PNG, TIFF, and string payloads are unavailable.");
        status = SIXEL_BAD_CLIPBOARD;
    }

cleanup:
    if (SIXEL_SUCCEEDED(status)) {
        *data = buffer;
        *size = length;
    } else {
        if (buffer != NULL) {
            free(buffer);
        }
    }

    return status;
}

static NSPasteboardType
clipboard_type_for_write(char const *format,
                         int *is_text)
{
    char lowered[32];
    size_t length;
    size_t index;

    *is_text = 0;

    if (format == NULL) {
        lowered[0] = '\0';
    } else {
        length = strlen(format);
        if (length >= sizeof(lowered)) {
            length = sizeof(lowered) - 1u;
        }
        for (index = 0u; index < length; ++index) {
            lowered[index] = (char)tolower((unsigned char)format[index]);
        }
        lowered[length] = '\0';
    }

    if (lowered[0] == '\0') {
        /*
         * Default to writing UTF-8 text when the caller omitted a format.
         * Users expect "clipboard:" to behave like the old converters,
         * which pasted textual SIXEL sequences rather than PNG blobs.
         */
        *is_text = 1;
        return NSPasteboardTypeString;
    }
    if (strcmp(lowered, "png") == 0) {
        return NSPasteboardTypePNG;
    }
    if (strcmp(lowered, "tiff") == 0) {
        return NSPasteboardTypeTIFF;
    }

    *is_text = 1;
    return NSPasteboardTypeString;
}

SIXELSTATUS
sixel_clipboard_write_macos(char const *format,
                            unsigned char const *data,
                            size_t size)
{
    SIXELSTATUS status;
    NSPasteboard *pasteboard;
    NSPasteboardType target_type;
    int treat_as_text;

    status = SIXEL_FALSE;
    pasteboard = nil;
    target_type = nil;
    treat_as_text = 0;

    if (data == NULL && size > 0u) {
        sixel_helper_set_additional_message(
            "clipboard: source buffer is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    @autoreleasepool {
        pasteboard = [NSPasteboard generalPasteboard];
        if (pasteboard == nil) {
            sixel_helper_set_additional_message(
                "clipboard: NSPasteboard generalPasteboard returned nil.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }

        target_type = clipboard_type_for_write(format, &treat_as_text);

        [pasteboard clearContents];

        if (treat_as_text) {
            NSString *string;

            if (size == 0u) {
                string = [[NSString alloc] initWithString:@""];
            } else {
                string = [[NSString alloc]
                    initWithBytes:data
                           length:(NSUInteger)size
                         encoding:NSUTF8StringEncoding];
            }
            if (string == nil) {
                sixel_helper_set_additional_message(
                    "clipboard: failed to build UTF-8 string.");
                status = SIXEL_RUNTIME_ERROR;
                goto cleanup;
            }
            BOOL success = [pasteboard setString:string
                                         forType:NSPasteboardTypeString];
            [string release];
            if (!success) {
                sixel_helper_set_additional_message(
                    "clipboard: failed to write string payload.");
                status = SIXEL_RUNTIME_ERROR;
                goto cleanup;
            }
        } else {
            NSData *payload = [NSData dataWithBytes:data length:size];
            if (payload == nil) {
                sixel_helper_set_additional_message(
                    "clipboard: failed to build NSData payload.");
                status = SIXEL_RUNTIME_ERROR;
                goto cleanup;
            }
            BOOL success = [pasteboard setData:payload forType:target_type];
            if (!success) {
                sixel_helper_set_additional_message(
                    "clipboard: failed to write binary payload.");
                status = SIXEL_RUNTIME_ERROR;
                goto cleanup;
            }
        }

        status = SIXEL_OK;
    }

cleanup:
    return status;
}

int
sixel_clipboard_is_available_macos(void)
{
    int available;

    available = 0;

    @autoreleasepool {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        if (pasteboard != nil) {
            available = 1;
        }
    }

    return available;
}

#endif

