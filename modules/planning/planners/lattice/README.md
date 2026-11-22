# planning-planner-lattice

## Introduction

`planning-planner-lattice` is a plugin package containing the LatticePlanner. LatticePlanner is suitable for planning in simple scenarios such as highways.

## Directory Structure

```shell

modules/planning/planner/lattice
├── lattice
    ├── behavior                // Behavior decision library
    ├── trajectory_generation   // Trajectory generation library
    ├── lattice_planner.h       // LatticePlanner header file
    ├── lattice_planner.cc      // LatticePlanner source file
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    ├── plugins.xml             // Plugin description file
    └── README.md               // Documentation
```
