#!/usr/bin/env python3
# ****************************************************************************
# Copyright 2019 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ****************************************************************************
"""Cyber parameter server / client."""

from conftest import requires_ros

from cyber.python.cyber_py3 import parameter

pytestmark = requires_ros


def test_parameter_values():
    p = parameter.Parameter("author_name", "WanderingEarth")
    assert p.name() == "author_name"
    assert p.type_name() == "STRING"
    assert p.as_string() == "WanderingEarth"
    assert p.debug_string() == \
        '{name: "author_name", type: "STRING", value: "WanderingEarth"}'
    assert parameter.Parameter("age", 5000).as_int64() == 5000
    assert parameter.Parameter("score", 888.88).debug_string() == \
        '{name: "score", type: "DOUBLE", value: 888.880000}'
    assert parameter.Parameter("flag", True).as_bool() is True
    assert parameter.Parameter("flag", True).type_name() == "BOOL"
    empty = parameter.Parameter("unset")
    assert empty.type_name() == "" and empty.value() is None
    assert empty.debug_string() == '{name: "unset", type: "", value: not set}'


def test_params(cyber_env, unique):
    cyber = cyber_env
    server_name = "global_parameter_service_" + unique
    param1 = parameter.Parameter("author_name", "WanderingEarth")
    param2 = parameter.Parameter("author_age", 5000)
    param3 = parameter.Parameter("author_score", 888.88)

    test_node = cyber.Node(server_name)
    srv = parameter.ParameterServer(test_node)

    node_handle = cyber.Node("service_client_node")
    clt = parameter.ParameterClient(node_handle, server_name)
    assert clt.set_parameter(param1)
    assert clt.set_parameter(param2)
    assert clt.set_parameter(param3)

    assert len(clt.get_paramslist()) == 3
    assert len(srv.get_paramslist()) == 3
    assert clt.get_parameter("author_age").as_int64() == 5000
    assert clt.get_parameter("author_score").as_double() == 888.88
    assert clt.get_parameter("missing").type_name() == ""

    srv.set_parameter(parameter.Parameter("server_side", "yes"))
    assert clt.get_parameter("server_side").as_string() == "yes"
    assert srv.get_parameter("author_name").as_string() == "WanderingEarth"
    services = cyber.ServiceUtils.get_services(0)
    for suffix in ('get_parameter', 'set_parameter', 'list_parameters'):
        assert '/%s/cyber_parameters/%s' % (server_name, suffix) in services

    # The parameters are ROS 2 parameters of the server's node as well ...
    ros_node = test_node.get_ros_node()
    assert ros_node.get_parameter('author_name').value == 'WanderingEarth'
    assert ros_node.get_parameter('author_age').value == 5000
    assert ros_node.get_parameter('author_score').value == 888.88
    assert ros_node.get_parameter('server_side').value == 'yes'
    # ... and `ros2 param set` reaches the Cyber side.
    import rclpy.parameter
    results = ros_node.set_parameters(
        [rclpy.parameter.Parameter('author_age', value=5001)])
    assert results[0].successful
    assert srv.get_parameter('author_age').as_int64() == 5001
    assert clt.get_parameter('author_age').as_int64() == 5001
    # The standard ROS 2 parameter services of the node are untouched.
    ros_services = dict(ros_node.get_service_names_and_types())
    assert ros_services['/%s/list_parameters' % server_name] == \
        ['rcl_interfaces/srv/ListParameters']


def test_client_without_server(cyber_env, unique):
    """No server: calls fail after the Cyber 5 s request timeout."""
    cyber = cyber_env
    clt = parameter.ParameterClient(cyber.Node("lonely_client"),
                                    "nobody_" + unique)
    assert not clt.set_parameter(parameter.Parameter("a", 1))
    assert clt.get_paramslist() == []
