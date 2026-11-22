# Cyber RT Python API on ROS 2

`cyber.python.cyber_py3` keeps the Cyber RT Python API (`cyber`, `cyber_time`,
`cyber_timer`, `parameter`, `record`) but is now pure Python on top of
`rclpy`. There is no C++ extension any more. Existing scripts keep working
unchanged:

```python3
#!/usr/bin/env python3
from cyber.python.cyber_py3 import cyber
from cyber.proto.unit_test_pb2 import ChatterBenchmark

cyber.init()
node = cyber.Node("listener")
node.create_reader("channel/chatter", ChatterBenchmark, print)
node.spin()          # returns on Ctrl-C
cyber.shutdown()
```

Requirements: ROS 2 Jazzy (`rclpy`, and `rosbag2_py` for the bag converters),
the `apollo_msgs` package, and the Python protobuf runtime
(`pip install "protobuf>=4.21,<5"`). `record.py` needs only protobuf.

## How it maps onto ROS 2

See [docs/ros2/architecture.md](../../docs/ros2/architecture.md) for the
contract shared with the C++ runtime.

* A channel is a ROS 2 topic named `naming.channel_to_topic(channel)`, for
  example `/apollo/sensor/lidar-16/PointCloud2` becomes
  `/apollo/sensor/lidar_16/PointCloud2`. Its type is the native ROS 2 type
  generated from the channel's protobuf message, for example
  `apollo.planning.ADCTrajectory` is `apollo_msgs/msg/PlanningADCTrajectory`
  (see [docs/ros2/native_types.md](../../docs/ros2/native_types.md)). A
  service has the native `apollo_msgs/srv/...` type of its request and
  response. Protobuf types without a generated interface use
  `apollo_msgs/msg/ProtoMessage` (protobuf type name plus wire bytes) and
  `apollo_msgs/srv/ProtoService`. A node is named `naming.to_node_name(name)`.
* `native_types` converts between protobuf messages and the generated ROS 2
  messages with the same rules as the C++ runtime, so Python nodes, C++ nodes
  and plain ROS 2 nodes exchange the same typed messages. A raw reader
  follows the type its channel's writers use and always receives protobuf
  wire bytes.
* Writers are reliable, volatile and keep the last `qos_depth` messages.
  Readers keep the last `qos_depth` messages (Cyber's pending queue size,
  default 1). They adapt reliability and durability to the publishers already
  on the graph.
* Callbacks run on background threads. Each node has its own executor thread.
  Each reader runs its callback on its own thread, so one reader's callbacks
  never overlap but different readers run in parallel. Each service handles
  one request at a time. Clients live on a hidden helper node, so
  `send_request()` works from inside any callback.
* SIGINT puts the runtime into the shutting-down state: `is_shutdown()`
  becomes True, and `spin()` and `waitforshutdown()` return. A second SIGINT
  raises `KeyboardInterrupt`.
* `ChannelUtils`, `NodeUtils` and `ServiceUtils` query the ROS 2 graph. A
  channel's protobuf type follows from its ROS 2 type (on the `ProtoMessage`
  wire format it is learnt from the samples).
* `proto_registry` maps protobuf type names to classes. It looks in installed
  `*_pb2` modules under `modules/` and `cyber/` (without importing unrelated
  ones), in `proto_desc` blobs, such as those stored in records, and in the
  descriptor set installed with the native interfaces, which covers every
  Apollo channel type.
* `parameter` keeps Cyber's parameter service protocol (`<node>/get_parameter`
  and the others) as ROS 2 services, so it interoperates with C++ parameter
  servers and clients.
* `record` is a pure-Python implementation of the `.record` file format, and
  its output is byte-identical to the C++ writer.

## Tools

`cyber_channel` (list, info, echo, hz, bw, type), `cyber_node` (list, info) and
`cyber_service` (list, info) work on the ROS 2 graph.
`cyber_recorder/record_to_rosbag2.py` and `rosbag2_to_record.py` convert
between `.record` files and rosbag2 bags. Bags get natively typed topics, the
same types a live system publishes and `ros2 bag record` stores, so they play
back into Apollo and open in standard ROS 2 tools; `--wire proto` writes
`ProtoMessage` topics with the protobuf bytes unchanged. Channel names, types
and descriptors are kept in the bag metadata.

## Tests

```bash
python3 -m pytest cyber/python/cyber_py3/test
```

The tests use a random `ROS_DOMAIN_ID`. If the Cyber protos are not
installed, they compile them with `protoc`.
