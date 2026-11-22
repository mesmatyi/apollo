## Introduction to Performance Analysis Capabilities

As the complexity of autonomous driving systems increases, more and more performance problems appear in the actual development and road-test process, and performance optimization has become an indispensable part of the whole process. However, finding, analyzing, locating, and solving performance problems has a certain threshold and requires extra workload. To address such pain points, the new version of cyber has reworked the framework, developed a performance visualization tool, and established performance analysis capabilities. The performance analysis capabilities of cyber have the following features:

**Multi-dimensional data collection and comprehensive monitoring, with nothing hidden:**

The cyber performance analysis tool can monitor the key performance metrics of the autonomous driving system, including:

<table>
  <thead>
    <tr>
      <th>Category</td>
      <th colspan="2">Metric</td>
      <th>System level</td>
      <th>Process level</td>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td>CPU</td>
      <td colspan="2">CPU utilization</td>
      <td>√</td>
      <td>√</td>
    </tr>
    <tr>
      <td>GPU</td>
      <td colspan="2">GPU utilization</td>
      <td>√</td>
      <td> </td>
    </tr>
    <tr>
      <td>Memory</td>
      <td colspan="2">Memory usage</td>
      <td>√</td>
      <td>√</td>
    </tr>
    <tr>
      <td>GPU memory</td>
      <td colspan="2">GPU memory usage</td>
      <td>√</td>
      <td>√</td>
    </tr>
    <tr>
      <td rowspan="6">IO</td>
      <td rowspan="4">Block device</td>
      <td>Read/write rate</td>
      <td>√</td>
      <td>√</td>
    </tr>
    <tr>
      <td>Device wait time</td>
      <td>√</td>
      <td> </td>
    </tr>
    <tr>
      <td>Device IOPS</td>
      <td>√</td>
      <td> </td>
    </tr>
    <tr>
      <td>Device wait queue length</td>
      <td>√</td>
      <td> </td>
    </tr>
    <tr>
      <td rowspan="2">Network card</td>
      <td>Read/write rate</td>
      <td>√</td>
      <td>√</td>
    </tr>
    <tr>
      <td>Device bandwidth utilization</td>
      <td>√</td>
      <td> </td>
    </tr>
  </tbody>
</table>

By tracking these key metrics in real time, you can quickly identify the bottleneck processes that affect performance in the autonomous driving system.

**Intuitive performance visualization interface, with insights at a glance:**

The performance analysis capabilities of cyber provide a user-friendly visualization interface that gives you an intuitive view of all performance data. With clear charts and reports, you can easily interpret the performance analysis results and identify the direction for optimization.

**Process-level analysis, macro insight, precise localization:**

![cyber_performance_analysis_1](./images/cyber_performance_analysis_1.png)

The visualization interface provides real-time data that lets you observe the performance of every process. This macro view lets you determine which process or processes may be slowing down the whole application, so that you can make corresponding adjustments.

**Function-level analysis, micro-level parsing, deep tuning:**

![cyber_performance_analysis_2](./images/cyber_performance_analysis_2.png)

Going further, the performance analysis capabilities of cyber provide process-level analysis of each module of the autonomous driving system. With this feature, flame graphs, directed graphs, and so on can be generated for each process for the cpu, memory, and gpu metrics (relying on the nvidia nsight series of tools), so you can identify the parts of the code that most need optimization. This granularity of analysis helps developers perform deep optimization.

## Using the Features

### Prerequisite: Deploy Apollo

To use the performance analysis features of the new version of cyber, you only need to deploy the latest version of Apollo. The overall process is the same as the previous Apollo deployment process

#### Source Code

Clone the latest Apollo code

```bash
git clone https://github.com/ApolloAuto/apollo.git
```

Use the script to start and enter the container

```bash
bash docker/scripts/dev_start.sh
```

Compile Apollo

```bash
bash apollo.sh build
```

#### Package Management

Clone the latest Apollo project

```bash
git clone https://github.com/ApolloAuto/application-core.git
```

Install aem, then start and enter the container

