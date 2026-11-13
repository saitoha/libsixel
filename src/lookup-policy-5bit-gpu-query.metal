/* SPDX-License-Identifier: MIT */
/* Rounded 5bit addressing belongs to this policy, including saturation
 * of the last cell. The consumer only dispatches to the selected lookup. */
static uint
lookup_5bit_palette_lookup(device const uchar *table, uchar3 query)
{
    uint3 cell;
    uint at;

    cell = min((uint3(query) + 4u) >> 3u, uint3(31u));
    at = (cell.x << 10u) | (cell.y << 5u) | cell.z;
    return uint(table[at]);
}
