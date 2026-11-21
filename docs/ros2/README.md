# Apollo on ROS 2

This branch runs Apollo on **ROS 2 Jazzy** (Ubuntu 24.04) instead of Cyber RT:

* **Middleware:** Apollo channels are ROS 2 topics and Apollo services are
  ROS 2 services, carried over DDS.
* **Components:** every Apollo component is a ROS 2 composable node.
* **Build system:** colcon/ament replaces Bazel.
* **Launch:** dag and cyber_launch files have ROS 2 launch equivalents.
* **Tooling:** `ros2 topic`, `ros2 bag`, RViz and rqt work on Apollo
  traffic, either directly or through `apollo_ros2_bridge`.

The algorithm code did not have to change. Modules still program against the
Cyber C++/Python API (`cyber::Component`, `Node::CreateReader`, `AINFO`, ...),
and that API is now implemented on rclcpp/rclpy. See
[architecture.md](architecture.md) for the design, and [launch.md](launch.md)
for how dag and launch files map to ROS 2 launch.

## Quick start (Docker)

```bash
docker build -f docker/ros2/Dockerfile -t apollo:ros2-jazzy .
docker run --rm -it --net=host -v $PWD:/apollo apollo:ros2-jazzy
# inside the container, the workspace is prebuilt in /opt/apollo/install
ros2 launch apollo planning.launch.py
```

## Build on Ubuntu 24.04 + ROS 2 Jazzy

```bash
# ROS 2 Jazzy: https://docs.ros.org/en/jazzy/Installation/Ubuntu-Install-Debs.html
sudo apt install python3-colcon-common-extensions python3-rosdep ninja-build
sudo rosdep init; rosdep update
rosdep install --from-paths ros2 cyber modules --ignore-src -y -r
sudo scripts/ros2/install_third_party.sh       # OSQP 0.6, ADOL-C

source /opt/ros/jazzy/setup.bash
scripts/ros2/build.sh                           # all packages
scripts/ros2/build.sh cyber                     # one package and its deps
APOLLO_BUILD_TESTS=1 scripts/ros2/build.sh cyber   # build and run tests

source scripts/ros2/setup.bash                  # sources install/, sets APOLLO_ROOT
```

The workspace contains four colcon packages:

| package              | path                      | what it is                                          |
|----------------------|---------------------------|-----------------------------------------------------|
| `apollo_msgs`        | `ros2/apollo_msgs`        | `ProtoMessage.msg` / `ProtoService.srv` (wire types) |
| `cyber`              | `cyber`                   | Cyber API on rclcpp/rclpy, `mainboard`, tools       |
| `apollo`             | `modules`                 | every Apollo module (one ament_cmake superbuild)    |
| `apollo_ros2_bridge` | `ros2/apollo_ros2_bridge` | Apollo ⇄ standard ROS 2 message bridge              |

The `apollo` package's CMake is generated from the Bazel `BUILD` files by
`tools/ros2/bazel2cmake.py`; the `BUILD` files stay the source of truth. When
a third-party dependency is missing (CUDA, TensorRT, libtorch, ...), the
targets that need it are skipped and the configure summary lists them. The
rest still builds.

## Running Apollo

Three equivalent ways to run components:

```bash
# 1. ROS 2 launch (component containers), generated from dag/launch files
ros2 launch apollo control.dag.launch.py
ros2 launch apollo planning.launch.py runner:=mainboard   # use mainboard

# 2. The dag runner, as with Cyber RT
mainboard -d modules/control/control_component/dag/control.dag

# 3. Any component container, Apollo and ROS 2 nodes side by side
ros2 run rclcpp_components component_container_mt
ros2 component load /ComponentManager apollo apollo::control::ControlComponent \
    -n control -p component_config:='name: "control" ...'
```

`cyber_launch start <file>.launch` still works: it runs the generated
`.launch.py` through `ros2 launch`.

Apollo configuration files reference `/apollo/...` paths. Either work in a
container that mounts the repository at `/apollo`, or set `APOLLO_ROOT` to the
checkout (`scripts/ros2/setup.bash` does this). The Cyber file utilities then
resolve `/apollo/<x>` to `$APOLLO_ROOT/<x>`.

