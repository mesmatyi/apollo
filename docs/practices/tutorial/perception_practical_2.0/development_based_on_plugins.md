# Development Based on Plugins

The goal of this practice session is to develop a plugin and illustrate the process of developing a perception plugin. The key points of secondary plugin development are building the code framework and configuration management. This plugin development implements an example function: filtering obstacles, fp_filter. This plugin is used to filter false detections. Here, objects with unreasonable sizes or low confidence are treated as false detections.

## Preparation

- [Environment configuration](https://apollo.baidu.com/community/article/1133)
- [Perception function debugging and parameter tuning](https://apollo.baidu.com/community/article/1134)

Enter the container:

```bash
# Enter the application-perception code repository
cd application-perception

# Pull and start the docker container; this command is not needed if it has already been pulled
aem start_gpu
# Enter the container
aem enter
# Download and install dependency packages: this installs all the dependency packages in cyberfile.xml under the core directory
buildtool build --gpu
```

Plugins are created through a command; after the command is executed, the code framework is created automatically.

## Create a plugin

Inside the container, run the command to create the plugin. fp_filter stands for false positive filter and is used to filter false detections.

```bash
#Create the plugin; --namespace specifies the namespace, --template specifies that a plugin is being created, and the last argument is the plugin path
buildtool create --namespace perception --template plugin modules/perception/fp_filter
```

The code structure after the plugin is successfully created:

![Create plugin.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E5%88%9B%E5%BB%BA%E6%8F%92%E4%BB%B6_89be993.png)

## Plugin development

This plugin is applied to lidar_detection_filter. The generated plugin has default configuration that needs to be modified and developed.

### Code modification

Add the perception-lidar-detection-filter package dependency in cyberfile.xml:

```bash
<depend repo_name="perception-lidar-detection-filter" type="binary">perception-lidar-detection-filter</depend>
```

Modify the code:

Modify the code in the header file fp_filter.h. The places that need to be modified are explained in the form of comments.

```bash
#pragma once

#include <memory>
#include "cyber/plugin_manager/plugin_manager.h"

#include "modules/perception/lidar_detection_filter/interface/base_object_filter.h"  // 0. Add the header file

namespace apollo {
namespace perception {
namespace lidar {  // 1. Define the namespace

class FpFilter : public BaseObjectFilter {  // 2. Inherit from the base class
public:
    FpFilter() = default;  // 3. Define the constructor and destructor
    virtual ~FpFilter() = default;

public:
    // 4. Override the following three functions; declare the FilterFlag method
    bool Init(const ObjectFilterInitOptions& options = ObjectFilterInitOptions()) override;
    bool Filter(const ObjectFilterOptions& options, LidarFrame* frame) override;
    std::string Name() const override {
        return "FalsePositiveFilter";
    }
    bool FilterFlag(base::ObjectPtr obj, double confidence_thresh, double volume_thresh, double height_diff_thresh);

private:
    std::vector<bool> fp_flag_;  // 5. Define the data structure
};

CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::perception::lidar::FpFilter, BaseObjectFilter)  // 6. Modify the plugin configuration

}  // namespace lidar
}  // namespace perception
}  // namespace apollo
```

Modify the configuration in plugin_fp_filter_description.xml:

```bash
<library path="modules/perception/fp_filter/libfp_filter.so">
    <class type="apollo::perception::lidar::FpFilter" base_class="apollo::perception::lidar::BaseObjectFilter"></class>
</library>
```

Add dependencies in BUILD:

```bash
apollo_plugin(
    name = "libfp_filter.so",
    srcs = [
        "fp_filter.cc",
    ],
    hdrs = [
        "fp_filter.h",
    ],
    description = ":plugin_fp_filter_description.xml",
    deps = [
        "//cyber",
        "//modules/perception/fp_filter/proto:fp_filter_proto",
        "//modules/perception/lidar_detection_filter:apollo_perception_lidar_detection_filter",  # Add dependency
    ],
)
```

### Function definition

The header file and plugin configuration have been modified. Define the code function in fp_filter.cc. The key points of the function are as follows:

- Filtering rule: objects with low confidence, small volume and small height difference are filtered as false detections;
- Filtered object categories: UNKNOWN, UNKNOWN_MOVABLE, UNKNOWN_UNMOVABLE, BICYCLE, PEDESTRIAN;

```bash
#include <memory>
#include "modules/perception/fp_filter/fp_filter.h"

namespace apollo {
namespace perception {
namespace lidar {

bool FpFilter::Init(const ObjectFilterInitOptions& options) {
    fp_flag_.clear();
    return true;
}

bool FpFilter::Filter(const ObjectFilterOptions& options, LidarFrame* frame) {
    if (!frame) {
        AINFO << "Lidar frame is nullptr.";
        return false;
    }
    fp_flag_.clear();
    size_t size = frame->segmented_objects.size();
    fp_flag_.assign(size, false);  // filter object if true
    // Except for vehicle, filter false detections for objects of other categories
    for (size_t i = 0; i < size; ++i) {
        base::ObjectPtr obj = frame->segmented_objects.at(i);
        base::ObjectType type = obj->type;
        if (type == base::ObjectType::UNKNOWN || type == base::ObjectType::UNKNOWN_MOVABLE
            || type == base::ObjectType::UNKNOWN_UNMOVABLE) {
            fp_flag_.at(i) = FilterFlag(obj, 0.4, 0.3, 0.3);
        } else if (type == base::ObjectType::BICYCLE) {
            fp_flag_.at(i) = FilterFlag(obj, 0.4, 0.4, 0.8);
        } else if (type == base::ObjectType::PEDESTRIAN) {
            fp_flag_.at(i) = FilterFlag(obj, 0.4, 0.3, 1.0);
        }
    }

    // do filter
    size_t valid_num = 0;
    for (size_t i = 0; i < fp_flag_.size(); ++i) {
        base::ObjectPtr obj = frame->segmented_objects.at(i);
        if (!fp_flag_.at(i)) {
            frame->segmented_objects.at(valid_num) = obj;
            valid_num++;
        }
    }
    frame->segmented_objects.resize(valid_num);
    AINFO << "FalsePositiveFilter, filter fp " << size - valid_num << " objects, from " << size << " objects";

    return true;
}

/**
 * @brief
 *
 * @param obj
 * @param confidence_thresh Filter objects with low confidence
 * @param volume_thresh Filter objects with small volume
 * @param height_diff_thresh Filter
 * @return true
 * @return false
 */
bool FpFilter::FilterFlag(
        base::ObjectPtr obj,
        double confidence_thresh,
        double volume_thresh,
        double height_diff_thresh) {
    // confidence
    if (obj->confidence < confidence_thresh) {
        return true;
    }
    // volume
    auto obj_size = obj->size;
    double volume = obj_size[0] * obj_size[1] * obj_size[2];
    if (volume < volume_thresh) {
        return true;
    }
    // height diff
    double max_z = -10000.0;
    double min_z = 10000.0;
    for (size_t i = 0; i < obj->lidar_supplement.cloud.size(); ++i) {
        double val_z = obj->lidar_supplement.cloud[i].z;
        max_z = std::max(max_z, val_z);
        min_z = std::min(min_z, val_z);
    }
    double height_diff = max_z - min_z;
    if (height_diff < height_diff_thresh) {
        return true;
    }

    return false;
}

}  // namespace lidar
}  // namespace perception
}  // namespace apollo
```

## Plugin usage

### Configure the plugin

Add the fp_fitler plugin in `/apollo/modules/perception/lidar_detection_filter/data/filter_bank.pb.txt`, as shown on line 11 in the figure below.

The configuration of the fp_filter plugin is hard-coded in the program, so both config_path and config_file are empty.

![Configure plugin.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E9%85%8D%E7%BD%AE%E6%8F%92%E4%BB%B6_a6ccc4a.png)

### Example result

Start perception: start the LiDAR perception and transform modules through cyber_launch.

![Example result.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E7%BB%93%E6%9E%9C%E7%A4%BA%E4%BE%8B_933949f.png)

Then play a record through `cyber_record play -f ***.record`, playing only the three necessary channels, controlled by -c. Go to the directory `/home/apollo/.apollo/resources/records` and run:

```bash
cyber_recorder play -f demo3_mkz_110_sunnybigloop.record -c /tf /apollo/sensor/velodyne64/compensator/PointCloud2 /apollo/localization/pose
```

Perception result:

![Perception result.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%84%9F%E7%9F%A5%E7%BB%93%E6%9E%9C_599bf66.png)

View the perception log through mainboard.INFO. You can see that fp_fitler is working normally and can filter objects according to the rules.

![Perception log.png](https://bce.bdstatic.com/doc/Apollo-Homepage-Document/Apollo_alpha_doc/%E6%84%9F%E7%9F%A5%E6%97%A5%E5%BF%97_37413f2.png)
