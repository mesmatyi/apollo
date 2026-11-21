# planning

## Introduction

`planning-base` is the base data structure and algorithm library of the planning module. It contains the data structure classes shared across the module's running flow, as well as low-level algorithm functions.

## Directory Structure

```shell

modules/planning/planning_base
├── planning_base
    ├── common                  // Common algorithm library
    ├── gflags                  // gflag parameter configuration
    ├── learning_based          // Libraries related to learning-based algorithms
    ├── math                    // Basic math library
    ├── open_space              // Open-space related algorithm library
    ├── proto                   // Definitions of common (global) parameter configuration structures
    ├── reference_line          // Reference line and reference line processing classes
    ├── testdata                // Unit test data
    ├── tools                   // Utility classes
    ├── BUILD                   // Build rules file
    ├── cyberfile.xml           // Package management configuration file
    └── README.md               // Documentation
```