```bash
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://apollo-pkg-beta.cdn.bcebos.com/neo/beta/key/deb.gpg.key | sudo gpg --dearmor -o /etc/apt/keyrings/apolloauto.gpg
sudo chmod a+r /etc/apt/keyrings/apolloauto.gpg
echo \
    "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/apolloauto.gpg] https://apollo-pkg-beta.cdn.bcebos.com/apollo/core"\
    $(. /etc/os-release && echo "$VERSION_CODENAME") "main" | \
    sudo tee /etc/apt/sources.list.d/apolloauto.list
sudo apt-get update
sudo apt install apollo-neo-env-manager-dev --reinstall
aem start
```

Install the Apollo packages

```bash
buildtool build
```

### Module-Level Performance Analysis in the Autonomous Driving System

cyber records the performance metrics of all cyber-based components and executable binaries, and they can be viewed in real time. With cyber's visualization tool, it is easy to locate the modules in the autonomous driving system that use a lot of hardware resources. Starting the visualization interface is also simple:

```bash
cyber_performance
```

After it starts, enter localhost:5000 in a browser and you will see a page similar to the following:

![cyber_performance_analysis_3](./images/cyber_performance_analysis_3.png)

The button in the upper left corner can be used to select the process to display, usually in the following format: mainboard.xxx.dag, where xxx.dag is the name of the dag used to start the module; for example, the control module is called mainboard.control.dag. There are also two special cases: system represents the system's performance metrics, and bvar.python3 represents the performance metrics of the cyber_performance software itself.

After selecting a process, there will be multiple charts named xx - yy - zz or xx - yy, where xx represents the type of metric. Currently the types are:

- BASIC: records basic performance metrics, including cpu usage, gpu usage, memory usage, and gpu memory usage
- BLOCK_DEVICE_IO: records metrics related to block device IO
- ETHERNET_DEVICE_IO: records metrics of network card device IO
- E2E_LATENCY: latency of the module's Proc function

yy refers to the metric name, while zz only exists in the BLOCK_DEVICE_IO and ETHERNET_DEVICE_IO types and represents the actual name of the device.

Through the above charts, you can quickly determine how much cpu, gpu, memory, gpu memory, and IO different processes in the autonomous driving system use, which helps locate process bottlenecks in autonomous driving.

> The visualization interface shows real-time values, and cyber dumps the performance data to the data directory. If you need to calculate the mean value, you can parse it with the performance_parse.py script in /apollo/scripts:
>
> python3 /apollo/scripts/performance_parse.py -f data/performance_dumps.07-29-2024.json
>
> Where:
>
> - -f specifies the performance monitoring file written to disk, which is currently generated in the data directory of the project directory

### Function-Level Performance Analysis in the Autonomous Driving System

#### CPU and Memory

The performance analysis feature of cyber can sample cpu and memory usage and finally generate heat maps or directed graphs for analysis.

#### Usage

##### mainboard has the following new parameters:

```bash
I0409 16:54:22.506175 18008 module_argument.cc:113] []command: mainboard -h
I0409 16:54:22.506390 18008 module_argument.cc:33] []Usage:
    mainboard [OPTION]...
Description:
    -h, --help: help information
    -d, --dag_conf=CONFIG_FILE: module dag config file
    -p, --process_group=process_group: the process namespace for running this module, default in manager process
    -s, --sched_name=sched_name: sched policy conf for hole process, sched_name should be conf in cyber.pb.conf
    --plugin=plugin_description_file_path: the description file of plugin
    --disable_plugin_autoload : default enable autoload mode of plugins, use disable_plugin_autoload to ingore autoload
    -c, --cpuprofile: enable gperftools cpu profile
    -o, --profile_filename=filename: the filename to dump the profile to, default value is ${process_group}_cpu.prof. Only work with -c option
    -H, --heapprofile: enable gperftools heap profile
    -O, --heapprofile_filename=filename: the filename to dump the profile to, default value is ${process_group}_mem.prof. Only work with -c option
```

Here -c enables the cpu sampling feature and -H enables the memory sampling feature. The module can be started with the following command:

```bash
mainboard -d /apollo/modules/planning/planning_component/dag/planning.dag -c -H
```

By default, two sampling files, ${process_group}\_cpu.prof and ${process_group}\_mem.prof, are generated in the current path.

