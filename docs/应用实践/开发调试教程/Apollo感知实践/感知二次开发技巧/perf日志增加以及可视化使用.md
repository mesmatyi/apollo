## Adding perf Performance Logs

 **Purpose**: In package management mode or source mode, use the perf tool in cyber to measure the time spent in each stage of the program, to facilitate performance analysis and problem localization.

The demo below (package management mode) describes how to use the perf tool.

If the user has downloaded the lidar_detection package:

1. In the cc file where you need to measure program time, include the header file

```plain
#include "cyber/profiler/profiler.h"
```

2. Add the following code to the BUILD file

```plain
load("//tools/platform:build_defs.bzl", "if_profiler")
```

Note: You need to add the copts option if_profiler() to the apollo_component or apollo_cc_library in the BUILD corresponding to the cc file where the perf code is added.

For example: performance measurement code is added in the lidar_detection_component.cc file, so the copts option switch copts = PERCEPTION_COPTS + if_profiler() needs to be added to apollo_component

```plain
apollo_component(
    name = "liblidar_detection_component.so",
    srcs = ["lidar_detection_component.cc"],
    hdrs = ["lidar_detection_component.h"],
    copts = PERCEPTION_COPTS + if_profiler(),
    linkopts = ["-lleveldb"],
    deps = [
        "//cyber",
        ":apollo_perception_lidar_detection",
        "//modules/perception/common/onboard:apollo_perception_common_onboard",
        "//modules/perception/lidar_detection/proto:lidar_detection_component_config_cc_proto",
    ],
)
```

3. Add the time measurement function call code

- To measure the overall running time of a function, use PERF_FUNCION(), which measures the total time from entering the function until its destructor releases

```plain
bool LidarDetectionComponent::Proc(
    const std::shared_ptr<LidarFrameMessage>& message) {
  PERF_FUNCION()
  // internal proc
  bool status = InternalProc(message);
  if (status) {
    writer_->Write(message);
    AINFO << "Send Lidar detection output message.";
  }
  return status;
}
```

- To measure the time of a section of the program, use PERF_BLOCK (with a custom log name) and PERF_BLOCK_END, which measure the time in between

```plain
  PERF_BLOCK("lidar_detector")
  LidarDetectorOptions detection_options;
  if (!detector_->Detect(detection_options, in_message->lidar_frame_.get())) {
    AERROR << "Lidar detector detect error!";
    return false;
  }
  PERF_BLOCK_END
```

4. After adding the corresponding code, build with the parameter --define ENABLE_PROFILER=true to turn on the perf switch; it is off by default

```plain
buildtool build -p modules/perception/lidar_detection --gpu --opt -a "--define ENABLE_PROFILER=true"
```

5. View the logs

The logs are stored under the /apollo_workspace/data/log path, where a perf.INFO log is generated that stores the related performance timing logs.

![img](./日志阶段耗时.png)

## Using the aperf Tool

Install aperf

```plain
pip3 install modules/tools/aperf/
```

Note: If you encounter the following situation, run sudo apt-get install python3-tk

![img](./安装错误提示.png)

Plot the time cost of all modules

```plain
aperf cost --file=data/perf.INFO
```

![img](./timecost.png)

Plot the timeline of all modules

```plain
aperf timeline --file=data/perf.INFO
```

![img](./timeline.png)