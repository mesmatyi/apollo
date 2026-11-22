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

// <boost/detail/endian.hpp> was removed in Boost 1.73; PCL 1.8's headers
// (pcl/PCLPointCloud2.h, pcl/io/...) still include it. Only on the include
// path of targets that use PCL (ROS 2 port, see ApolloExternalDeps.cmake).
#pragma once

#include <boost/predef/other/endian.h>

#if BOOST_ENDIAN_BIG_BYTE
#define BOOST_BIG_ENDIAN
#define BOOST_BYTE_ORDER 4321
#elif BOOST_ENDIAN_LITTLE_BYTE
#define BOOST_LITTLE_ENDIAN
#define BOOST_BYTE_ORDER 1234
#endif
