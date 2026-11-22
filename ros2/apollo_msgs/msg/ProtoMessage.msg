# An Apollo message carried over ROS 2.
#
# Apollo modules exchange protobuf messages. Each ROS 2 topic created by the
# Cyber-on-ROS 2 runtime uses this type and carries one serialized protobuf
# message per sample, so every Apollo message (including oneof, map, recursive
# and proto2 optional fields) round-trips losslessly.
#
# Use `ros2 run cyber apollo_topic echo <topic>` to decode samples, or the
# apollo_ros2_bridge package to republish common topics as standard ROS 2
# messages (sensor_msgs, nav_msgs, tf2, visualization_msgs).

# Fully-qualified protobuf type name, e.g. "apollo.localization.LocalizationEstimate".
string type

# Monotonic sequence number assigned by the writer.
uint64 seq

# Writer-side publish time (nanoseconds since epoch, Apollo clock).
int64 stamp_ns

# Protobuf wire-format bytes of the message.
uint8[] data
