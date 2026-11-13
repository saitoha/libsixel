/* SPDX-License-Identifier: MIT */
/* Rounded 6bit addressing belongs to this policy, including saturation
 * of the last cell. The consumer only dispatches to the selected lookup. */
static uint
lookup_6bit_palette_lookup(device const uchar *table, uchar3 query)
{
    uint3 cell;
    uint at;

    cell = min((uint3(query) + 2u) >> 2u, uint3(63u));
    at = (cell.x << 12u) | (cell.y << 6u) | cell.z;
    return uint(table[at]);
}
