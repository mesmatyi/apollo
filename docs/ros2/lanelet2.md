# Lanelet2 maps in Apollo

Apollo reads [Lanelet2](https://github.com/fzi-forschungszentrum-informatik/Lanelet2)
maps, the OSM XML (`.osm`) map format of Lanelet2 and Autoware, and writes
them:

* **Load.** `HDMapImpl::LoadMapFromFile()` hands every `*.osm` file to
  `adapter::Lanelet2Adapter` (`modules/map/hdmap/adapter/lanelet2_adapter.h`),
  next to the OpenDRIVE adapter for `*.xml`. The adapter builds a complete
  `apollo.hdmap.Map`: lanes, roads, junctions, signals, stop and yield signs,
  crosswalks and the overlaps between them.
* **Convert.** `tools/ros2/lanelet2/lanelet2_to_apollo.py` turns an `.osm` file
  into an Apollo map directory (base, routing and sim map) with one command.
* **Export.** `tools/ros2/lanelet2/apollo_to_lanelet2.py` writes an Apollo base
  map as a Lanelet2 map that the Lanelet2 library loads and routes on.

Nothing here needs the Lanelet2 library: the file is parsed with tinyxml2 and
projected with PROJ, both already used by the OpenDRIVE adapter. The library
is only used by the optional cross-check tool (`pip install lanelet2`).

## Quick start

```bash
source scripts/ros2/setup.bash        # ROS 2, the apollo package, APOLLO_ROOT

# Lanelet2 -> Apollo map directory
tools/ros2/lanelet2/lanelet2_to_apollo.py --osm lanelet2_map.osm \
    --output_dir /data/maps/my_map
#   writes base_map.{bin,txt}, routing_map.{bin,txt}, sim_map.{bin,txt},
#   default_end_way_point.txt, lanelet2_conversion.json (report)
# then run Apollo with --map_dir=/data/maps/my_map, e.g. set in
# modules/common/data/global_flagfile.txt

# Apollo -> Lanelet2
tools/ros2/lanelet2/apollo_to_lanelet2.py --map modules/map/data/borregas_ave \
    --output borregas_ave.osm
```

`lanelet2_to_apollo.py` runs `lanelet2_map_converter` (new, in
`modules/map/tools`), `topo_creator` and `sim_map_generator`, which it finds in
the installed `apollo` package (`--bin_dir <build tree>` to use others). Flags
starting with `--lanelet2_` are passed on to the converter, for example
`--lanelet2_projection=local`.

## Two ways to use a Lanelet2 map

**A. Converted directory (recommended).** Run `lanelet2_to_apollo.py` once and
point `--map_dir` at the result. Every module and tool works as with any
other Apollo map, and no module converts anything at start-up.

**B. The `.osm` as base map.** The default of `--base_map_filename` is now
`base_map.bin|base_map.xml|base_map.txt|base_map.osm|lanelet2_map.osm`, so a
directory that holds only `lanelet2_map.osm` (the name Autoware uses) or
`base_map.osm` is a valid `--map_dir` for everything that loads the HD map:
each process converts the file when it loads the map (the official example
map with 371 lanelets loads in 0.21 s this way, against 0.16 s from its
converted `base_map.bin`). Conversion is deterministic, so all processes see
the same lane ids. Two files cannot come from the `.osm` at run time and
must be generated once:

```bash
tools/ros2/lanelet2/lanelet2_to_apollo.py --osm /data/maps/town/lanelet2_map.osm --in_place
#   writes routing_map.*, sim_map.* and default_end_way_point.txt next to it
```

Routing needs `routing_map.bin` (`topo_creator` itself does not read `.osm`
files; the script feeds it a converted base map), Dreamview needs
`sim_map.bin`. With workflow B the `--lanelet2_*` flags must be the same in
every process, for example in `modules/common/data/global_flagfile.txt`.

## Coordinates

| `--lanelet2_projection` | map coordinates | `Map.header.projection` |
|---|---|---|
| `utm` (default) | latitude/longitude of the nodes projected to UTM (WGS84). The zone is `--lanelet2_utm_zone`, or the zone of the first node of the file. | `+proj=utm +zone=N ...` |
| `utm` with `--lanelet2_origin=lat,lon` | UTM minus the UTM coordinates of the origin, i.e. the frame of `lanelet::projection::UtmProjector(Origin(lat, lon))` | the equivalent `+proj=tmerc ... +x_0 +y_0` |
| `local` | the `local_x` / `local_y` node tags (Autoware maps without geo reference) | empty |
| `+proj=...` | the nodes projected with this PROJ string | the string |

The `ele` tag becomes `z`. Apollo's localization must use the same frame; for
UTM that means `--local_utm_zone_id` equals the zone the converter reports.
Autoware's MGRS, `LocalCartesianUTM` and `TransverseMercator` projector types
are not read from `map_projector_info.yaml`; pass the equivalent
`--lanelet2_origin` or PROJ string.

## Lanelet2 → Apollo

### Lanelets become lanes

A relation `type=lanelet` with exactly one `left` and one `right` way becomes
one lane whose id is the lanelet id (`45070`).

| Lanelet2 | Apollo |
|---|---|
| `left` / `right` ways | `left_boundary` / `right_boundary` curves with the points of the ways. A way stored against the driving direction is turned around exactly like `lanelet::geometry::align()` does. |
| `centerline` way | `central_curve` (made to run in driving direction; points are added so that none are further apart than `--lanelet2_sample_distance`, the vertices are kept) |
| no `centerline` | `central_curve` = mean of both bounds sampled at equal normalized arc length, about every `--lanelet2_sample_distance` (0.5 m). This is how Autoware computes its fine centerline; Lanelet2's own centerline has other points (see results). |
| – | `left_sample` / `right_sample`: distance of every centerline point to the bounds; `left_road_sample` / `right_road_sample`: distance to the edges of the road; `length` |
| `subtype`, `participant:*` | lane `type`, see below |
| `one_way=no` | a second lane `<id>_inv` for the other direction, linked by `self_reverse_lane_id` (`--nolanelet2_bidirectional_lanes`: one lane with `direction: BIDIRECTION`) |
| `speed_limit` regulatory element, `speed_limit` tag, `location` | `speed_limit` in m/s, in this order of precedence (the order Lanelet2 1.2 applies). Tag values as in Lanelet2: `50` (km/h), `50 km/h`, `50kmh`, `13.9 m/s`, `10 mps`, `30 mph`. Signs: `de274-60`, `de274`, `de274_1`, `de310` or a plain velocity. Defaults: urban 50 km/h, nonurban 100, highway 130 (flags `--lanelet2_default_speed_*`), play street 7, `location=private` 20, bicycle lanes 20, walkways 5. |
| `turn_direction` (Autoware: `left`, `right`, `straight`) | `turn`; a tagged turn whose heading changes by more than 150° is a `U_TURN`. Without the tag, lanes inside a junction get their turn from the heading change (more than 30°: left/right, more than 150°: u-turn); lanes outside junctions are `NO_TURN` even when the road bends. |

Which lanelets are driving lanes is decided as in Lanelet2's traffic rules for
the participant `--lanelet2_participant` (default `vehicle`):

| lanelet | lane type |
|---|---|
| passable for the participant: `subtype` `road`, `highway`, `play_street`, `exit`, none; `bus_lane` for `vehicle:bus`/`vehicle:taxi`/`vehicle:emergency`; or `participant:<...>=yes` | `CITY_DRIVING` |
| `crosswalk` | no lane, a `Crosswalk` |
| `road_shoulder` (Autoware), `emergency_lane` | `SHOULDER` |
| passable for pedestrians (`walkway`, `shared_walkway`, `stairs`, `participant:pedestrian=yes`) | `SIDEWALK` |
| passable for bicycles only (`bicycle_lane`, `participant:bicycle=yes`) | `BIKING` |
| anything else (`rail`, a `bus_lane` the participant may not use, `participant:vehicle=no`, unknown subtypes) | not converted, counted in the report |

`--nolanelet2_non_driving_lanes` drops everything but driving lanes.

### Topology

Lanelet2 stores no topology; it follows from shared primitives, and the
adapter applies the same rules:

* **Successor / predecessor:** the last nodes of both bounds of A are the
  first nodes of both bounds of B (`lanelet::geometry::follows`).
* **Left / right neighbor (same direction):** the left bound of A is the right
  bound of B, in the same direction.
* **Left / right reverse neighbor:** A and B use the same bound in opposite
  directions.
* Two lanelets on the same two bounds in opposite directions are each other's
  `self_reverse_lane_id`.

Bounds are compared by their node ids. `--lanelet2_node_merge_tolerance=0.01`
treats nodes closer than 1 cm as one, for maps with duplicated nodes.
Successors are linked between lanes of the same type only. Lanes of different
types (a road next to a bicycle lane or walkway) become neighbors only across
a bound that may be crossed (dashed): Apollo's planning borrows neighbor
lanes and must not do so across a curb or solid line.

### Boundary types

| way `type` (`subtype`) | `LaneBoundaryType` |
|---|---|
| `line_thin`, `line_thick`: `dashed` | `DOTTED_WHITE` (`color=yellow`: `DOTTED_YELLOW`) |
| ... `solid`, no subtype | `SOLID_WHITE` / `SOLID_YELLOW` |
| ... `solid_solid` | `DOUBLE_YELLOW` |
| ... `dashed_solid`, `solid_dashed` | dotted for the lane on the dashed side, solid for the other one |
| `curbstone`, `road_border`, `guard_rail`, `wall`, `fence`, `jersey_barrier`, `gate`, `lift_gate`, `door`, `rail` | `CURB` |
| `virtual` | `UNKNOWN`, `virtual: true` |
| `zebra_marking`, `pedestrian_marking`, `bike_marking`, `keepout`, `stop_line`, `zig-zag`, ... | `SOLID_WHITE` |
| unknown or missing type | `UNKNOWN`, counted in the report |
| `lane_change=yes/no`, `lane_change:left`, `lane_change:right` | override as in Lanelet2: dotted where the change is allowed |

`topo_creator` creates lane change edges across dotted boundaries only, so
Apollo's routing graph has a lane change exactly where Lanelet2's has one.

### Roads

Lanes connected by same-direction neighbor relations form one `Road` with one
`RoadSection` (lanes from left to right, `LEFT_BOUNDARY` and `RIGHT_BOUNDARY`
edges from the outermost bounds). Every lane belongs to a road; the two
directions of a street are two roads. A road whose lanes all lie in the same
junction carries its `junction_id`; a `highway` lanelet makes it a `HIGHWAY`.

### Regulatory elements, crosswalks, junctions

| Lanelet2 | Apollo |
|---|---|
| `traffic_light` (`refers`: lights, `ref_line`: stop line) | one `Signal` per light way (`signal_<way id>`) with boundary (the way is the lower edge, `height` its size), type and subsignals from `subtype` (`red_yellow_green`, ...), the stop line, and a lane/signal overlap for every lanelet that references the element |
| `traffic_sign` whose sign is `stop_sign`, `stop`, `de206`, `usR1-1` | `StopSign` (`stop_sign_<relation id>`) + overlaps |
| `traffic_sign` `yield`, `de205`, `usR1-2`; `right_of_way` (its `yield` lanelets) | `YieldSign` (`yield_<relation id>`) + overlaps |
| `all_way_stop` | one `StopSign` of type `ALL_WAY` per `yield` lanelet |
| `road_marking` referring to a `stop_line` (Autoware) | `StopSign` |
| `speed_limit` | speed limit of the lanes |
| lanelet `subtype=crosswalk` | `Crosswalk` (`crosswalk_<lanelet id>`), polygon from both bounds, overlaps with the lanes it lies on |
| way `type=intersection_area` and lanelet tag `intersection_area=<id>` (Autoware) | `Junction` (`junction_<way id>`) |
| lanes that cross, without `intersection_area` | inferred `Junction`, see below |

*Stop position.* The overlap of a lane with a signal or sign is where its
centerline crosses the stop line (or the point nearest to a stop line that
ends within 5 m of it). Without `ref_line` it is the end of the lanelet, as
Lanelet2 defines, and the stop line is the line between the last points of
the bounds.

*Overlaps.* Lane/crosswalk: the part of the lane whose cross section (left to
right bound) touches the polygon. Lane/junction: the part of the centerline
inside the polygon; a lane that is inside for 80 % of its length gets the
`junction_id`. Lane/lane: two lanes overlap if the centerline of one runs at
least 0.2 m inside the other; neighbors, consecutive lanes and the two
directions of a lanelet never do. `is_merge` marks pairs with a common
successor. Signals and signs also overlap the junction their lane leads into
(Apollo finds the other stop signs of a junction that way).

*Inferred junctions.* Driving lanes that overlap form groups. A group in
which at least two lanes really cross (neither a common predecessor nor a
common successor) becomes a junction `junction_<smallest lanelet id>`; forks
and merges alone do not. The polygon is the convex hull of the member lanes:
the whole lane if it is in conflict over more than half of its length (a
connector), otherwise only its conflict zone. `--nolanelet2_infer_junctions`
turns this off. Limits: the hull of a non-convex intersection also covers
what lies between its arms, and an intersection whose turning lanelets never
cross each other (a T-junction of one-way roads, a roundabout) gets no
junction.

### Robustness

The adapter never aborts on bad input. Lanelets without exactly one left and
one right way with two or more points are skipped; unknown line types,
lanelet subtypes, relation types and regulatory elements are counted. All of
it is in the conversion report (`Lanelet2Report`, written by the converter as
`lanelet2_conversion.json` and logged when a map is loaded). `LoadData()`
returns false only if the file cannot be read, is not an OSM document, cannot
be projected or has no usable lanelet.

### Flags

| flag | default | meaning |
|---|---|---|
| `--lanelet2_projection` | `utm` | `utm`, `local` or a PROJ string |
| `--lanelet2_utm_zone` | `0` | UTM zone; 0: zone of the first node |
| `--lanelet2_origin` | empty | `lat,lon`: shift the UTM frame to this point |
| `--lanelet2_participant` | `vehicle` | Lanelet2 participant the map is converted for |
| `--lanelet2_bidirectional_lanes` | `true` | second lane `<id>_inv` for two-way lanelets |
| `--lanelet2_non_driving_lanes` | `true` | keep bicycle lanes, walkways, shoulders |
| `--lanelet2_infer_junctions` | `true` | junctions where lanes cross |
| `--lanelet2_use_apollo_tags` | `true` | honor `apollo:id` and `apollo:turn` (written by the exporter) |
| `--lanelet2_sample_distance` | `0.5` | point spacing of computed centerlines (m) |
| `--lanelet2_node_merge_tolerance` | `0` | treat nodes closer than this as one (m) |
| `--lanelet2_default_speed_urban` / `_nonurban` / `_highway` | 50 / 100 / 130 | default speed limits (km/h) |

## Apollo → Lanelet2

`apollo_to_lanelet2.py` writes one lanelet per lane. Apollo stores topology
explicitly and Lanelet2 derives it from shared primitives, so the exporter
*builds* the primitives from the topology:

* neighbor lanes get **one** way as their common bound,
* a lane and its successors share the end nodes of both bounds.

Where the boundaries of neighbor lanes coincide in the Apollo map (maps made
with Apollo's tools) nothing moves. Where they do not (maps exported by LGSVL
overlap their lanes at lane splits and leave gaps between directions), the
shared bound is the mean of both boundaries, junction lanes follow the end
points of the lanes outside junctions, and the report lists how far each
bound moved.

| Apollo | Lanelet2 |
|---|---|
| lane | relation `type=lanelet`, `subtype` `road` (`highway` for `HIGHWAY` roads), `bicycle_lane`, `walkway`, `road_shoulder`; `location` (`--location`, default `urban`); `one_way`; `apollo:id` |
| `central_curve` | `centerline` way (`--centerline none`: omitted, Lanelet2 computes it) |
| `speed_limit` | `speed_limit` in km/h |
| `turn` | `turn_direction` `left` / `right`, `straight` for junction lanes; u-turn: `left` plus `apollo:turn=U_TURN` |
| boundary types | `line_thin` `dashed` / `solid` / `solid_solid` (+ `color=yellow`), `dashed_solid` / `solid_dashed` when the two lanes differ, `curbstone`, `virtual` |
| `Signal` | `traffic_light` regulatory element: light way (lower edge of the boundary, `height`, `subtype` from the number of subsignals), stop line as `ref_line`; referenced by the lanelets of its lane overlaps |
| `StopSign`, `YieldSign` | `traffic_sign` regulatory element (`stop_sign` / `de205`) with the stop line as `ref_line`. Apollo does not know where the sign stands; a small sign way is placed at the end of the stop line and tagged `apollo:synthetic=yes`. |
| `Crosswalk` | lanelet `subtype=crosswalk` (the two long sides of a quadrilateral are the bounds) |
| `Junction` | way `type=intersection_area`, `area=yes`; its lanelets carry `intersection_area=<id>` |
| lane, signal, sign, crosswalk and junction ids | tag `apollo:id`, which the adapter uses as id again |

Latitude and longitude come from the inverse of the projection in the map
header (`+proj=utm` and `+proj=tmerc` are computed in the script, anything
else through `pyproj` or `cs2cs`). Maps without a usable header, such as
`modules/map/data/demo`, need `--utm_zone N` or `--proj "..."`.

What Lanelet2 cannot express is reported, not hidden:

* A lane that continues into **two neighbor lanes** (a lane split) has only
  one Lanelet2 follower; the other successor stays reachable by a lane change.
* **Oncoming neighbors** whose left boundaries are more than
  `--max_reverse_neighbor_gap` (1 m) apart do not share a bound, so the
  relation is not in the file.
* Boundary types that change along a boundary become the type covering most
  of it.
* `ClearArea`, `SpeedBump`, `ParkingSpace`, `PNCJunction`, `RSU`, `Area` and
  `BarrierGate` objects are not exported.

## Tools

| file | purpose |
|---|---|
| `modules/map/tools/lanelet2_map_converter` | `--osm_file=... --output_dir=...`: `.osm` → `base_map.bin` + `.txt`, report with `--report_file` |
| `tools/ros2/lanelet2/lanelet2_to_apollo.py` | `.osm` → complete Apollo map directory, or `--in_place` |
| `tools/ros2/lanelet2/apollo_to_lanelet2.py` | Apollo base map → `.osm` |
| `tools/ros2/lanelet2/compare_maps.py` | compares two Apollo maps (round trip check) |
| `tools/ros2/lanelet2/check_with_lanelet2.py` | cross-checks a conversion in either direction against the Lanelet2 library |
| `tools/ros2/lanelet2/validate.py` | runs all of the checks below |
| `tools/ros2/lanelet2/run_route_closed_loop.py` | the closed-loop test of `tools/ros2/closed_loop` along a route of several lanes |
| `tools/ros2/lanelet2/make_test_maps.py` | writes the unit test maps in `modules/map/hdmap/adapter/lanelet2/testdata` |
| `tools/ros2/lanelet2/fetch_example_map.py` | finds or downloads the official Lanelet2 example map |

## Verification

All numbers below were measured on this branch (GCC 13, ROS 2 Jazzy, PROJ 9,
Lanelet2 1.2.3 from PyPI).

```bash
# unit tests (part of `ctest -L '^map$'`), in the build tree of package apollo
ninja map__lanelet2_adapter_test && ctest -R lanelet2
# everything else
pip install lanelet2            # only for the cross-checks
tools/ros2/lanelet2/validate.py --work_dir /tmp/lanelet2_validation --fuzz 200
```

**Unit tests.** `map__lanelet2_adapter_test`: 20 tests on five synthetic maps
(two-lane road with an inverted dashed divider, explicit centerline and speed
limits; a bend; a signalized intersection with stop sign, right of way and
crosswalk; an Autoware style local map; a malformed map) and generated ones
(21 line taggings, participants, duplicated nodes, `apollo:*` tags). They
assert lane counts, lengths, widths, boundary types, successor / predecessor
/ neighbor ids, roads, signals with stop positions, crosswalk ranges,
junctions, and that `HDMapImpl` and `HDMapUtil` (with `--map_dir`) serve the
`.osm` like a `.bin` base map. All pass. The three geo-referenced maps were
also compared with the Lanelet2 library (no mismatch).

**Official Lanelet2 example map** (`mapping_example.osm` of `lanelet2_maps`,
downloaded by `validate.py`, not part of this repository):

| | Lanelet2 library | Apollo adapter |
|---|---|---|
| lanelets | 371 | 371 read, 2 ignored (`rail`) |
| drivable for vehicles | 328, of which 60 two-way | 388 `CITY_DRIVING` lanes (328 + 60 `_inv`) |
| other | 14 `bicycle_lane`, 2 `walkway`, 17 `road` lanelets for bicycles and pedestrians only, 8 `crosswalk`, 2 `rail` | 14 `BIKING` and 19 `SIDEWALK` lanes, 8 crosswalks, 2 ignored |
| regulatory elements | 6 traffic lights, 2 right of way (`fallback`), 1 speed limit | 10 signals (one per light), 18 lane/signal overlaps; the fallback rules are ignored |
| routing graph: following edges | 378 | 378 forward edges (`topo_creator`) |
| routing graph: lane changes | 113 | 113 left/right edges |
| routing graph: adjacent, no lane change | 109 | 109 neighbor relations across non-dotted boundaries |
| junctions | – | 16 inferred, 310 roads, 290 overlaps |

Every drivable lanelet has its lane and vice versa; bound points agree within
3·10⁻⁹ m (projection and bound alignment), speed limits are equal, and the
successor, predecessor and neighbor ids of all 388 lanes equal Lanelet2's
`following`, `previous`, `left`/`right` and `adjacentLeft`/`adjacentRight`.
Stop positions of the 18 traffic light/lane pairs agree within 0.012 m. The
computed centerlines differ from Lanelet2's by 0.08 m (median), 0.59 m (95 %)
and at most 2.0 m, on lanelets whose two bounds differ a lot in length; the
adapter's centerlines are the better centered ones (difference between left
and right width: median 0.004 m against 0.049 m). The conversion reports no
warning; `lanelet2_to_apollo.py` produces the whole map directory in 0.6 s.

