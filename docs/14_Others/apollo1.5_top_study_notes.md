# Code analysis of the Apollo system architecture
Taking the localization module as an example; the nodes of the other modules are similar.

## Code analysis
Main files: main.cc, apollo_app.cc, apollo_app.h.
### main.c file
The file contains only one line of code: APOLLO_MAIN(apollo::localization::Localization). The macro APOLLO_MAIN starts the Localization node, and the localization node starts running here. The node here is the same concept as a node in ROS, equivalent to a process.

### APOLLO_MAIN macro analysis:
 * The APOLLO_MAIN macro is defined in the file "modules/common/apollo_app.h".
 * Sets up the log and the SIGINT signal handler; upon receiving the signal, this node is shut down.
 * Creates the module class object, sets the node name, and calls the Spin() function of the base class (ApolloApp).

### ApolloApp class:
 * The Spin() function is a public member function of the ApolloApp class, which is the base class of all module classes.
 * The public members include the name() function, used to get the module name. The Spin() function is used to initialize and start the module node and to shut it down when ROS shuts down. There is also a destructor.
 * The protected members are all virtual interfaces that subclasses override and that are called in the Spin() function; their implementations are inside each specific module. The Init() function loads the module's configuration file and creates the subscribed topics. The Start() function: registers callback functions, which are responsible for the core tasks of the node and are usually triggered by upstream topics or timers. The Stop() function ends the node and is not normally reached. ReportModuleStatus() returns the module status. The apollo_app_sigint_handler() function is the signal handler.
 * The Spin() function uses the init(), start() and stop() functions to implement the module node. This function is generally not overridden, that is, the ApolloApp implementation is used.