The sampling feature can also be added in the launch file, for example planning's launch

```xml


<cyber>
    <module>
        <name>planning</name>
        <dag_conf>/apollo/modules/planning/planning_component/dag/planning.dag</dag_conf>
        <cpuprofile>/apollo/planning_cpu.prof</cpuprofile>
        <memprofile>/apollo/planning_mem.prof</memprofile>
        <process_name>planning</process_name>
    </module>
</cyber>
```

Add the cpuprofile and memprofile tags. After running this launch, the sampling files planning_cpu.prof and planning_mem.prof are generated under the /apollo path.

> CPU sampling generates only one prof file, while memory sampling generates multiple prof files, each of which represents the process's memory usage at the time it was generated:

![cyber_performance_analysis_4](./images/cyber_performance_analysis_4.png)

> Because memory and cpu sampling affect performance, it is not recommended to enable both types of sampling at the same time; it is recommended to enable only one per launch

##### Generating Visualization Results

CPU: Use the following commands to visualize the sampling results and generate a flame graph:

Source code:

```bash
bash scripts/sampling.sh planning_cpu.prof
```

Package management:

```bash
buildtool sampling planning_cpu.prof
```

Memory: Generate a directed acyclic graph or flame graph of the memory sampling as needed

Analyzing memory hotspots: find out which code paths or functions cause large memory allocations

```bash
pprof --svg $(which mainboard) planning_mem.prof.0001.heap > a.svg # Generate a directed graph
```

> A directed graph carries memory information, which makes it clearer which function allocated how much memory, while a flame graph is more intuitive. Developers can choose which type of graph to generate according to their habits

Analyze how much memory each function allocated between two moments, which is usually used to troubleshoot memory leak problems

```bash
pprof --svg --base planning_mem.prof.0005.heap $(which mainboard) planning_mem.prof.0001.heap > a.svg
```

##### Analysis

cpu

![cyber_performance_analysis_5](./images/cyber_performance_analysis_5.png)

The above is the flame graph of the cyber example communication program. From bottom to top it represents the function call stack, and the length represents the percentage of CPU resources occupied by the function in the whole process. For example, ReadMessage in the figure above accounts for 81.82%, so it can be seen that this part occupies the vast majority of the memory. Therefore, the flame graph tells us that we need to optimize the functions at the top of the graph and the longer ones (with a high percentage), so as to reduce the process's cpu usage

Memory

Memory hotspot analysis

Find memory hotspot allocations through heat maps or directed graphs

![cyber_performance_analysis_6](./images/cyber_performance_analysis_6.png)
![cyber_performance_analysis_7](./images/cyber_performance_analysis_7.png)

Both heat maps and directed graphs can find memory hotspot allocations. Likewise, a heat map represents the function call stack from bottom to top, and the length represents the percentage of memory allocated by the function in the whole process;

while for the directed graph:

- The upper left corner of the directed graph shows how much memory the process has allocated in total at the current snapshot point in time
- Boxes represent the actual functions, and each box has two numbers:
  - The first number represents how much memory the current function allocated
  - The second number represents how much memory the functions it calls allocated
  - Lines represent the call chain

In this way, you can quickly locate the functions where memory hotspots are allocated and the corresponding call chains.

Memory leak analysis:

Memory leaks are usually analyzed by comparing the memory allocation diff at different points in time with the following command:

```bash
pprof --svg --base planning_mem.prof.0005.heap $(which mainboard) planning_mem.prof.0001.heap > a.svg
```

The above command generates a directed graph of the memory diff between two points in time. If you find that the allocation size of a function keeps increasing at different points in time, there may be a memory leak:

![cyber_performance_analysis_8](./images/cyber_performance_analysis_8.png)

### GPU and GPU Memory

GPU analysis needs to be based on the NVIDIA Nsight Systems tools, namely:

- nsys system: used to find performance bottlenecks, understand application structure, identify synchronization and concurrency problems, and optimize overall performance
- nsys compute: focuses on the performance analysis of individual CUDA kernel functions, used to tune CUDA kernel function parameters and analyze register usage, shared memory usage, execution paths, and memory access patterns to improve kernel function performance

