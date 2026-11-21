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
  `/apollo/sensor/lidar_16/PointCloud2`. Its type is
  `apollo_msgs/msg/ProtoMessage`, which carries the protobuf type name and the
  wire bytes. A service is an `apollo_msgs/srv/ProtoService`. A node is named
  `naming.to_node_name(name)`.
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
  channel's protobuf type is learnt from the samples it carries.
* `proto_registry` maps protobuf type names to classes. It looks in installed
  `*_pb2` modules under `modules/` and `cyber/` (without importing unrelated
  ones), and in `proto_desc` blobs, such as those stored in records.
* `parameter` keeps Cyber's parameter service protocol (`<node>/get_parameter`
  and the others) over `ProtoService`, so it interoperates with C++ parameter
  servers and clients.
* `record` is a pure-Python implementation of the `.record` file format, and
  its output is byte-identical to the C++ writer.

## Tools

`cyber_channel` (list, info, echo, hz, bw, type), `cyber_node` (list, info) and
`cyber_service` (list, info) work on the ROS 2 graph.
`cyber_recorder/record_to_rosbag2.py` and `rosbag2_to_record.py` convert
between `.record` files and rosbag2 bags of `ProtoMessage` topics. The
conversion is lossless: channel names, types and descriptors are kept in the
bag metadata.

## Tests

```bash
python3 -m pytest cyber/python/cyber_py3/test
```

The tests use a random `ROS_DOMAIN_ID`. If the Cyber protos are not
installed, they compile them with `protoc`.
