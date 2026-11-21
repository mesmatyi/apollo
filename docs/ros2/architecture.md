# Apollo on ROS 2: architecture

This branch moves Apollo off Cyber RT and onto **ROS 2 (Jazzy)**. The
middleware, build system, process model and tooling are now ROS 2. The ~685K
lines of Apollo algorithm code (perception, prediction, planning, control, ...)
stay as they are. They compile against the same `cyber/...` C++ API, and that
API is now implemented on `rclcpp`.

```
          Apollo modules (unchanged algorithm code, cyber::Component API)
 ┌───────────────────────────────────────────────────────────────────────┐
 │ cyber/  (package "cyber")  Cyber API implemented on ROS 2              │
 │   Node / Reader / Writer  -> rclcpp::Node, rclcpp pub/sub              │
 │   Service / Client        -> rclcpp services                           │
 │   Component / Timer comp. -> rclcpp_components (composable nodes)      │
 │   mainboard (dag runner)  -> rclcpp executor                           │
 │   TopologyManager         -> rclcpp graph API                          │
 │   intra-process           -> zero-copy shared_ptr delivery (Waitable)  │
 ├───────────────────────────────────────────────────────────────────────┤
 │ ROS 2 Jazzy: rclcpp, rmw (Fast DDS / Cyclone DDS), launch, ros2 bag    │
 └───────────────────────────────────────────────────────────────────────┘
```

## Colcon workspace layout

| path                      | colcon package       | contents                                              |
|---------------------------|----------------------|-------------------------------------------------------|
| `ros2/apollo_msgs`        | `apollo_msgs`        | `ProtoMessage.msg`, `ProtoService.srv`                |
| `cyber`                   | `cyber`              | Cyber API on rclcpp, cyber protos, mainboard, tools   |
| `modules`                 | `apollo`             | all Apollo modules (one ament_cmake superbuild)       |
| `ros2/apollo_ros2_bridge` | `apollo_ros2_bridge` | republishes Apollo topics as standard ROS 2 messages  |

`third_party/`, `docker/`, `docs/` and `scripts/` hold `COLCON_IGNORE` files.

## Transport contract

* **Message type on the wire:** every Apollo channel is a ROS 2 topic of type
  `apollo_msgs/msg/ProtoMessage`:
  * `type`: the fully-qualified protobuf name, e.g. `apollo.control.ControlCommand`
  * `data`: the protobuf wire bytes

  This round-trips every Apollo message without loss: oneof, map, recursive
  types and proto2 field presence all survive.
* **Services:** `apollo_msgs/srv/ProtoService`, which pairs a request
  `(type, data)` with a response `(type, data)`.
* **Channel → topic name:** `cyber::ros2::ChannelToTopic()` in C++ and
  `cyber.python.cyber_py3.naming.channel_to_topic()` in Python. Both apply the
  same rules:
  1. Replace every character outside `[A-Za-z0-9_/]` with `_`.
  2. Prefix `/` if the name is not absolute.
  3. Collapse repeated `/` and drop any trailing `/`.
  4. Prefix any `/`-separated token that starts with a digit with `_`.
  5. Move names that ROS 2 reserves for standard types (`/tf`,
     `/tf_static`, `/clock`, `/rosout`, `/parameter_events`) under
     `/apollo`. For example, Apollo's `/tf` channel becomes `/apollo/tf`,
     and `apollo_ros2_bridge` publishes real `tf2_msgs/TFMessage` on `/tf`.

  For example, `/apollo/sensor/lidar-16/PointCloud2` becomes
  `/apollo/sensor/lidar_16/PointCloud2`.
* **Node names:** characters outside `[A-Za-z0-9_]` become `_`. A name that
  starts with a digit gets a leading `_`. An empty name becomes `apollo_node`.
* **QoS:** writers default to reliable, volatile, keep-last 10. A reader's
  depth is its `pending_queue_size` (Cyber default: 1). An explicit Cyber
  `QosProfile` maps field by field onto `rclcpp::QoS`.
* **Intra-process:** a Writer hands the same `shared_ptr<const M>` to every
  in-process Reader of the channel, with no serialization. Delivery goes
  through an `rclcpp::Waitable`, so callbacks run on the executor that spins
  the reader's node. ROS 2 subscriptions use `ignore_local_publications`, so
  a message is never delivered twice.
* **Callback concurrency:** each Reader, Service and TimerComponent gets its
  own mutually exclusive callback group. Callbacks of one reader run
  serially, and different readers run in parallel. This matches Cyber's
  croutine-per-reader model.

## Components

`CYBER_REGISTER_COMPONENT(Foo)` registers `Foo` in two places:

1. Cyber's own class loader, under the unqualified name `Foo`, for the dag
   runner `ros2 run cyber mainboard -d foo.dag`.
2. ROS 2 `class_loader` as an `rclcpp_components::NodeFactory`, under both the
   fully-qualified name (`apollo::foo::Foo`) and the unqualified one. This lets
   any ROS 2 component container load it:

```python
ComposableNode(
    package='apollo',
    plugin='apollo::control::ControlComponent',
    name='control',
    parameters=[{
        # text-format cyber.proto.ComponentConfig / TimerComponentConfig,
        # i.e. exactly the `config { ... }` block of the dag file
        'component_config': 'name: "control" config_file_path: "..." ...',
    }])
```

Instead of `component_config`, the factory also accepts these individual
parameters: `config_file_path`, `flag_file_path`, `readers` (string array of
channels), `reader_pending_queue_sizes` (int array), and `interval` (ms,
timer components).

The `apollo` package registers every component in the ament index
`rclcpp_components` resource as
`<fq class>;lib/apollo/<bazel package path>/lib<name>.so`.

## Install layout (package `apollo`)

* Component and plugin shared libraries go to
  `lib/apollo/<bazel package path>/lib<name>.so`. This mirrors the
  `module_library:` paths in the dag files.
* Plugin descriptions go to `share/apollo/<bazel package path>/plugins.xml`,
  and the plugin index to `share/cyber_plugin_index/<plugin name>`. Each index
  file holds the relative path to its description.
* dag, conf, launch and plugin xml files go to
  `share/apollo/<bazel package path>/...`.
* Python protos (`*_pb2.py`) go to `<python site-packages>/modules/...`.

## Runtime paths

The runtime sets the Cyber/Apollo search variables from `AMENT_PREFIX_PATH`:
`APOLLO_PLUGIN_INDEX_PATH`, `APOLLO_PLUGIN_DESCRIPTION_PATH`,
`APOLLO_PLUGIN_LIB_PATH`, `APOLLO_LIB_PATH`, `APOLLO_CONF_PATH` and
`APOLLO_FLAG_PATH`.

Apollo configs hard-code paths under `/apollo/...`. Set `APOLLO_ROOT` to a
source checkout, or to `<install>/share/apollo`; `/apollo` is the Docker
default. The Cyber file utilities then map `/apollo/<x>` to `$APOLLO_ROOT/<x>`
whenever the literal path does not exist.
