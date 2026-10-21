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

/*
 * Closed Voronoi adjacency for Delaunay-graph walks.
 *
 * Two distinct points i and j are neighbors when some x of the box is
 * equally near to both and no point is strictly nearer:
 *
 *   |x - p_i| = |x - p_j| <= |x - p_k|   for every k
 *
 * This keeps every pair of a degenerate cospherical set, which a
 * triangulation would drop.  The builder returns a superset: the walk is
 * exact for any superset, and the tolerance absorbs rounding.
 *
 * For each i, a convex polytope starts as the box and is clipped by the
 * outward-relaxed half-spaces |x - p_i| <= |x - p_k| in increasing
 * distance from p_i.  It contains the cell of i, so its reach R bounds the
 * cell; a half-space with |p_k - p_i| / 2 > R cannot cut it and the scan
 * stops.  j is a candidate when the bisector of (i, j) comes within the
 * margin of the polytope.  A true neighbor's witness x lies on that
 * bisector and inside the polytope, so candidates contain the neighbors;
 * the graph keeps pairs that are candidates from both sides.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "allocator.h"
#include "lookup-delaunay-graph.h"
#include "status.h"

typedef struct sixel_lookup_delaunay_graph_point {
    double v[3];
} sixel_lookup_delaunay_graph_point_t;

/*
 * Convex polytope as faces in a shared vertex pool.  It only bounds the
 * Voronoi cell from outside, so face topology needs no exact arithmetic:
 * every clip uses the outward-relaxed half-space, and the result contains
 * the cell inside the cube.
 */
typedef struct sixel_lookup_delaunay_graph_polytope {
    int capacity_faces;
    int capacity_points;
    int face_count;
    int *face_start;
    int *face_length;
    int point_count;
    sixel_lookup_delaunay_graph_point_t *points;
    int *next_face_start;
    int *next_face_length;
    sixel_lookup_delaunay_graph_point_t *next_points;
    sixel_lookup_delaunay_graph_point_t *cap;
    double *angles;
    int *cap_order;
    /* Distinct vertices; every face vertex appears in about three faces. */
    int vertex_count;
    sixel_lookup_delaunay_graph_point_t *vertices;
    sixel_lookup_delaunay_graph_point_t *next_vertices;
} sixel_lookup_delaunay_graph_polytope_t;

static void
sixel_lookup_delaunay_graph_polytope_cube(
    sixel_lookup_delaunay_graph_polytope_t *polytope,
    double const *low,
    double const *high)
{
    /* Corner c has coordinates (c >> 2 & 1, c >> 1 & 1, c & 1). */
    static int const faces[6][4] = {
        { 0, 1, 3, 2 }, { 4, 6, 7, 5 },
        { 0, 4, 5, 1 }, { 2, 3, 7, 6 },
        { 0, 2, 6, 4 }, { 1, 5, 7, 3 }
    };
    int face;
    int corner;
    int index;

    polytope->face_count = 6;
    polytope->point_count = 24;
    polytope->vertex_count = 8;
    for (corner = 0; corner < 8; ++corner) {
        polytope->vertices[corner].v[0] = (corner & 4) ? high[0] : low[0];
        polytope->vertices[corner].v[1] = (corner & 2) ? high[1] : low[1];
        polytope->vertices[corner].v[2] = (corner & 1) ? high[2] : low[2];
    }
    for (face = 0; face < 6; ++face) {
        polytope->face_start[face] = face * 4;
        polytope->face_length[face] = 4;
        for (index = 0; index < 4; ++index) {
            corner = faces[face][index];
            polytope->points[face * 4 + index].v[0] =
                (corner & 4) ? high[0] : low[0];
            polytope->points[face * 4 + index].v[1] =
                (corner & 2) ? high[1] : low[1];
            polytope->points[face * 4 + index].v[2] =
                (corner & 1) ? high[2] : low[2];
        }
    }
}