> Similar to gperftools, nsys has a serious impact on process performance and should only be used in the development and analysis phases, not during routine road tests

#### Usage

Start the tool to sample the process (taking perception as an example):

```bash
sudo -E nsys profile --gpu-metrics-device all --cuda-memory-usage true --delay=15 --duration=30 -f true -o preception --trace=cuda,cudnn,cublas,osrt,nvtx mainboard -d /apollo/modules/perception/msg_adapter/dag/msg_adapter.dag -d /apollo/modules/perception/pointcloud_preprocess/dag/pointcloud_preprocess.dag -d /apollo/modules/perception/pointcloud_map_based_roi/dag/pointcloud_map_based_roi.dag -d /apollo/modules/perception/pointcloud_ground_detection/dag/pointcloud_ground_detection.dag -d /apollo/modules/perception/lidar_detection/dag/lidar_detection.dag -d /apollo/modules/perception/lidar_detection_filter/dag/lidar_detection_filter.dag -d /apollo/modules/perception/lidar_tracking/dag/lidar_tracking.dag
```

- profile specifies that sampling is to be performed
- --gpu-metrics-device all specifies that all gpu-related metrics should be collected
- --cuda-memory-usage true specifies that the gpu memory allocated in cuda should be tracked
- --delay=15 --duration=30 specifies that sampling starts 15s after the program starts and ends after collecting for 30 seconds
- -f true specifies that the generated report file overwrites the existing report
- -o preception specifies the name of the generated report. After collection ends, a perception.nsys-rep file is generated in the current path
- --trace=cuda,cudnn,cublas,osrt,nvtx specifies the interfaces to collect
- mainboard ... is the normal command for starting perception in apollo

> Note: collection requires sudo privileges

Open the nsys graphical interface

```bash
nsys-ui
```

Open the file generated by the collection in the interface

![cyber_performance_analysis_9](./images/cyber_performance_analysis_9.png)

After opening, the following rows appear:

![cyber_performance_analysis_10](./images/cyber_performance_analysis_10.png)

Where:

- The cpu row includes the instantaneous cpu utilization while the process is running
- The iGPU row includes the various gpu performance metrics on orin
- CUDA HW contains the kernel functions running in each cuda stream
- Threads contains the functions running in the process, including cpu system libraries and interfaces such as cuda, cublas, and cudnn. These functions place the corresponding kernel functions into the stream's waiting queue to wait to be run by CUDA HW

> A brief introduction to the logic of executing cuda kernel functions: when a program calls a kernel function at the application layer, the hardware does not execute it immediately, but launches it into a queue waiting for execution, and executes it when the gpu is able to process it

The horizontal axis is the timeline of the process run

After expanding:

![cyber_performance_analysis_11](./images/cyber_performance_analysis_11.png)

The timeline can be divided into two parts. In the second part (green box), the gpu behavior is seen to repeat continuously, which shows it belongs to model inference; the first part (blue box) is the model being initialized. Performance analysis mainly focuses on the model inference part

Zooming in on one inference:

![cyber_performance_analysis_12](./images/cyber_performance_analysis_12.png)

As mentioned earlier, the iGPU row contains multiple gpu metrics, and the one we care about most is the SM occupancy (corresponding to the red box). In addition, the CUDA HW row records the kernel functions executed on the hardware along the timeline; and the green box is the cuda API used to launch this kernel function

Through the SM Warp Occupancy row, when the SM occupancy is high (as shown in the figure above, the occupancy reaches 95%), you can quickly locate and obtain detailed information about the corresponding executing kernel function:

![cyber_performance_analysis_13](./images/cyber_performance_analysis_13.png)

As well as the call chain at the point where this kernel function is launched:

![cyber_performance_analysis_14](./images/cyber_performance_analysis_14.png)

With the above information, for kernel functions we write ourselves, we can quickly locate the bottleneck; for model inference the situation is more complicated, because each kernel function in it is equivalent to an operator, and there is no way to precisely match it to a layer of the model

For model inference problems, if you are using