**Round trips** Apollo → Lanelet2 → Apollo of the maps in this repository.
The exported files load in the Lanelet2 library without errors.

| map | lanes | Lanelet2 routing graph of the export | back in Apollo |
|---|---|---|---|
| `modules/map/data/demo` | 1 | 1 lanelet, 1 regulatory element | length, centerline and width within 10⁻⁶ m, stop sign at the same position |
| same, `--centerline none` | 1 | 1 lanelet, 1 regulatory element | centerline recomputed by the adapter: within 0.153 m of the original, length within 0.079 m, half width within 0.062 m |
| `modules/map/data/borregas_ave` (LGSVL export) | 60 | 60 lanelets + 6 crosswalks, 17 regulatory elements; 60 of 62 successors are followers, all 62 are routable; 28 of 28 lane changes | 60 lanes, centerlines within 10⁻⁶ m; 60 of 62 successors, 28 of 28 forward neighbors, 6 of 18 reverse neighbors; 15 signals, 2 stop signs, 6 crosswalks, 2 junctions with all their lanes |
| `modules/map/hdmap/test-data` (Baidu) | 11 | 11 lanelets + 1 crosswalk, 3 regulatory elements; 6 of 6 successors, 2 adjacent (no lane change) edges | 11 lanes, centerlines within 10⁻⁹ m, half widths within 0.02 m; all successors; signal, stop sign, yield sign, crosswalk, junction preserved |