/* Monotone substitute for atan2(y, x) with range [0, 4). */
static double
sixel_lookup_delaunay_graph_pseudo_angle(double x, double y)
{
    double ratio;

    if (x == 0.0 && y == 0.0) {
        return 0.0;
    }
    ratio = y / (fabs(x) + fabs(y));
    if (x < 0.0) {
        return 2.0 - ratio;
    }
    if (y < 0.0) {
        return 4.0 + ratio;
    }
    return ratio;
}

/* Clip the polytope by dot(normal, x) <= limit.  Returns 0 on overflow. */
static int
sixel_lookup_delaunay_graph_polytope_clip(
    sixel_lookup_delaunay_graph_polytope_t *polytope,
    double const *normal,
    double limit)
{
    int face;
    int index;
    int start;
    int length;
    int written;
    int face_out;
    int cap_count;
    int inside_a;
    int inside_b;
    int position;
    int key;
    int unique;
    int vertex_out;
    double ha;
    double hb;
    double t;
    double center[3];
    double u[3];
    double w[3];
    double norm;
    double dx;
    double dy;
    double dz;
    double angle;
    sixel_lookup_delaunay_graph_point_t const *a;
    sixel_lookup_delaunay_graph_point_t const *b;
    sixel_lookup_delaunay_graph_point_t *swap_points;
    int *swap_ints;

    written = 0;
    face_out = 0;
    cap_count = 0;
    for (face = 0; face < polytope->face_count; ++face) {
        start = polytope->face_start[face];
        length = polytope->face_length[face];
        if (written + length + 2 > polytope->capacity_points
                || face_out >= polytope->capacity_faces) {
            return 0;
        }
        polytope->next_face_start[face_out] = written;
        for (index = 0; index < length; ++index) {
            a = &polytope->points[start + index];
            b = &polytope->points[start + (index + 1) % length];
            ha = normal[0] * a->v[0] + normal[1] * a->v[1]
                + normal[2] * a->v[2] - limit;
            hb = normal[0] * b->v[0] + normal[1] * b->v[1]
                + normal[2] * b->v[2] - limit;
            inside_a = ha <= 0.0;
            inside_b = hb <= 0.0;
            if (inside_a) {
                polytope->next_points[written++] = *a;
            }
            if (inside_a != inside_b) {
                t = ha / (ha - hb);
                polytope->next_points[written].v[0] =
                    a->v[0] + t * (b->v[0] - a->v[0]);
                polytope->next_points[written].v[1] =
                    a->v[1] + t * (b->v[1] - a->v[1]);
                polytope->next_points[written].v[2] =
                    a->v[2] + t * (b->v[2] - a->v[2]);
                if (cap_count < polytope->capacity_points) {
                    polytope->cap[cap_count++] =
                        polytope->next_points[written];
                }
                ++written;
            }
        }
        polytope->next_face_length[face_out] =
            written - polytope->next_face_start[face_out];
        if (polytope->next_face_length[face_out] >= 3) {
            ++face_out;
        } else {
            written = polytope->next_face_start[face_out];
        }
    }

    /* The cap polygon is convex: order its points by angle about their
     * centroid within the clipping plane. */
    vertex_out = 0;
    for (index = 0; index < polytope->vertex_count; ++index) {
        a = &polytope->vertices[index];
        if (normal[0] * a->v[0] + normal[1] * a->v[1] + normal[2] * a->v[2]
                <= limit) {
            polytope->next_vertices[vertex_out++] = *a;
        }
    }
    if (written + cap_count > polytope->capacity_points
            || vertex_out + cap_count > polytope->capacity_points) {
        return 0;
    }
    if (cap_count > 0) {
        center[0] = 0.0;
        center[1] = 0.0;
        center[2] = 0.0;
        for (index = 0; index < cap_count; ++index) {
            center[0] += polytope->cap[index].v[0];
            center[1] += polytope->cap[index].v[1];
            center[2] += polytope->cap[index].v[2];
        }
        center[0] /= (double)cap_count;
        center[1] /= (double)cap_count;
        center[2] /= (double)cap_count;
        if (fabs(normal[0]) <= fabs(normal[1])
                && fabs(normal[0]) <= fabs(normal[2])) {
            u[0] = 0.0;
            u[1] = normal[2];
            u[2] = -normal[1];
        } else if (fabs(normal[1]) <= fabs(normal[2])) {
            u[0] = -normal[2];
            u[1] = 0.0;
            u[2] = normal[0];
        } else {
            u[0] = normal[1];
            u[1] = -normal[0];
            u[2] = 0.0;
        }
        norm = sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        u[0] /= norm;
        u[1] /= norm;
        u[2] /= norm;
        w[0] = normal[1] * u[2] - normal[2] * u[1];
        w[1] = normal[2] * u[0] - normal[0] * u[2];
        w[2] = normal[0] * u[1] - normal[1] * u[0];
        for (index = 0; index < cap_count; ++index) {
            dx = polytope->cap[index].v[0] - center[0];
            dy = polytope->cap[index].v[1] - center[1];
            dz = polytope->cap[index].v[2] - center[2];
            angle = sixel_lookup_delaunay_graph_pseudo_angle(
                dx * u[0] + dy * u[1] + dz * u[2],
                dx * w[0] + dy * w[1] + dz * w[2]);
            /* Insertion sort; caps hold few points. */
            position = index;
            while (position > 0 && polytope->angles[position - 1] > angle) {
                polytope->angles[position] = polytope->angles[position - 1];
                polytope->cap_order[position] =
                    polytope->cap_order[position - 1];
                --position;
            }
            polytope->angles[position] = angle;
            polytope->cap_order[position] = index;
        }
        if (face_out >= polytope->capacity_faces) {
            return 0;
        }
        polytope->next_face_start[face_out] = written;
        unique = 0;
        for (index = 0; index < cap_count; ++index) {
            key = polytope->cap_order[index];
            if (unique > 0) {
                a = &polytope->next_points[written + unique - 1];
                b = &polytope->cap[key];
                dx = a->v[0] - b->v[0];
                dy = a->v[1] - b->v[1];
                dz = a->v[2] - b->v[2];
                if (dx * dx + dy * dy + dz * dz < 1.0e-18) {
                    continue;
                }
            }
            polytope->next_points[written + unique] = polytope->cap[key];
            ++unique;
        }
        /* Close the loop: the last point may repeat the first. */
        if (unique > 1) {
            a = &polytope->next_points[written];
            b = &polytope->next_points[written + unique - 1];
            dx = a->v[0] - b->v[0];
            dy = a->v[1] - b->v[1];
            dz = a->v[2] - b->v[2];
            if (dx * dx + dy * dy + dz * dz < 1.0e-18) {
                --unique;
            }
        }
        for (index = 0; index < unique; ++index) {
            polytope->next_vertices[vertex_out++] =
                polytope->next_points[written + index];
        }
        if (unique >= 3 && face_out < polytope->capacity_faces) {
            polytope->next_face_length[face_out] = unique;
            written += unique;
            ++face_out;
        }
    }
    swap_points = polytope->points;
    polytope->points = polytope->next_points;
    polytope->next_points = swap_points;
    swap_ints = polytope->face_start;
    polytope->face_start = polytope->next_face_start;
    polytope->next_face_start = swap_ints;
    swap_ints = polytope->face_length;
    polytope->face_length = polytope->next_face_length;
    polytope->next_face_length = swap_ints;
    swap_points = polytope->vertices;
    polytope->vertices = polytope->next_vertices;
    polytope->next_vertices = swap_points;
    polytope->face_count = face_out;
    polytope->point_count = written;
    polytope->vertex_count = vertex_out;
    /* A lost cell cannot justify removing any edge. Let the caller retain
     * every candidate if clipping became numerically degenerate. */
    return vertex_out > 0 && face_out > 0;
}