- tensorRT inference:

  - You can determine the layer corresponding to a kernel function by naming layers through the tensorRT API, nvinfer1::INetworkDefinition::setLayerName
  - Or create corresponding time points with NVTX:

  ```cpp
  #include <nvToolsExt.h>

  // Start a new NVTX range
  nvtxRangePush("LayerName");

  // Execute the computation of a TensorRT layer

  // End the NVTX range
  nvtxRangePop();
  ```

  - Then you can see the statistics of the corresponding kernel functions on the corresponding timeline in nsight system

- paddle inference: paddle has a built-in profiler, which can also be used to view the running status of each operator in each layer. For details, refer to:https://www.paddlepaddle.org.cn/documentation/docs/zh/2.3/guides/performance_improving/profiling_model.html

For memory analysis, nsight system also has similar output:

![cyber_performance_analysis_15](./images/cyber_performance_analysis_15.png)

Likewise, with the above information, for allocation functions such as cudaMalloc that we write ourselves, we can quickly locate the bottleneck; for model inference, a method like NVTX is also needed to determine which layer the allocated memory belongs to

## Optimization Suggestions

### cpu

![cyber_performance_analysis_16](./images/cyber_performance_analysis_16.png)

The main optimization idea is to optimize the functions at the top and the longer ones (with a high percentage) in the flame graph. In general, the following points can be considered:

- Whether this type of function can be removed:
  - For example, in the figure above, the function tzset takes about 20% of the cpu time slices. When the environment variable TZ is not set, the kernel solves for the time zone through complex calculations, while after setting the TZ environment variable there is no such calculation
- This type of function involves complex computation:

  - Consider whether an algorithm with lower time complexity can be used
  - Consider whether the computation can be parallelized:

    - Use cuda to accelerate the computation and release the occupied cpu time to reduce cpu utilization
    - Use the SIMD instruction set to process computation in parallel: SIMD is short for single instruction, multiple data, and this type of instruction set can process mathematical operations on multiple data at the same time. For example, the following code uses the neon instruction set on the arm architecture to process 16 pixels in parallel to convert a yuyv image into an rgb image, reducing the time the function occupies on the cpu:

    ```cpp
    for (int j = 0; j < height_; j += 2) {
        for (int i = 0; i < width_; i += 16) {
            // Read 16 pixels into dual-channel registers, where channel 1 stores the y component and channel 2 stores the uv component (32 bytes)
            uint8x16x2_t row0 = vld2q_u8(usrc + j * width_ * 2 + i * 2);
            uint8x16x2_t row1 = vld2q_u8(usrc + (j + 1) * width_ * 2 + i * 2);

            // Extract the y component
            uint8x16_t y0 = row0.val[0];
            uint8x16_t y1 = row1.val[0];

            // Save the y component into the array
            vst1q_u8(yPlane + yIndex, y0);
            vst1q_u8(yPlane + yIndex + width_, y1);
            yIndex += 16;

            // Extract the uv component
            uint8x16_t u0 = row0.val[1];
            uint8x16_t v0 = row1.val[1];

            // Compute the mean of the uv components (to mitigate the data loss caused by downsampling)
            uint16x8_t u16_low = vaddl_u8(vget_low_u8(u0), vget_low_u8(v0));
            uint16x8_t u16_high = vaddl_u8(vget_high_u8(u0), vget_high_u8(v0));
            uint8x8_t u8_avg_low = vshrn_n_u16(u16_low, 1);
            uint8x8_t u8_avg_high = vshrn_n_u16(u16_high, 1);

            uint16x8_t v16_low = vaddl_u8(vget_low_u8(row0.val[1]), vget_low_u8(row1.val[1]));
            uint16x8_t v16_high = vaddl_u8(vget_high_u8(row0.val[1]), vget_high_u8(row1.val[1]));
            uint8x8_t v8_avg_low = vshrn_n_u16(v16_low, 1);
            uint8x8_t v8_avg_high = vshrn_n_u16(v16_high, 1);

            uint8x16_t u_avg = vcombine_u8(u8_avg_low, u8_avg_high);
            uint8x16_t v_avg = vcombine_u8(v8_avg_low, v8_avg_high);

            // Save the uv components into the array
            vst1_u8(uPlane + uIndex, u8_avg_low);
            vst1_u8(vPlane + vIndex, v8_avg_low);
            uIndex += 8;
            vIndex += 8;
        }
        yIndex += width_; // Skip to the next line
    }
    ```

  - Consider whether the computation can be executed on non-cpu hardware: for example, the nvjpeg chip on orin can be dedicated to tasks such as image compression

