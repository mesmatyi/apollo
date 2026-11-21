
## Usage Instructions

### Create and Log In to the Development Machine

After successfully creating the development machine according to the deployment environment requirements:
Click to log in to the development machine, enter the development machine's webIDE, and open the terminal in VScode
![webIDE](./images/webIDE.png)
Code save path: `/root/Apollo-Vision-Net`
Default data mount path: `/mnt/dataset/nuscenes_data`

### Dataset Processing

#### Storage Mount Configuration

**Basic configuration:**
By default, the Nuscenes dataset is mounted on BOS
![Mount PFS](./images/挂载PFS.png)

Mount PFS or CFS

When creating or updating the development machine, select the PFS or CFS storage corresponding to the cluster in the storage mount configuration, and set a custom mount path (example path: /mnt/pfs/nuscenes_data). You can select the storage mount when creating or updating the development machine configuration, choose the PFS or CFS corresponding to the cluster, and customize the mount path
![Environment Configuration](./images/环境配置.png)


#### Dataset Migration

```bash
# Create the dataset directory structure
cd /mnt/pfs/nuscenes_data
mkdir -p bev_data/nuscenes

# Migrate the dataset from BOS to PFS files
cp -rv bos_mount_path/apollo_nuscenes_bev_data/nuscenes/* /mnt/pfs/nuscenes_data/bev_data/nuscenes/
cp -rv bos_mount_path/apollo_nuscenes_bev_data/can_bus/* /mnt/pfs/nuscenes_data/bev_data/
cp -rv bos_mount_path/apollo_nuscenes_bev_data/occ_gt_release_v1_0/* /mnt/pfs/nuscenes_data/bev_data/
```

#### Dataset Extraction and Organization

**Nuscenes dataset processing:**
```bash
cd /mnt/pfs/nuscenes_data/bev_data/nuscenes
bash tar_files.sh  # Run the command to extract the dataset
```

**Directory structure after extraction:**
```
nuscenes/
├── maps/                           # Maps
├── samples/                        # Dataset
├── sweeps/                         # Dataset
├── v1.0-trainval/                  # Dataset
├── tar_files.sh                    # Extraction command
├── nuscenes_infos_temporal_train_occ_gt.pkl
├── nuscenes_infos_temporal_val_occ_gt.pkl
├── v1.0-trainval01_blobs.tgz //Dataset archive
├── v1.0-trainval02_blobs.tgz //Dataset archive
├── v1.0-trainval03_blobs.tgz //Dataset archive
├── v1.0-trainval04_blobs.tgz //Dataset archive
├── v1.0-trainval05_blobs.tgz //Dataset archive
├── v1.0-trainval06_blobs.tgz //Dataset archive
├── v1.0-trainval07_blobs.tgz //Dataset archive
├── v1.0-trainval08_blobs.tgz //Dataset archive
├── v1.0-trainval09_blobs.tgz //Dataset archive
└── v1.0-trainval10_blobs.tgz //Dataset archive
```

**occ_gt_release_v1_0 dataset processing:**
Extract the occ_gt_release_v1_0 archive
```bash
cd /mnt/pfs/nuscenes_data/bev_data
tar -zxvf occ_gt_release_v1_0.tar.gz
```
After extraction
```
occ_gt_release_v1_0
├── occ_gt_train.json
├── occ_gt_val.json
├── train
└── val
```
**Copy the object detection and occupancy pkl files to occ_gt_release_v1_0:**
```
bash
cp /mnt/pfs/nuscenes_data/bev_data/nuscenes/*.pkl /mnt/pfs/nuscenes_data/bev_data/occ_gt_release_v1_0/
```
**Final directory structure**
```
/mnt/pfs/nuscenes_data/bev_data# tree occ_gt_release_v1_0 -L 1
occ_gt_release_v1_0
├── nuscenes_infos_temporal_train_occ_gt.pkl
├── nuscenes_infos_temporal_val_occ_gt.pkl
├── occ_gt_train.json
├── occ_gt_val.json
├── train
└── val
```

**CAN-Bus data processing:**
```bash
cd /mnt/pfs/nuscenes_data/bev_data
tar -zxvf can_bus.tar.gz
```
**Dataset structure specification**
```
├── data/
│   ├── can_bus/
│   ├── nuscenes/
│   │   ├── maps/
│   │   ├── samples/
│   │   ├── sweeps/
│   │   ├── v1.0-test
│   │   ├── v1.0-trainval
│   │   ├── nuscenes_infos_temporal_train.pkl
│   │   ├── nuscenes_infos_temporal_val.pkl
│   ├── occ_gt_release_v1_0/
│   │   ├── train/
│   │   ├── val/
│   │   ├── occ_gt_train.json
│   │   ├── occ_gt_val.json
│   │   ├── nuscenes_infos_temporal_train_occ_gt.pkl
│   │   ├── nuscenes_infos_temporal_val_occ_gt.pkl
```
#### Mount the Dataset
Mount the dataset inside the development machine:
```bash
cd ~/Apollo-Vision-Net/data

# Create symbolic links
ln -s /mnt/pfs/nuscenes_data/bev_data/nuscenes nuscenes
ln -s /mnt/pfs/nuscenes_data/bev_data/occ_gt_release_v1_0 occ_gt_release_v1_0
ln -s /mnt/pfs/nuscenes_data/bev_data/can_bus can_bus
```

**Final project structure:**
```
~/Apollo-Vision-Net/data/
├── can_bus -> /mnt/pfs/nuscenes_data/bev_data/can_bus
├── nuscenes -> /mnt/pfs/nuscenes_data/bev_data/nuscenes
└── occ_gt_release_v1_0 -> /mnt/pfs/nuscenes_data/bev_data/occ_gt_release_v1_0
```

### Train the Model

```bash
# Enter the working directory
cd ~/Apollo-Vision-Net/

# Train the model
./tools/dist_train.sh \
  ./projects/configs/bevformer/bev_tiny_det_occ_apollo.py \  # Model configuration file
  8  # Use 8 GPUs for distributed training
```

**Resume training from a checkpoint:**
```bash
# Train the model (resume from a checkpoint)
./tools/dist_train.sh \
  ./projects/configs/bevformer/bev_tiny_det_occ_apollo.py \   # Model configuration file
  8 \                                                         # Use 8 GPUs
  --resume-from work_dirs/bev_tiny_det_occ_apollo/epoch_4.pth # Resume from epoch 4
```

### Model Evaluation

```bash
# Enter the working directory
cd ~/Apollo-Vision-Net/

# Start the evaluation script
./tools/dist_test.sh \
  ./projects/configs/bevformer/bev_tiny_det_occ_apollo.py \  # Model configuration file
  work_dirs/bev_tiny_det_occ_apollo/epoch_4.pth \          # Trained model weights
  4                                                        # Use 4 GPUs for testing
```
> **Note**: Run the model export function on the development machine used for model export with the `.pth` model file obtained from training, converting it into an ONNX model file that meets Apollo's requirements, for subsequent Apollo system deployment to verify the model's performance.