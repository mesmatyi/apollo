# Piecewise Jerk Path Optimization

_**Tip**: To display the equations in this document better, we recommend using the Chrome browser with the [plugin](https://chrome.google.com/webstore/detail/tex-all-the-things/cbimabofgmfdkicghcadidpemeenbffn), or copying the LaTeX equations into an [online formula editing website](http://www.hostmath.com/) to view them._

### *Table of Contents*

- [Overview](#overview)
- [Related code and corresponding versions](#related-code-and-corresponding-versions)
- [Code flow and framework](#code-flow-and-framework)
- [Related algorithm analysis](#related-algorithm-analysis)

# Overview

`Piecewise jerk path optimization` is a task of the planning module and belongs to the optimizer category of tasks.

The overall motion flow chart of the planning module is as follows:

![Overall flow chart](images/lane_follow.png)

The overall flow chart is explained using the [lane follow](https://github.com/ApolloAuto/apollo/tree/v9.0.0/modules/planning/scenarios/lane_follow) scenario as an example. The main functionality of a task is in the `Process` function.

For the detailed running process of Fig.1, see [path_bounds_decider]().

The flow of `piecewise jerk path optimization` is shown in the following figure:
![Overall flow chart](images/piecewise_jerk_path.png)


# Related code and corresponding versions

This section describes the code and algorithm of `piecewise jerk path optimization`.

Please refer to the code [Apollo r6.0.0 piecewise_jerk_path_optimization](https://github.com/ApolloAuto/apollo/tree/r6.0.0/modules/planning/tasks/optimizers/piecewise_jerk_path)

- Input
`PiecewiseJerkPathOptimizer::Process(
    const SpeedData& speed_data, const ReferenceLine& reference_line,
    const common::TrajectoryPoint& init_point, const bool path_reusable,
    PathData* const final_path_data)`

These include the reference line, the starting point and so on.

- Output

The `OptimizePath` function obtains the optimal path, with information including $opt\_l, opt\_dl, opt\_ddl$. In the `Process` function, the final result is saved in the variable reference_line_info_ of the task base class.

# Code flow and framework

The flow chart of the `piecewise jerk path optimization` code is as follows.

![Code flow chart](images/code.png)

- If the path is reused, return
```C++
common::Status PiecewiseJerkPathOptimizer::Process(
    const SpeedData& speed_data, const ReferenceLine& reference_line,
    const common::TrajectoryPoint& init_point, const bool path_reusable,
    PathData* const final_path_data) {
  // Skip piecewise_jerk_path_optimizer if the path is reused
  if (FLAGS_enable_skip_path_tasks && path_reusable) {
    return Status::OK();
  }
  ... ...
```

- Convert the adc starting point to Frenet coordinates
```C++
  ... ...
  const auto init_frenet_state =
      reference_line.ToFrenetFrame(planning_start_point);

  // For lane-change, select lane_change_path_config
  // Otherwise, select default_path_config
  const auto& config = reference_line_info_->IsChangeLanePath()
                           ? config_.piecewise_jerk_path_optimizer_config()
                                 .lane_change_path_config()
                           : config_.piecewise_jerk_path_optimizer_config()
                                 .default_path_config();
  ... ...
```

- Iterate over each path bound
```C++
  ... ...
  const auto& path_boundaries =
      reference_line_info_->GetCandidatePathBoundaries();
  ADEBUG << "There are " << path_boundaries.size() << " path boundaries.";
  const auto& reference_path_data = reference_line_info_->path_data();

  std::vector<PathData> candidate_path_data;
  // Iterate over each path
  for (const auto& path_boundary : path_boundaries) {
    size_t path_boundary_size = path_boundary.boundary().size();
  ... ...
```

- Determine whether it is pull-over or regular
① Determine whether it is pull-over

```C++
    ... ...
    if (!FLAGS_enable_force_pull_over_open_space_parking_test) {
      // pull over scenario
      const auto& pull_over_status =
          injector_->planning_context()->planning_status().pull_over();
      if (pull_over_status.has_position() &&
          pull_over_status.position().has_x() &&
          pull_over_status.position().has_y() &&
          path_boundary.label().find("pullover") != std::string::npos) {
        common::SLPoint pull_over_sl;
        reference_line.XYToSL(pull_over_status.position(), &pull_over_sl);
        end_state[0] = pull_over_sl.l();
      }
    }
    ... ...
```

② Determine whether it is regular

```C++
    ... ...
    if (path_boundary.label().find("regular") != std::string::npos &&
        reference_path_data.is_valid_path_reference()) {
      ADEBUG << "path label is: " << path_boundary.label();
      // When the reference path is in place
      for (size_t i = 0; i < path_reference_size; ++i) {
        common::SLPoint path_reference_sl;
        reference_line.XYToSL(
            common::util::PointFactory::ToPointENU(
                reference_path_data.path_reference().at(i).x(),
                reference_path_data.path_reference().at(i).y()),
            &path_reference_sl);
        path_reference_l[i] = path_reference_sl.l();
      }
      end_state[0] = path_reference_l.back();
      path_data.set_is_optimized_towards_trajectory_reference(true);
      is_valid_path_reference = true;
    }
    ... ...
```

- Optimize the path

```C++
    ... ...
    // Set parameters
    const auto& veh_param =
        common::VehicleConfigHelper::GetConfig().vehicle_param();
    const double lat_acc_bound =
        std::tan(veh_param.max_steer_angle() / veh_param.steer_ratio()) /
        veh_param.wheel_base();
    std::vector<std::pair<double, double>> ddl_bounds;
    for (size_t i = 0; i < path_boundary_size; ++i) {
      double s = static_cast<double>(i) * path_boundary.delta_s() +
                 path_boundary.start_s();
      double kappa = reference_line.GetNearestReferencePoint(s).kappa();
      ddl_bounds.emplace_back(-lat_acc_bound - kappa, lat_acc_bound - kappa);
    }
    // Optimization algorithm
    bool res_opt = OptimizePath(
        init_frenet_state.second, end_state, std::move(path_reference_l),
        path_reference_size, path_boundary.delta_s(), is_valid_path_reference,
        path_boundary.boundary(), ddl_bounds, w, max_iter, &opt_l, &opt_dl,
        &opt_ddl);
    ... ...
```

Optimization process:
1). Define the piecewise_jerk_problem variable, the optimization algorithm
2). Set the variables
&emsp; a. Weights
&emsp; b. Distance, velocity and acceleration bounds in the D direction
&emsp; c. Maximum steering angle rate
&emsp; d.jerk bound
3). Optimization algorithm
4). Get the result

- If successful, save the values to candidate_path_data

```C++
    ... ...
    if (res_opt) {
      for (size_t i = 0; i < path_boundary_size; i += 4) {
        ADEBUG << "for s[" << static_cast<double>(i) * path_boundary.delta_s()
               << "], l = " << opt_l[i] << ", dl = " << opt_dl[i];
      }
      auto frenet_frame_path =
          ToPiecewiseJerkPath(opt_l, opt_dl, opt_ddl, path_boundary.delta_s(),
                              path_boundary.start_s());

      path_data.SetReferenceLine(&reference_line);
      path_data.SetFrenetPath(std::move(frenet_frame_path));
      if (FLAGS_use_front_axe_center_in_path_planning) {
        auto discretized_path = DiscretizedPath(
            ConvertPathPointRefFromFrontAxeToRearAxe(path_data));
        path_data.SetDiscretizedPath(discretized_path);
      }
      path_data.set_path_label(path_boundary.label());
      path_data.set_blocking_obstacle_id(path_boundary.blocking_obstacle_id());
      candidate_path_data.push_back(std::move(path_data));
    }
    ... ...
```

- On failure, return an error code; on success, save the path points

```C++
  ... ...
  if (candidate_path_data.empty()) {
    return Status(ErrorCode::PLANNING_ERROR,
                  "Path Optimizer failed to generate path");
  }
  reference_line_info_->SetCandidatePathData(std::move(candidate_path_data));
  return Status::OK();
  ... ...
```

# Related algorithm analysis

The `piecewise jerk path optimization` algorithm is described in detail in the paper [Optimal Vehicle Path Planning Using Quadratic Optimization for Baidu Apollo Open Platform
](https://ieeexplore.ieee.org/document/9304787).

![Algorithm](images/path.png)

Path optimization algorithm:
- Generate path bounds from the guide line and obstacles
- Sample the guide line at equal intervals in the s direction
- Iteratively optimize $𝑙, 𝑙^{'}, 𝑙^{''}$ at each discrete point in the s direction.

## Building the mathematical model

### (1) Trajectory smoothing

![Smoothing](images/smooth.png)

$$
min \sum_{k=1}^{n-2} ||2P_k - P_{k-1} + P_{k+1}||_2^2 +\\
\sum_{k=0}^{n-1} ||2P_k - P_{k-ref}||_2^2 +\\
\sum_{k=0}^{n-2} ||P_{k+1} - P_k||_2^2
$$

subject to:
$$
P_k \in B, for: k = 0,...,n-1 \\
||2P_k - P_{k-1} - P_{k+1}||_2 < \frac{d_{ref}^2}{R_{min}} \\
for: k=1,...,n-2 
$$

where
- $P_k$ is $(x_k, y_k)$
- $P_{k\_ref}$ is the original point of the routing line
- $B$ is the bound of $P_k$ at $P_{k\_ref}$
- $\frac{d_{ref}^2}{R_{min}}$ is the maximum curvature constraint

### (2) Optimization objective



$$
\tilde{f}(l(s)) = w_l * \sum_{i=0}^{n-1} l_i^2 + w_{l^{'}} * \sum_{i=0}^{n-1} l_i^{'2} + w_{l^{''}} * \sum_{i=0}^{n-1} l_i^{''2} +\\
w_{l^{'''}} * \sum_{i=0}^{n-2}(\frac{l_{i+1}^{''} - l_i^{''}}{\Delta s})^2 +\\
w_{obs} * \sum_{i=0}^{n-1}(l_i - 0.5*(l_{min}^i + l_{max}^i))^2
$$

### (3) Constraints

- Continuity constraint

$$
l_{i+1}^{'''} = l_i^{''} + \int_0^{\Delta{s}} l_{i\rightarrow{i+1}}^{'''} ds = l_i^{''} + l_{i\rightarrow{i+1}}^{'''} * \Delta{s} \\
l_{i+1}^{'} = l_i^{'} + \int_0^{\Delta{s}}l^{''}(s)ds = l_i^{'} + l_i^{''}*\Delta{s} + \frac{1}{2} * l_{i\rightarrow{i+1}}^{'''} * \Delta{s^2} \\
l_{i+1} = l_i + \int_0^{\Delta{s}}l^{'}(s)ds \\
= l_i + l_i^{'}*\Delta(s^2) + \frac{1}{6}*l_{i\rightarrow{i+1}}*\Delta{s^3}
$$

- Safety constraint

The points in the $l$ direction must be within the bounds.

$$
l(s) \in l_B(s), \forall{s} \in [0, s_{max}]
$$

- Curvature constraint

The steering angle of the ego vehicle cannot exceed the maximum steering angle.

$$
tan(\alpha_{max})*k_r*l - tan(\alpha_{max}) + |k_r|*L \leqslant 0
$$

The optimization method uses [OSQP](https://osqp.org/).
