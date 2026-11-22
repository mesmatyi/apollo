/******************************************************************************
 * Copyright 2026 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * OSQP 0.5/0.6 API on top of OSQP >= 1.0 (ROS 2 port).
 *
 * Apollo is written against the OSQP 0.5/0.6 C API (OSQPData, OSQPWorkspace,
 * `work = osqp_setup(data, settings)`, csc_matrix(), c_malloc(), ...). Ubuntu
 * 24.04 / conda-forge ship OSQP 1.x whose API is different and whose type
 * names partly overlap (OSQPSettings, OSQPWorkspace). This header is found
 * first as "osqp/osqp.h", includes the real OSQP 1.x header with
 * #include_next and provides the old API as C++ overloads/macros:
 *
 *   - OSQPSettings, OSQPWorkspace are macros for ApolloOSQPSettings and
 *     ApolloOSQPWorkspace (0.6 field names: polish, warm_start, ...).
 *   - OSQPData, csc, c_float, c_int, c_malloc, c_calloc, c_free, csc_matrix.
 *   - osqp_setup(data, settings) -> workspace, osqp_solve(work),
 *     osqp_cleanup(work), osqp_set_default_settings(settings),
 *     osqp_warm_start{,_x,_y}, osqp_update_{lin_cost,bounds,lower_bound,
 *     upper_bound,P,A,P_A,max_iter,eps_abs,eps_rel,polish,verbose,
 *     warm_start,time_limit}.
 *
 * OSQP >= 1.0 requires P to be upper triangular; like OSQP 0.5, the
 * compatibility layer only uses the upper triangular part of P.
 */
#pragma once

#include_next <osqp/osqp.h>

// OSQP 0.6's osqp.h (glob_opts.h) also pulled in these C headers.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

typedef OSQPFloat c_float;
typedef OSQPInt c_int;
typedef OSQPCscMatrix csc;

#define c_malloc std::malloc
#define c_calloc std::calloc
#define c_realloc std::realloc
#define c_free std::free

/** OSQP 0.6 settings (field names of the old API). */
struct ApolloOSQPSettings {
  c_float rho;
  c_float sigma;
  c_int scaling;
  c_int adaptive_rho;
  c_int adaptive_rho_interval;
  c_float adaptive_rho_tolerance;
  c_float adaptive_rho_fraction;
  c_int max_iter;
  c_float eps_abs;
  c_float eps_rel;
  c_float eps_prim_inf;
  c_float eps_dual_inf;
  c_float alpha;
  c_int linsys_solver;
  c_float delta;
  c_int polish;
  c_int polish_refine_iter;
  c_int verbose;
  c_int scaled_termination;
  c_int check_termination;
  c_int warm_start;
  c_float time_limit;
};

/** OSQP 0.5/0.6 problem data. */
struct OSQPData {
  c_int n;     ///< number of variables
  c_int m;     ///< number of constraints
  csc *P;      ///< quadratic cost (upper triangular part is used), n x n
  csc *A;      ///< linear constraints, m x n
  c_float *q;  ///< linear cost, size n
  c_float *l;  ///< lower bounds, size m
  c_float *u;  ///< upper bounds, size m
};

/** OSQP 0.5/0.6 workspace: solution and info as in the old API. */
struct ApolloOSQPWorkspace {
  OSQPSolver *solver;
  OSQPSolution *solution;
  OSQPInfo *info;
  OSQPData *data;
  ApolloOSQPSettings *settings;
  csc *P_triu;  ///< upper triangular copy of P when P was not triangular
};

/** csc_matrix() of OSQP 0.5: wraps (does not copy) the arrays. */
csc *csc_matrix(c_int m, c_int n, c_int nzmax, c_float *x, c_int *i, c_int *p);

void osqp_set_default_settings(ApolloOSQPSettings *settings);
ApolloOSQPWorkspace *osqp_setup(const OSQPData *data,
                                const ApolloOSQPSettings *settings);
c_int osqp_solve(ApolloOSQPWorkspace *work);
c_int osqp_cleanup(ApolloOSQPWorkspace *work);

c_int osqp_warm_start(ApolloOSQPWorkspace *work, const c_float *x,
                      const c_float *y);
c_int osqp_warm_start_x(ApolloOSQPWorkspace *work, const c_float *x);
c_int osqp_warm_start_y(ApolloOSQPWorkspace *work, const c_float *y);
c_int osqp_update_lin_cost(ApolloOSQPWorkspace *work, const c_float *q_new);
c_int osqp_update_bounds(ApolloOSQPWorkspace *work, const c_float *l_new,
                         const c_float *u_new);
c_int osqp_update_lower_bound(ApolloOSQPWorkspace *work, const c_float *l_new);
c_int osqp_update_upper_bound(ApolloOSQPWorkspace *work, const c_float *u_new);
c_int osqp_update_P(ApolloOSQPWorkspace *work, const c_float *Px_new,
                    const c_int *Px_new_idx, c_int P_new_n);
c_int osqp_update_A(ApolloOSQPWorkspace *work, const c_float *Ax_new,
                    const c_int *Ax_new_idx, c_int A_new_n);
c_int osqp_update_P_A(ApolloOSQPWorkspace *work, const c_float *Px_new,
                      const c_int *Px_new_idx, c_int P_new_n,
                      const c_float *Ax_new, const c_int *Ax_new_idx,
                      c_int A_new_n);
c_int osqp_update_max_iter(ApolloOSQPWorkspace *work, c_int max_iter_new);
c_int osqp_update_eps_abs(ApolloOSQPWorkspace *work, c_float eps_abs_new);
c_int osqp_update_eps_rel(ApolloOSQPWorkspace *work, c_float eps_rel_new);
c_int osqp_update_polish(ApolloOSQPWorkspace *work, c_int polish_new);
c_int osqp_update_verbose(ApolloOSQPWorkspace *work, c_int verbose_new);
c_int osqp_update_warm_start(ApolloOSQPWorkspace *work, c_int warm_start_new);
c_int osqp_update_time_limit(ApolloOSQPWorkspace *work, c_float time_limit_new);

#ifndef APOLLO_OSQP_COMPAT_IMPLEMENTATION
#define OSQPSettings ApolloOSQPSettings
#define OSQPWorkspace ApolloOSQPWorkspace
#endif
