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
carried by ROS 2 services of type apollo_msgs/srv/ProtoService:

  <server node name>/get_parameter    ParamName -> Param
  <server node name>/set_parameter    Param     -> BoolResult
  <server node name>/list_parameters  NodeName  -> Params

(service names mapped with naming.channel_to_topic()), so Python and C++
parameter servers/clients built on the Cyber API interoperate. Supported
value types: bool, int, float and str (PROTOBUF parameters are not supported
from Python, as before).
"""

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


def _service_name(node_name, service):
    return node_name + '/' + service


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
        with self._lock:
            stored = _pb.Param()
            stored.CopyFrom(request)
            self._params[request.name] = stored
        response = _pb.BoolResult()
        response.value = True
        return response

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
        with self._lock:
            stored = _pb.Param()
            stored.CopyFrom(param.param)
            self._params[param.name()] = stored

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