The differences are the ones named above. `borregas_ave`: two lane splits and
six pairs of oncoming lanes whose boundaries are 1.1 to 4.1 m apart cannot be
expressed, and because its neighbor lanes overlap or leave gaps, 76 of 120
bounds moved by more than 0.1 m (at most 3.1 m), which changes the half width
of 53 lanes by more than 0.05 m (median over all lanes 0.28 m, at most 1.64 m
where lanes split). The Baidu
map round-trips without moving a bound; its two bicycle lanes are no longer
neighbors of the driving lanes next to them (solid line, see Topology).

**Robustness.** 200 randomly damaged copies of the example map (dropped
nodes, members and tags, wrong references, values and coordinates): 186
converted, 14 rejected as not being XML, no crash, and every converted map
was accepted by the HD map engine.

**Closed loop** (`tools/ros2/closed_loop`: Apollo routing, planning and
control against a simulated vehicle, pass criteria from the MCAP recording).
One run each at 5 m/s; all criteria of the tool passed in every run (goal
reached, no planning e-stop, lateral deviation below 1 m, ...). Repeated runs
of the same scenario differ by a few decimeters:

| map | route | result | drive time | front bumper to goal | max lateral deviation |
|---|---|---|---|---|---|
| demo map exported to Lanelet2 (`--centerline none`) and converted back | lane `1_-1`, 146 m | PASS | 55.0 s | 0.754 m | 0.647 m |
| official Lanelet2 example map | 17 lanelets `45262` … `45302` through two inferred junctions, 144 m | PASS | 45.5 s | 0.864 m | 0.288 m |
| official Lanelet2 example map | the same street in the other direction: 17 `_inv` lanes of two-way lanelets | PASS | 45.5 s | 0.921 m | 0.295 m |
| stock `modules/map/data/demo` (regression) | lane `1_-1`, 146 m | PASS | 56.0 s | 0.447 m | 0.483 m |

