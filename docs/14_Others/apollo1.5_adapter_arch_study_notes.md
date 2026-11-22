# Code analysis of the adapter design architecture
## Code analysis:
Main files: adapter_manager.cc, adapter_manager.h.

### Analysis of the AdapterManager class:
 * Init(AdapterManagerConfig &configs) function: configures the ROS node according to the AdapterManagerConfig passed in, including creating the NodeHandle and creating subscribed or published topics.
 * Init(const std::string &adapter_config_filename) function: configures the ROS node according to the configuration file path passed in, by calling the Init(AdapterManagerConfig &configs) function.
 * Initialized() function: returns whether this ROS nodehandle has been initialized, which is set in the two functions above.
 * The CreateTimer() function creates a periodic timer with a class member function as the timeout function; it is used in the RTK localization function.
 * node_handle_ pointer: points to the node handle, on which the later topic publishing is all based.
 * Enable##name() function: creates the nodehandle and topic, that is, the function called by init.
 * Add##name##Callback function: adds a callback function. Replace name with the corresponding module name.
 * observers_ variable and Observe() function: //TODO
### Using the AdapterManager class:
 * The adapter can be used to create node handles and their topics, and can be configured through a configuration file.
 * To use it, you only need to create the corresponding configuration file (see /apollo/modules/perception/conf/adapter.conf and the corresponding BUILD file),
 * and call the interface (see the Init() function in /apollo/modules/perception/perception.cc).
