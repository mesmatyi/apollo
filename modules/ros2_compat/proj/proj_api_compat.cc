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

// PROJ 4 API (proj_api.h) on PROJ >= 6, see include/proj_api.h.

#include "proj_api.h"

#include <proj.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct ApolloProj4Handle {
  std::string definition;
  PJ_CONTEXT *ctx = nullptr;
  PJ *crs = nullptr;
  PJ_TYPE type = PJ_TYPE_UNKNOWN;
  std::mutex mutex;
  std::map<const ApolloProj4Handle *, PJ *> transforms;  // to dst, cached
};

namespace {

thread_local int g_pj_errno = 0;

bool IsLatLong(PJ_TYPE type) {
  return type == PJ_TYPE_GEOGRAPHIC_CRS || type == PJ_TYPE_GEOGRAPHIC_2D_CRS ||
         type == PJ_TYPE_GEOGRAPHIC_3D_CRS;
}

PJ *TransformTo(ApolloProj4Handle *src, const ApolloProj4Handle *dst) {
  auto it = src->transforms.find(dst);
  if (it != src->transforms.end()) {
    return it->second;
  }
  PJ *raw = proj_create_crs_to_crs_from_pj(src->ctx, src->crs, dst->crs,
                                           nullptr, nullptr);
  PJ *norm = nullptr;
  if (raw != nullptr) {
    // PROJ 4 axis order: longitude/easting first.
    norm = proj_normalize_for_visualization(src->ctx, raw);
    proj_destroy(raw);
  }
  src->transforms[dst] = norm;
  return norm;
}

}  // namespace

extern "C" {

projPJ pj_init_plus(const char *definition) {
  if (definition == nullptr) {
    g_pj_errno = -1;
    return nullptr;
  }
  auto *pj = new ApolloProj4Handle();
  pj->definition = definition;
  pj->ctx = proj_context_create();
  std::string def = definition;
  if (def.find("+type=crs") == std::string::npos) {
    def += " +type=crs";
  }
  pj->crs = proj_create(pj->ctx, def.c_str());
  if (pj->crs == nullptr) {
    g_pj_errno = proj_context_errno(pj->ctx);
    proj_context_destroy(pj->ctx);
    delete pj;
    return nullptr;
  }
  pj->type = proj_get_type(pj->crs);
  return pj;
}

void pj_free(projPJ pj) {
  if (pj == nullptr) {
    return;
  }
  for (auto &kv : pj->transforms) {
    if (kv.second != nullptr) {
      proj_destroy(kv.second);
    }
  }
  proj_destroy(pj->crs);
  proj_context_destroy(pj->ctx);
  delete pj;
}

int pj_is_latlong(projPJ pj) { return pj != nullptr && IsLatLong(pj->type); }

int pj_is_geocent(projPJ pj) {
  return pj != nullptr && pj->type == PJ_TYPE_GEOCENTRIC_CRS;
}

char *pj_get_def(projPJ pj, int /*options*/) {
  if (pj == nullptr) {
    return nullptr;
  }
  char *out = static_cast<char *>(std::malloc(pj->definition.size() + 1));
  std::memcpy(out, pj->definition.c_str(), pj->definition.size() + 1);
  return out;
}

int pj_transform(projPJ src, projPJ dst, long point_count, int point_offset,
                 double *x, double *y, double *z) {
  if (src == nullptr || dst == nullptr || x == nullptr || y == nullptr) {
    return g_pj_errno = -1;
  }
  if (point_count <= 0) {
    return 0;
  }
  const int offset = point_offset > 0 ? point_offset : 1;
  std::lock_guard<std::mutex> lock(src->mutex);
  PJ *P = TransformTo(src, dst);
  if (P == nullptr) {
    return g_pj_errno = proj_context_errno(src->ctx) ? proj_context_errno(src->ctx) : -1;
  }
  const bool src_latlong = IsLatLong(src->type);
  const bool dst_latlong = IsLatLong(dst->type);
  for (long k = 0; k < point_count; ++k) {
    const long i = k * offset;
    PJ_COORD c = proj_coord(x[i], y[i], z != nullptr ? z[i] : 0.0, 0.0);
    if (src_latlong) {  // PROJ 4: radians, PROJ >= 6: degrees
      c.xyzt.x *= RAD_TO_DEG;
      c.xyzt.y *= RAD_TO_DEG;
    }
    proj_errno_reset(P);
    PJ_COORD r = proj_trans(P, PJ_FWD, c);
    const int err = proj_errno(P);
    if (err != 0 || r.xyzt.x == HUGE_VAL) {
      return g_pj_errno = (err != 0 ? err : -1);
    }
    if (dst_latlong) {
      r.xyzt.x *= DEG_TO_RAD;
      r.xyzt.y *= DEG_TO_RAD;
    }
    x[i] = r.xyzt.x;
    y[i] = r.xyzt.y;
    if (z != nullptr) {
      z[i] = r.xyzt.z;
    }
  }
  return 0;
}

int *pj_get_errno_ref(void) { return &g_pj_errno; }

const char *pj_strerrno(int err) { return proj_errno_string(err); }

}  // extern "C"
