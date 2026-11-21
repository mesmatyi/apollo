# Copyright 2026 The Apollo Authors. All Rights Reserved.
# Licensed under the Apache License, Version 2.0.
"""Apollo closed-loop stack: external command (routing), planning, control.

  ros2 launch tools/ros2/closed_loop/closed_loop.launch.py \
      map_dir:=/apollo/modules/map/data/demo

The three components run in one ROS 2 component container, configured from
their original dag files. Apollo flag files reference /apollo/... paths and
nest other flag files; gflags opens those literally, so each module's flag
file is flattened into a generated file with /apollo/... resolved against
APOLLO_ROOT and the map directory overridden. The vehicle simulator
(vehicle_sim.py) closes the loop.
"""

import os
import re
import tempfile

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.actions import SetEnvironmentVariable
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode

from google.protobuf import text_format

from cyber.proto import dag_conf_pb2

DAGS = [
    # (dag, fully-qualified component class)
    ('modules/external_command/process_component/dag/'
     'external_command_process.dag',
     'apollo::external_command::ExternalCommandProcessComponent'),
    ('modules/planning/planning_component/dag/planning.dag',
     'apollo::planning::PlanningComponent'),
    ('modules/control/control_component/dag/control.dag',
     'apollo::control::ControlComponent'),
]

_APOLLO_PATH = re.compile(r'(?<![\w.])/apollo/[^\s"\',;]*')


def resolve(path, root):
    """/apollo/<x> -> <root>/<x> when /apollo/<x> does not exist."""
    if path.startswith('/apollo/') and not os.path.exists(path):
        candidate = os.path.join(root, path[len('/apollo/'):])
        if os.path.exists(candidate) or os.path.exists(os.path.dirname(candidate)):
            return candidate
    return path


def flatten_flagfile(path, root, seen=None):
    """Returns the flag lines of `path`, expanding nested --flagfile= and
    resolving /apollo/... paths in flag values."""
    seen = seen if seen is not None else set()
    path = resolve(path, root)
    if path in seen or not os.path.exists(path):
        return ['# missing flagfile: %s' % path] if path not in seen else []
    seen.add(path)
    lines = []
    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('--flagfile='):
                lines.extend(flatten_flagfile(line.split('=', 1)[1], root, seen))
                continue
            lines.append(_APOLLO_PATH.sub(lambda m: resolve(m.group(0), root),
                                          line))
    return lines


def make_nodes(context):
    root = LaunchConfiguration('apollo_root').perform(context)
    map_dir = resolve(LaunchConfiguration('map_dir').perform(context), root)
    out_dir = tempfile.mkdtemp(prefix='apollo_closed_loop_flags_')
    nodes = []
    for dag_path, plugin in DAGS:
        dag = dag_conf_pb2.DagConfig()
        with open(os.path.join(root, dag_path)) as f:
            text_format.Parse(f.read(), dag)
        module = dag.module_config[0]
        infos = list(module.components) + list(module.timer_components)
        config = infos[0].config
        if config.flag_file_path:
            lines = flatten_flagfile(config.flag_file_path, root)
            # later flags win in gflags: put the overrides last
            lines.append('--map_dir=%s' % map_dir)
            lines.append('--use_navigation_mode=false')
            flagfile = os.path.join(out_dir, '%s.flags' % config.name)
            with open(flagfile, 'w') as f:
                f.write('\n'.join(lines) + '\n')
            config.flag_file_path = flagfile
        if config.config_file_path:
            config.config_file_path = resolve(config.config_file_path, root)
        nodes.append(ComposableNode(
            package='apollo', plugin=plugin, name=config.name,
            parameters=[{'component_config':
                         text_format.MessageToString(config, as_one_line=True)}]))
    container = ComposableNodeContainer(
        name='apollo_closed_loop', namespace='',
        package='rclcpp_components', executable='component_container_mt',
        composable_node_descriptions=nodes, output='screen')
    return [container]


def generate_launch_description():
    default_root = os.environ.get('APOLLO_ROOT', '/apollo')
    return LaunchDescription([
        DeclareLaunchArgument('apollo_root', default_value=default_root),
        DeclareLaunchArgument('map_dir',
                              default_value='/apollo/modules/map/data/demo'),
        # IPOPT (planning smoothers) with an OpenMP MUMPS deadlocks when more
        # than one OpenMP thread is used (seen with conda-forge MUMPS 5.8).
        SetEnvironmentVariable('OMP_NUM_THREADS', '1'),
        SetEnvironmentVariable('APOLLO_ROOT',
                               LaunchConfiguration('apollo_root')),
        OpaqueFunction(function=make_nodes),
    ])
