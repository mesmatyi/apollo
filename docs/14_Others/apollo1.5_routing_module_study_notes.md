# Routing module analysis
## Function:
Based on a request, output navigation information between two points.
## Input:
 * The user's start and end positions. /apollo/routing_request[pb_msgs/RoutingRequest].
 * HD map.
 * /apollo/monitor[pb_msgs/MonitorMessage].

## Output:
 * The response to the request: /apollo/routing_response[pb_msgs/RoutingResponse], containing the final routing information.
 * /apollo/monitor[pb_msgs/MonitorMessage].

## Node I/O:
![routing data flow](images/routing_node_arch.bmp)
## Routing source code:
### routing.cc and routing.h
 * Name() function: returns the node name.
 * Routing(): constructor, initializes monitor_.
 * Init(): node initialization function.
   * Gets the routing map file routing_map_file.
   * Creates the navigator navigator_ptr_.
   * Reads the navigation configuration file modules/routing/conf/routing.pb.txt into routing_conf_.
   * Creates the nodehandle and the response topics according to the Adapter configuration file modules/routing/conf/adapter.conf.
   * Sets the callback function OnRouting_Request for the routing request topic. Why not use rosservice directly here? //TODO.
 * OnRouting_Request(): handles the request.
   * Takes the routing request as input.
   * Executes the SearchRoute() function, which searches for a route according to routing_request and sets routing_response.
   * Publishes routing_response.

### navigation.cc and navigation.h:
 * SearchRoute() function:
   * Displays the information of the request command.
   * Checks whether the navigator is ready (it is opened at construction).
   * Through the initialization function, converts the points in the request into the format used in the graph and stores them in way_nodes and way_s.
   * Computes the route points result_nodes from way_nodes and way_s using the A star algorithm.
   * Merges the route points result_nodes into the response structure.
 * SearchRouteByStrategy() function:
 Mainly uses the A star algorithm to find a global path.
 * GetWayNodes() function:
 Converts the request into points in the topology graph graph_.
