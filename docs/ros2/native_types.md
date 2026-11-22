# Native ROS 2 interfaces for Apollo channels and services

Every Apollo channel is an ordinary, typed ROS 2 topic, and every Apollo
service an ordinary, typed ROS 2 service. A ROS 2 node that has never heard of
Apollo, protobuf or Cyber can subscribe to planning, publish localization or
call the routing command service with the generated `apollo_msgs` types.

```bash
$ ros2 topic list -t
/apollo/canbus/chassis [apollo_msgs/msg/CanbusChassis]
/apollo/localization/pose [apollo_msgs/msg/LocalizationLocalizationEstimate]
/apollo/planning [apollo_msgs/msg/PlanningADCTrajectory]
...
$ ros2 topic echo --once /apollo/localization/pose
$ ros2 service call /apollo/external_command/lane_follow \
    apollo_msgs/srv/ExternalCommandLaneFollowCommand \
    "{end_pose: {x: 587042.8, y: 4141540.9, heading: 1.57}}"
```

Apollo modules keep using the Cyber API with protobuf messages. The Cyber
runtime converts at the process boundary; inside a process a message still
goes from writer to reader as the same `shared_ptr`, without any conversion.

## Where the types come from

`tools/ros2/proto2ros.py` runs when the `apollo_msgs` package is configured.
It reads the protobuf definitions listed in `ros2/apollo_msgs/proto_files.txt`
(all of `modules/common_msgs`, the Cyber protos used on channels, and the
channel protos that live inside modules) and writes, into the build tree:

* one `.msg` file per protobuf message: **801 messages from 147 `.proto`
  files** at the time of writing;
* one `.srv` file per request/response pair listed in
  `ros2/apollo_msgs/services.txt` (**14 services**: the external commands,
  the Cyber parameter service, the Cyber example);
* `registry.txt` (protobuf name ↔ ROS 2 type, member order, presence bits and
  a hash of each protobuf definition) and `protos.desc` (the descriptors), both
  installed in `share/apollo_msgs/proto_interfaces` and found at run time
  through the ament index (resource type `apollo_proto_types`).

Nothing generated is checked in. To add a channel type, add its `.proto` to
`proto_files.txt` (or the pair to `services.txt`) and rebuild `apollo_msgs`.
Other packages can ship their own interfaces the same way: run `proto2ros.py`
with `--package <name>` and register the output directory as an
`apollo_proto_types` resource.

## Mapping

| protobuf | ROS 2 |
|---|---|
| message `apollo.<pkg>.<Message>` | `apollo_msgs/msg/<Pkg><Message>`: the package without the leading `apollo`, in CamelCase, then the message name (for a nested message, the names of the enclosing messages first). `apollo.planning.ADCTrajectory` → `PlanningADCTrajectory`, `apollo.common.Header` → `CommonHeader`, `apollo.common.LatencyTrack.LatencyTrackMessage` → `CommonLatencyTrackLatencyTrackMessage` |
| `double`, `float`, `int32`, `int64`, `uint32`, `uint64`, `bool`, `string` | `float64`, `float32`, `int32`, `int64`, `uint32`, `uint64`, `bool`, `string` |
| `bytes` | `uint8[]` |
| enum | `int32`, with one constant per value: `<FIELD>_<VALUE>` |
| message field | nested message |
| `repeated T` | `T[]` |
| `map<K, V>` | array of entry messages (`key`, `value`) |
| default value | default value of the `.msg` field (when a `.msg` file can express it) |
| a message that contains itself (4 fields) | `apollo_msgs/ProtoMessage` holding the serialized sub-message |
| optional field presence | `uint64 proto_has_bits` with one constant `HAS_<FIELD>` per field |

Field names are the protobuf names in snake_case (a name that is a keyword in
C++, Python or IDL gets the suffix `_field`). Comments are copied from the
`.proto` files, so `ros2 interface show apollo_msgs/msg/CanbusChassis` is
documented.

All types live in the one package `apollo_msgs`: Apollo's protobuf packages
import each other in cycles (`common` ↔ `localization`, `planning` ↔
`dreamview` ↔ `planning_internal`), which ROS 2 interface packages cannot.

### Field presence

Protobuf (proto2) knows whether an optional field was set; a ROS 2 message
always has a value for every field. The bridge between the two is
`proto_has_bits`:

* **Apollo → ROS 2.** Every field holds the value Apollo would read: the set
  value, or the protobuf default of an unset field. The bit of every set field
  is 1. What `ros2 topic echo` prints is what an Apollo module sees.
* **ROS 2 → Apollo.** A field is present when its bit is set **or** its value
  differs from the protobuf default. A plain ROS 2 publisher therefore never
  needs to touch the bits: whatever it fills in arrives. A sub-message is
  present when anything inside it is.
* Apollo → ROS 2 → Apollo reproduces a message exactly, including presence.
  This is tested for all 801 types with random messages
  (`cyber_ros2_test --gtest_filter=NativeTypesTest.*` in C++, 
  `native_types_test.py` in Python).

To send an explicit default (say, `gear_location: 0` as a statement rather
than "not set"), set the bit:

```python
msg = CanbusChassis()
msg.proto_has_bits |= CanbusChassis.HAS_GEAR_LOCATION
```

### Fields that default to NaN

Apollo's geometry types (`PointENU`, `Point3D`, `Quaternion`, ...) and some
chassis fields default to NaN, meaning "unknown". A `.msg` file cannot express
a NaN default, so a newly constructed ROS 2 message holds 0 in those fields
(124 of the 801 types contain, somewhere inside, a field whose default a
`.msg` file cannot express).

