# Plugin Development

## Parameter Directory Structure

First, the configuration directory of each component, taking the LiDAR detection module as an example:

```bash
├── lidar_detection
    ├── conf            // component configuration files
    ├── dag             // component dag files
    ├── data            // function configuration files
    ├── detector
    │   ├── center_point_detection
    │   ├── cnn_segmentation
    │   ├── mask_pillars_detection
    │   └── point_pillars_detection
    ├── interface
    ├── proto
    ├── lidar_detection_component.cc
    ├── lidar_detection_component.h
    ├── cyberfile.xml   // package configuration file
    ├── README.md
    └── BUILD
```

Common configuration directory: the common configuration of multiple components is placed under the `modules/perception/data` path.

```bash
modules/perception/data
├── BUILD
├── conf    // perception common configuration, e.g. some singletons
├── cyberfile.xml
├── flag    // all gflags command-line arguments of the perception module, used by dag
├── models  // model storage path
└── params  // sensor intrinsic and extrinsic parameters
```

## Global Configuration

Global configuration is defined in gflag files, including `modules/perception/common/perception_gflags.h`.

| Parameter                       | Default Value                                                                                                                | Meaning                                 |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------- | ------------------------------------ |
| obs_sensor_intrinsic_path    | /apollo/modules/perception/data/params                                                                                | Sensor intrinsic parameter path                       |
| obs_sensor_meta_file         | sensor_meta.pb.txt                                                                                                    | Sensor meta file name                     |
| enable_base_object_pool      | enable_base_object_pool                                                                                               | Enable the object pool                           |
| config_manager_path          | ./                                                                                                                    | Configuration manager path                       |
| work_root                    | /apollo/modules/perception                                                                                            | Working directory                             |
| onnx_obstacle_detector_model | /apollo/modules/perception/camera/lib/obstacle/detector/yolov4/model/yolov4_1_3_416_416.onnx                          | Object detection model                         |
| onnx_test_input_path         | /apollo/modules/perception/inference/onnx/testdata/dog.jpg                                                            | Object detection test path                     |
| onnx_test_input_name_file    | /apollo/modules/perception/inference/onnx/testdata/coco.names                                                         | Object detection test file                     |
| onnx_prediction_image_path   | /apollo/modules/perception/inference/onnx/testdata/prediction.jpg                                                     | Object detection prediction image path                 |
| num_classes                  | 80                                                                                                                    | Number of object types                         |
| torch_detector_model         | /apollo/modules/perception/camera/lib/obstacle/detector/yolov4/model/yolov4.pt                                        | torch detection model path                    |
| lidar_sensor_name            | velodyne128                                                                                                           | LiDAR sensor name                      |
| use_trt                      | false                                                                                                                 | Whether to use tensorrt                     |
| trt_precision                | 1                                                                                                                     | tensorrt precision, 0: 32float, 1: kInt8 |
| trt_use_static               | true                                                                                                                  | Whether to load the tensorrt graph optimization from a disk path     |
| use_calibration              | true                                                                                                                  | Whether to use the calibration table                       |
| use_dynamicshape             | true                                                                                                                  | Whether to use dynamic shapes                     |
| collect_shape_info           | true                                                                                                                  | Whether to collect dynamic shape information                 |
| dynamic_shape_file           | /apollo/modules/perception/lidar_detection/data/center_point_paddle/pillar_20_0625/collect_shape_info_3lidar_20.pbtxt | Dynamic shape file path                         |
| object_template_file         | object_template.pb.txt                                                                                                | Object template configuration file                     |
| hdmap_sample_step            | 5                                                                                                                     | HD map sampling rate                     |
| scene_manager_file           | scene_manager.conf                                                                                                    | Scene manager configuration file                   |
| roi_service_file             | roi_service.conf                                                                                                      | ROI service configuration file                      |
| ground_service_file          | ground_service.conf                                                                                                   | Ground detection service configuration file                 |

modules/perception/common/onboard/common_flags/common_flags.h