### Memory

The idea is the same as for the cpu: we need to see which functions we can optimize. Heat maps and directed graphs let us see the approximate percentage of memory allocated by each function as well as the exact values, for example for the prediction process:

![cyber_performance_analysis_17](./images/cyber_performance_analysis_17.png)

![cyber_performance_analysis_18](./images/cyber_performance_analysis_18.png)

In general, the following points can be considered:

- Whether the process links libraries such as cuda and tensorrt: libraries such as cuda and tensorrt request memory in each process, and this memory is similar to a "singleton" in c++; in other words, this part of the memory can be reused. If the perception and prediction processes are started separately, this part of the memory is requested twice; while if perception and prediction are merged in the launch file to start a single process, this part of the memory is requested only once. If the autonomous driving system you build has a large number of processes that use the gpu, they can be merged into one to further reduce memory
- Whether there is memory that was requested but not used, which can be analyzed in detail through the sampling results

### gpu

gpu optimization is divided into latency optimization and utilization optimization, which correspond to different goals:

- Latency optimization: reduce the processing latency of one or more gpu kernel functions; this type of optimization may increase gpu utilization
- Utilization optimization: reduce the utilization of one or more gpu kernel functions

#### Algorithm Optimization:

Adopt a more efficient model or an algorithm with lower time complexity

#### Engineering Optimization:

##### CPU-Bound

Symptom: gaps on the timeline:

![cyber_performance_analysis_19](./images/cyber_performance_analysis_19.png)

Optimization: determine what the cpu is doing during this gap and reduce the gap time (refer to the cpu optimization methods)

Improves latency, with little effect on utilization

##### Memcpy Operations on the Timeline:

![cyber_performance_analysis_20](./images/cyber_performance_analysis_20.png)

Optimization:

- Reduce redundant transfers
- Use large-block transfers to make the most of the bandwidth
- If the hardware supports unified memory (orin), use unified memory to reduce memcpy:

Unified memory usage example:

```cpp
// Allocate memory normally
char* ptr = (char*) malloc(nums * sizeof(char))
char* gpu_ptr = nullptr;
// Mark the memory
cudaError_t err = cudaHostRegister(ptr, nums * sizeof(char), cudaHostRegisterMapped);
// Map the contents of ptr to gpu_ptr
err = cudaHostGetDevicePointer(&gpu_ptr, ptr, 0);

// Afterwards, the gpu reads the actual contents from gpu_ptr
```

Improves both latency and utilization

##### Many small kernels, with kernel launch time higher than execution time:

![cyber_performance_analysis_21](./images/cyber_performance_analysis_21.png)

Optimization:

Kernel fusion: fuse small kernels into large kernels to reduce the number of launches

Apply the cudaGraph technique: merge multiple kernel launches into one cudaGraph launch:

![cyber_performance_analysis_22](./images/cyber_performance_analysis_22.png)

cudaGraph usage example:

```cpp
#include <cuda_runtime.h>

// Suppose you have a CUDA kernel function
__global__ void myKernel(int *data, int N) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < N) {
        data[idx] += 1;
    }
}

int main() {
    const int N = 1024;
    int *d_data;
    cudaStream_t stream;
    cudaGraph_t graph;
    cudaGraphExec_t graphExec;

    // Allocate device memory
    cudaMalloc(&d_data, N * sizeof(int));

    // Create a CUDA stream
    cudaStreamCreate(&stream);

    // Start capturing the graph
    cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);

    // Perform operations in the captured graph
    dim3 blockSize(256);
    dim3 gridSize((N + blockSize.x - 1) / blockSize.x);
    myKernel<<<gridSize, blockSize, 0, stream>>>(d_data, N);

    // End capturing the graph
    cudaStreamEndCapture(stream, &graph);

    // Instantiate and launch the graph
    cudaGraphInstantiate(&graphExec, graph, NULL, NULL, 0);
    cudaGraphLaunch(graphExec, stream);

    // Synchronize the stream to ensure the graph execution is complete
    cudaStreamSynchronize(stream);

    // Clean up resources
    cudaGraphExecDestroy(graphExec);
    cudaGraphDestroy(graph);
    cudaStreamDestroy(stream);
    cudaFree(d_data);

    return 0;
}
```