A zero is a value: a standing obstacle has velocity 0, and it must not turn
into NaN on the way to Apollo. So from a plain ROS 2 publisher such a
sub-message arrives as "present, all zero" even if the publisher never touched
it. To say "absent", set the fields to NaN (that is what Apollo itself puts
there, and what you see when Apollo sends an absent field).

### Oneof

Each member of a protobuf oneof is a separate field in the ROS 2 message. A
member selects the oneof when its presence bit is set, or when the publisher
changed it. A member that was never touched never selects the oneof, also not
when it contains NaN-default fields that read 0.

## Talking to Apollo from a plain ROS 2 node

Python, publishing a localization estimate and listening to planning:

```python
import rclpy
from rclpy.node import Node
from apollo_msgs.msg import LocalizationLocalizationEstimate, PlanningADCTrajectory

class Driver(Node):
    def __init__(self):
        super().__init__('my_localization')
        self.pub = self.create_publisher(
            LocalizationLocalizationEstimate, '/apollo/localization/pose', 10)
        self.create_subscription(
            PlanningADCTrajectory, '/apollo/planning', self.on_plan, 10)

    def publish_pose(self, x, y, heading, stamp):
        msg = LocalizationLocalizationEstimate()
        msg.header.timestamp_sec = stamp
        msg.pose.position.x, msg.pose.position.y = x, y
        msg.pose.heading = heading
        self.pub.publish(msg)

    def on_plan(self, msg):
        self.get_logger().info('%d trajectory points' % len(msg.trajectory_point))
```

C++ is the same with `apollo_msgs/msg/planning_adc_trajectory.hpp` and
`apollo_msgs::msg::PlanningADCTrajectory`. Such nodes can run in their own
process or be composed into the same container as Apollo components.

Use the QoS Apollo uses: reliable, volatile, keep-last (the Cyber default
reader keeps 1 message, writers 10). Latched channels such as
`/apollo/tf_static` are transient-local.

## Channels that keep the protobuf wire format

`apollo_msgs/msg/ProtoMessage` (`string type` + `uint8[] data`, the serialized
protobuf) and `apollo_msgs/srv/ProtoService` remain as the fallback:

* **Types without a generated interface**, i.e. messages defined in a
  `.proto` that is not in `proto_files.txt`.
* **A different definition under the same name.** The registry stores a hash
  of each protobuf definition. A process built against another definition
  (the per-vehicle `modules/canbus_vehicle/*` protos redefine 24 message
  names) logs a warning and uses `ProtoMessage` for that type instead of
  sending wrong data.
* **By choice:** `APOLLO_ROS_PROTO_CHANNELS=<regex>` keeps the matching
  channels and services on the protobuf wire, `APOLLO_ROS_WIRE_FORMAT=proto`
  all of them. Every process on a channel must agree.

Readers that do not know the type of their channel (`cyber_recorder`,
`cyber_channel`, the bridge) follow whatever the writers use, found in the
ROS 2 graph, and get protobuf bytes either way.

## Cost

Conversion happens only when a message leaves or enters a process; it uses
protobuf reflection on one side and ROS 2 type introspection on the other.
Measured on this workstation (4 shared cores, unoptimized for this purpose):

| message | direction | time |
|---|---|---|
| `drivers.PointCloud`, 100 000 points (2.9 MB protobuf, 3.2 MB CDR), C++ | to ROS 2 | 11–18 ms |
| | from ROS 2 | 16–22 ms |
| `planning.ADCTrajectory`, 100 points, Python | to ROS 2 | 1.8 ms |
| | from ROS 2 | 1.1 ms |

For a heavy sensor stream that crosses a process boundary and does not need
to be readable by other ROS 2 nodes, `APOLLO_ROS_PROTO_CHANNELS` avoids the
conversion; composing writer and reader into one container avoids
serialization altogether. For point clouds and images in standard ROS 2 form
(`sensor_msgs/PointCloud2`, `sensor_msgs/Image`) use
[`apollo_ros2_bridge`](README.md#the-bridge).

The `apollo_msgs` package takes about 16 minutes to build on 4 cores (about
8 200 compile steps); its type support libraries are 2–10 MB each.

## Recording

`ros2 bag record` stores the typed messages, so the bags are ordinary ROS 2
bags (`ros2 bag info`, `ros2 bag play`; tools that plot or visualize ROS 2
bags were not tried here). `cyber_recorder record` keeps
writing `.record` files with protobuf payloads and descriptors, and
`cyber_recorder play` publishes them with the native types again.
`record_to_rosbag2.py` and `rosbag2_to_record.py` convert between the two.

## Tests

| test | what it checks |
|---|---|
| `cyber_ros2_test` (C++, `NativeTypesTest.*`, 10 tests) | registry; all 801 types × 6 random messages round-trip protobuf → ROS 2 → CDR → ROS 2 → protobuf; presence, NaN-default and oneof rules; a mismatching definition falls back |
| `cyber_ros2_test` (C++, `NativeTransportTest.*`, 10 tests) | Cyber writers, readers, raw readers/writers, latched channels, services and clients against plain rclcpp publishers, subscriptions, services and clients in the same process; no duplicates; fallback to `ProtoMessage` |
| `cyber/ros2/test/cli_interop_test.sh` (22 checks per RMW) | the stock `ros2 topic`, `ros2 service`, `ros2 interface`, `ros2 param` tools against Cyber executables |
| `cyber/python/cyber_py3/test/native_types_test.py` (24 tests) | the same conversion rules in Python for all 801 types; plain rclpy peers; C++ talker → Python reader, Python writer → C++ listener, Python client → C++ service; a random `ADCTrajectory` written by Python, recorded and played back by the C++ `cyber_recorder`, arrives unchanged |

All of them pass with `rmw_fastrtps_cpp` and `rmw_cyclonedds_cpp`.
