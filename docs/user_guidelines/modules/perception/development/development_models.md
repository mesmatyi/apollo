# Adding a Model

Apollo currently provides the following LiDAR detection models: centerpoint, pointpillars, maskpillars, and cnnseg. Users who want better detection results may wish to deploy their own trained LiDAR detection models in Apollo.

Based on the centerpoint model and the paddle3d open-source repository, this document describes how to add a model in Apollo, including: installing paddle3d, an introduction to training configuration, training, evaluating and exporting centerpoint, and how to deploy centerpoint in Apollo to see the results.

## CenterPoint Training

For the complete centerpoint training workflow, refer to the AI Studio project [[Autonomous Driving in Practice] LiDAR Object Detection Based on Paddle3D&amp;Apollo - PaddlePaddle AI Studio Galaxy Community](https://aistudio.baidu.com/projectdetail/6778231).

### 1. Installing Paddle3D

Paddle3D is PaddlePaddle's official open-source end-to-end deep learning 3D perception suite. It covers many cutting-edge and classic 3D perception models and supports multiple modalities and tasks, helping developers conveniently complete the full workflow of autonomous driving models, from training to deployment.

- [Code repository](https://github.com/PaddlePaddle/Paddle3D).
- [Paddle3D installation tutorial](https://github.com/PaddlePaddle/Paddle3D/blob/develop/docs/installation.md).

### 2. Training Configuration

For the centerpoint-related configuration, see [centerpoint configuration](https://github.com/PaddlePaddle/Paddle3D/tree/develop/configs/centerpoint). The following uses centerpoint_pillars_016voxel_kitti.yml as an example to describe the training configuration.

#### Basic Configuration

| Configuration Name       | Default Value | Meaning                          |
| -------------- | ------ | ----------------------------- |
| batch_size     | 4      | Number of point cloud frames in one batch during training |
| val_batch_size | 1      | Number of point cloud frames in one batch during evaluation |
| epochs         | 160    | Number of epochs                     |

#### AMP Configuration amp_cfg

Automatic Mixed Precision (AMP) refers to using fp16 precision during training to improve computing speed and reduce storage space. For details, see [Automatic Mixed Precision Training (AMP)](https://www.paddlepaddle.org.cn/documentation/docs/zh/guides/performance_improving/amp_cn.html).

| Configuration Name                | Default Value | Meaning                                   |
| ----------------------- | ------ | -------------------------------------- |
| use_amp                 | False  | Whether to enable AMP training                        |
| enable                  | False  | -                                      |
| level                   | O1     | Mixed precision training strategy; options: O1, O2     |
| scalerinit_loss_scaling | 32.0   | Controls the loss scaling ratio to avoid floating-point underflow |

#### Dataset Configuration train_dataset/val_datasets

<table>
<thead>
  <tr>
    <th colspan="3">Configuration Name</th>
    <th>Default Value</th>
    <th>Meaning</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td colspan="3">type</td>
    <td>KittiPCDataset</td>
    <td>Dataset type</td>
  </tr>
  <tr>
    <td colspan="3">dataset_root</td>
    <td>Dataset path</td>
    <td>datasets/KITTI</td>
  </tr>
  <tr>
    <td rowspan="27">transforms</td>
    <td rowspan="2">LoadPointCloud</td>
    <td>dim</td>
    <td>4</td>
    <td>Number of features per point, generally x, y, z, intensity</td>
  </tr>
  <tr>
    <td>use_dim</td>
    <td>4</td>
    <td>Use the first use_dim features of each point</td>
  </tr>
  <tr>
    <td colspan="2">RemoveCameraInvisiblePointsKITTI</td>
    <td>-</td>
    <td>Remove point clouds in areas not visible to the camera</td>
  </tr>
  <tr>
    <td rowspan="6">SamplingDatabase</td>
    <td>min_num_points_in_box_per_class</td>
    <td>Car: 5Cyclist: 5Pedestrian: 5</td>
    <td>Minimum number of points for each object category</td>
  </tr>
  <tr>
    <td>max_num_samples_per_class</td>
    <td>Car: 15Cyclist: 10</td>
    <td>Maximum number of objects of each category per point cloud frame</td>
  </tr>
  <tr>
    <td>ignored_difficulty</td>
    <td>[-1]</td>
    <td>Difficulty limit</td>
  </tr>
  <tr>
    <td>database_anno_path</td>
    <td>datasets/KITTI/kitti_train_gt_database/anno_info_train.pkl</td>
    <td>Path of anno_info_train.pkl</td>
  </tr>
  <tr>
    <td>database_root</td>
    <td>datasets/KITTI/</td>
    <td>Dataset path</td>
  </tr>
  <tr>
    <td>class_names</td>
    <td>["Car", "Cyclist", "Pedestrian"]</td>
    <td>Category information</td>
  </tr>
  <tr>
    <td rowspan="3">RandomObjectPerturb</td>
    <td>rotation_range</td>
    <td>[-0.15707963267, 0.15707963267]</td>
    <td>Angle range for random rotation of gt</td>
  </tr>
  <tr>
    <td>translation_std</td>
    <td>[0.25, 0.25, 0.25]</td>
    <td>Standard deviation for random translation of gt</td>
  </tr>
  <tr>
    <td>max_num_attempts</td>
    <td>100</td>
    <td>Maximum number of candidates</td>
  </tr>
  <tr>
    <td>RandomVerticalFlip</td>
    <td></td>
    <td>-</td>
    <td>Enable random vertical flip</td>
  </tr>
  <tr>
    <td rowspan="2">GlobalRotate</td>
    <td>min_rot</td>
    <td>-0.78539816</td>
    <td>Minimum rotation angle of global rotation</td>
  </tr>
  <tr>
    <td>max_rot</td>
    <td>0.78539816</td>
    <td>Maximum rotation angle of global rotation</td>
  </tr>
  <tr>
    <td rowspan="2">GlobalScale</td>
    <td>min_scale</td>
    <td>0.95</td>
    <td>Minimum scale factor of global scaling</td>
  </tr>
  <tr>
    <td>max_scale</td>
    <td>1.05</td>
    <td>Maximum scale factor of global scaling</td>
  </tr>
  <tr>
    <td>GlobalTranslate</td>
    <td>translation_std</td>
    <td>[0.2, 0.2, 0.2]</td>
    <td>Standard deviation of global translation</td>
  </tr>
  <tr>
    <td>ShufflePoint</td>
    <td></td>
    <td>-</td>
    <td>Shuffle the order of the point cloud</td>
  </tr>
  <tr>
    <td>FilterBBoxOutsideRange</td>
    <td>point_cloud_range</td>
    <td>[0, -39.68, -3, 69.12, 39.68, 1]</td>
    <td>Filter out gt outside the range</td>
  </tr>
  <tr>
    <td rowspan="7">Gt2CenterPointTarget</td>
    <td>tasks</td>
    <td>- num_class: 1class_names: ["Car"]- num_class: 2class_names: ["Cyclist", "Pedestrian"]</td>
    <td>centerpoint head settings</td>
  </tr>
  <tr>
    <td>down_ratio</td>
    <td>2</td>
    <td>Downsampling ratio</td>
  </tr>
  <tr>
    <td>point_cloud_range</td>
    <td>[0, -39.68, -3, 69.12, 39.68, 1]</td>
    <td>Point cloud range</td>
  </tr>
  <tr>
    <td>voxel_size</td>
    <td>[0.16, 0.16, 4]</td>
    <td>Voxel size</td>
  </tr>
  <tr>
    <td>gaussian_overlap</td>
    <td>0.1</td>
    <td>Gaussian overlap value</td>
  </tr>
  <tr>
    <td>max_objs</td>
    <td>500</td>
    <td>Maximum number of gt allowed per frame</td>
  </tr>
  <tr>
    <td>min_radius</td>
    <td>2</td>
    <td>Minimum radius of each gt when generating the heatmap</td>
  </tr>
  <tr>
    <td colspan="3">mode</td>
    <td>train</td>
    <td>Training mode</td>
  </tr>
  <tr>
    <td colspan="3">class_balanced_sampling</td>
    <td>False</td>
    <td>Enable class-balanced sampling</td>
  </tr>
  <tr>
    <td colspan="3">class_names</td>
    <td>["Car", "Cyclist", "Pedestrian"]</td>
    <td>Category information</td>
  </tr>
</tbody>
</table>

SamplingDatabase

| Before Data Augmentation                                                                                                                                                   | After Data Augmentation                                                                                                                                                   |
| -------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ![before data augmentation.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%95%B0%E6%8D%AE%E5%A2%9E%E5%BC%BA%E4%B9%8B%E5%89%8D_fc166b5.png) | ![after data augmentation.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%95%B0%E6%8D%AE%E5%A2%9E%E5%BC%BA%E4%B9%8B%E5%90%8E_158f954.png) |

#### Optimizer Configuration optimizer

<table>
<thead>
  <tr>
    <th>Configuration Name</th>
    <th></th>
    <th>Default Value</th>
    <th>Meaning</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td colspan="2">type</td>
    <td>OneCycleAdam</td>
    <td>Optimizer type</td>
  </tr>
  <tr>
    <td colspan="2">beta2</td>
    <td>0.99</td>
    <td>beta2</td>
  </tr>
  <tr>
    <td colspan="2">weight_decay</td>
    <td>0.01</td>
    <td>Decay coefficient</td>
  </tr>
  <tr>
    <td rowspan="2">grad_clip</td>
    <td>type</td>
    <td>ClipGradByGlobalNorm</td>
    <td rowspan="2">Gradient clipping</td>
  </tr>
  <tr>
    <td>clip_norm</td>
    <td>35</td>
  </tr>
  <tr>
    <td rowspan="4">beta1</td>
    <td>type</td>
    <td>OneCycleDecayWarmupMomentum</td>
    <td rowspan="4">Momentum settings</td>
  </tr>
  <tr>
    <td>momentum_peak</td>
    <td>0.95</td>
  </tr>
  <tr>
    <td>momentum_trough</td>
    <td>0.85</td>
  </tr>
  <tr>
    <td>step_ratio_peak</td>
    <td>0.4</td>
  </tr>
</tbody>
</table>

#### Learning Rate Settings lr_scheduler

<table>
<thead>
  <tr>
    <th>Configuration Name</th>
    <th>Default Value</th>
    <th>Meaning</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td>type</td>
    <td>OneCycleWarmupDecayLr</td>
    <td>Learning rate type</td>
  </tr>
  <tr>
    <td>base_learning_rate</td>
    <td>0.001</td>
    <td>Initial learning rate</td>
  </tr>
  <tr>
    <td>lr_ratio_peak</td>
    <td>10</td>
    <td>Scale factor of the maximum learning rate</td>
  </tr>
  <tr>
    <td>lr_ratio_trough</td>
    <td>0.0001</td>
    <td>Scale factor of the minimum learning rate</td>
  </tr>
  <tr>
    <td>step_ratio_peak</td>
    <td>0.4</td>
    <td>Step ratio at which the maximum is reached</td>
  </tr>
</tbody>
</table>

![learningrate.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/learningrate_5954540.png)

#### Model Configuration model

<table>
<thead>
  <tr>
    <th colspan="2">Configuration Name</th>
    <th>Default Value</th>
    <th>Meaning</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td colspan="2">type</td>
    <td>CenterPoint</td>
    <td>Model name</td>
  </tr>
  <tr>
    <td rowspan="5">voxelizer</td>
    <td>type</td>
    <td>HardVoxelizer</td>
    <td>Voxelization method</td>
  </tr>
  <tr>
    <td>point_cloud_range</td>
    <td>[0, -39.68, -3, 69.12, 39.68, 1]</td>
    <td>Point cloud range</td>
  </tr>
  <tr>
    <td>voxel_size</td>
    <td>[0.16, 0.16, 4]</td>
    <td>Voxel size</td>
  </tr>
  <tr>
    <td>max_num_points_in_voxel</td>
    <td>100</td>
    <td>Maximum number of points in each voxel</td>
  </tr>
  <tr>
    <td>max_num_voxels</td>
    <td>[12000, 40000]</td>
    <td>Number of voxels during training/inference</td>
  </tr>
  <tr>
    <td rowspan="7">voxel_encoder</td>
    <td>type</td>
    <td>PillarFeatureNet</td>
    <td>voxel_encoder name</td>
  </tr>
  <tr>
    <td>in_channels</td>
    <td>4</td>
    <td>Number of input channels</td>
  </tr>
  <tr>
    <td>feat_channels</td>
    <td>[64, 64]</td>
    <td>Output channels of the fc layer</td>
  </tr>
  <tr>
    <td>with_distance</td>
    <td>False</td>
    <td>Whether to use the distance feature</td>
  </tr>
  <tr>
    <td>max_num_points_in_voxel</td>
    <td>100</td>
    <td>Maximum number of points in each voxel</td>
  </tr>
  <tr>
    <td>voxel_size</td>
    <td>[0.16, 0.16, 4]</td>
    <td>Voxel size</td>
  </tr>
  <tr>
    <td>point_cloud_range</td>
    <td>[0, -39.68, -3, 69.12, 39.68, 1]</td>
    <td>Point cloud range</td>
  </tr>
  <tr>
    <td rowspan="4">middle_encoder</td>
    <td>type</td>
    <td>PointPillarsScatter</td>
    <td>middle_encoder name</td>
  </tr>
  <tr>
    <td>in_channels</td>
    <td>64</td>
    <td>Number of input channels</td>
  </tr>
  <tr>
    <td>voxel_size</td>
    <td>[0.16, 0.16, 4]</td>
    <td>Voxel size</td>
  </tr>
  <tr>
    <td>point_cloud_range</td>
    <td>[0, -39.68, -3, 69.12, 39.68, 1]</td>
    <td>Point cloud range</td>
  </tr>
  <tr>
    <td rowspan="5">backbone</td>
    <td>type</td>
    <td>SecondBackbone</td>
    <td>backbone name</td>
  </tr>
  <tr>
    <td>in_channels</td>
    <td>64</td>
    <td>Input channels</td>
  </tr>
  <tr>
    <td>out_channels</td>
    <td>[64, 128, 256]</td>
    <td>Output channels of each stage</td>
  </tr>
  <tr>
    <td>layer_nums</td>
    <td>[3, 5, 5]</td>
    <td>Number of convolution layers in each stage</td>
  </tr>
  <tr>
    <td>downsample_strides</td>
    <td>[1, 2, 2]</td>
    <td>Downsampling stride of each stage</td>
  </tr>
  <tr>
    <td rowspan="5">neck</td>
    <td>type</td>
    <td>SecondFPN</td>
    <td>neck name</td>
  </tr>
  <tr>
    <td>in_channels</td>
    <td>[64, 128, 256]</td>
    <td>Input channels</td>
  </tr>
  <tr>
    <td>out_channels</td>
    <td>[128, 128, 128]</td>
    <td>Output channels</td>
  </tr>
  <tr>
    <td>upsample_strides</td>
    <td>[0.5, 1, 2]</td>
    <td>Upsampling stride</td>
  </tr>
  <tr>
    <td>use_conv_for_no_stride</td>
    <td>True</td>
    <td>Whether to use a convolution layer where there is no upsampling</td>
  </tr>
  <tr>
    <td rowspan="6">bbox_head</td>
    <td>type</td>
    <td>CenterHead</td>
    <td>head name</td>
  </tr>
  <tr>
    <td>in_channels</td>
    <td>384</td>
    <td>Input channels</td>
  </tr>
  <tr>
    <td>tasks</td>
    <td>- num_class: 1class_names: ["Car"]- num_class: 2class_names: ["Cyclist", "Pedestrian"]</td>
    <td>head settings</td>
  </tr>
  <tr>
    <td>weight</td>
    <td>0.25</td>
    <td>Weight of the localization loss</td>
  </tr>
  <tr>
    <td>code_weights</td>
    <td>[1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]</td>
    <td>Weights of the 8 localization attributes</td>
  </tr>
  <tr>
    <td>common_heads</td>
    <td>reg: [2, 2]height: [1, 2]dim: [3, 2]rot: [2, 2]</td>
    <td>Settings of the 4 heads</td>
  </tr>
  <tr>
    <td rowspan="8">test_cfg</td>
    <td>post_center_limit_range</td>
    <td>[-10., -50., -10., 80., 50., 10.]</td>
    <td>Range of dt center</td>
  </tr>
  <tr>
    <td>nms_pre_max_size</td>
    <td>1000</td>
    <td>Number of dt before nms</td>
  </tr>
  <tr>
    <td>nms_post_max_size</td>
    <td>83</td>
    <td>Number of dt after nms</td>
  </tr>
  <tr>
    <td>nms_iou_threshold</td>
    <td>0.1</td>
    <td>nms iou threshold</td>
  </tr>
  <tr>
    <td>score_threshold</td>
    <td>0.1</td>
    <td>Confidence threshold</td>
  </tr>
  <tr>
    <td>point_cloud_range</td>
    <td>[0, -39.68, -3, 69.12, 39.68, 1]</td>
    <td>Point cloud range</td>
  </tr>
  <tr>
    <td>down_ratio</td>
    <td>2</td>
    <td>Downsampling ratio</td>
  </tr>
  <tr>
    <td>voxel_size</td>
    <td>[0.16, 0.16, 4]</td>
    <td>Voxel size</td>
  </tr>
</tbody>
</table>

### 3. Training Workflow

The following uses the KITTI dataset as an example to train a CenterPoint model.

#### Data Preparation

1. First, download the relevant data from the [official website](https://www.cvlibs.net/datasets/kitti/eval_object.php?obj_benchmark=3d)

- Download Velodyne point clouds, if you want to use laser information (29 GB)
- training labels of object data set (5 MB)
- camera calibration matrices of object data set (16 MB)

2. Download the dataset split file list:

   ```bash
   wget https://bj.bcebos.com/paddle3d/datasets/KITTI/ImageSets.tar.gz
   ```

3. After extracting the data, organize it according to the directory structure below:

   ```bash
   kitti_dataset_root
    |—— training
    |   |—— label_2
    |   |   |—— 000001.txt
    |   |   |—— ...
    |   |—— calib
    |   |   |—— 000001.txt
    |   |   |—— ...
    |   |—— velodyne
    |   |   |—— 000001.bin
    |   |   |—— ...
    |—— ImageSets
    │   |—— test.txt
    │   |—— train.txt
    │   |—— trainval.txt
    │   |—— val.txt
   ```

4. Create a symbolic link `datasets/KITTI` under the Paddle3D directory, pointing to the dataset directory above:

   ```bash
   mkdir datasets
    ln -s /path/to/kitti_dataset_root ./datasets
    mv ./datasets/kitti_dataset_root ./datasets/KITTI
   ```

5. Generate the ground-truth database required for data augmentation during training:

   ```bash
   python tools/create_det_gt_database.py --dataset_name kitti --dataset_root ./datasets/KITTI --save_dir ./datasets/KITTI
   ```

   `-dataset_root` specifies the path of the KITTI dataset, and `--save_dir` specifies the path where the generated ground-truth database is saved. After this command is executed, the directory generated under `save_dir` is as follows:

   ```bash
   kitti_train_gt_database
    |—— anno_info_train.pkl
    |—— Car
    |   |—— 4371_Car_7.bin
    |   |—— ...
    |—— Cyclist
   ```

   > Note: If you want to train with your own dataset, it is recommended to organize it in the standard KITTI format.

#### Training

Training on the KITTI dataset uses 8 GPUs:

```bash
python -m paddle.distributed.launch --gpus 0,1,2,3,4,5,6,7 tools/train.py --config configs/centerpoint/centerpoint_pillars_016voxel_kitti.yml --save_dir ./output_kitti --num_workers 4 --save_interval 5
```

Training launch parameters:

| Parameter              | Purpose                                                                                                             | Required | Default Value           |
| ------------------- | ---------------------------------------------------------------------------------------------------------------- | ---------- | ---------------- |
| iters               | Number of training iteration steps                                                                                                     | No         | Value specified in the configuration file |
| epochs              | Number of training epochs                                                                                                     | No         | Value specified in the configuration file |
| batch_size          | Batch size per GPU                                                                                                   | No         | Value specified in the configuration file |
| learning_rate       | Initial learning rate                                                                                                       | No         | Value specified in the configuration file |
| config              | Configuration file path                                                                                                     | Yes         | -                |
| save_dir            | Root path for saving checkpoints (models and visualdl log files)                                                                     | No         | output           |
| num_workers         | Number of processes for asynchronous data reading; when greater than or equal to 1, subprocesses are started to read data                                                       | No         | 2                |
| save_interval       | Interval, in steps, between model saves                                                                                               | No         | 1000             |
| do_eval             | Whether to run evaluation when saving the model                                                                                         | No         | No               |
| log_interval        | Interval, in steps, between log prints                                                                                               | No         | 10               |
| resume              | Whether to resume the training state from a checkpoint                                                                                       | No         | None             |
| keep_checkpoint_max | Maximum number of models to keep                                                                                               | No         | 5                |
| quant_config        | Quantization configuration file, generally placed in the [configs/quant](https://github.com/PaddlePaddle/Paddle3D/tree/develop/configs/quant) directory | No         | None             |
| seed                | Global random seed for Paddle/numpy/random                                                                              | No         | None             |
| model               | Fine-tune based on a pretrained model                                                                                       | No         | .pdparam model file |

#### Evaluation

```bash
python tools/evaluate.py --config configs/centerpoint/centerpoint_pillars_016voxel_kitti.yml --model ./output_kitti/epoch_160/model.pdparams --batch_size 1 --num_workers 4
```

> Note: CenterPoint evaluation currently supports only batch_size of 1. The evaluation launch parameters are as follows:

| Parameter       | Purpose                                                                                                             | Required | Default Value           |
| ------------ | ---------------------------------------------------------------------------------------------------------------- | ---------- | ---------------- |
| batch_size   | Batch size per GPU                                                                                                   | No         | Value specified in the configuration file |
| config       | Configuration file path                                                                                                     | Yes         | -                |
| model        | Path of the model parameter file                                                                                               | Yes         | -                |
| num_workers  | Number of processes for asynchronous data reading; when greater than or equal to 1, subprocesses are started to read data                                                       | No         | 2                |
| quant_config | Quantization configuration file, generally placed in the [configs/quant](https://github.com/PaddlePaddle/Paddle3D/tree/develop/configs/quant) directory | No         | None             |

#### Training Visualization

Paddle3D uses VisualDL to record metrics and data during training. During training, we can start a server with VisualDL on the command line and view the corresponding data in a browser.

```bash
# logdir must be consistent with the save_dir specified in the training script
visualdl --logdir output
```

![visualization 1.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E5%8F%AF%E8%A7%86%E5%8C%961_f5cf08e.png)

### 4. Exporting the Model

Run the following command to export the dynamic graph model file saved during training into a static graph model file that the inference engine can load.

```bash
python tools/export.py --config configs/centerpoint/centerpoint_pillars_016voxel_kitti.yml --model /path/to/model.pdparams --save_dir /path/to/output
```

Export script parameters

| Parameter            | Purpose                                                                                                                                                                           | Required | Default Value                 |
| ----------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------- | ---------------------- |
| config            | Configuration file path                                                                                                                                                                   | Yes         | -                      |
| model             | Model parameter path                                                                                                                                                                   | No         | -                      |
| export_for_apollo | Whether it is used for Apollo deployment; when this switch is on, the meta file for Apollo deployment is generated as well                                                                                                         | No         | False                  |
| save_dir          | Save path of the inference model file                                                                                                                                                         | No         | exported_model         |
| save_name         | Save name of the inference model file                                                                                                                                                         | No         | None (determined by each model) |
| quant_config      | Quantization configuration file, generally placed in the [configs/quant](https://github.com/PaddlePaddle/Paddle3D/tree/develop/configs/quant) directory. If the model uses quantization training, the quantization configuration file must also be specified when exporting the model | No         |                        |

After the run completes, the following files are output:

```bash
|-- centerpoint_pillars_016voxel_kitti
    |-- apollo_deploy.yaml
    |-- centerpoint.pdiparams
    |-- centerpoint.pdiparams.info
    |-- centerpoint.pdmodel
```

## Deploying CenterPoint in Apollo

The following describes how to deploy the exported centerpoint model in Apollo and use it for 3D object detection:

### 1. CenterPoint Deployment Configuration

First, the centerpoint deployment configuration. The configuration file path is:`/apollo/modules/perception/lidar_detection/data/center_point_param.pb.txt`.

```bash
# center point params
info {
  name: "center_point"
  version: ""
  dataset: "apollo"
  task_type: Detection3D
  sensor_type: Lidar
  framework: PaddlePaddle
  proto_file {
    file: "centerpoint.pdmodel"
  }
  weight_file {
    file: "centerpoint.pdiparams"
  }
  inputs {name: "data"}
  outputs {name: "_generated_var_9"}
  outputs {name: "_generated_var_10"}
  outputs {name: "_generated_var_11"}
}
preprocess {
  gpu_id: 0
  normalizing_factor: 255
  num_point_feature: 4
  enable_ground_removal: false
  ground_removal_height: -1.5
  enable_downsample_beams: false
  downsample_beams_factor: 1
  enable_downsample_pointcloud: true
  downsample_voxel_size_x: 0.09
  downsample_voxel_size_y: 0.09
  downsample_voxel_size_z: 0.09
  enable_fuse_frames: false
  num_fuse_frames: 5
  fuse_time_interval: 0.5
  enable_shuffle_points: false
  max_num_points: 2147483647
  reproduce_result_mode: false
  enable_roi_outside_removal: false
}
postprocess {
  score_threshold: 0.25
  num_output_box_feature: 7
  bottom_enlarge_height: 0.25
  top_enlarge_height: 0.25
  width_enlarge_value: 0
  length_enlarge_value: 0
}
paddle_settings {
  use_trt: false
  trt_precision: 1
  trt_use_static: true
  use_calibration: true
  collect_shape_info: false
  use_dynamicshape: true
  dynamic_shape_file: "modules/perception/lidar_detection/data/center_point_paddle/collect_shape_info_3lidar_20.pbtxt"
}
point2box_max_num: 5
quantize: 0.2
```

ModelInfo configuration, file path:`modules/perception/common/proto/model_info.proto`.

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

Configuration specific to centerpoint

| Parameter Type        | Parameter            | Default Value | Meaning                        |
| --------------------- | ----------------- | ------ | --------------------------- |
| ModelInfo             | info              | /      | General model configuration                |
| PointCloudPreProcess  | preprocess        | /      | Preprocessing                      |
| PointCloudPostProcess | postprocess       | /      | Postprocessing                      |
| int32                 | point2box_max_num | 5      | Maximum number of boxes each point can belong to |
| float                 | quantize          | 0.2    | Quantize sizes to multiples of quantize  |

### 2. Deploying in Apollo and Viewing the Results

For setting up the Apollo environment, see [perception 2.0 overview](https://apollo.baidu.com/community/article/1133).

#### Entering the Docker Environment

```bash
# Enter the container
aem enter

# Download and install dependency packages: this pulls and installs all dependency packages listed in cyberfile.xml under the core directory
# It also automatically downloads the centerpoint model and places it in the specified location
buildtool build --gpu
```

The steps above deploy Apollo's official centerpoint model. To deploy your own centerpoint model, place the files exported in the previous step under `/apollo/modules/perception/data/models/center_point_paddle`:

```bash
|-- /apollo/modules/perception/data/models/center_point_paddle
    |-- apollo_deploy.yaml
    |-- centerpoint.pdiparams
    |-- centerpoint.pdiparams.info
    |-- centerpoint.pdmodel
```

#### Starting Dreamview+

```bash
aem bootstrap start --plus
```

#### Starting the LiDAR Perception Program

Select the corresponding vehicle configuration:

![select the vehicle configuration.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E9%80%89%E6%8B%A9%E7%9B%B8%E5%BA%94%E8%BD%A6%E5%9E%8B%E9%85%8D%E7%BD%AE_3bb824c.png)

Start the transform module:

![start the transform module.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E5%90%AF%E5%8A%A8transform%E6%A8%A1%E5%9D%97_6f4ffc3.png)

Start the lidar module:

![start the lidar module.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E5%90%AF%E5%8A%A8lidar%E6%A8%A1%E5%9D%97_4d8bb34.png)

#### Downloading and Playing the Perception Package

- Method 1: In the lower-left corner of dreamview+, click Resource Manager, download the sensor_rgb data package, and after the download completes, select the sensor_rgb data package and click play.

  ![method 1 playback 1.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%96%B9%E5%BC%8F%E4%B8%80%E6%92%AD%E5%8C%851_cbbbcd5.png)

  ![method 1 playback 2.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%96%B9%E5%BC%8F%E4%B8%80%E6%92%AD%E5%8C%852_953aab2.png)

- Method 2: Play the downloaded sensor_rgb data package from the terminal; for the download link, see the open resources record downloads.

  ```bash
  cyber_recorder play -f sensor_rgb.record
  ```

  View the LiDAR detection results in dreamview+:

  ![playback result.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%92%AD%E6%94%BE%E6%95%88%E6%9E%9C_f6ef232.png)