| Parameter                     | Default Value | Meaning                                |
| -------------------------- | ------ | ----------------------------------- |
| obs_enable_hdmap_input     | true   | Enable hdmap input for the roi filter        |
| obs_enable_visualization   | false  | Whether to send visualization messages                  |
| obs_screen_output_dir      | ./     | Output directory, used to save visualization screenshots    |
| obs_benchmark_mode         | false  | Whether to enable benchmark mode, default false |
| obs_save_fusion_supplement | false  | Whether to save fusion supplementary data, default false     |
| start_visualizer           | false  |                                     |

### Modifying the Configuration

Global configuration is modified in `/apollo/modules/perception/data/flag/perception_common.flag`. For example, to set the value of obs_enable_hdmap_input to false, add the following to the file:

```bash
--obs_enable_hdmap_input=false
```

## Component Configuration

The configuration of each component is in the conf and data directories of the corresponding component; just modify the values in the corresponding files directly. Taking the 3D object detection module (lidar_detection) as an example.

lidar_detection module configuration file paths:

- modules/perception/lidar_detection/conf
- modules/perception/lidar_detection/data

ModelInfo configuration, file path: modules/perception/common/proto/model_info.proto:

| Parameter Type | Parameter                 | Default Value | Meaning                          |
| -------- | ---------------------- | ------ | ----------------------------- |
| string   | name                   | /      | Model name, same as the folder name under models/ |
| string   | framework              | /      | Model inference framework                  |
| string   | ModelFile.proto_file   | /      | Model network structure                  |
| string   | ModelFile.weight_file  | /      | Model weight file                  |
| string   | ModelFile.anchors_file | /      | anchor size                   |
| string   | ModelBlob.inputs       | /      | Names and dimensions of the model input data        |
| int32    | ModelBlob.outputs      | /      | Names and dimensions of the model output data        |

PointCloudPreProcess:

| Parameter Type | Parameter                       | Default Value     | Meaning                                             |
| -------- | ---------------------------- | ---------- | ------------------------------------------------ |
| int32    | gpu_id                       | 0          | GPU id                                          |
| double   | normalizing_factor           | 255        | Scale factor for intensity normalization                             |
| int32    | num_point_feature            | 4          | Number of features per point                                 |
| bool     | enable_ground_removal        | false      | Whether to filter out ground points                                 |
| double   | ground_removal_height        | -1.5       | Filter out points whose z value is less than the threshold                            |
| bool     | enable_downsample_beams      | false      | Whether to filter the point cloud by beam id                    |
| int32    | downsample_beams_factor      | 4          | Keep points whose beam id is a multiple of downsample_beams_factor |
| bool     | enable_downsample_pointcloud | false      | Whether to filter the point cloud by voxel                            |
| double   | downsample_voxel_size_x      | 0.01       | Length of the voxel in the x direction when filtering                           |
| double   | downsample_voxel_size_y      | 0.01       | Length of the voxel in the y direction when filtering                           |
| double   | downsample_voxel_size_z      | 0.01       | Length of the voxel in the z direction when filtering                           |
| bool     | enable_fuse_frames           | false      | Whether to fuse multiple point cloud frames                                 |
| int32    | num_fuse_frames              | 5          | Number of frames to fuse                                   |
| double   | fuse_time_interval           | 0.5        | Time interval for fusing point clouds                               |
| bool     | enable_shuffle_points        | false      | Whether to shuffle the point cloud indices                                 |
| int32    | max_num_points               | 2147483647 | Maximum number of points allowed                               |
| bool     | reproduce_result_mode        | false      | Whether to enable reproducible-results mode                             |
| bool     | enable_roi_outside_removal   | false      | Whether to filter out points outside the ROI before feeding the model          |

PointCloudPostProcess

