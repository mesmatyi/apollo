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
 * The PROJ 4 API subset used by Apollo (proj_api.h), implemented on PROJ >= 6
 * (proj.h) for the ROS 2 port. PROJ 8 removed proj_api.h; Ubuntu 24.04 and
 * conda-forge ship PROJ 9. Used by modules/map (xml map loader),
 * modules/drivers/gnss and modules/localization/msf.
 *
 * Semantics follow PROJ 4: angles of lat/long systems are in radians, x is
 * longitude and y latitude; pj_transform() returns 0 on success.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RAD_TO_DEG 57.295779513082321
#define DEG_TO_RAD .017453292519943296

typedef struct ApolloProj4Handle *projPJ;
typedef void *projCtx;

projPJ pj_init_plus(const char *definition);
void pj_free(projPJ pj);
int pj_is_latlong(projPJ pj);
int pj_is_geocent(projPJ pj);
char *pj_get_def(projPJ pj, int options);
int pj_transform(projPJ src, projPJ dst, long point_count, int point_offset,
                 double *x, double *y, double *z);
int *pj_get_errno_ref(void);
const char *pj_strerrno(int err);

#ifdef __cplusplus
}
#endif
