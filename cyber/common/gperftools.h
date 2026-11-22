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
 * @file gperftools.h
 * @brief The gperftools CPU/heap profiler entry points, or no-ops.
 *
 * Profiling with gperftools is opt-in (-DCYBER_ENABLE_GPERFTOOLS=ON, which
 * links the library and defines APOLLO_HAS_GPERFTOOLS=1): having the headers
 * installed is not enough, and linking tcmalloc replaces the allocator of
 * the whole process.
 */

#ifndef CYBER_COMMON_GPERFTOOLS_H_
#define CYBER_COMMON_GPERFTOOLS_H_

#if defined(APOLLO_HAS_GPERFTOOLS) && APOLLO_HAS_GPERFTOOLS
#include "gperftools/heap-profiler.h"
#include "gperftools/profiler.h"
#else
#undef APOLLO_HAS_GPERFTOOLS
#define APOLLO_HAS_GPERFTOOLS 0
inline void ProfilerStart(const char*) {}
inline void ProfilerStop() {}
inline void HeapProfilerStart(const char*) {}
inline void HeapProfilerDump(const char*) {}
inline void HeapProfilerStop() {}
#endif

#endif  // CYBER_COMMON_GPERFTOOLS_H_
