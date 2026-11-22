#!/usr/bin/env python3
###############################################################################
# Copyright 2026 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###############################################################################
"""Helpers shared by the Lanelet2 map tools: Apollo map I/O, polylines and a
transverse Mercator projection that needs no external library."""

import math
import os
import re
import shutil
import subprocess

from google.protobuf import text_format

from modules.common_msgs.map_msgs import map_pb2


def load_apollo_map(path):
    """Loads a base map from a file (.bin/.txt) or a map directory."""
    if os.path.isdir(path):
        for name in ('base_map.bin', 'base_map.txt'):
            candidate = os.path.join(path, name)
            if os.path.exists(candidate):
                path = candidate
                break
        else:
            raise FileNotFoundError('no base_map.bin/.txt in %s' % path)
    hdmap = map_pb2.Map()
    if path.endswith('.txt'):
        with open(path) as f:
            text_format.Parse(f.read(), hdmap)
    else:
        with open(path, 'rb') as f:
            hdmap.ParseFromString(f.read())
    return hdmap


def curve_points(curve):
    """All points of an apollo.hdmap.Curve as (x, y, z) tuples."""
    points = []
    for segment in curve.segment:
        for p in segment.line_segment.point:
            point = (p.x, p.y, p.z)
            if not points or point != points[-1]:
                points.append(point)
    return points


def polyline_length(points):
    return sum(math.hypot(b[0] - a[0], b[1] - a[1])
               for a, b in zip(points, points[1:]))


def point_at(points, s):
    """Point at arc length s of a polyline (clamped)."""
    travelled = 0.0
    for a, b in zip(points, points[1:]):
        seg = math.hypot(b[0] - a[0], b[1] - a[1])
        if seg > 0.0 and travelled + seg >= s:
            r = max(0.0, s - travelled) / seg
            return tuple(a[i] + r * (b[i] - a[i]) for i in range(len(a)))
        travelled += seg
    return points[-1]


def resample(points, count):
    """`count` points at equal arc length steps."""
    length = polyline_length(points)
    if count < 2 or length <= 0.0:
        return [points[0], points[-1]]
    return [point_at(points, length * i / (count - 1)) for i in range(count)]


def distance_to_polyline(point, points):
    best = float('inf')
    for a, b in zip(points, points[1:]):
        dx, dy = b[0] - a[0], b[1] - a[1]
        seg = dx * dx + dy * dy
        t = 0.0
        if seg > 0.0:
            t = ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / seg
            t = max(0.0, min(1.0, t))
        best = min(best, math.hypot(a[0] + t * dx - point[0],
                                    a[1] + t * dy - point[1]))
    if len(points) == 1:
        best = math.hypot(points[0][0] - point[0], points[0][1] - point[1])
    return best


def max_deviation(points_a, points_b):
    """Symmetric maximum distance between two polylines (at their vertices
    and 1 m samples)."""
    def one_way(src, dst):
        samples = list(src)
        length = polyline_length(src)
        n = int(length)
        samples += [point_at(src, length * i / max(n, 1)) for i in range(n + 1)]
        return max(distance_to_polyline(p, dst) for p in samples)
    return max(one_way(points_a, points_b), one_way(points_b, points_a))


###############################################################################
# Projection


