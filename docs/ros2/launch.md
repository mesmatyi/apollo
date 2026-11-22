# Apollo on ROS 2: launch files

Cyber `*.dag` files and `cyber_launch` XML files (`*.launch`) are converted
into ROS 2 Python launch files by `tools/ros2/dag2launch.py`. The generated
files are committed next to their sources:

| source                  | generated                      |
|-------------------------|--------------------------------|
| `P/dag/foo.dag`         | `P/launch/foo.dag.launch.py`   |
| `P/launch/bar.launch`   | `P/launch/bar.launch.py`       |
| `P/x/baz.dag` / `.launch` (not in `dag/` or `launch/`) | `P/x/launch/baz.dag.launch.py` / `baz.launch.py` |

Do not edit generated files. Change the `.dag`/`.launch` file and re-run the
converter.

## Running

```bash
# components of one dag, in one component container (default)
ros2 launch apollo control.dag.launch.py
# the same dag with the Cyber dag runner: mainboard -d <dag>
ros2 launch apollo control.dag.launch.py runner:=mainboard
# a cyber_launch file: one container (or mainboard) per <process_name>
ros2 launch apollo perception_all.launch.py
# from a source checkout, without installing
APOLLO_ROOT=$PWD ros2 launch modules/control/control_component/launch/control.launch.py
```

`ros2 launch apollo <file>` searches the whole `share/apollo` tree, so the name
must be unique there. `lidar_detection.launch.py` and
`lidar_detection.dag.launch.py` exist in both `perception/lidar_detection` and
`perception/lidar_cpdet_detection`. For those, pass the path instead.

The old command line still works:

```bash
cyber_launch start /apollo/modules/control/control_component/launch/control.launch [runner:=mainboard]
cyber_launch stop  /apollo/modules/control/control_component/launch/control.launch   # or: cyber_launch stop
cyber_launch convert modules/canbus/dag/canbus.dag                                  # print the launch file
```

`cyber/tools/cyber_launch/cyber_launch` (and the `cyber_launch.py` shim) is
now a wrapper around `ros2 launch`. It accepts `.launch`, `.dag` and
`.launch.py` files, and extra arguments are passed to `ros2 launch`.

* If it finds `tools/ros2/dag2launch.py` (in `$APOLLO_ROOT`, the checkout that
  holds the script or the launch file, or `/apollo`), it converts the file on
  the fly. This also covers files that are not pre-generated, such as
  `cyber/examples/*`.
* Otherwise it runs the generated `.launch.py` next to the file.
* `stop` sends SIGINT to the matching `cyber_launch start` processes. After
  20 s it sends SIGTERM, and the wrapper then kills its whole process group.
  Do not send SIGTERM to `ros2 launch` directly: it has no SIGTERM handler and
  leaves its containers behind.

### Paths: `APOLLO_ROOT`

