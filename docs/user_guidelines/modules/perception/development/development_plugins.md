# Plugin Development

Perception plugins come in two parts: in the object_filter_bank of lidar_detection_filter, and in the roi_filter of pointcloud_map_based_roi.

The plugin mechanism is a dynamic loading mechanism that is more lightweight than components. Its purpose is to let developers focus more on the functional logic and to extend functionality more conveniently. Plugin development is highly independent, and plugins can be flexibly used by components and loaded as needed.
This section takes lidar_detection_filter as an example for plugin development. The object_filter_bank can run multiple filters, and all filters are placed in std::vector<std::shared_ptr<BaseObjectFilter>>.

## Create a Folder

Create a folder in the directory below, with the filter named new_filter, to hold the code and related configuration.

```bash
├── lidar_detection_filter
    ├── conf
    ├── dag
    ├── data
    ├── object_filter_bank
    │   ├── object_filter_bank.cc
    │   ├── object_filter_bank.h
    │   ├── roi_boundary_filter
    │   ├── new_filter  // newly defined filter
    ├── interface
    ├── proto
    ├── lidar_detection_filter_component.cc
    ├── lidar_detection_filter_component.h
    ├── cyberfile.xml
    ├── README.md
    └── BUILD
```

## Function Definition

Implement the filter function.
First, inherit from the BaseObjectFilter class and implement Init (initialize configuration items), Filter (function code), and Name (get the class name).

Then, at the end of new_filter.h, define: CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(apollo::perception::lidar::NewFilter, BaseObjectFilter). This indicates that this function is developed as a plugin.

```bash
class NewFilter : public BaseObjectFilter {
 public:
  /**
   * @brief Construct a new NewFilter object
   *
   */
  NewFilter() = default;
  virtual ~NewFilter() = default;

  /**
   * @brief Init of NewFilter
   *
   * @param options object filer options
   * @return true
   * @return false
   */
  bool Init(const ObjectFilterInitOptions& options =
                ObjectFilterInitOptions()) override;

  /**
   * @brief filter objects using new filter algorithm
   *
   * @param options object filter options
   * @param frame lidar frame to filter
   * @return true
   * @return false
   */
  bool Filter(const ObjectFilterOptions& options, LidarFrame* frame) override;

  /**
   * @brief Name of NewFilter object
   *
   * @return std::string name
   */
  std::string Name() const override { return "NewFilter"; }
 private:
   ... ...
 };

 CYBER_PLUGIN_MANAGER_REGISTER_PLUGIN(
    apollo::perception::lidar::NewFilter, BaseObjectFilter)
```

The specific function of the plugin is defined in the Filter function. This function receives the frame of the current point cloud frame, and the data in the frame mainly includes the following:

- The point cloud and its indices;
- Indices of non-ground points and of points inside the ROI;
- Detected obstacles (including model detection and background clustering);
- The pose from the LiDAR coordinate system to the world coordinate system;
- HD map information within a certain range of the ego vehicle.

lidar_detection_filter filters based on detected obstacles, and the filtering function is customized by developers according to the actual situation.

## Configuration File

Define the configuration items of new_filter in object_filter_bank/proto, and add the configuration of new_filter in `data/new_filter.pb.txt`.

```bash
├── lidar_detection_filter
    ├── conf
    ├── dag
    ├── data
    │   ├── new_filter.pb.txt  // filter bank; add the new_filter configuration here
    ├── object_filter_bank
    │   ├── object_filter_bank.cc
    │   ├── object_filter_bank.h
    │   ├── roi_boundary_filter
    │   ├── proto  // new_filter configuration definition
    │   ├── new_filter
    ├── interface
    ├── proto
    ├── lidar_detection_filter_component.cc
    ├── lidar_detection_filter_component.h
    ├── cyberfile.xml
    ├── README.md
    └── BUILD
```

## Plugin Management

First, the build rules in BUILD are as follows. Plugins need to be built with apollo_plugin.

```bash
load("//tools:apollo_package.bzl", "apollo_package", "apollo_plugin")
load("//tools:cpplint.bzl", "cpplint")

package(default_visibility = ["//visibility:public"])

apollo_plugin(
    name = "libnew_filter.so",
    srcs = ["new_filter.cc"],
    hdrs = ["new_filter.h"],
    description = ":plugins.xml",
    deps = [
    ],
)
```

Then, create new_filter/plugins.xml and define the following in it:

```bash
<library path="modules/perception/lidar_detection_filter/object_filter_bank/new_filter/libnew_filter.so">
    <class type="apollo::perception::lidar::NewFilter" base_class="apollo::perception::lidar::BaseObjectFilter"></class>
</library>
```

## Using the Plugin

Once the plugin is defined, it can be used. To use it, add the plugin's configuration to the configuration file `modules/perception/lidar_detection_filter/data/filter_bank.pb.txt`.

```bash
plugins {
    name: "ROIBoundaryFilter"
    config_path: "perception/lidar_detection_filter/data"
    config_file: "roi_boundary_filter.pb.txt"
}
plugins {
    name: "BackgroundFilter"
    config_path: "perception/lidar_detection_filter/data"
    config_file: "background_filter.pb.txt"
}
plugins {
    name: "NewFilter"  # add the new_filter configuration here
    config_path: "perception/lidar_detection_filter/data"
    config_file: "new_filter.pb.txt"
}
```

After configuring the new_filter plugin, start the lidar_detection_filter component and run perception, and the configuration of each plugin is loaded. When data comes in, new_filter executes the Filter function to complete the filtering function.
