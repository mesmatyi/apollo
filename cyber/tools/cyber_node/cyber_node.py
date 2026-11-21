#!/usr/bin/env python3
# ****************************************************************************
# Copyright 2019 The Apollo Authors. All Rights Reserved.

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
"""cyber_node: inspect Apollo nodes on the ROS 2 graph.

Node names may be given in Cyber form; they are matched against the ROS 2
node names produced by cyber.python.cyber_py3.naming.to_node_name().
"""

import os
import sys
from optparse import OptionParser

from cyber.python.cyber_py3 import _cyber_protos
from cyber.python.cyber_py3 import cyber


def print_node_info(node_name, sleep_s=2):
    raw_data = cyber.NodeUtils.get_node_attr(node_name, sleep_s)
    if not raw_data:
        print("no node named: %s" % node_name)
        return
    msg = _cyber_protos.RoleAttributes()
    msg.ParseFromString(raw_data)
    print("Node:    \t%s" % msg.node_name)
    if msg.process_id:
        print("ProcessId: \t%d" % msg.process_id)
    if msg.host_name:
        print("Hostname:\t%s" % msg.host_name)

    print("[Reading Channels]:")
    for channel in sorted(cyber.NodeUtils.get_readersofnode(node_name, 0)):
        print(channel)
    print("")

    print("[Writing Channels]:")
    for channel in sorted(cyber.NodeUtils.get_writersofnode(node_name, 0)):
        print(channel)
    print("")

    services = sorted(cyber.NodeUtils.get_servicesofnode(node_name, 0))
    if services:
        print("[Services]:")
        for service in services:
            print(service)
        print("")


def _node_cmd_info(argv):
    """
    Command-line parsing for 'cyber_node info' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_node info [OPTION...] [NODE...]")
    parser.add_option("-a", "--all",
                      dest="all_nodes", default=False,
                      action="store_true",
                      help="display all nodes' info")
    (options, args) = parser.parse_args(args)
    if options.all_nodes:
        if len(args) != 0:
            parser.error(
                """"-a/--all" option is expected to run w/o node name(s)""")
        else:
            nodes = cyber.NodeUtils.get_nodes()
            for nodename in nodes:
                print_node_info(nodename, 0)
    elif len(args) == 0:
        parser.error("No node name provided.")
    else:
        for arg in args:
            print_node_info(arg)


def print_node_list():
    nodes = cyber.NodeUtils.get_nodes()
    print("Number of active nodes: {}".format(len(nodes)))
    for node_name in sorted(nodes):
        print(node_name)


def _node_cmd_list(argv):
    """
    Command-line parsing for 'cyber_node list'
    """
    args = argv[2:]
    parser = OptionParser(usage="usage: cyber_node list")
    (options, args) = parser.parse_args(args)
    if len(args) > 0:
        parser.error("too many arguments")
    print_node_list()


def _usage():
    print("""cyber_node is a command-line tool to show information about Apollo nodes (ROS 2 nodes).

Commands:
\tcyber_node list \tList active nodes.
\tcyber_node info \tPrint node info.

Type cyber_node <command> -h for more detailed usage, e.g. 'cyber_node info -h'
""")
    sys.exit(getattr(os, "EX_USAGE", 1))


def main(argv=None):
    argv = list(sys.argv if argv is None else argv)
    if len(argv) == 1 or argv[1] not in ("list", "info"):
        _usage()
    cyber.init("cyber_node")
    try:
        if argv[1] == "list":
            _node_cmd_list(argv)
        else:
            _node_cmd_info(argv)
    finally:
        cyber.shutdown()


if __name__ == '__main__':
    main()
