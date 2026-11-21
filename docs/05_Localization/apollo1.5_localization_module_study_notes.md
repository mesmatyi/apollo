# Localization module analysis
## Purpose:
Determine the vehicle's position in real time.

## Localization node flow diagram:
### Node data flow diagram:
![localization data flow](images/localization_node_arch.bmp)
### Input:
 * IMU data: /apollo/sensor/gnss/corrected_imu.
 * GPS data: /apollo/sensor/gnss/odometry.

### Output:
 * Localization information: /apollo/localization/pose.
 * Monitoring information: /apollo/localization/.

## Code analysis:
Main files: localization.cc, localization.h, localization_base.h.
### Localization class:
 * Inherits from the ApolloApp class and has three data members: localization_, a pointer to the concrete localization algorithm instance; localization_factory_, which stores the localization algorithm classes; and config_, which stores the configuration file information.
 * The Init() function registers the RTKLocalization class to localization_factory_ (this class implements the localization algorithm based on the RTK algorithm) and reads the configuration file into config_.
 * The Name() function returns the module name.
 * The Start() function instantiates a localization algorithm object using localization_factory_ and points localization_ to it. If creation succeeds, it calls the localization algorithm's start() function.

### LocalizationBase class:
 * The base class of localization algorithms.
 * The Start() function starts the localization algorithm.
 * The Stop() function terminates the localization algorithm.

### RTKLocalization class:
 * Inherits from the LocalizationBase class.
 * Start() function: creates a timer that checks whether there is GPS and IMU data. On timeout, the OnTimer() function is executed. OnTimer() checks whether there is GPS and IMU data, then calls PublishLocalization() to publish the localization information and updates the data reception time. In PublishLocalization(), PrepareLocalizationMsg() is called first to compute the localization information and fill the published data structure localization; then AdapterManager::PublishLocalization is called to publish the localization topic. The PrepareLocalizationMsg(&localization) function first obtains the GPS data, then obtains the IMU data according to the GPS data timestamp, and then calls ComposeLocalizationMsg() to perform data fusion for localization. In fact, the so-called data fusion simply means that the final localization information is set using the data measured by the GPS and the IMU separately; the only quantity fused from the two devices is the linear velocity. The so-called RTK algorithm should refer only to obtaining GPS localization information from the GPS data; the localization stage actually uses this localization information, while the real RTK algorithm should act on the processing of the GPS data.
* The Stop() function stops the timer, that is, exits localization mode.

## Open questions:
* The localization information should give the vehicle's absolute position in the world. Since GPS data is used, how is this position related to the map?

 Answer: The points in the map may also be absolute positions in the world.

* The purpose of the map_offset quantity: //TODO