/* Axis-aligned bounds of the polytope vertices. */
static void
sixel_lookup_delaunay_graph_polytope_box(
    sixel_lookup_delaunay_graph_polytope_t const *polytope,
    double *low,
    double *high)
{
    int index;
    int axis;
    double value;

    for (axis = 0; axis < 3; ++axis) {
        low[axis] = polytope->vertices[0].v[axis];
        high[axis] = polytope->vertices[0].v[axis];
    }
    for (index = 1; index < polytope->vertex_count; ++index) {
        for (axis = 0; axis < 3; ++axis) {
            value = polytope->vertices[index].v[axis];
            if (value < low[axis]) {
                low[axis] = value;
            } else if (value > high[axis]) {
                high[axis] = value;
            }
        }
    }
}

/* Largest dot(normal, x) - limit over the box; an upper bound for the
 * polytope inside it. */
static double
sixel_lookup_delaunay_graph_box_support(double const *low,
                                       double const *high,
                                       double const *normal,
                                       double limit)
{
    double sum;
    int axis;

    sum = -limit;
    for (axis = 0; axis < 3; ++axis) {
        sum += normal[axis] * (normal[axis] > 0.0 ? high[axis] : low[axis]);
    }
    return sum;
}

/* Unit normal and offset of the half-space of p_i against p_k:
 * dot(normal, x) <= limit holds where p_i is at least as near as p_k. */
