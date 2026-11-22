# Apollo processes as ROS 2 nodes

An Apollo process is a set of ordinary ROS 2 nodes. Everything on this page
works with the stock tools (`ros2 node`, `ros2 param`, `ros2 topic`,
`ros2 launch`, rqt) and with both supported middlewares, Fast DDS and
Cyclone DDS.

## Nodes

* **Component nodes.** Every Apollo component is a node, named after the
  component in its dag file (`planning`, `control`, ...). Under `ros2 launch`
  it is a composable node inside a component container; under `mainboard` it
  is created by the Cyber runtime.
* **Other Cyber nodes.** Each `apollo::cyber::CreateNode("name")` is a node
  with that name.
* **The process node.** One node per process stands for the process itself:
  its gflags as parameters, its `/rosout` publisher, its `/clock`
  subscription. It is called `apollo_cyber_<pid>`, or whatever the command
  line says with `-r __node:=<name>` (which is how `launch_ros` names a
  process).

All of them have the standard parameter services and `use_sim_time`.

## ROS arguments

Every Apollo executable accepts `--ros-args`, in any position on its command
line, next to its own flags:

```bash
mainboard -d modules/planning/planning_component/dag/planning.dag \
  --ros-args -r __node:=planning_main -r __ns:=/vehicle1 \
             -r /apollo/planning:=/vehicle1/planning \
             -p use_sim_time:=true -p gflags.map_dir:=/maps/town \
             --params-file planning.yaml --log-level warn
```

| argument | effect |
|---|---|
| `-r <from>:=<to>` | remaps a topic or service for every node of the process. Readers and writers inside one process follow the remapped name too |
| `-r __ns:=<namespace>` | namespace of the nodes. Apollo channel names are absolute (`/apollo/...`), so the namespace changes node names and their parameter services, not the channels; use remap rules or separate `ROS_DOMAIN_ID`s to run several vehicles |
| `-r __node:=<name>` | names the process node only (a process has many nodes) |
| `-p <name>:=<value>`, `--params-file <yaml>` | parameters for the nodes of the process |
| `--log-level`, `--disable-rosout-logs`, `--enclave`, ... | as for any ROS 2 process |

The executables do not have to pass their `argv` anywhere for this: the
runtime reads the ROS arguments from the command line of the process, and
gflags is told to ignore them. `mainboard` and `cyber_recorder` strip them
before parsing their own options.

## gflags are parameters

Apollo modules are configured with gflags (thousands of them). Each flag is
the ROS 2 parameter `gflags.<flag name>`, with the flag's type and help text.

**At start**, give flags to any Apollo node the way you give any parameter:

```python
ComposableNode(package='apollo', plugin='apollo::planning::PlanningComponent',
               name='planning',
               parameters=[{'gflags.map_dir': '/maps/town',
                            'gflags.planning_upper_speed_limit': 12.5}])
```

```yaml
/**:
  ros__parameters:
    gflags.map_dir: /maps/town
    gflags.use_navigation_mode: false
```

Parameters win over the module's flag file: they are applied again after a
flag file is read and after a library that defines more flags is loaded.

**At run time**, the process node lists every flag:

```bash
$ ros2 param list /planning_main | grep gflags.planning_ | head -3
$ ros2 param get /planning_main gflags.planning_upper_speed_limit
Double value is: 12.5
$ ros2 param set /planning_main gflags.planning_upper_speed_limit 8.0
Set parameter successful
$ ros2 param dump /planning_main          # the configuration a process runs with
```

A value of the wrong type, or a value the flag's validator refuses, is
rejected and the flag keeps its value. A flag changed by the program itself
shows up as the parameter value within about ten seconds. A node that was given
`gflags.*` parameters at start lists those too, and setting one there changes
the flag just the same.

Flags are global to a process. In a component container all components share
them, exactly as modules sharing one `mainboard` process always did.

The process node does not publish its (thousands of) parameters on
`/parameter_events`. `APOLLO_ROS_FLAG_PARAMETERS=0` switches the flag
parameters of the process node off; flags given as parameters at start still
apply.

## Simulation time

When `use_sim_time` is true on any Apollo node of a process, Cyber's clock
(`apollo::cyber::Clock::Now()`, which Apollo modules use for message time
stamps) follows `/clock` (`rosgraph_msgs/msg/Clock`) instead of the system
clock, as `rclcpp::Node::now()` does. It can be switched at run time:

```bash
ros2 param set /planning use_sim_time true
```

Cyber's own mock-clock mode (`clock_mode: MODE_MOCK` in `cyber.pb.conf`)
subscribes to `/clock` as well, in addition to Apollo's clock channel
(`/apollo/clock`, `apollo_msgs/msg/CyberProtoClock`). Timers of timer
components keep running on the wall clock, as in Cyber RT.

## Logs on /rosout

Apollo logs with glog (`AINFO`, `AWARN`, `AERROR`, `AFATAL`). Messages of level
WARNING and above are also published on `/rosout` (`rcl_interfaces/msg/Log`)
with file and line; the logger name is the module name. glog still writes its
own log files, and nothing is printed twice on a console.

| environment | effect |
|---|---|
| `APOLLO_ROSOUT_LEVEL=INFO\|WARN\|ERROR\|FATAL\|OFF` | lowest level forwarded (default `WARN`) |
| `--ros-args --disable-rosout-logs` | no `/rosout` for the process |

## Mixing with other ROS 2 nodes

* A plain ROS 2 node can be loaded into the same component container as
  Apollo components. Apollo readers receive its messages, and it receives
  Apollo's, through the middleware; Apollo-to-Apollo delivery inside the
  process stays zero-copy.
* Latched channels (transient local, e.g. `/apollo/tf_static`) reach readers
  that start later and ask for transient-local durability, inside the
  process and across processes.
* `node_->RosNode()` gives a component its `rclcpp::Node` for anything else:
  tf2, actions, diagnostics.

## Environment variables

| variable | meaning |
|---|---|
| `APOLLO_ROS_WIRE_FORMAT=proto` | all channels as `apollo_msgs/msg/ProtoMessage` (see [native_types.md](native_types.md)) |
| `APOLLO_ROS_PROTO_CHANNELS=<regex>` | the matching channels as `ProtoMessage` |
| `APOLLO_ROS_FLAG_PARAMETERS=0` | the process node does not list gflags |
| `APOLLO_ROSOUT_LEVEL` | lowest glog level forwarded to `/rosout` |
| `APOLLO_EXECUTOR_THREADS`, `APOLLO_TASK_THREADS` | thread pool sizes |

## Tests

`cyber_ros2_test --gtest_filter=Ros2FeaturesTest.*` drives all of the above
from a plain rclcpp node (parameter client, `/clock` publisher, `/rosout`
subscription). `cyber/ros2/test/cli_interop_test.sh` does it with the `ros2`
command line tools against `cyber_example_talker`, `cyber_example_listener`,
`cyber_example_service` and `mainboard`: 21 checks, run with Fast DDS and
with Cyclone DDS.
