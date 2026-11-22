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
# -*- coding: utf-8 -*-
"""Cyber parameter server/client in pure Python.

This keeps Cyber's own protocol (cyber/parameter/parameter_server.cc), now
carried by ROS 2 services with native types (apollo_msgs/srv/CyberProtoParamName
and the like, generated from the protobuf request/response messages):

  <server node name>/cyber_parameters/get_parameter    ParamName -> Param
  <server node name>/cyber_parameters/set_parameter    Param     -> BoolResult
  <server node name>/cyber_parameters/list_parameters  NodeName  -> Params

(service names mapped with naming.channel_to_topic()), so Python and C++
parameter servers/clients built on the Cyber API interoperate. The services
sit in a sub-namespace of the node, next to the standard ROS 2 parameter
services (<node>/list_parameters, ...) every node has.

The parameters of a ParameterServer are also ordinary ROS 2 parameters of its
node: `ros2 param list|get|set <node>` reads and changes them, and a
parameter given to the node at start (launch file, `--ros-args -p`) takes
the place of the first value the program sets.

Supported value types: bool, int, float and str (PROTOBUF parameters are not
supported from Python, as before).
"""

import sys
import threading

from . import _cyber_protos as _pb

GET_PARAMETER_SERVICE_NAME = 'get_parameter'
SET_PARAMETER_SERVICE_NAME = 'set_parameter'
LIST_PARAMETERS_SERVICE_NAME = 'list_parameters'

_NOT_SET = _pb.ParamType['NOT_SET']
_BOOL = _pb.ParamType['BOOL']
_INT = _pb.ParamType['INT']
_DOUBLE = _pb.ParamType['DOUBLE']
_STRING = _pb.ParamType['STRING']


PARAMETER_SERVICE_NAMESPACE = 'cyber_parameters'


def _service_name(node_name, service):
    return node_name + '/' + PARAMETER_SERVICE_NAMESPACE + '/' + service


def _ros_value(param):
    """Value of a Cyber Param as a ROS 2 parameter value (None when the type
    has no ROS 2 counterpart)."""
    if param.type == _BOOL:
        return bool(param.bool_value)
    if param.type == _INT:
        return int(param.int_value)
    if param.type == _DOUBLE:
        return float(param.double_value)
    if param.type == _STRING:
        return param.string_value
    return None


def _set_from_ros(param, value):
    """Puts a ROS 2 parameter value into a Cyber Param. Returns success."""
    if isinstance(value, bool):
        param.type, param.type_name, param.bool_value = _BOOL, 'BOOL', value
    elif isinstance(value, int):
        param.type, param.type_name, param.int_value = _INT, 'INT', value
    elif isinstance(value, float):
        param.type, param.type_name, param.double_value = \
            _DOUBLE, 'DOUBLE', value
    elif isinstance(value, str):
        param.type, param.type_name, param.string_value = \
            _STRING, 'STRING', value
    else:
        return False
    return True


class Parameter(object):

    """
    Class for Parameter wrapper.
    """

    def __init__(self, name, value=None):
        self.param = _pb.Param()
        if isinstance(name, _pb.Param):
            self.param.CopyFrom(name)
            return
        self.param.name = name or ''
        if value is None:
            self.param.type = _NOT_SET
        elif isinstance(value, bool):
            self.param.bool_value = value
            self.param.type = _BOOL
            self.param.type_name = 'BOOL'
        elif isinstance(value, int):
            self.param.int_value = value
            self.param.type = _INT
            self.param.type_name = 'INT'
        elif isinstance(value, float):
            self.param.double_value = value
            self.param.type = _DOUBLE
            self.param.type_name = 'DOUBLE'
        elif isinstance(value, str):
            self.param.string_value = value
            self.param.type = _STRING
            self.param.type_name = 'STRING'
        else:
            raise TypeError('type is not supported: %s' % type(value))

    def type(self):
        """Cyber ParamType value (cyber.proto.ParamType)."""
        return self.param.type

    def type_name(self):
        """
        return Parameter typename
        """
        return self.param.type_name

    def descriptor(self):
        """
        return Parameter descriptor
        """
        return self.param.proto_desc

    def name(self):
        """
        return Parameter name
        """
        return self.param.name

    def debug_string(self):
        """
        return Parameter debug string
        """
        param = self.param
        value = ''
        if param.type == _BOOL:
            value = 'true' if param.bool_value else 'false'
        elif param.type == _INT:
            value = str(param.int_value)
        elif param.type == _DOUBLE:
            value = '%f' % param.double_value  # std::to_string(double)
        elif param.type == _STRING:
            value = '"%s"' % param.string_value
        elif param.type == _NOT_SET:
            value = 'not set'
        return '{name: "%s", type: "%s", value: %s}' % (
            param.name, param.type_name, value)

    def as_string(self):
        """
        return native value
        """
        return self.param.string_value if self.param.type == _STRING else ''

    def as_double(self):
        """
        return native value
        """
        return self.param.double_value if self.param.type == _DOUBLE else 0.0

    def as_int64(self):
        """
        return native value
        """
        return self.param.int_value if self.param.type == _INT else 0

    def as_bool(self):
        """
        return native value
        """
        return self.param.bool_value if self.param.type == _BOOL else False

    def value(self):
        """Native Python value (None when not set)."""
        kind = self.param.type
        if kind == _BOOL:
            return self.param.bool_value
        if kind == _INT:
            return self.param.int_value
        if kind == _DOUBLE:
            return self.param.double_value
        if kind == _STRING:
            return self.param.string_value
        return None


