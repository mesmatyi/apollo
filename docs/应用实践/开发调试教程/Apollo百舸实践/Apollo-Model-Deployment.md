# Usage Instructions
## Create and Log In to the Development Machine
After successfully creating the development machine according to the deployment environment requirements, click to log in to the development machine, enter the development machine's webIDE, and open the terminal in VScode

![](./images/webIDE.png)
* Code save path: /apollo_workspace
* Default data mount path: /mnt/pfs/nuscenes_data/bev_data/nuscenes

## Replace with Your Own Trained ONNX File in the Apollo Environment
To replace it with your own trained onnx file, you need to replace the onnx file in the directory

```
sudo rm -rf /apollo/modules/perception/data/models/apollo_bevnet_onnx/apollo_bevnet.onnx
sudo cp your-own-onnx.onnx /apollo/modules/perception/data/models/apollo_bevnet_onnx/apollo_bevnet.onnx
```


## Run the Pure-Vision Model in Apollo
Prepare the record data

The nuscenes record can be downloaded from the links below:

|id|link|
|-|-|
|6f83169d067343658251f72e1dd17dbc|[http://apollo-perception.bj.bcebos.com/nuscenes_occ_records/6f83169d067343658251f72e1dd17dbc.record?authorization=bce-auth-v1%2FALTAKr8RyUUttStVHwGaOsvJyP%2F2024-12-02T08%3A12%3A56Z%2F-1%2Fhost%2F15500778f03ba45f19e6a7818b3af88952a739cbd53761f166d8bd542347821b](http://apollo-perception.bj.bcebos.com/nuscenes_occ_records/6f83169d067343658251f72e1dd17dbc.record?authorization=bce-auth-v1%2FALTAKr8RyUUttStVHwGaOsvJyP%2F2024-12-02T08%3A12%3A56Z%2F-1%2Fhost%2F15500778f03ba45f19e6a7818b3af88952a739cbd53761f166d8bd542347821b)|
|2fc3753772e241f2ab2cd16a784cc680|[http://apollo-perception.bj.bcebos.com/nuscenes_occ_records/2fc3753772e241f2ab2cd16a784cc680.record?authorization=bce-auth-v1%2FALTAKr8RyUUttStVHwGaOsvJyP%2F2024-12-02T08%3A11%3A30Z%2F-1%2Fhost%2Fb7b99869a804ed08c7f6804d8b724e15aebc3e1c13fde45b18b61e459b1b7ae5](http://apollo-perception.bj.bcebos.com/nuscenes_occ_records/2fc3753772e241f2ab2cd16a784cc680.record?authorization=bce-auth-v1%2FALTAKr8RyUUttStVHwGaOsvJyP%2F2024-12-02T08%3A11%3A30Z%2F-1%2Fhost%2Fb7b99869a804ed08c7f6804d8b724e15aebc3e1c13fde45b18b61e459b1b7ae5)|
|bebf5f5b2a674631ab5c88fd1aa9e87a|[http://apollo-perception.bj.bcebos.com/nuscenes_occ_records/bebf5f5b2a674631ab5c88fd1aa9e87a.record?authorization=bce-auth-v1%2FALTAKr8RyUUttStVHwGaOsvJyP%2F2024-12-02T08%3A13%3A18Z%2F-1%2Fhost%2F1c25a91e913f512b8f188dc6e54bd07ffcbb301e1be2d92a3cc7b686e1456d80](http://apollo-perception.bj.bcebos.com/nuscenes_occ_records/bebf5f5b2a674631ab5c88fd1aa9e87a.record?authorization=bce-auth-v1%2FALTAKr8RyUUttStVHwGaOsvJyP%2F2024-12-02T08%3A13%3A18Z%2F-1%2Fhost%2F1c25a91e913f512b8f188dc6e54bd07ffcbb301e1be2d92a3cc7b686e1456d80)|

Select the vehicle model environment and apply the parameters

```
aem profile use nuscenes_occ
```
Open dreamview_plus

```
aem bootstrap start --plus
```
How to display dreamview_plus in the web page:

```
📡 Access methods (choose one):

  Method 1 - Open from the Baige platform:
    1. After running the script, click ports, find the Forwarded Address link of port 8888, and right-click it and select "Open in Browser"

  Method 2 - SSH port forwarding
    1. Run on your local computer: ssh -L 8888:localhost:8888 -p ssh_mapped_port user@server_ip
    2. Open in a browser: http://localhost:8888
```
![](./images/网页显示.png)
Start the transform dag file in the terminal

```
mainboard -d /apollo/modules/transform/dag/static_transform.dag
```


Start the perception dag file in the terminal

```
mainboard -d /apollo/modules/perception/camera_detection_occupancy/dag/camera_detection_occupancy_nus.dag
```


Wait for the model to be serialized. When the terminal gives the following log, serialization is complete and you can proceed with the steps below

![](./images/序列化模型完成.png)
Open a terminal and play the record package

```
cyber_recorder play -f package_name.record
```
You can then see the object detection results on the dreamview_plus page

![](./images/检测结果.png)
### Visualize the occ Results
To view the occ results, perform the following steps:

Set save_occ_result to true in the configuration occ_det_nus.pb.txt, and also set the save path occ_save_path (default path /apollo/data/occ_results)

#### Using the Web-Side OCC Visualization Tool (Supports Remote Access):
```
cd /apollo_workspace/bev_tools
python3 occ_vis.py
```
**Remarks:**

To view the effect of other occ files

Modify the occ_vis.py code

```
 # Around line 470
       # Check the data file path
    possible_paths = [
        "/apollo/data/occ_results/1535489308.678113.bin"
    ]
```
![](https://rte.weiyun.baidu.com/wiki/attach/image/api/imageDownloadAddress?attachId=8fbcc71b04a74858addbfa96d3b86ed0&docGuid=gKPrz83Wmmt0mo)
How to open the web page:

```
📡 Access methods (choose one):

  Method 1 - Open from the Baige platform:
    1. After running the script, click ports, find the Forwarded Address link of port 9000, and right-click it and select "Open in Browser"

  Method 2 - SSH port forwarding
    1. Run on your local computer: ssh -L 9000:localhost:9000 -p ssh_mapped_port user@server_ip
    2. Open in a browser: http://localhost:9000
```
![](./images/打开网页方式.png)![](./images/显示2.png)

 **Web-side OCC visualization feature**, supporting remote real-time access and multi-dimensional scene analysis. Users can access the service port (such as localhost:9000) through a browser to achieve cross-platform, cross-network OCC rendering, without relying on a local display environment. This feature fully renders **16 classes of scene elements** (including vehicles 🚗, pedestrians 🚶, vegetation 🌳, and so on) and provides a dedicated visual identifier for each class of element. It supports interactive view operations (360° rotation/pan/zoom), view reset and screenshot saving (PNG format), significantly improving debugging efficiency and achieving a closed loop of "start the service → verify model results in real time".

![](./images/occweb.png)

**Complete color mapping table**

![](./images/occ元素.png)