| Parameter Type | Parameter                 | Default Value | Meaning                             |
| -------- | ---------------------- | ------ | -------------------------------- |
| float    | score_threshold        | 0.5    | Confidence threshold                       |
| float    | nms_overlap_threshold  | 0.5    | IoU threshold for NMS                     |
| int32    | num_output_box_feature | 7      | Number of attributes of the output obstacles             |
| float    | bottom_enlarge_height  | 0.25   | Range to expand upward when obtaining the object's actual point cloud |
| float    | top_enlarge_height     | 0.25   | Range to expand downward when obtaining the object's actual point cloud |
| float    | width_enlarge_value    | 0      | Range to expand in width when obtaining the object's actual point cloud |
| float    | length_enlarge_value   | 0      | Range to expand in length when obtaining the object's actual point cloud |

cnnseg configuration

modules/perception/lidar_detection/detector/cnn_segmentation/proto/model_param.proto

<table>
<thead>
  <tr>
    <th colspan="2">Parameter Type</th>
    <th>Parameter</th>
    <th>Default Value</th>
    <th>Meaning</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td colspan="2">ModelInfo</td>
    <td>info</td>
    <td>/</td>
    <td>Basic model configuration</td>
  </tr>
  <tr>
    <td rowspan="7">FeatureParam</td>
    <td>float</td>
    <td>point_cloud_range</td>
    <td>90</td>
    <td>Point cloud range</td>
  </tr>
  <tr>
    <td>uint32</td>
    <td>width</td>
    <td>864</td>
    <td>BEV width</td>
  </tr>
  <tr>
    <td>uint32</td>
    <td>height</td>
    <td>864</td>
    <td>BEV height</td>
  </tr>
  <tr>
    <td>float</td>
    <td>min_height</td>
    <td>-5.0</td>
    <td>Minimum z value of the point cloud</td>
  </tr>
  <tr>
    <td>float</td>
    <td>max_height</td>
    <td>5.0</td>
    <td>Maximum z value of the point cloud</td>
  </tr>
  <tr>
    <td>bool</td>
    <td>use_intensity_feature</td>
    <td>true</td>
    <td>Whether to use the intensity feature</td>
  </tr>
  <tr>
    <td>bool</td>
    <td>use_constant_feature</td>
    <td>false</td>
    <td>Whether to use the constant feature</td>
  </tr>
  <tr>
    <td colspan="2">bool</td>
    <td>do_classification</td>
    <td>true</td>
    <td>Whether to predict classification information</td>
  </tr>
  <tr>
    <td colspan="2">bool</td>
    <td>do_heading</td>
    <td>true</td>
    <td>Whether to predict heading angle information</td>
  </tr>
  <tr>
    <td colspan="2">PointCloudPreProcess</td>
    <td>preprocess</td>
    <td>/</td>
    <td>Preprocessing information</td>
  </tr>
  <tr>
    <td>SppEngineConfig</td>
    <td>float</td>
    <td>height_gap</td>
    <td>0.5</td>
    <td>Height gap</td>
  </tr>
  <tr>
    <td colspan="2">bool</td>
    <td>remove_ground_points</td>
    <td>true</td>
    <td>Whether to filter out ground points in obstacles</td>
  </tr>
  <tr>
    <td colspan="2">float</td>
    <td>objectness_thresh</td>
    <td>0.5</td>
    <td>Threshold of objectness</td>
  </tr>
  <tr>
    <td colspan="2">float</td>
    <td>confidence_thresh</td>
    <td>0.1</td>
    <td>Threshold of confidence</td>
  </tr>
  <tr>
    <td colspan="2">float</td>
    <td>height_thresh</td>
    <td>0.5</td>
    <td>Height threshold</td>
  </tr>
  <tr>
    <td colspan="2">uint32</td>
    <td>min_pts_num</td>
    <td>3</td>
    <td>Minimum number of points of an object</td>
  </tr>
  <tr>
    <td colspan="2">float</td>
    <td>confidence_range</td>
    <td>85</td>
    <td>Confidence range</td>
  </tr>
  <tr>
    <td colspan="2">bool</td>
    <td>fill_recall_with_segmentor</td>
    <td>true</td>
    <td>Whether to use background segmentation</td>
  </tr>
