# Usage Instructions
## Create and Log In to the Development Machine
After successfully creating the development machine according to the deployment environment requirements, click to log in to the development machine, enter the development machine's webIDE, and open the terminal in VScode

![](./images/webIDE.png)
* Code save path: /root/Apollo-Vision-Net-Deployment
* Default data mount path: /mnt/pfs/nuscenes_data/bev_data/nuscenes

## Mount the Dataset
Mount the dataset inside the development machine (it is recommended to share the pfs disk with the Apollo-BEV-Train development machine; if it is not shared, the dataset needs to be processed again)
```
cd ~/Apollo-Vision-Net-Deployment/data

# Create symbolic links (note to use the correct -s parameter)
ln -s /mnt/pfs/nuscenes_data/bev_data/nuscenes nuscenes
ln -s /mnt/pfs/nuscenes_data/bev_data/occ_gt_release_v1_0 occ_gt_release_v1_0
ln -s /mnt/pfs/nuscenes_data/bev_data/can_bus can_bus
```


## Model Export
### Generate the onnx File
Run the following command, providing the pth file, to generate the onnx file

```
cd ~/Apollo-Vision-Net-Deployment/ 
python tools/pth2onnx.py configs/apollo_bev/bev_tiny_det_occ_apollo_trt.py bos_path/epoch_*.pth --opset_version 13 --cuda
# When exporting the onnx model with a 4090 card, the --cuda parameter needs to be removed
```