# Perception module analysis
## Function:
Perceive obstacles and predict their motion trajectories.

## Data flow diagram:
![perception data flow](images/perception_node_arch.bmp)

### Input:
 * Point cloud data: /apollo/sensor/velodyne64/compensator/PointCloud2.
 * Coordinate transform relationship: /tf (world->novatel).
 * HD Map.
 * LiDAR extrinsics: /tf_static (novatel->velodyne64).
 * Note: in the minimal simulation system, /tf, /tf_static and the HD map are not provided as input in the form of topics.

### Output:
 * 3D obstacle track messages with heading and velocity: /apollo/perception/obstacles.

## Code analysis:
### main.cc:
 * The main node file.
 * Creates the ROS node Perception through the Apollo top-level macro APOLLO_MAIN.

### perception.h, perception.cc:
 * The main body files of the module.
 * Define and implement the Perception class, which represents the perception module.
 * Name() function: returns the module name, which is also the ROS node name, namely "perception".
 * Init() function: the module initialization function. The AdapterManager::Init() function creates the node handle and the corresponding topics through the configuration file adapter.conf; the lidar_process_->Init() function configures the algorithms for processing LiDAR data; it checks whether the LiDAR has data; and it registers the callback function for point cloud data.
 * OnPointCloud() function: the data processing callback. It determines whether the lidar processing algorithms are ready; the lidar_process_->Process() function processes the LiDAR data in turn using the previously registered LiDAR processing algorithms; the lidar_process_->GeneratePbMsg() function generates obstacle information from the processed data; the AdapterManager::PublishPerceptionObstacles() function publishes the obstacle information based on the adapter architecture.

### lidar_process.h, lidar_process.c:
 * LiDAR data processing files, mainly containing the LiDAR processing algorithms.
 * Define and implement the LidarProcess class, which processes LiDAR data.
 * init() function: the RegistAllAlgorithm() function registers the LiDAR processing functions, namely HdmapROIFilter, CNNSegmentation, MinBoxObjectBuilder and HmObjectTracker, which correspond to the LiDAR data processing pipeline. The InitFrameDependence() function configures the HD map. The InitAlgorithmPlugin() function instantiates roi_filter_, segmentor_, object_builder_ and tracker_ (probably used only as plugins) and calls their init() methods.
 * Process(const sensor_msgs::PointCloud2& message) function: calls the GetVelodyneTrans() function to obtain the velodyne2world coordinate transform, calls the TransPointCloudToPCL() function to generate a PCL point cloud from the LiDAR data and store it in the point_cloud variable. It then calls the Process(timestamp_, point_cloud, velodyne_trans) function.
 * Process(timestamp_, point_cloud, velodyne_trans) function: the hdmap_input_->GetROI() function obtains the ROI from the HD map; the roi_filter_->Filter() function obtains the ROI indices; the segmentor_->Segment() function segments the obstacles according to the indices; the object_builder_->Build() function reconstructs the obstacles, forming hexagons; the tracker_->Track() function predicts the obstacle motion trajectories. This is the core part of the perception algorithm; its four steps use four different algorithms, and the detailed algorithms need further study. After this step, LiDAR data processing is finished, and what remains is to assemble the obstacles and publish them.

### Introduction of the HD map:
 * The HD map is introduced through the hdmap_input.cc and hdmap_input.h files, which define the HDMapInput class to represent and operate on the HD map.

### LiDAR data processing algorithms:
 * Each algorithm corresponds to a directory under obstacle\lidar\, and each algorithm is wrapped as a plugin class. Each algorithm involves fairly deep mathematical theory; each one can later be studied as a separate topic, for example segment, whose core is a CNN algorithm.