Dag paths for `runner:=mainboard` are resolved at launch time:
`$APOLLO_ROOT/<P>/dag/foo.dag` (`APOLLO_ROOT` defaults to `/apollo`), falling
back to `<share>/apollo/<P>/dag/foo.dag` of the installed `apollo` package.
`/apollo/...` arguments of `binary` modules (e.g. `--flagfile=`) are mapped
the same way when the literal path does not exist. The paths inside the
component configs stay `/apollo/...`, and the Cyber file utilities map those
at run time (see [architecture.md](architecture.md#runtime-paths)).

## Mapping

### dag file → `*.dag.launch.py`

* One `ComposableNodeContainer` (`rclcpp_components` / `component_container_mt`,
  node name `<dag name>_container`, process label `<dag name>`).
* One `ComposableNode` per `components` / `timer_components` entry, in
  mainboard's order (per `module_config`: components, then timer components):
  * `package='apollo'` (`'cyber'` for libraries under `cyber/`)
  * `plugin` = fully-qualified class name
  * `name` = `to_node_name(config.name)`
  * `parameters=[{'component_config': '<text proto>'}]`: the `config { ... }`
    block re-serialized as text-format `ComponentConfig` /
    `TimerComponentConfig`. This is a plain Python `str`, and launch_ros only
    YAML-parses substitutions, so it stays a string parameter.
* `runner:=mainboard` runs `Node(package='cyber', executable='mainboard',
  arguments=['-d', <dag path>])` instead. launch_ros appends `--ros-args`
  (with `-r __node:=<name>`, remappings and parameters) to every `Node`
  command line; Apollo executables take those arguments, see
  [ros2_nodes.md](ros2_nodes.md).

To configure a component from a launch file, add parameters next to
`component_config`: `gflags.<flag name>` sets a gflag of the process (and
wins over the flag file), `use_sim_time` makes Cyber's clock follow `/clock`,
and `remappings=[('/apollo/planning', '/vehicle1/planning')]` renames a
channel. All of it is described in [ros2_nodes.md](ros2_nodes.md).

Class names are resolved with `find_registered_components()` from
`tools/ros2/apollo_ros2_common.py` (the build generator uses the same
function). It scans the Bazel package directory of `module_library`
(`modules/x/liby.so`, `/apollo/bazel-bin/modules/x/liby.so`, ...) for
`CYBER_REGISTER_COMPONENT(<class_name>)`. If the class is not registered
there, the converter looks in subdirectories, then in all of `modules/` and
`cyber/`. A unique match is used, and the generated file gets a `# NOTE:`.
If nothing matches, the unqualified class name is emitted with a `# WARNING:`
and the converter reports an error.

### cyber_launch file → `*.launch.py`

| cyber_launch                             | ROS 2 launch                                             |
|------------------------------------------|----------------------------------------------------------|
| modules sharing a `<process_name>`       | one container with the components of all their dags, or one `mainboard -d ... -d ... -p <process> -s <sched>` with `runner:=mainboard` |
| no `<process_name>`                      | process `mainboard_default`                              |
| `<type>binary</type>`                    | `ExecuteProcess(cmd=<process_name>.split())` in both runner modes. The executable is looked up as given, on `PATH`, under `$APOLLO_ROOT`, or under `lib/apollo` of the `apollo` package |
| `<exception_handler>respawn</exception_handler>` | `respawn=True, respawn_max_retries=<respawn_limit>` (default 3). Containers load their components again after every restart (`reload_on_start`) |
| `<exception_handler>exit</exception_handler>` | `on_exit=[Shutdown()]`                              |
| `<environment><VAR>v</VAR></environment>` | `SetEnvironmentVariable`                                |
| `<sched_name>`, `<plugin>`, `<cpuprofile>`, `<memprofile>`, `extra_args="..."` | `mainboard` arguments with `runner:=mainboard`; comments in container mode |
| `<nice>`, `<desc>`, `<version>`, XML comments, unknown tags | comments / docstring                     |

A `dag_conf` that does not exist is replaced by the dag with the same file
name under the same `modules/<module>/`, if there is exactly one (with a
`# NOTE:`). Otherwise it is reported and left out of the container.

## Converter

```bash
python3 tools/ros2/dag2launch.py               # regenerate everything under modules/
python3 tools/ros2/dag2launch.py --check       # CI: exit 1 if anything is stale or orphaned
python3 tools/ros2/dag2launch.py --check --strict -v   # also fail on unresolved classes
python3 tools/ros2/dag2launch.py FILE -o -     # convert one file to stdout
python3 -m pytest tools/ros2/test_dag2launch.py  # unit tests (+ ROS 2 checks if sourced)
```

The converter only needs the Python standard library. Its output depends only
on the input files and the C++ registrations, so it is deterministic. A
generated file whose source was removed is deleted on the next run, and
`--check` reports it. Under a sourced ROS 2 environment, the tests load every
generated file and evaluate both runners. They also check that each
`component_config` parses as the real `cyber.proto` message and equals the
dag's config block.

### Known gaps

* `modules/bridge/dag/bridge_{sender,receiver}.dag` use class template
  instances (`UDPBridgeSenderComponent<planning::ADCTrajectory>`). These are
  registered through wrapper macros that `find_registered_components()` does
  not expand, so they are emitted unqualified and will not load until
  registration and resolution handle templates.
* `modules/calibration/data/**/*.launch` are ROS 1 launch files and are
  skipped.
* `cyber/examples/**` dags and launch files are not pre-generated.
  `cyber_launch start` converts them on the fly.