class Projection(object):
    """Map frame <-> WGS84 for the PROJ strings Apollo maps use.

    +proj=utm and +proj=tmerc on the WGS84 ellipsoid are computed here (Krueger
    series, sub-millimeter inside a UTM zone). Anything else goes through
    pyproj or the cs2cs tool of PROJ if one of them is installed.
    """

    A = 6378137.0
    F = 1.0 / 298.257223563

    def __init__(self, proj_string):
        self.proj_string = proj_string.strip()
        params = {}
        for token in self.proj_string.split():
            if token.startswith('+'):
                key, _, value = token[1:].partition('=')
                params[key] = value
        self.params = params
        self.kind = params.get('proj', '')
        self._pyproj = None
        self._cs2cs = None
        ellps_ok = (params.get('ellps', 'WGS84') == 'WGS84' and
                    params.get('datum', 'WGS84') == 'WGS84')
        if self.kind == 'utm' and ellps_ok:
            zone = int(params['zone'])
            self.lon0 = math.radians(zone * 6 - 183)
            self.lat0 = 0.0
            self.k0 = 0.9996
            self.x0 = 500000.0
            self.y0 = 10000000.0 if 'south' in params else 0.0
        elif self.kind == 'tmerc' and ellps_ok:
            self.lon0 = math.radians(float(params.get('lon_0', 0.0)))
            self.lat0 = math.radians(float(params.get('lat_0', 0.0)))
            self.k0 = float(params.get('k', params.get('k_0', 1.0)))
            self.x0 = float(params.get('x_0', 0.0))
            self.y0 = float(params.get('y_0', 0.0))
        else:
            self.kind = 'external'
            try:
                import pyproj  # pylint: disable=import-outside-toplevel
                self._pyproj = pyproj.Transformer.from_crs(
                    pyproj.CRS.from_proj4(self.proj_string), 'EPSG:4326',
                    always_xy=True)
            except Exception:  # pylint: disable=broad-except
                self._cs2cs = shutil.which('cs2cs')
                if not self._cs2cs:
                    raise ValueError(
                        'projection "%s" needs pyproj or cs2cs' %
                        self.proj_string)
        if self.kind != 'external':
            # Krueger series in the third flattening n, to n^4.
            n = self.F / (2.0 - self.F)
            n2, n3, n4 = n * n, n ** 3, n ** 4
            self._e = 2 * math.sqrt(n) / (1 + n)
            self._a_hat = self.A / (1 + n) * (1 + n2 / 4 + n4 / 64)
            self._alpha = (
                n / 2 - 2 * n2 / 3 + 5 * n3 / 16 + 41 * n4 / 180,
                13 * n2 / 48 - 3 * n3 / 5 + 557 * n4 / 1440,
                61 * n3 / 240 - 103 * n4 / 140,
                49561 * n4 / 161280)
            self._beta = (
                n / 2 - 2 * n2 / 3 + 37 * n3 / 96 - n4 / 360,
                n2 / 48 + n3 / 15 - 437 * n4 / 1440,
                17 * n3 / 480 - 37 * n4 / 840,
                4397 * n4 / 161280)
            self._m0 = self._northing(self.lat0, 0.0)[1]

    def _northing(self, lat, dlon):
        """(easting, northing) on the scaled sphere, without false origin."""
        e = self._e
        t = math.sinh(math.atanh(math.sin(lat)) -
                      e * math.atanh(e * math.sin(lat)))
        xi = math.atan2(t, math.cos(dlon))
        eta = math.atanh(math.sin(dlon) / math.sqrt(1 + t * t))
        x = eta
        y = xi
        for j, a in enumerate(self._alpha, start=1):
            x += a * math.cos(2 * j * xi) * math.sinh(2 * j * eta)
            y += a * math.sin(2 * j * xi) * math.cosh(2 * j * eta)
        return self.k0 * self._a_hat * x, self.k0 * self._a_hat * y

    def forward(self, lat, lon):
        """Degrees -> (x, y)."""
        if self.kind == 'external':
            raise NotImplementedError('forward projection of ' +
                                      self.proj_string)
        x, y = self._northing(math.radians(lat), math.radians(lon) - self.lon0)
        return x + self.x0, y - self._m0 + self.y0

    def inverse(self, x, y):
        """(x, y) -> (lat, lon) in degrees."""
        if self.kind == 'external':
            return self.inverse_many([(x, y)])[0]
        xi = (y - self.y0 + self._m0) / (self.k0 * self._a_hat)
        eta = (x - self.x0) / (self.k0 * self._a_hat)
        xi_p = xi
        eta_p = eta
        for j, b in enumerate(self._beta, start=1):
            xi_p -= b * math.sin(2 * j * xi) * math.cosh(2 * j * eta)
            eta_p -= b * math.cos(2 * j * xi) * math.sinh(2 * j * eta)
        # tan of the conformal latitude, then Newton for the geodetic one.
        tau_c = math.sin(xi_p) / math.hypot(math.sinh(eta_p), math.cos(xi_p))
        e = self._e
        tau = tau_c
        for _ in range(8):
            sigma = math.sinh(e * math.atanh(e * tau / math.hypot(1.0, tau)))
            f = tau * math.hypot(1.0, sigma) - sigma * math.hypot(1.0, tau)
            dtau = ((tau_c - f) / math.hypot(1.0, f) *
                    (1 + (1 - e * e) * tau * tau) /
                    ((1 - e * e) * math.hypot(1.0, tau)))
            tau += dtau
            if abs(dtau) < 1e-14 * max(1.0, abs(tau)):
                break
        lat = math.atan(tau)
        lon = self.lon0 + math.atan2(math.sinh(eta_p), math.cos(xi_p))
        return math.degrees(lat), math.degrees(lon)

    def inverse_many(self, points):
        if self.kind != 'external':
            return [self.inverse(x, y) for x, y in points]
        if self._pyproj is not None:
            out = []
            for x, y in points:
                lon, lat = self._pyproj.transform(x, y)
                out.append((lat, lon))
            return out
        text = ''.join('%.6f %.6f\n' % (x, y) for x, y in points)
        result = subprocess.run(
            [self._cs2cs, '-f', '%.12f'] + self.proj_string.split() +
            ['+to', '+proj=longlat', '+datum=WGS84', '+no_defs'],
            input=text, capture_output=True, text=True, check=True)
        out = []
        for line in result.stdout.splitlines():
            fields = line.split()
            out.append((float(fields[1]), float(fields[0])))
        return out


def utm_proj_string(zone, south=False):
    return ('+proj=utm +zone=%d%s +ellps=WGS84 +datum=WGS84 +units=m +no_defs'
            % (zone, ' +south' if south else ''))


def usable_proj_string(text):
    """True if a Map.header.projection.proj can be used as is (the demo map
    of Apollo carries a template with braces instead)."""
    return bool(text) and re.search(r'[{}]', text) is None and \
        '+proj=' in text
