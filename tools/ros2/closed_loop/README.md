# Apollo closed-loop test on ROS 2 (recorded to MCAP)

A software-in-the-loop test that drives Apollo's real routing, planning and
control modules, unmodified, against a simulated vehicle. Everything runs over
ROS 2 and every run is recorded to a rosbag2 MCAP file.

```
                LaneFollowCommand (service)
                          |
                          v
  +------------------ ROS 2 component container ----------------------+
  | external_command (routing)  ->  /apollo/planning/command          |
  | planning  <- /apollo/prediction, /apollo/canbus/chassis,          |
  |              /apollo/localization/pose  ->  /apollo/planning      |
  | control   <- planning, chassis, localization  ->  /apollo/control |
  +-------------------------------------------------------------------+
        ^                                              |
        |   /apollo/localization/pose  (100 Hz)        v
        |   /apollo/canbus/chassis     (100 Hz)   vehicle_sim.py
        +-- /apollo/prediction (10 Hz, empty) --  (kinematic bicycle model)

  ros2 bag record -a -s mcap   records every topic of the run
```

## Running

```bash
source scripts/ros2/setup.bash          # ROS 2 + install/, APOLLO_ROOT
tools/ros2/closed_loop/run_closed_loop.py --runs 3 --storage mcap
```

* `--runs N` repeats the closed-loop scenario N times. Each run uses its own
  ROS domain, its own processes and its own bag.
* `--storage mcap|sqlite3` selects the rosbag2 storage of the recording.
* `--map_dir`, `--lane`, `--start_s`, `--goal X Y HEADING` and
  `--target_speed` select the scenario. The default drives lane `1_-1` of the
  `demo` map, about 150 m around a 90° bend, to the map's default end point.
* `--goal_tolerance`, `--max_lateral_deviation` and `--timeout` set the pass
  criteria.

The command writes `summary.md` and `summary.json`, plus one
`run_NN/{bag/,report.json,stack.log,vehicle_sim.log,command.log}` per run. It
exits non-zero unless every run passes.

A run passes when all of these hold:

* the vehicle stops with its front bumper within `--goal_tolerance` of the
  goal. Localization reports the rear axle, and planning stops the car with
  the bumper at the destination stop line.
* the bag uses the requested storage (MCAP).
* planning published trajectories and control published commands.
* planning never issued an emergency stop.
* every recorded Apollo message decodes.
* the maximum lateral deviation from the lane centre line is below
  `--max_lateral_deviation`.
* the Apollo stack shut down cleanly.

## Pieces

| file | purpose |
|---|---|
| `closed_loop.launch.py` | external command, planning and control in one component container, configured from their dag files. Flag files are flattened with `/apollo/...` resolved against `APOLLO_ROOT`, and the map is overridden. |
| `vehicle_sim.py` | kinematic bicycle model driven by `ControlCommand.acceleration` / `steering_target`, with actuator lag and the limits from `vehicle_param.pb.txt`. Publishes localization, chassis and an empty prediction stream, which triggers planning. |
| `send_lane_follow.py` | sends a `LaneFollowCommand` to `/apollo/external_command/lane_follow` |
| `analyze_bag.py` | decodes a rosbag2 (MCAP or sqlite3) recording: message rates, path, lateral deviation, planning/control health, distance to goal |
| `run_closed_loop.py` | orchestrates recording, simulator, stack, command, pass/fail and the reports |

The recordings are ordinary rosbag2 bags with natively typed topics
(`/apollo/planning` is `apollo_msgs/msg/PlanningADCTrajectory`, and so on):
`ros2 bag info run_00/bag` and `ros2 bag play run_00/bag` work on them, as
should any tool that reads ROS 2 bags. Use
`cyber/tools/cyber_recorder/rosbag2_to_record.py` to turn one into a Cyber
`.record` file.
