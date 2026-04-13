/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBSIXEL_PALETTE_KMEANS_ASSIGN_H
#define LIBSIXEL_PALETTE_KMEANS_ASSIGN_H

#ifdef __cplusplus
extern "C" {
#endif

void
sixel_kmeans_compute_half_center_distances(double const *centers,
                                           unsigned int k,
                                           double *half_center_dist);

void
sixel_kmeans_compute_half_center_distance_matrix(double const *centers,
                                                 unsigned int k,
                                                 double *half_center_dist,
                                                 double *half_center_matrix);

unsigned int
sixel_kmeans_yinyang_group_count(unsigned int k);

void
sixel_kmeans_build_yinyang_groups(unsigned int k,
                                  unsigned int group_count,
                                  unsigned int *group_offsets,
                                  unsigned int *center_groups);

double
sixel_kmeans_assign_samples_full_second(double const *centers,
                                        unsigned int k,
                                        double const *samples,
                                        double const *weights,
                                        unsigned int sample_count,
                                        unsigned int *membership,
                                        double *distance_cache,
                                        double *cluster_weights,
                                        double *accum,
                                        double *upper_bounds,
                                        double *lower_bounds);

double
sixel_kmeans_assign_samples_full_elkan(double const *centers,
                                       unsigned int k,
                                       double const *samples,
                                       double const *weights,
                                       unsigned int sample_count,
                                       unsigned int *membership,
                                       double *distance_cache,
                                       double *cluster_weights,
                                       double *accum,
                                       double *upper_bounds,
                                       double *lower_bounds,
                                       double *lower_matrix);

double
sixel_kmeans_assign_samples_full_yinyang(double const *centers,
                                         unsigned int k,
                                         double const *samples,
                                         double const *weights,
                                         unsigned int sample_count,
                                         unsigned int *membership,
                                         double *distance_cache,
                                         double *cluster_weights,
                                         double *accum,
                                         double *upper_bounds,
                                         double *lower_bounds,
                                         double *lower_matrix,
                                         unsigned int group_count,
                                         unsigned int const *group_offsets,
                                         double *group_lower_bounds);

double
sixel_kmeans_assign_samples_elkan(double const *centers,
                                  unsigned int k,
                                  double const *samples,
                                  double const *weights,
                                  unsigned int sample_count,
                                  unsigned int *membership,
                                  double *distance_cache,
                                  double *cluster_weights,
                                  double *accum,
                                  double *upper_bounds,
                                  double *lower_bounds,
                                  double *lower_matrix,
                                  double const *half_center_dist,
                                  double const *half_center_matrix);

double
sixel_kmeans_assign_samples_yinyang(double const *centers,
                                    unsigned int k,
                                    double const *samples,
                                    double const *weights,
                                    unsigned int sample_count,
                                    unsigned int *membership,
                                    double *distance_cache,
                                    double *cluster_weights,
                                    double *accum,
                                    double *upper_bounds,
                                    double *lower_bounds,
                                    double *lower_matrix,
                                    double const *half_center_dist,
                                    double const *half_center_matrix,
                                    unsigned int group_count,
                                    unsigned int const *group_offsets,
                                    unsigned int const *center_groups,
                                    double *group_lower_bounds);

void
sixel_kmeans_update_elkan_bounds(unsigned int sample_count,
                                 unsigned int k,
                                 unsigned int const *membership,
                                 double const *weights,
                                 double *upper_bounds,
                                 double *lower_bounds,
                                 double *lower_matrix,
                                 double const *center_shift);

double
sixel_kmeans_assign_samples_hamerly(double const *centers,
                                    unsigned int k,
                                    double const *samples,
                                    double const *weights,
                                    unsigned int sample_count,
                                    unsigned int *membership,
                                    double *distance_cache,
                                    double *cluster_weights,
                                    double *accum,
                                    double *upper_bounds,
                                    double *lower_bounds,
                                    double const *half_center_dist);

double
sixel_kmeans_assign_samples(double const *centers,
                            unsigned int k,
                            double const *samples,
                            double const *weights,
                            unsigned int sample_count,
                            unsigned int *membership,
                            double *distance_cache,
                            double *cluster_weights,
                            double *accum);

#ifdef __cplusplus
}
#endif

#endif /* LIBSIXEL_PALETTE_KMEANS_ASSIGN_H */

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
