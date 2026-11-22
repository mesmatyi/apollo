### Problem Description:

In the source-code environment, the user entered the container with sudo bash docker/scripts/dev_into.sh and then compiled, which fails with: cannot find bazel. Please install bazel first.

![](images/cannot_find_bazel.png)

### Cause:

sudo was used when entering the container, which caused a permissions mix-up.

### Solution:

Do not add sudo when running the commands that start the container and enter the container.

**Note:** If running the start-container and enter-container commands as the current user fails with: dial unix /var/run/dcker.sock: connect: permission denied, you need to add the current user to the docker group or grant 777 permissions to docker.sock.