static void
sixel_lookup_delaunay_graph_bisector(double const *pi,
                                     double const *pk,
                                     double *normal,
                                     double *limit)
{
    double length;

    normal[0] = pk[0] - pi[0];
    normal[1] = pk[1] - pi[1];
    normal[2] = pk[2] - pi[2];
    length = sqrt(normal[0] * normal[0] + normal[1] * normal[1]
                  + normal[2] * normal[2]);
    *limit = 0.5 * ((pk[0] - pi[0]) * (pk[0] + pi[0])
                    + (pk[1] - pi[1]) * (pk[1] + pi[1])
                    + (pk[2] - pi[2]) * (pk[2] + pi[2])) / length;
    normal[0] /= length;
    normal[1] /= length;
    normal[2] /= length;
}

/*
 * Sort the other points by distance from one point.  Keys hold the squared
 * distance rounded toward zero to float, then the id: rounding is monotone,
 * so a key above a bound proves every later distance is above it too.
 * keys and scratch each hold count words.
 */
static void
sixel_lookup_delaunay_graph_sort(unsigned long long *keys,
                                 unsigned long long *scratch,
                                 int count)
{
    unsigned int buckets[256];
    unsigned long long *swap;
    unsigned int sum;
    unsigned int value;
    int index;
    int shift;

    /* Ids use the low 16 bits; float bits use the next 32. */
    for (shift = 0; shift < 48; shift += 8) {
        memset(buckets, 0, sizeof(buckets));
        for (index = 0; index < count; ++index) {
            ++buckets[(unsigned int)(keys[index] >> shift) & 255U];
        }
        sum = 0U;
        for (index = 0; index < 256; ++index) {
            value = buckets[index];
            buckets[index] = sum;
            sum += value;
        }
        for (index = 0; index < count; ++index) {
            scratch[buckets[(unsigned int)(keys[index] >> shift) & 255U]++]
                = keys[index];
        }
        swap = keys;
        keys = scratch;
        scratch = swap;
    }
    /* Six passes leave the result in the original keys array. */
}

static double
sixel_lookup_delaunay_graph_key_distance(unsigned long long key)
{
    unsigned int bits;
    float value;

    bits = (unsigned int)(key >> 16);
    memcpy(&value, &bits, sizeof(value));
    return (double)value;
}

static unsigned long long
sixel_lookup_delaunay_graph_key(double distance, int id)
{
    float value;
    unsigned int bits;

    value = (float)distance;
    if ((double)value > distance) {
        value = nextafterf(value, 0.0f);
    }
    memcpy(&bits, &value, sizeof(bits));
    return ((unsigned long long)bits << 16) | (unsigned long long)id;
}

/* Mark candidate[k] for the bisectors that reach the polytope of i.
 * Returns 0 when the polytope overflowed. */