class ParameterClient(object):

    """
    Class for ParameterClient wrapper.
    """

    ##
    # @brief constructor the ParameterClient by a node and the parameter server node name.
    #
    # @param node a node to create client.
    # @param server_node_name the parameter server's node name.
    def __init__(self, node, server_node_name):
        self.node = node
        self.server_node_name = server_node_name
        self._get = node.create_client(
            _service_name(server_node_name, GET_PARAMETER_SERVICE_NAME),
            _pb.ParamName, _pb.Param)
        self._set = node.create_client(
            _service_name(server_node_name, SET_PARAMETER_SERVICE_NAME),
            _pb.Param, _pb.BoolResult)
        self._list = node.create_client(
            _service_name(server_node_name, LIST_PARAMETERS_SERVICE_NAME),
            _pb.NodeName, _pb.Params)

    def set_parameter(self, param):
        """
        set parameter, param is Parameter.
        """
        response = self._set.send_request(param.param)
        return bool(response is not None and response.value)

    def get_parameter(self, param_name):
        """
        get Parameter by param name param_name.
        """
        request = _pb.ParamName()
        request.value = param_name
        response = self._get.send_request(request)
        if response is None or response.type == _NOT_SET:
            return Parameter(None)
        return Parameter(response)

    def get_paramslist(self):
        """
        get all params of the server_node_name parameterserver.
        """
        request = _pb.NodeName()
        request.value = self.node.name
        response = self._list.send_request(request)
        if response is None:
            return []
        return [Parameter(p) for p in response.param]


class ParameterServer(object):

    """
    Class for ParameterServer wrapper.
    """

    ##
    # @brief constructor the ParameterServer by the node object.
    #
    # @param node the node to support the parameter server.
    def __init__(self, node):
        self.node = node
        self._lock = threading.Lock()
        self._params = {}
        self._mirroring = threading.local()
        self._ros_node = None
        get_ros_node = getattr(node, 'get_ros_node', None)
        if get_ros_node is not None:
            self._ros_node = get_ros_node()
        if self._ros_node is not None:
            # `ros2 param set` on a parameter of this server changes it here.
            self._ros_node.add_on_set_parameters_callback(self._on_ros_set)
        name = node.name
        node.create_service(
            _service_name(name, GET_PARAMETER_SERVICE_NAME),
            _pb.ParamName, _pb.Param, self._on_get)
        node.create_service(
            _service_name(name, SET_PARAMETER_SERVICE_NAME),
            _pb.Param, _pb.BoolResult, self._on_set)
        node.create_service(
            _service_name(name, LIST_PARAMETERS_SERVICE_NAME),
            _pb.NodeName, _pb.Params, self._on_list)

    def _on_get(self, request):
        response = _pb.Param()
        with self._lock:
            param = self._params.get(request.value)
            if param is not None:
                response.CopyFrom(param)
        # An unset parameter yields an empty Param (type NOT_SET).
        return response

    def _on_set(self, request):
        self._store(request)
        response = _pb.BoolResult()
        response.value = True
        return response

    def _store(self, param):
        """Stores a parameter and shows it as a ROS 2 parameter of the node."""
        stored = _pb.Param()
        stored.CopyFrom(param)
        with self._lock:
            first_time = param.name not in self._params
        value = _ros_value(param)
        ros_node = self._ros_node
        if ros_node is not None and param.name and value is not None:
            from rcl_interfaces.msg import ParameterDescriptor
            import rclpy.parameter
            self._mirroring.active = True
            try:
                if not ros_node.has_parameter(param.name):
                    ros_node.declare_parameter(
                        param.name, value, ParameterDescriptor(
                            description='Cyber parameter (ParameterServer)',
                            dynamic_typing=True))
                else:
                    given = ros_node.get_parameter(param.name).value
                    # Given to the node at start (launch file, -p): that
                    # value takes the place of the first one the program sets.
                    if not (first_time and given is not None and
                            given != value and
                            _set_from_ros(stored, given)):
                        ros_node.set_parameters([rclpy.parameter.Parameter(
                            param.name, value=value)])
            except Exception as e:  # pylint: disable=broad-except
                sys.stderr.write('cyber: parameter %s is not shown as a '
                                 'ROS 2 parameter: %s\n' % (param.name, e))
            finally:
                self._mirroring.active = False
        with self._lock:
            self._params[stored.name] = stored

    def _on_ros_set(self, parameters):
        from rcl_interfaces.msg import SetParametersResult
        result = SetParametersResult(successful=True)
        if getattr(self._mirroring, 'active', False):
            return result
        with self._lock:
            for parameter in parameters:
                stored = self._params.get(parameter.name)
                if stored is None:
                    continue
                if not _set_from_ros(stored, parameter.value):
                    return SetParametersResult(
                        successful=False,
                        reason="unsupported type for Cyber parameter '%s'" %
                        parameter.name)
        return result

    def _on_list(self, request):
        response = _pb.Params()
        with self._lock:
            for param in self._params.values():
                response.param.add().CopyFrom(param)
        return response

    def set_parameter(self, param):
        """
        set parameter, param is Parameter.
        """
        self._store(param.param)

    def get_parameter(self, param_name):
        """
        get Parameter by param name param_name.
        """
        with self._lock:
            param = self._params.get(param_name)
            return Parameter(param) if param is not None else Parameter(None)

    def get_paramslist(self):
        """
        get all params of this parameterserver.
        """
        with self._lock:
            return [Parameter(p) for p in self._params.values()]

