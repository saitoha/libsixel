/*
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

#if defined(HAVE_CLIPBOARD_MACOS) && !defined(HAVE_APPKIT)

#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "clipboard.h"

static const CFStringRef clipboard_carbon_png = CFSTR("public.png");
static const CFStringRef clipboard_carbon_tiff = CFSTR("public.tiff");
static const CFStringRef clipboard_carbon_utf8 =
    CFSTR("public.utf8-plain-text");
static const CFStringRef clipboard_carbon_utf16 =
    CFSTR("public.utf16-plain-text");

static OSStatus
clipboard_carbon_open(PasteboardRef *pasteboard_out)
{
    OSStatus status;
    PasteboardRef pasteboard;

    status = noErr;
    pasteboard = NULL;

    if (pasteboard_out == NULL) {
        return paramErr;
    }

    *pasteboard_out = NULL;

    status = PasteboardCreate(kPasteboardClipboard, &pasteboard);
    if (status != noErr) {
        return status;
    }

    status = PasteboardSynchronize(pasteboard);
    if (status != noErr) {
        CFRelease(pasteboard);
        return status;
    }

    *pasteboard_out = pasteboard;

    return noErr;
}

static int
clipboard_carbon_flavor_equals(CFStringRef lhs, CFStringRef rhs)
{
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }

    if (CFStringCompare(lhs, rhs, 0) == kCFCompareEqualTo) {
        return 1;
    }

    return 0;
}

static SIXELSTATUS
clipboard_carbon_duplicate_cfdata(CFDataRef payload,
                                  unsigned char **buffer_out,
                                  size_t *length_out)
{
    SIXELSTATUS status;
    CFIndex length;
    CGDataProviderRef provider;
    unsigned char *buffer;
    UInt8 const *source;

    status = SIXEL_FALSE;
    length = 0;
    provider = NULL;
    buffer = NULL;
    source = NULL;

    if (payload == NULL || buffer_out == NULL || length_out == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: internal payload duplication failure.");
        return SIXEL_RUNTIME_ERROR;
    }

    *buffer_out = NULL;
    *length_out = 0u;

    provider = CGDataProviderCreateWithCFData(payload);
    if (provider == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to map CoreGraphics payload.");
        return SIXEL_RUNTIME_ERROR;
    }
    CGDataProviderRelease(provider);

    length = CFDataGetLength(payload);
    source = CFDataGetBytePtr(payload);

    if (length <= 0 || source == NULL) {
        return SIXEL_OK;
    }

    buffer = (unsigned char *)malloc((size_t)length);
    if (buffer == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: malloc() failed while duplicating payload.");
        return SIXEL_BAD_ALLOCATION;
    }

    memcpy(buffer, source, (size_t)length);

    *buffer_out = buffer;
    *length_out = (size_t)length;

    return SIXEL_OK;
}

static SIXELSTATUS
clipboard_carbon_copy_text(PasteboardRef pasteboard,
                           PasteboardItemID item,
                           CFStringRef flavor,
                           CFStringEncoding encoding,
                           unsigned char **data_out,
                           size_t *size_out)
{
    OSStatus os_status;
    CFDataRef flavor_data;
    CFStringRef string_payload;
    CFDataRef utf8_payload;
    SIXELSTATUS status;

    os_status = noErr;
    flavor_data = NULL;
    string_payload = NULL;
    utf8_payload = NULL;
    status = SIXEL_FALSE;

    os_status = PasteboardCopyItemFlavorData(pasteboard,
                                             item,
                                             flavor,
                                             &flavor_data);
    if (os_status != noErr || flavor_data == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to copy string payload.");
        status = SIXEL_BAD_CLIPBOARD;
        goto cleanup;
    }

    string_payload = CFStringCreateWithBytes(kCFAllocatorDefault,
                                             CFDataGetBytePtr(flavor_data),
                                             CFDataGetLength(flavor_data),
                                             encoding,
                                             0);
    if (string_payload == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to decode string payload.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    utf8_payload = CFStringCreateExternalRepresentation(
        kCFAllocatorDefault,
        string_payload,
        kCFStringEncodingUTF8,
        0);
    if (utf8_payload == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to encode UTF-8 payload.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    status = clipboard_carbon_duplicate_cfdata(utf8_payload,
                                               data_out,
                                               size_out);

cleanup:
    if (utf8_payload != NULL) {
        CFRelease(utf8_payload);
    }
    if (string_payload != NULL) {
        CFRelease(string_payload);
    }
    if (flavor_data != NULL) {
        CFRelease(flavor_data);
    }

    return status;
}

static SIXELSTATUS
clipboard_carbon_copy_binary(PasteboardRef pasteboard,
                             PasteboardItemID item,
                             CFStringRef flavor,
                             unsigned char **data_out,
                             size_t *size_out)
{
    OSStatus os_status;
    CFDataRef payload;
    SIXELSTATUS status;

    os_status = noErr;
    payload = NULL;
    status = SIXEL_FALSE;

    os_status = PasteboardCopyItemFlavorData(pasteboard,
                                             item,
                                             flavor,
                                             &payload);
    if (os_status != noErr || payload == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to copy binary payload.");
        status = SIXEL_BAD_CLIPBOARD;
        goto cleanup;
    }

    status = clipboard_carbon_duplicate_cfdata(payload,
                                               data_out,
                                               size_out);

cleanup:
    if (payload != NULL) {
        CFRelease(payload);
    }

    return status;
}

static SIXELSTATUS
clipboard_carbon_select_item(PasteboardRef pasteboard,
                             PasteboardItemID item,
                             unsigned char **data_out,
                             size_t *size_out)
{
    struct clipboard_carbon_candidate {
        CFStringRef flavor;
        int is_text;
        CFStringEncoding encoding;
    };
    static struct clipboard_carbon_candidate const candidates[] = {
        { clipboard_carbon_png, 0, kCFStringEncodingUTF8 },
        { clipboard_carbon_tiff, 0, kCFStringEncodingUTF8 },
        { clipboard_carbon_utf8, 1, kCFStringEncodingUTF8 },
        { clipboard_carbon_utf16, 1, kCFStringEncodingUTF16 },
    };
    SIXELSTATUS status;
    CFArrayRef flavors;
    CFIndex flavor_index;
    CFIndex candidate_index;
    CFStringRef entry;

    status = SIXEL_FALSE;
    flavors = NULL;
    flavor_index = 0;
    candidate_index = 0;
    entry = NULL;

    status = SIXEL_BAD_CLIPBOARD;

    if (PasteboardCopyItemFlavors(pasteboard, item, &flavors) != noErr) {
        sixel_helper_set_additional_message(
            "clipboard: failed to enumerate pasteboard flavors.");
        return SIXEL_BAD_CLIPBOARD;
    }

    for (candidate_index = 0;
         candidate_index < (CFIndex)(sizeof(candidates)
             / sizeof(candidates[0]));
         ++candidate_index) {
        for (flavor_index = 0;
             flavor_index < CFArrayGetCount(flavors);
             ++flavor_index) {
            entry = (CFStringRef)CFArrayGetValueAtIndex(flavors,
                                                        flavor_index);
            if (!clipboard_carbon_flavor_equals(entry,
                                                candidates[candidate_index]
                                                    .flavor)) {
                continue;
            }

            if (candidates[candidate_index].is_text) {
                status = clipboard_carbon_copy_text(
                    pasteboard,
                    item,
                    candidates[candidate_index].flavor,
                    candidates[candidate_index].encoding,
                    data_out,
                    size_out);
            } else {
                status = clipboard_carbon_copy_binary(
                    pasteboard,
                    item,
                    candidates[candidate_index].flavor,
                    data_out,
                    size_out);
            }

            if (status != SIXEL_BAD_CLIPBOARD) {
                goto cleanup;
            }
        }
    }

    sixel_helper_set_additional_message(
        "clipboard: PNG, TIFF, and string payloads are unavailable.");

cleanup:
    if (flavors != NULL) {
        CFRelease(flavors);
    }

    return status;
}

static SIXELSTATUS
clipboard_carbon_resolve(PasteboardRef pasteboard,
                         unsigned char **data_out,
                         size_t *size_out)
{
    SIXELSTATUS status;
    ItemCount item_count;
    ItemCount item_index;
    PasteboardItemID item;

    status = SIXEL_FALSE;
    item_count = 0u;
    item_index = 0u;
    item = (PasteboardItemID)0u;

    if (PasteboardGetItemCount(pasteboard, &item_count) != noErr) {
        sixel_helper_set_additional_message(
            "clipboard: failed to count pasteboard entries.");
        return SIXEL_BAD_CLIPBOARD;
    }

    for (item_index = 1u; item_index <= item_count; ++item_index) {
        if (PasteboardGetItemIdentifier(pasteboard,
                                        item_index,
                                        &item) != noErr) {
            continue;
        }

        status = clipboard_carbon_select_item(pasteboard,
                                              item,
                                              data_out,
                                              size_out);
        if (status != SIXEL_BAD_CLIPBOARD) {
            return status;
        }
    }

    sixel_helper_set_additional_message(
        "clipboard: no supported payloads found on pasteboard.");

    return SIXEL_BAD_CLIPBOARD;
}

SIXELSTATUS
sixel_clipboard_read_macos(char const *format,
                           unsigned char **data,
                           size_t *size,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    PasteboardRef pasteboard;
    unsigned char *buffer;
    size_t length;

    (void)format;
    (void)allocator;

    status = SIXEL_FALSE;
    pasteboard = NULL;
    buffer = NULL;
    length = 0u;

    if (data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: destination pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    if (clipboard_carbon_open(&pasteboard) != noErr) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open pasteboard.");
        return SIXEL_BAD_CLIPBOARD;
    }

    status = clipboard_carbon_resolve(pasteboard, &buffer, &length);

    CFRelease(pasteboard);

    if (SIXEL_SUCCEEDED(status)) {
        *data = buffer;
        *size = length;
    } else if (buffer != NULL) {
        free(buffer);
    }

    return status;
}

static CFStringRef
clipboard_carbon_target_for_write(char const *format, int *is_text)
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
        *is_text = 1;
        return clipboard_carbon_utf8;
    }
    if (strcmp(lowered, "png") == 0) {
        return clipboard_carbon_png;
    }
    if (strcmp(lowered, "tiff") == 0) {
        return clipboard_carbon_tiff;
    }

    *is_text = 1;
    return clipboard_carbon_utf8;
}

SIXELSTATUS
sixel_clipboard_write_macos(char const *format,
                            unsigned char const *data,
                            size_t size)
{
    SIXELSTATUS status;
    PasteboardRef pasteboard;
    CFStringRef target_flavor;
    int treat_as_text;
    OSStatus os_status;
    CFStringRef string_payload;
    CFDataRef payload;

    status = SIXEL_FALSE;
    pasteboard = NULL;
    target_flavor = NULL;
    treat_as_text = 0;
    os_status = noErr;
    string_payload = NULL;
    payload = NULL;

    if (data == NULL && size > 0u) {
        sixel_helper_set_additional_message(
            "clipboard: source buffer is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    target_flavor = clipboard_carbon_target_for_write(format,
                                                      &treat_as_text);

    if (clipboard_carbon_open(&pasteboard) != noErr) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open pasteboard.");
        return SIXEL_BAD_CLIPBOARD;
    }

    os_status = PasteboardClear(pasteboard);
    if (os_status != noErr) {
        sixel_helper_set_additional_message(
            "clipboard: failed to clear pasteboard.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    if (treat_as_text) {
        string_payload = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                 data,
                                                 (CFIndex)size,
                                                 kCFStringEncodingUTF8,
                                                 0);
        if (string_payload == NULL) {
            sixel_helper_set_additional_message(
                "clipboard: failed to build UTF-8 string.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }
        payload = CFStringCreateExternalRepresentation(
            kCFAllocatorDefault,
            string_payload,
            kCFStringEncodingUTF8,
            0);
    } else {
        payload = CFDataCreate(kCFAllocatorDefault,
                               data,
                               (CFIndex)size);
    }

    if (payload == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to prepare pasteboard payload.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    os_status = PasteboardPutItemFlavor(pasteboard,
                                        (PasteboardItemID)1u,
                                        target_flavor,
                                        payload,
                                        kPasteboardFlavorNoFlags);
    if (os_status != noErr) {
        sixel_helper_set_additional_message(
            "clipboard: failed to write pasteboard payload.");
        status = SIXEL_RUNTIME_ERROR;
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    if (payload != NULL) {
        CFRelease(payload);
    }
    if (string_payload != NULL) {
        CFRelease(string_payload);
    }
    if (pasteboard != NULL) {
        CFRelease(pasteboard);
    }

    return status;
}

int
sixel_clipboard_is_available_macos(void)
{
    PasteboardRef pasteboard;

    pasteboard = NULL;

    if (clipboard_carbon_open(&pasteboard) != noErr) {
        return 0;
    }

    CFRelease(pasteboard);

    return 1;
}

#endif