static int
sixel_lookup_delaunay_graph_mark(
    double const *points,
    int count,
    int i,
    unsigned long long const *order,
    double const *box_low,
    double const *box_high,
    double tolerance,
    double margin,
    sixel_lookup_delaunay_graph_polytope_t *polytope,
    unsigned char *candidate)
{
    double const *pi;
    double const *pk;
    double normal[3];
    double limit;
    double reach;
    double h;
    double best;
    double dx;
    double dy;
    double dz;
    double distance;
    double low[3];
    double high[3];
    int position;
    int index;
    int k;

    pi = points + (size_t)i * 3U;
    for (index = 0; index < 3; ++index) {
        low[index] = box_low[index] - tolerance;
        high[index] = box_high[index] + tolerance;
    }
    sixel_lookup_delaunay_graph_polytope_cube(polytope, low, high);
    best = 0.0;
    for (index = 0; index < polytope->vertex_count; ++index) {
        dx = polytope->vertices[index].v[0] - pi[0];
        dy = polytope->vertices[index].v[1] - pi[1];
        dz = polytope->vertices[index].v[2] - pi[2];
        distance = dx * dx + dy * dy + dz * dz;
        if (distance > best) {
            best = distance;
        }
    }
    reach = sqrt(best);

    for (position = 0; position < count - 1; ++position) {
        if (0.25 * sixel_lookup_delaunay_graph_key_distance(order[position])
                > (reach + tolerance) * (reach + tolerance)) {
            break;
        }
        k = (int)(order[position] & 0xffffU);
        pk = points + (size_t)k * 3U;
        dx = pk[0] - pi[0];
        dy = pk[1] - pi[1];
        dz = pk[2] - pi[2];
        /* Almost coincident sites can separate mathematical cells while
         * their query distances round to the same double. Clipping by that
         * plane could hide a useful neighbor behind a rounded-distance
         * plateau. Omitting the constraint only enlarges the candidate
         * cell, preserving true edges and the scan-index tie contract. */
        if (dx * dx + dy * dy + dz * dz <= tolerance * tolerance) {
            continue;
        }
        sixel_lookup_delaunay_graph_bisector(pi, pk, normal, &limit);
        limit += tolerance;
        if (sixel_lookup_delaunay_graph_box_support(low, high, normal, limit)
                <= 0.0) {
            continue;
        }
        best = -1.0;
        for (index = 0; index < polytope->vertex_count; ++index) {
            h = normal[0] * polytope->vertices[index].v[0]
                + normal[1] * polytope->vertices[index].v[1]
                + normal[2] * polytope->vertices[index].v[2] - limit;
            if (h > best) {
                best = h;
            }
        }
        if (best <= 0.0) {
            continue;
        }
        if (!sixel_lookup_delaunay_graph_polytope_clip(polytope, normal,
                                                       limit)) {
            return 0;
        }
        best = 0.0;
        for (index = 0; index < polytope->vertex_count; ++index) {
            dx = polytope->vertices[index].v[0] - pi[0];
            dy = polytope->vertices[index].v[1] - pi[1];
            dz = polytope->vertices[index].v[2] - pi[2];
            distance = dx * dx + dy * dy + dz * dz;
            if (distance > best) {
                best = distance;
            }
        }
        reach = sqrt(best);
        sixel_lookup_delaunay_graph_polytope_box(polytope, low, high);
    }

    /* The margin absorbs rounding in the polytope vertices. */
    for (position = 0; position < count - 1; ++position) {
        if (0.25 * sixel_lookup_delaunay_graph_key_distance(order[position])
                > (reach + margin) * (reach + margin)) {
            break;
        }
        k = (int)(order[position] & 0xffffU);
        pk = points + (size_t)k * 3U;
        sixel_lookup_delaunay_graph_bisector(pi, pk, normal, &limit);
        if (sixel_lookup_delaunay_graph_box_support(low, high, normal, limit)
                < -margin) {
            continue;
        }
        for (index = 0; index < polytope->vertex_count; ++index) {
            h = normal[0] * polytope->vertices[index].v[0]
                + normal[1] * polytope->vertices[index].v[1]
                + normal[2] * polytope->vertices[index].v[2] - limit;
            if (h >= -margin) {
                candidate[k] = 1U;
                break;
            }
        }
    }
    return 1;
}