Improves latency, with little effect on utilization

##### When SM occupancy has not reached 100%, kernel functions without dependencies can be executed in parallel through multiple streams (that is, changing serial execution to parallel)

Single stream:

![cyber_performance_analysis_23](./images/cyber_performance_analysis_23.png)

Multiple streams:

![cyber_performance_analysis_24](./images/cyber_performance_analysis_24.png)

Usage example:

```cpp
#include <cuda_runtime.h>

// Suppose you have a simple CUDA kernel function
__global__ void myKernel(int *data, int N) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < N) {
        data[idx] = data[idx] + 1;
    }
}

int main() {
    const int N = 1024 * 1024; // Data size
    const int numStreams = 4;  // Number of streams used
    const int streamSize = N / numStreams; // Workload of each stream
    const int blockSize = 256; // Block size
    const int gridSize = streamSize / blockSize; // Grid size of each stream

    int *d_data;
    cudaStream_t streams[numStreams];

    // Allocate device memory
    cudaMalloc(&d_data, N * sizeof(int));

    // Create CUDA streams
    for (int i = 0; i < numStreams; ++i) {
        cudaStreamCreate(&streams[i]);
    }

    // Launch kernel execution in each stream
    for (int i = 0; i < numStreams; ++i) {
        int offset = i * streamSize;
        myKernel<<<gridSize, blockSize, 0, streams[i]>>>(d_data + offset, streamSize);
    }

    // Wait for all streams to complete
    for (int i = 0; i < numStreams; ++i) {
        cudaStreamSynchronize(streams[i]);
    }

    // Clean up resources
    for (int i = 0; i < numStreams; ++i) {
        cudaStreamDestroy(streams[i]);
    }
    cudaFree(d_data);

    return 0;
}
```

Improves latency, and utilization also increases noticeably

##### Reducing Kernel Latency:

![cyber_performance_analysis_25](./images/cyber_performance_analysis_25.png)

To reduce the latency of a kernel, you need to understand the SOL (Speed Of Light) metrics of that kernel, which can be obtained by sampling with nsight compute:

- Memory SOL: This metric represents the efficiency of the application when accessing global memory. It is obtained by comparing the memory bandwidth actually used by the application with the GPU's maximum memory bandwidth. If Memory SOL is low, there may be a memory access bottleneck.
- SM SOL: This metric represents the efficiency of the application when performing computation tasks. It is obtained by comparing the application's computation performance with the GPU's maximum computation performance (peak floating-point performance). If SM SOL is low, the computing resources may not be fully utilized.

Generally, the following situations may occur:

- Memory-bound: low Memory SOL

![cyber_performance_analysis_26](./images/cyber_performance_analysis_26.png)

- Compute-bound: low SM SOL

![cyber_performance_analysis_27](./images/cyber_performance_analysis_27.png)

Optimization:

- Memory-bound: reduce the use of global memory and use shared memory within a block

![cyber_performance_analysis_28](./images/cyber_performance_analysis_28.png)

- Compute-bound:

  - Use high-throughput instructions: reduce precision (fp64->fp32, fp32->fp16/int8); use fast math functions (\_\_func())
  - Reduce intra-warp branching: a warp is an execution unit made up of a certain number of threads. The warp is the basic unit of GPU scheduling and instruction execution, and all threads in it execute the same instruction sequence. Suppose a warp has 32 threads; for a kernel function that contains a branch:

  ```cpp
  if (threadIdx.x < 16) {
      // Perform some operation A
  } else {
      // Perform some operation B
  }
  ```

  the first half of the warp (threads 0-15) executes operation A, and the second half (threads 16-31) executes operation B. The GPU first executes the threads performing operation A and then the threads performing operation B. In this case, the warp's efficiency is only half of the maximum, resulting in being compute-bound.

  Therefore, make sure as much as possible that all threads in a warp execute the same instructions; reducing intra-warp branching can improve the SM SOL