```bash
tools/ros2/lanelet2/apollo_to_lanelet2.py --map modules/map/data/demo \
    --utm_zone 10 --centerline none --output demo.osm
tools/ros2/lanelet2/lanelet2_to_apollo.py --osm demo.osm --output_dir demo_ll2
tools/ros2/closed_loop/run_closed_loop.py --runs 1 --storage mcap \
    --map_dir demo_ll2 --lane 1_-1 --start_s 2.0 \
    --goal 586367.706490 4140785.357946 1.305

tools/ros2/lanelet2/lanelet2_to_apollo.py --osm mapping_example.osm --output_dir example
tools/ros2/lanelet2/run_route_closed_loop.py --map_dir example --lanes \
    45262 45264 45268 45272 45274 45276 45278 45280 45282 45284 45286 45288 \
    45290 45294 45298 45300 45302
```

`run_closed_loop.py` measures the lateral deviation against the start lane
only; `run_route_closed_loop.py` calls the same test with the centerlines of
all lanes of the route as reference. The routes avoid traffic lights: the
test has no perception, so a light would never turn green.

## Limitations

Not supported:

* Lanelet2 **areas** (`multipolygon` relations: parking, vegetation, traffic
  islands, ...) are ignored. No `ParkingSpace`, `ClearArea`, `SpeedBump` or
  `PNCJunction` is created.