## Tooling

| Cyber RT                    | ROS 2 equivalent                                                    |
|-----------------------------|---------------------------------------------------------------------|
| `cyber_monitor`             | `ros2 topic list/hz/bw`, `rqt`; `cyber_channel` decodes payloads    |
| `cyber_channel echo <ch>`   | `cyber_channel echo <ch>` (rclpy port, decodes protobuf)            |
| `cyber_node list`           | `cyber_node list` or `ros2 node list`                               |
| `cyber_service list`        | `cyber_service list` or `ros2 service list`                         |
| `cyber_recorder record`     | `ros2 bag record -a` (Apollo topics are `apollo_msgs/ProtoMessage`) |
| `cyber_recorder play`       | `ros2 bag play`                                                     |
| `.record` files             | `record_to_rosbag2.py` / `rosbag2_to_record.py` converters          |
| `cyber_launch`              | `ros2 launch` (wrapper kept)                                        |
| Dreamview `/tf` view, RViz  | `ros2 launch apollo_ros2_bridge bridge.launch.py`, then RViz        |

`ros2 topic echo` prints the raw `ProtoMessage` (type name and bytes). Use
`cyber_channel echo` to see decoded protobuf, or the bridge for standard
message types.

### The bridge

`apollo_ros2_bridge` converts between Apollo protobuf channels and standard
ROS 2 messages, in both directions:

* Apollo → ROS 2: point clouds (`sensor_msgs/PointCloud2`), images,
  localization (`nav_msgs/Odometry` plus `world → localization` on `/tf`),
  Apollo transforms (`/tf`, `/tf_static`), IMU, GNSS (`NavSatFix`), planning
  trajectory (`nav_msgs/Path`) and perception obstacles (`MarkerArray`).
* ROS 2 → Apollo: `PointCloud2`, `Image` and `CompressedImage` from ROS 2
  drivers.

Output topics are `/ros<apollo channel>`, for example
`/ros/apollo/localization/pose`. Load the bridge into the same container as
Apollo's components (`container:=<name>`) and it reads them zero-copy. See
`ros2/apollo_ros2_bridge/config/bridge.yaml`.

## Notes for module developers

* **Keep using the Cyber API.** Readers, writers, services, timers,
  components, `cyber::Async`, the record API, the plugin manager and the
  class loader all keep their signatures.
* **Mix in native ROS 2.** `node_->RosNode()` returns the underlying
  `rclcpp::Node`, so a component can create rclcpp publishers, parameters or
  tf2 listeners next to Cyber readers.
* **Channel names** map to ROS 2 names: invalid characters become `_`, and
  `/tf`, `/tf_static` and `/clock` move to `/apollo/...`. See
  architecture.md.
* **QoS:** a reader's `pending_queue_size` is the ROS 2 subscription depth
  (Cyber default: 1, so only the newest message waits). Explicit Cyber
  `QosProfile`s map field by field. Incompatible QoS pairs are logged.
* **Threading** matches Cyber: one reader's callbacks run serially, and
  different readers run in parallel on a multi-threaded executor.
  `APOLLO_EXECUTOR_THREADS` and `APOLLO_TASK_THREADS` size the pools.
* **Zero-copy:** in-process readers receive the writer's `shared_ptr`, as
  with Cyber. Do not modify a message after writing it.
* **Python:** `from cyber.python.cyber_py3 import cyber` runs on rclpy with
  the same API.

## What was removed

These Cyber RT internals are replaced by ROS 2 and no longer exist:

* transport (shm/RTPS/intra)
* the croutine scheduler and its `*_sched*.conf` scheduling policies
* the data-visitor/blocker machinery
* RTPS service discovery
* `sysmo` and the bvar statistics
* `cyber_monitor` and `cyber_performance`

Code that included those headers directly (rare outside `cyber/`) needs
porting. `cyber/scheduler/scheduler_factory.h` and
`cyber/transport/shm/protobuf_arena_manager.h` remain as compatibility stubs.

The Bazel build (`WORKSPACE`, `BUILD`, `tools/*.bzl`) is no longer maintained
for building. The `BUILD` files are kept only as the input to
`tools/ros2/bazel2cmake.py`.