SIXELSTATUS
sixel_lookup_delaunay_graph_build(sixel_allocator_t *allocator,
                                  double const *points,
                                  int count,
                                  double const *low,
                                  double const *high,
                                  int **offsets_out,
                                  int **neighbors_out)
{
    SIXELSTATUS status;
    unsigned long long *order;
    unsigned long long *scratch;
    unsigned char *candidate;
    int *offsets;
    int *neighbors;
    sixel_lookup_delaunay_graph_polytope_t polytope;
    double extent;
    double tolerance;
    double margin;
    double dx;
    double dy;
    double dz;
    size_t faces;
    size_t capacity;
    size_t row;
    int i;
    int j;
    int position;
    int total;

    status = SIXEL_BAD_ALLOCATION;
    order = NULL;
    scratch = NULL;
    candidate = NULL;
    offsets = NULL;
    neighbors = NULL;
    memset(&polytope, 0, sizeof(polytope));
    if (allocator == NULL || points == NULL || low == NULL || high == NULL
            || offsets_out == NULL || neighbors_out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *offsets_out = NULL;
    *neighbors_out = NULL;
    if (count <= 0 || count > 32768
            || (size_t)count > SIZE_MAX / (size_t)count / sizeof(*order)) {
        return SIXEL_BAD_ARGUMENT;
    }

    /* Outward slack scales with the domain, including extended float
     * palettes. Failure to represent a cell only adds candidate edges. */
    extent = 0.0;
    for (i = 0; i < 3; ++i) {
        if (high[i] - low[i] > extent) {
            extent = high[i] - low[i];
        }
    }
    if (extent <= 0.0) {
        extent = 1.0;
    }
    tolerance = extent * 4.0e-7;
    margin = extent * 4.0e-6;

    faces = (size_t)count + 16U;
    capacity = faces * 8U + 64U;
    order = (unsigned long long *)sixel_allocator_malloc(
        allocator, (size_t)count * (size_t)count * sizeof(*order));
    scratch = (unsigned long long *)sixel_allocator_malloc(
        allocator, (size_t)count * sizeof(*scratch));
    candidate = (unsigned char *)sixel_allocator_calloc(
        allocator, (size_t)count * (size_t)count, 1U);
    offsets = (int *)sixel_allocator_malloc(
        allocator, ((size_t)count + 1U) * sizeof(int));
    polytope.capacity_faces = (int)faces;
    polytope.capacity_points = (int)capacity;
    polytope.face_start = (int *)sixel_allocator_malloc(
        allocator, faces * sizeof(int));
    polytope.face_length = (int *)sixel_allocator_malloc(
        allocator, faces * sizeof(int));
    polytope.next_face_start = (int *)sixel_allocator_malloc(
        allocator, faces * sizeof(int));
    polytope.next_face_length = (int *)sixel_allocator_malloc(
        allocator, faces * sizeof(int));
    polytope.points = (sixel_lookup_delaunay_graph_point_t *)
        sixel_allocator_malloc(
            allocator, capacity * sizeof(sixel_lookup_delaunay_graph_point_t));
    polytope.next_points = (sixel_lookup_delaunay_graph_point_t *)
        sixel_allocator_malloc(
            allocator, capacity * sizeof(sixel_lookup_delaunay_graph_point_t));
    polytope.cap = (sixel_lookup_delaunay_graph_point_t *)
        sixel_allocator_malloc(
            allocator, capacity * sizeof(sixel_lookup_delaunay_graph_point_t));
    polytope.vertices = (sixel_lookup_delaunay_graph_point_t *)
        sixel_allocator_malloc(
            allocator, capacity * sizeof(sixel_lookup_delaunay_graph_point_t));
    polytope.next_vertices = (sixel_lookup_delaunay_graph_point_t *)
        sixel_allocator_malloc(
            allocator, capacity * sizeof(sixel_lookup_delaunay_graph_point_t));
    polytope.angles = (double *)sixel_allocator_malloc(
        allocator, capacity * sizeof(double));
    polytope.cap_order = (int *)sixel_allocator_malloc(
        allocator, capacity * sizeof(int));
    if (order == NULL || scratch == NULL || candidate == NULL
            || offsets == NULL || polytope.face_start == NULL
            || polytope.face_length == NULL
            || polytope.next_face_start == NULL
            || polytope.next_face_length == NULL
            || polytope.points == NULL || polytope.next_points == NULL
            || polytope.cap == NULL || polytope.vertices == NULL
            || polytope.next_vertices == NULL || polytope.angles == NULL
            || polytope.cap_order == NULL) {
        goto end;
    }

    for (i = 0; i < count; ++i) {
        row = (size_t)i * (size_t)count;
        position = 0;
        for (j = 0; j < count; ++j) {
            if (j == i) {
                continue;
            }
            dx = points[(size_t)j * 3U] - points[(size_t)i * 3U];
            dy = points[(size_t)j * 3U + 1U] - points[(size_t)i * 3U + 1U];
            dz = points[(size_t)j * 3U + 2U] - points[(size_t)i * 3U + 2U];
            order[row + (size_t)position++] = sixel_lookup_delaunay_graph_key(
                dx * dx + dy * dy + dz * dz, j);
        }
        sixel_lookup_delaunay_graph_sort(order + row, scratch, count - 1);
        if (!sixel_lookup_delaunay_graph_mark(points, count, i, order + row,
                                              low, high, tolerance, margin,
                                              &polytope, candidate + row)) {
            memset(candidate + row, 1, (size_t)count);
        }
    }

    /* Keep pairs marked from both sides; each side holds every neighbor. */
    total = 0;
    for (i = 0; i < count; ++i) {
        row = (size_t)i * (size_t)count;
        for (j = 0; j < count; ++j) {
            if (j != i && candidate[row + (size_t)j] != 0U
                    && candidate[(size_t)j * (size_t)count + (size_t)i]
                        != 0U) {
                ++total;
            }
        }
    }
    neighbors = (int *)sixel_allocator_malloc(
        allocator, ((size_t)total + 1U) * sizeof(int));
    if (neighbors == NULL) {
        goto end;
    }
    total = 0;
    for (i = 0; i < count; ++i) {
        row = (size_t)i * (size_t)count;
        offsets[i] = total;
        for (position = 0; position < count - 1; ++position) {
            j = (int)(order[row + (size_t)position] & 0xffffU);
            if (candidate[row + (size_t)j] != 0U
                    && candidate[(size_t)j * (size_t)count + (size_t)i]
                        != 0U) {
                neighbors[total++] = j;
            }
        }
    }
    offsets[count] = total;
    *offsets_out = offsets;
    *neighbors_out = neighbors;
    offsets = NULL;
    neighbors = NULL;
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, order);
    sixel_allocator_free(allocator, scratch);
    sixel_allocator_free(allocator, candidate);
    sixel_allocator_free(allocator, offsets);
    sixel_allocator_free(allocator, neighbors);
    sixel_allocator_free(allocator, polytope.face_start);
    sixel_allocator_free(allocator, polytope.face_length);
    sixel_allocator_free(allocator, polytope.next_face_start);
    sixel_allocator_free(allocator, polytope.next_face_length);
    sixel_allocator_free(allocator, polytope.points);
    sixel_allocator_free(allocator, polytope.next_points);
    sixel_allocator_free(allocator, polytope.cap);
    sixel_allocator_free(allocator, polytope.vertices);
    sixel_allocator_free(allocator, polytope.next_vertices);
    sixel_allocator_free(allocator, polytope.angles);
    sixel_allocator_free(allocator, polytope.cap_order);
    return status;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
