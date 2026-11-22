### Problem Description:

Compilation fails with: collect2: error: ld returned 1 exit status, and glibc symbols appear in the error.

![](images/build_error_ld.png)

### Cause:

On the ARM architecture, nvidia-docker-runtime mounts some GPU-related dynamic libraries from the host machine into the container.
The Ubuntu version that Apollo currently supports is 20.04, which corresponds to glibc 2.31. When the user uses Ubuntu 22.04, the corresponding glibc version is 2.33+, and the container lacks these symbols, so ld reports an error when linking these dynamic libraries.


### Solution:

On the ARM architecture, the version Apollo currently supports is Ubuntu 20.04, and users are advised to use this version of Ubuntu.
x86 does not have this problem.

