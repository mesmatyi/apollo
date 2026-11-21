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

// OSQP 0.5/0.6 API implemented on OSQP >= 1.0, see include/osqp/osqp.h.

#define APOLLO_OSQP_COMPAT_IMPLEMENTATION
#include "osqp/osqp.h"

#include <algorithm>
#include <vector>

namespace {

void ToNativeSettings(const ApolloOSQPSettings &in, ::OSQPSettings *out) {
  osqp_set_default_settings(out);  // OSQP >= 1.0 defaults
  out->rho = in.rho;
  out->sigma = in.sigma;
  out->scaling = in.scaling;
  out->adaptive_rho = in.adaptive_rho ? OSQP_ADAPTIVE_RHO_UPDATE_DEFAULT
                                      : OSQP_ADAPTIVE_RHO_UPDATE_DISABLED;
  if (in.adaptive_rho_interval > 0) {
    out->adaptive_rho_interval = in.adaptive_rho_interval;
  }
  out->adaptive_rho_tolerance = in.adaptive_rho_tolerance;
  out->adaptive_rho_fraction = in.adaptive_rho_fraction;
  out->max_iter = in.max_iter;
  out->eps_abs = in.eps_abs;
  out->eps_rel = in.eps_rel;
  out->eps_prim_inf = in.eps_prim_inf;
  out->eps_dual_inf = in.eps_dual_inf;
  out->alpha = in.alpha;
  out->linsys_solver = OSQP_DIRECT_SOLVER;
  out->delta = in.delta;
  out->polishing = in.polish;
  out->polish_refine_iter = in.polish_refine_iter;
  out->verbose = in.verbose;
  out->scaled_termination = in.scaled_termination;
  out->check_termination = in.check_termination;
  out->warm_starting = in.warm_start;
  // OSQP 0.6: time_limit == 0 disables the limit.
  out->time_limit = in.time_limit > 0 ? in.time_limit : OSQP_TIME_LIMIT;
}

bool IsUpperTriangular(const csc *M) {
  for (c_int col = 0; col < M->n; ++col) {
    for (c_int k = M->p[col]; k < M->p[col + 1]; ++k) {
      if (M->i[k] > col) {
        return false;
      }
    }
  }
  return true;
}

// Upper triangular part of M; arrays allocated with malloc.
csc *UpperTriangular(const csc *M) {
  std::vector<c_int> p(M->n + 1, 0);
  std::vector<c_int> idx;
  std::vector<c_float> val;
  for (c_int col = 0; col < M->n; ++col) {
    for (c_int k = M->p[col]; k < M->p[col + 1]; ++k) {
      if (M->i[k] <= col) {
        idx.push_back(M->i[k]);
        val.push_back(M->x[k]);
      }
    }
    p[col + 1] = static_cast<c_int>(idx.size());
  }
  const size_t nnz = idx.size();
  c_float *x = static_cast<c_float *>(std::malloc(sizeof(c_float) * (nnz + 1)));
  c_int *i = static_cast<c_int *>(std::malloc(sizeof(c_int) * (nnz + 1)));
  c_int *pp = static_cast<c_int *>(std::malloc(sizeof(c_int) * (M->n + 1)));
  std::copy(val.begin(), val.end(), x);
  std::copy(idx.begin(), idx.end(), i);
  std::copy(p.begin(), p.end(), pp);
  return csc_matrix(M->m, M->n, static_cast<c_int>(nnz), x, i, pp);
}

void FreeCsc(csc *M) {
  if (M == nullptr) {
    return;
  }
  std::free(M->x);
  std::free(M->i);
  std::free(M->p);
  std::free(M);
}

}  // namespace

csc *csc_matrix(c_int m, c_int n, c_int nzmax, c_float *x, c_int *i,
                c_int *p) {
  csc *M = static_cast<csc *>(std::calloc(1, sizeof(csc)));
  if (M == nullptr) {
    return nullptr;
  }
  M->m = m;
  M->n = n;
  M->nzmax = nzmax;
  M->nz = -1;
  M->x = x;
  M->i = i;
  M->p = p;
  M->owned = 0;
  return M;
}

void osqp_set_default_settings(ApolloOSQPSettings *s) {
  if (s == nullptr) {
    return;
  }
  // OSQP 0.6 defaults (osqp/include/constants.h of 0.6.x)
  s->rho = 0.1;
  s->sigma = 1e-6;
  s->scaling = 10;
  s->adaptive_rho = 1;
  s->adaptive_rho_interval = 0;
  s->adaptive_rho_tolerance = 5.0;
  s->adaptive_rho_fraction = 0.4;
  s->max_iter = 4000;
  s->eps_abs = 1e-3;
  s->eps_rel = 1e-3;
  s->eps_prim_inf = 1e-4;
  s->eps_dual_inf = 1e-4;
  s->alpha = 1.6;
  s->linsys_solver = 0;  // QDLDL_SOLVER
  s->delta = 1e-6;
  s->polish = 0;
  s->polish_refine_iter = 3;
  s->verbose = 1;
  s->scaled_termination = 0;
  s->check_termination = 25;
  s->warm_start = 1;
  s->time_limit = 0;
}