* Autoware regulatory elements `detection_area`, `no_stopping_area`,
  `no_parking_area`, `speed_bump`, `virtual_traffic_light`, `bus_stop_area`,
  `roundabout` and `crosswalk` (the crosswalk lanelet itself is converted),
  and the `light_bulbs` of traffic lights: subsignals are derived from the
  `subtype` of the light, all as `CIRCLE`.
* Traffic signs other than stop, yield and speed limit.
* Regulatory elements with `fallback=yes` (valid only while traffic lights are
  out of order) are ignored.
* Regulatory elements apply to the stored direction of a two-way lanelet; its
  `_inv` lane has none.
* Traffic rules are the generic/German ones of Lanelet2 (the only ones the
  library ships): participants, one-way and lane change rules are evaluated
  for vehicles, `speed_limit_mandatory` and time dependent tags are not.
* Junction `type`, signal orientation and stop sign type (other than
  `ALL_WAY`) are not derived.
* `topo_creator` and `sim_map_generator` do not read `.osm` files themselves
  (use `lanelet2_to_apollo.py`, also for workflow B).
* Lanes narrower than half the vehicle width (tapered lanelets) make
  `LaneInfo` log an error per centerline point when the map is loaded, as for
  any Apollo map.

Not verified:

* Real Autoware maps. The Autoware conventions (`local_x`/`local_y`,
  `turn_direction`, `intersection_area`, `road_marking`, `road_shoulder`,
  stop signs as `traffic_sign`) are implemented from the documentation of
  `autoware_lanelet2_extension` and tested on synthetic maps only.
* A closed-loop run with the `.osm` itself as base map (workflow B). The unit
  tests load `.osm` files through `HDMapImpl` and `HDMapUtil`, and
  `map_xysl --map_dir=<directory with lanelet2_map.osm>` works, but the
  closed-loop tool needs a `base_map.bin`.
* Driving through signals, stop signs and crosswalks converted from Lanelet2
  (their overlaps are unit-tested and compared with Lanelet2, not driven).
* Dreamview with a converted map.
* Maps outside UTM zone 32N and 10N, and the southern hemisphere, beyond the
  projection unit test.
