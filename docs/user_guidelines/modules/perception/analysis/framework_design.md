# Framework Design

In the original perception framework, the four parts lidar, camera, radar, and fusion were defined in four modules. The functions inside each lidar and camera module were complex, so the learning cost was high. After the perception framework was split, the module dependencies are clear. The lidar and camera perception flows are divided into multiple modules with linear dependencies; the radar and fusion functions are within a single module, with functions as shown in the block diagram. Under the new perception framework, each lidar and camera module is more lightweight than before, which is better for secondary development. Each component can be started and debugged independently.

![apollo perception block diagram.drawio.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/apollo%E6%84%9F%E7%9F%A5%E6%A1%86%E5%9B%BE.drawio_830f71c.png)

Developers can use the perception 2.0 components as needed. To make extension easier, for example replacing a model, developers can also add new models as plugins.

- Green marks the parts that users often need to replace. When developing a new algorithm or function, based on the existing framework you only need to replace the part you want to replace, without having to care about message flow and the framework.
- Blue marks the parts that users do not need to replace often. The module integrates all algorithms and its functions work as a whole; functions are modified directly through configuration, and if you want to replace it, you can write a new component.

## LiDAR Detection Flow

LiDAR detection is used for 3D object detection. Its input is the LiDAR point cloud, and its output is the type and coordinates of the detected objects. Developers mostly focus on three components: pointcloud_map_based_roi, lidar_detection, and lidar_detection_filter. Among them, pointcloud_map_based_roi involves the related ROI filtering rules, and the lidar_detection component handles model deployment during object detection. Apollo and paddle3D have connected the entire workflow from model training to deployment in Apollo, which will also be covered in later course practice. The Apollo LiDAR perception module provides 4 object detection models, and developers can select different detectors through configuration files to verify detection results. With the latest algorithm plugins, developers can focus more on the algorithm itself without paying too much attention to the framework implementation. The lidar_detection_filter component handles filtering of detected obstacles. LiDAR tracking is used to track the 3D objects detected above. Its input is the LiDAR point cloud detection result, and its output is the ID of the tracked object.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_464498a.png)

![image (1).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image%20%281%29_77ac62a.png)

## Camera Detection Flow

In the camera detection flow, developers mainly focus on the camera_detection_2d, camera_detection_3d, and camera_tracking components. The camera_detection_2d and camera_detection_3d components cover 2D/3D model deployment, preprocessing, and other functions, so developers can replace models and develop related functions as needed. The functions in camera_tracking are more integrated, and developers can modify the configuration items in the component according to the tracking strategy. The camera_detection_2d detection flow outputs 2D bounding boxes; compared with the camera_detection_3d component, which directly outputs 3D detection results, it has an additional step of converting 2D information into 3D obstacle information.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_471b4f3.png)

## Fusion Flow

The multi-sensor fusion module fuses the detection results of multiple sensors. Its input is the detected objects from LiDAR, camera, and radar, and its output is the fused result. The roles of several important base classes are as follows: BaseFusionSystem is the entry point of the fusion module and initializes the ProbabilisticFusion probabilistic fusion algorithm; BaseTracker updates the state of the tracked trajectories; BaseGatekeeper is used to decide whether to publish the tracked obstacles to the final channel.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_2db0415.png)

The detailed functional block diagram of the multi-sensor fusion component is shown below:

![image (2).png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image%20%282%29_6811047.png)

## Radar Detection Flow

radar_detection is a complete functional component that includes preprocessing, detection, tracking, and the corresponding filtering functions.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_400c283.png)

## Traffic Light Detection Flow

In the traffic light detection flow, developers focus on the two components traffic_light_detection and traffic_light_recognition, which correspond to model deployment and other aspects of traffic light detection and recognition respectively.

![image.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/image_ead83a9.png)
