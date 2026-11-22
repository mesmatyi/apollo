### Problem Description:

Compilation fails with: Server terminated abruptly (error code: 14, error message: 'Socket closed')

![](images/build_error_socket_closed.png)

### Cause:

Hardware resources are exhausted, causing an OOM.

### Solution:

#### 9.0 Source Code

Modify the script to control the hardware resources used for compilation: lines 746 and 748 in scripts/apollo_base.sh

```shell
--jobs=${count} --local_ram_resources=HOST_RAM*0.7
Change to
--jobs=2 --local_ram_resources=HOST_RAM*0.5
```

![](images/build_socket_closed.png)

#### 9.0 Package Management and 10.0

When compiling, you can use the -j and -m parameters to control the number of threads and the percentage of memory used. For example, buildtool build -j 4 -m 0.5 compiles with 4 threads and 50% of total memory.

![](images/build_command_show.png)
