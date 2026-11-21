### Explanation of the Paddle Inference Interface Flags

```plain
// lidar_center_point
DECLARE_bool(use_trt);  // Whether to use the tensorrt inference interface
DECLARE_int32(trt_precision); // Flag for fp32, int8 and fp16 inference precision, 0: kFloat32, 1: kInt8, 2: kHalf
DECLARE_int32(trt_use_static); // Whether to use the serialized file; 1 means use it. The serialized file is generated on first load, and on later loads the file is deserialized to speed up model loading
DECLARE_bool(use_calibration); // Whether to use the calibration table; it must be enabled for int8 precision inference
DECLARE_bool(use_dynamicshape); // Whether to load the dynamic shape file
DECLARE_bool(collect_shape_info); // Collect the dynamic shape file
DECLARE_string(dynamic_shape_file); // Path for generating and reading the dynamic shape file
```

### How to Use (Generating the Dynamic Shape File and the Serialized File):

#### Why the Generated Dynamic Shape File Is Needed:

PaddlePaddle integrates TensorRT in the form of subgraphs. After a model is loaded, the neural network can be represented as a computation graph made up of variables and operator nodes. What Paddle TensorRT does is scan the whole graph, find the subgraphs that can be optimized with TensorRT, and replace them with TensorRT nodes. During model inference, when a TensorRT node is encountered, Paddle calls the TensorRT library to optimize that node, while the other nodes call Paddle's native implementation. During inference, TensorRT can perform horizontal and vertical fusion of Ops, filter out redundant Ops, and choose suitable kernels for specific Ops on specific platforms, among other optimizations, which speeds up model prediction.

#### Step 1: Generate the Dynamic Shape File

Set the flags as follows:

```plain
###########################################################################
# centerpoint paddle inference flag
--use_trt=true
--trt_precision=2
--collect_shape_info=true
--use_dynamicshape=false
```

Note: collect_shape_info and use_dynamicshape cannot both be set to true at the same time. You should first collect the dynamic shape file and then use the generated file.

After setting the flags, start the lidar perception pipeline and play the data package. After the program ends, the dynamic shape file is stored under the dynamic_shape_file path set by the developer.

```plain
cyber_launch start modules/perception/launch/perception_lidar.launch
```

tips: The range covered by the dynamic shape file collected by playing a data package may be limited, so a model that loads this dynamic shape file may report errors at actual runtime. It is recommended to run the above lidar perception pipeline in a real-vehicle test environment to collect a more complete dynamic shape range.

#### Step 2: Load the Generated Dynamic Shape File

Set the flags as follows. It is recommended to set trt_use_static to 1. If the generated serialized file is not used, every model load takes a long time (about 5 minutes). With trt_use_static set to 1, the first model load takes longer but generates the corresponding serialized file. The generated serialized file directory is _opt_cache, which is placed automatically under the model directory path. Later model loads directly deserialize the files in this directory, greatly reducing model loading time (about 5 seconds).

```plain
###########################################################################
# centerpoint paddle inference flag
--use_trt=true
--trt_precision=2
--trt_use_static=1
--collect_shape_info=false
--use_dynamicshape=true
```

Note: If you want to use int8 quantization, change the trt_precision value in Step 1 to 1.