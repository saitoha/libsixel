/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include "loader-order-schema.h"
#include "loader-common.h"
#include "options-registry.h"

sixel_option_argument_schema_t const *
sixel_loader_order_schema_get(void)
{
    return sixel_option_registry_get(SIXEL_OPTION_SCHEMA_LOADERS);
}

SIXELSTATUS
sixel_loader_order_validate_resolution(
    sixel_option_argument_list_resolution_t const *resolution)
{
    size_t item_index;
    size_t assignment_index;
    sixel_option_argument_resolution_t const *item;
    sixel_suboption_assignment_t const *assignment;

    item_index = 0u;
    assignment_index = 0u;
    item = NULL;
    assignment = NULL;

    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    while (item_index < resolution->item_count) {
        item = &resolution->items[item_index].resolution;
        if (item->base_def == NULL || item->base_def->name == NULL) {
            return SIXEL_BAD_ARGUMENT;
        }
        assignment_index = 0u;
        while (assignment_index < item->assignment_count) {
            assignment = item->assignments + assignment_index;
            if (assignment->resolved_key_name == NULL ||
                assignment->resolved_value_text == NULL) {
                return SIXEL_BAD_ARGUMENT;
            }
            ++assignment_index;
        }
        ++item_index;
    }

    return SIXEL_OK;
}

SIXELSTATUS
sixel_loader_order_parse_and_validate(
    char const *argument,
    sixel_option_argument_list_resolution_t *resolution,
    char *diagnostic,
    size_t diagnostic_size)
{
    SIXELSTATUS status;

    status = SIXEL_OK;
    if (resolution == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_option_parse_argument_list_with_suboptions(
        argument,
        sixel_loader_order_schema_get(),
        resolution,
        diagnostic,
        diagnostic_size);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_loader_order_validate_resolution(resolution);
    if (SIXEL_FAILED(status)) {
        sixel_option_free_argument_list_resolution(resolution);
        return status;
    }

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
