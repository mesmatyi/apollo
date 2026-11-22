/* Copyright 2026 The Apollo Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
=========================================================================*/
#include "modules/map/hdmap/adapter/lanelet2/lanelet2_projector.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#define ACCEPT_USE_OF_DEPRECATED_PROJ_API_H
#include <proj_api.h>

namespace apollo {
namespace hdmap {
namespace adapter {
namespace lanelet2 {

namespace {

constexpr char kWgs84[] = "+proj=longlat +datum=WGS84 +no_defs";

std::string Format(const char* format, double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), format, value);
  return buffer;
}

}  // namespace

Lanelet2Projector::~Lanelet2Projector() { Reset(); }

void Lanelet2Projector::Reset() {
  if (source_ != nullptr) {
    pj_free(static_cast<projPJ>(source_));
    source_ = nullptr;
  }
  if (target_ != nullptr) {
    pj_free(static_cast<projPJ>(target_));
    target_ = nullptr;
  }
}

int Lanelet2Projector::UtmZone(double longitude) {
  const int zone = static_cast<int>(std::floor((longitude + 180.0) / 6.0)) + 1;
  return std::max(1, std::min(60, zone));
}

bool Lanelet2Projector::Init(const Lanelet2Options& options,
                             double reference_lat, double reference_lon,
                             std::string* error) {
  Reset();
  local_ = false;
  utm_zone_ = 0;
  proj_string_.clear();

  if (options.projection == "local") {
    local_ = true;
    return true;
  }

  if (options.projection == "utm") {
    const double lat = options.has_origin ? options.origin_lat : reference_lat;
    const double lon = options.has_origin ? options.origin_lon : reference_lon;
    utm_zone_ = options.utm_zone > 0 ? options.utm_zone : UtmZone(lon);
    if (utm_zone_ < 1 || utm_zone_ > 60) {
      *error = "invalid UTM zone " + std::to_string(utm_zone_);
      return false;
    }
    const bool south = lat < 0.0;
    proj_string_ = "+proj=utm +zone=" + std::to_string(utm_zone_) +
                   (south ? " +south" : "") +
                   " +ellps=WGS84 +datum=WGS84 +units=m +no_defs";
    if (options.has_origin) {
      // UTM is a transverse Mercator with a false easting/northing; shifting
      // those moves the origin and keeps the frame expressible in PROJ.
      Lanelet2Options plain = options;
      plain.has_origin = false;
      plain.utm_zone = utm_zone_;
      Lanelet2Projector utm;
      if (!utm.Init(plain, lat, lon, error)) {
        return false;
      }
      std::vector<double> x = {options.origin_lon};
      std::vector<double> y = {options.origin_lat};
      if (!utm.Forward(&x, &y, error)) {
        return false;
      }
      const double central_meridian = utm_zone_ * 6.0 - 183.0;
      proj_string_ =
          "+proj=tmerc +lat_0=0 +lon_0=" + Format("%.1f", central_meridian) +
          " +k=0.9996 +x_0=" + Format("%.4f", 500000.0 - x[0]) +
          " +y_0=" + Format("%.4f", (south ? 10000000.0 : 0.0) - y[0]) +
          " +ellps=WGS84 +units=m +no_defs";
    }
  } else if (!options.projection.empty() && options.projection[0] == '+') {
    proj_string_ = options.projection;
  } else {
    *error = "unknown projection \"" + options.projection +
             "\" (expected utm, local or a PROJ string)";
    return false;
  }

  source_ = pj_init_plus(kWgs84);
  target_ = pj_init_plus(proj_string_.c_str());
  if (source_ == nullptr || target_ == nullptr) {
    *error = "PROJ cannot initialize \"" + proj_string_ + "\"";
    Reset();
    return false;
  }
  return true;
}

bool Lanelet2Projector::Forward(std::vector<double>* x, std::vector<double>* y,
                                std::string* error) const {
  if (local_ || source_ == nullptr || target_ == nullptr) {
    *error = "the projector is not initialized for lat/lon input";
    return false;
  }
  if (x->size() != y->size()) {
    *error = "coordinate arrays differ in size";
    return false;
  }
  if (x->empty()) {
    return true;
  }
  const projPJ source = static_cast<projPJ>(source_);
  const projPJ target = static_cast<projPJ>(target_);
  std::vector<double> px(x->size());
  std::vector<double> py(x->size());
  std::vector<double> pz(x->size(), 0.0);
  for (size_t i = 0; i < x->size(); ++i) {
    px[i] = (*x)[i] * DEG_TO_RAD;
    py[i] = (*y)[i] * DEG_TO_RAD;
  }
  if (pj_transform(source, target, static_cast<long>(px.size()), 1,  // NOLINT
                   px.data(), py.data(), pz.data()) == 0) {
    x->swap(px);
    y->swap(py);
    return true;
  }
  // One bad coordinate fails the whole batch: project point by point and
  // mark what PROJ rejects.
  size_t projected = 0;
  for (size_t i = 0; i < x->size(); ++i) {
    double vx = (*x)[i] * DEG_TO_RAD;
    double vy = (*y)[i] * DEG_TO_RAD;
    double vz = 0.0;
    if (pj_transform(source, target, 1, 1, &vx, &vy, &vz) == 0 &&
        std::isfinite(vx) && std::isfinite(vy)) {
      (*x)[i] = vx;
      (*y)[i] = vy;
      ++projected;
    } else {
      (*x)[i] = std::nan("");
      (*y)[i] = std::nan("");
    }
  }
  if (projected == 0) {
    *error =
        "PROJ failed to project the map nodes with \"" + proj_string_ + "\"";
    return false;
  }
  return true;
}

}  // namespace lanelet2
}  // namespace adapter
}  // namespace hdmap
}  // namespace apollo
