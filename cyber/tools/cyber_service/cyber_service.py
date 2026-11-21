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
"""cyber_service: inspect Apollo services (apollo_msgs/srv/ProtoService)."""

import os
import sys
from optparse import OptionParser

from cyber.python.cyber_py3 import _cyber_protos
from cyber.python.cyber_py3 import cyber
from cyber.python.cyber_py3.naming import channel_to_topic


def print_service_info(service_name, sleep_s=2):
    roleattr_rawdata = cyber.ServiceUtils.get_service_attr(service_name,
                                                           sleep_s)
    if not roleattr_rawdata:
        print("no service named: %s" % channel_to_topic(service_name))
        return
    msg = _cyber_protos.RoleAttributes()
    msg.ParseFromString(roleattr_rawdata)
    print(msg.service_name)
    if msg.process_id:
        print("\tprocessid\t", msg.process_id)
    print("\tnodename\t", msg.node_name)
    if msg.host_name:
        print("\thostname\t", msg.host_name)
    print("")


def _service_cmd_info(argv):
    """
    Command-line parsing for 'cyber_service info' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_service info servicename ")
    parser.add_option("-a", "--all",
                      dest="all_services", default=False,
                      action="store_true",
                      help="display all services info")

    (options, args) = parser.parse_args(args)
    if len(args) == 0 and not options.all_services:
        parser.error("servicename must be specified")
    elif len(args) > 1:
        parser.error("you may only specify one service name")
    elif len(args) == 1:
        print_service_info(args[0])
    elif len(args) == 0 and options.all_services:
        services = cyber.ServiceUtils.get_services()
        for servicename in services:
            print_service_info(servicename, 0)


def print_service_list():
    services = cyber.ServiceUtils.get_services()
    print("The number of services is: ", len(services))
    for service_name in services:
        print(service_name)


def _service_cmd_list(argv):
    """
    Command-line parsing for 'cyber_service list' command.
    """
    args = argv[2:]
    parser = OptionParser(
        usage="usage: cyber_service list")
    (options, args) = parser.parse_args(args)
    if len(args) > 0:
        parser.error("param is too much")
    print_service_list()


def _printallusage():
    print("""cyber_service is a command-line tool for printing information about Apollo services (ROS 2 services).

Commands:
\tcyber_service list\tlist active services
\tcyber_service info\tprint information about active service

Type cyber_service <command> -h for more detailed usage, e.g. 'cyber_service info -h'
""")
    sys.exit(getattr(os, 'EX_USAGE', 1))


def main(argv=None):
    argv = list(sys.argv if argv is None else argv)
    if len(argv) == 1 or argv[1] not in ('list', 'info'):
        _printallusage()
    cyber.init("cyber_service")
    try:
        if argv[1] == 'list':
            _service_cmd_list(argv)
        else:
            _service_cmd_info(argv)
    finally:
        cyber.shutdown()


if __name__ == '__main__':
    main()
