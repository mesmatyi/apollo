### Problem Description

When using buildtool install planning, only the source code of the planning_component module is downloaded locally. How can I download all planning-related modules?

### Solution

You can use the wildcard "*" to download the source code of all planning-related modules:
```shell
buildtool install planning*
```