</tbody>
</table>

The centerpoint configuration is in modules/perception/lidar_detection/data/center_point_param.pb.txt; modify the values in it to select different configurations.

| Parameter Type              | Parameter            | Default Value | Meaning                        |
| --------------------- | ----------------- | ------ | --------------------------- |
| ModelInfo             | info              | /      | General model configuration                |
| PointCloudPreProcess  | preprocess        | /      | Preprocessing                      |
| PointCloudPostProcess | postprocess       | /      | Postprocessing                      |
| int32                 | point2box_max_num | 5      | Maximum number of boxes each point can belong to |
| float                 | quantize          | 0.2    |                             |

## LiDAR Perception Parameter Development Mode

### Entering the Docker Environment

```bash
# Enter the container
aem enter

# Download and install dependency packages: this pulls and installs all dependency packages listed in cyberfile.xml under the core directory
buildtool build --gpu
```

### Starting Dreamview+

```bash
aem bootstrap start --plus
```

> Note: The plus parameter means starting dreamview+. To start the old version of dreamview, simply remove the --plus parameter.

### Modifying Parameters

#### Modifying the Model Output Confidence

By adjusting the model output confidence, you can learn how to adjust the detection output of the model. The higher the model's confidence threshold, the more accurate the detection results, but the lower the recall; the lower the confidence threshold, the higher the recall and the lower the precision. Therefore, precision and recall need to be balanced to achieve a good result.

First, check whether the configuration in `/apollo/modules/perception/lidar_detection/conf/lidar_detection_config.pb.txt` is CenterPointDetection.

```bash
plugin_param {
  name: "CenterPointDetection"
  config_path: "perception/lidar_detection/data"
  config_file: "center_point_param.pb.txt"
}
```

Then modify score_threshold in `/apollo/modules/perception/lidar_detection/data/center_point_param.pb.txt`:

```bash
postprocess {
  score_threshold: 0.25      // detection result confidence
  num_output_box_feature: 7
  bottom_enlarge_height: 0.1  // 0.25 -> 0.1
  top_enlarge_height: 0.25
  width_enlarge_value: 0
  length_enlarge_value: 0
}
```

![modify model output.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E4%BF%AE%E6%94%B9%E6%A8%A1%E5%9E%8B%E8%BE%93%E5%87%BA_9a82260.png)

#### Modifying the ROI Range

Modifying the ROI parameters filters out, by rule, obstacles that do not need attention, avoiding interference with the autonomous vehicle.

Find the file `/apollo/modules/perception/lidar_detection_filter/data/roi_boundary_filter.pb.txt` and modify the parameters:

```bash
distance_to_boundary_threshold: 0.0
confidence_threshold: 0.0
cross_roi_threshold: 0.6
inside_threshold: -1.0
```

![modify ROI.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E4%BF%AE%E6%94%B9ROI_108597c.png)

### Starting the LiDAR Perception Program

Select the corresponding vehicle configuration:

![start the lidar perception program.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E5%90%AF%E5%8A%A8lidar%E6%84%9F%E7%9F%A5%E7%A8%8B%E5%BA%8F_bff5aaf.png)

Start the transform module:

![start the transform module 1.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E5%90%AF%E5%8A%A8transform%E6%A8%A1%E5%9D%971_a46d7a4.png)

Start the lidar perception module:

![start the lidar perception module.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E5%90%AF%E5%8A%A8lidar%20%E6%84%9F%E7%9F%A5%E6%A8%A1%E5%9D%97_60b8392.png)

Download and play the perception package: in the lower-left corner of Dreamview+, click Resource Manager, download the sensor_rgb data package, and after the download completes, select the sensor_rgb data package and click play.

![lidar.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/lidar_0106d45.png)

![lidar 2.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/lidar%202_c46da07.png)

View the LiDAR detection results in dreamview+:

![view the lidar detection result.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%9F%A5%E7%9C%8Blidar%E6%A3%80%E6%B5%8B%E6%95%88%E6%9E%9C_add2416.png)