ApolloOSQPWorkspace *osqp_setup(const OSQPData *data,
                                const ApolloOSQPSettings *settings) {
  if (data == nullptr || settings == nullptr || data->P == nullptr ||
      data->A == nullptr) {
    return nullptr;
  }
  ApolloOSQPWorkspace *work = static_cast<ApolloOSQPWorkspace *>(
      std::calloc(1, sizeof(ApolloOSQPWorkspace)));
  if (work == nullptr) {
    return nullptr;
  }
  const csc *P = data->P;
  if (!IsUpperTriangular(P)) {
    work->P_triu = UpperTriangular(P);
    P = work->P_triu;
  }
  ::OSQPSettings native;
  ToNativeSettings(*settings, &native);
  OSQPSolver *solver = nullptr;
  const c_int flag = osqp_setup(&solver, P, data->q, data->A, data->l,
                                data->u, data->m, data->n, &native);
  if (flag != 0 || solver == nullptr) {
    if (solver != nullptr) {
      osqp_cleanup(solver);
    }
    FreeCsc(work->P_triu);
    std::free(work);
    return nullptr;
  }
  work->solver = solver;
  work->solution = solver->solution;
  work->info = solver->info;
  work->data = const_cast<OSQPData *>(data);
  work->settings = static_cast<ApolloOSQPSettings *>(
      std::malloc(sizeof(ApolloOSQPSettings)));
  if (work->settings != nullptr) {
    *work->settings = *settings;
  }
  return work;
}

c_int osqp_solve(ApolloOSQPWorkspace *work) {
  if (work == nullptr || work->solver == nullptr) {
    return OSQP_WORKSPACE_NOT_INIT_ERROR;
  }
  return osqp_solve(work->solver);
}

c_int osqp_cleanup(ApolloOSQPWorkspace *work) {
  if (work == nullptr) {
    return 0;
  }
  c_int flag = 0;
  if (work->solver != nullptr) {
    flag = osqp_cleanup(work->solver);
  }
  FreeCsc(work->P_triu);
  std::free(work->settings);
  std::free(work);
  return flag;
}

c_int osqp_warm_start(ApolloOSQPWorkspace *work, const c_float *x,
                      const c_float *y) {
  return osqp_warm_start(work->solver, x, y);
}

c_int osqp_warm_start_x(ApolloOSQPWorkspace *work, const c_float *x) {
  return osqp_warm_start(work->solver, x, OSQP_NULL);
}

c_int osqp_warm_start_y(ApolloOSQPWorkspace *work, const c_float *y) {
  return osqp_warm_start(work->solver, OSQP_NULL, y);
}

c_int osqp_update_lin_cost(ApolloOSQPWorkspace *work, const c_float *q_new) {
  return osqp_update_data_vec(work->solver, q_new, OSQP_NULL, OSQP_NULL);
}

c_int osqp_update_bounds(ApolloOSQPWorkspace *work, const c_float *l_new,
                         const c_float *u_new) {
  return osqp_update_data_vec(work->solver, OSQP_NULL, l_new, u_new);
}

c_int osqp_update_lower_bound(ApolloOSQPWorkspace *work, const c_float *l_new) {
  return osqp_update_data_vec(work->solver, OSQP_NULL, l_new, OSQP_NULL);
}

c_int osqp_update_upper_bound(ApolloOSQPWorkspace *work, const c_float *u_new) {
  return osqp_update_data_vec(work->solver, OSQP_NULL, OSQP_NULL, u_new);
}

c_int osqp_update_P(ApolloOSQPWorkspace *work, const c_float *Px_new,
                    const c_int *Px_new_idx, c_int P_new_n) {
  return osqp_update_data_mat(work->solver, Px_new, Px_new_idx, P_new_n,
                              OSQP_NULL, OSQP_NULL, 0);
}

c_int osqp_update_A(ApolloOSQPWorkspace *work, const c_float *Ax_new,
                    const c_int *Ax_new_idx, c_int A_new_n) {
  return osqp_update_data_mat(work->solver, OSQP_NULL, OSQP_NULL, 0, Ax_new,
                              Ax_new_idx, A_new_n);
}

c_int osqp_update_P_A(ApolloOSQPWorkspace *work, const c_float *Px_new,
                      const c_int *Px_new_idx, c_int P_new_n,
                      const c_float *Ax_new, const c_int *Ax_new_idx,
                      c_int A_new_n) {
  return osqp_update_data_mat(work->solver, Px_new, Px_new_idx, P_new_n,
                              Ax_new, Ax_new_idx, A_new_n);
}

namespace {
template <typename F>
c_int UpdateSetting(ApolloOSQPWorkspace *work, F f) {
  if (work == nullptr || work->solver == nullptr) {
    return OSQP_WORKSPACE_NOT_INIT_ERROR;
  }
  ::OSQPSettings native = *work->solver->settings;
  f(&native);
  return osqp_update_settings(work->solver, &native);
}
}  // namespace

c_int osqp_update_max_iter(ApolloOSQPWorkspace *work, c_int v) {
  return UpdateSetting(work, [v](::OSQPSettings *s) { s->max_iter = v; });
}

c_int osqp_update_eps_abs(ApolloOSQPWorkspace *work, c_float v) {
  return UpdateSetting(work, [v](::OSQPSettings *s) { s->eps_abs = v; });
}

c_int osqp_update_eps_rel(ApolloOSQPWorkspace *work, c_float v) {
  return UpdateSetting(work, [v](::OSQPSettings *s) { s->eps_rel = v; });
}

c_int osqp_update_polish(ApolloOSQPWorkspace *work, c_int v) {
  return UpdateSetting(work, [v](::OSQPSettings *s) { s->polishing = v; });
}

c_int osqp_update_verbose(ApolloOSQPWorkspace *work, c_int v) {
  return UpdateSetting(work, [v](::OSQPSettings *s) { s->verbose = v; });
}

c_int osqp_update_warm_start(ApolloOSQPWorkspace *work, c_int v) {
  return UpdateSetting(work, [v](::OSQPSettings *s) { s->warm_starting = v; });
}

c_int osqp_update_time_limit(ApolloOSQPWorkspace *work, c_float v) {
  return UpdateSetting(work, [v](::OSQPSettings *s) {
    s->time_limit = v > 0 ? v : OSQP_TIME_LIMIT;
  });
}
