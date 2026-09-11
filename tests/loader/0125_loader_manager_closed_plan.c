/* Verify a trailing bang prevents default loaders from being appended. */

#include <stdio.h>
#include <string.h>

#include "src/loader-manager.h"

int
test_loader_0125_loader_manager_closed_plan(int argc, char **argv)
{
    sixel_loader_entry_t entries[3];
    sixel_option_value_schema_t base_defs[3];
    sixel_option_argument_list_item_t items[3];
    sixel_option_argument_list_resolution_t resolution;
    sixel_loader_entry_t const *plan[3];
    size_t plan_length;

    (void)argc;
    (void)argv;
    memset(entries, 0, sizeof(entries));
    memset(base_defs, 0, sizeof(base_defs));
    memset(items, 0, sizeof(items));
    memset(&resolution, 0, sizeof(resolution));
    memset(plan, 0, sizeof(plan));

    entries[0].name = "alpha";
    entries[0].default_enabled = 1;
    entries[1].name = "beta";
    entries[1].default_enabled = 1;
    entries[2].name = "gamma";
    entries[2].default_enabled = 0;

    base_defs[0].name = "gamma";
    base_defs[1].name = "beta";
    base_defs[2].name = "gamma";
    items[0].resolution.base_def = &base_defs[0];
    items[1].resolution.base_def = &base_defs[1];
    items[2].resolution.base_def = &base_defs[2];
    resolution.has_trailing_bang = 1;
    resolution.items = items;
    resolution.item_count = 3u;

    plan_length = loader_manager_build_plan_from_resolution(&resolution,
                                                            entries,
                                                            3u,
                                                            plan,
                                                            3u);
    if (plan_length != 2u ||
        plan[0] != &entries[2] ||
        plan[1] != &entries[1]) {
        fprintf(stderr, "closed loader plan order mismatch\n");
        return 1;
    }
    return 0;
}
