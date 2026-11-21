### Problem Description:

Downloading bazel-related dependencies fails during compilation: downloading [https://github.com/bazelbuild/...... connect time out

![](images/build_download_error.png)

### Cause:

A network problem caused the download of bazel dependencies to fail.

### Solution:

Pre-downloaded caches are now provided. Users can download the corresponding cache file according to their needs:
- [Apollo 8.0 pre-downloaded cache](https://apollo-system.cdn.bcebos.com/bazel_deps/3.7.1/cache.tar.gz)
- [Apollo 9.0 x86 pre-downloaded cache](https://apollo-system.cdn.bcebos.com/bazel_deps/5.2.1/cache.tar.gz)
- [Apollo 9.0 arm pre-downloaded cache](https://apollo-system.cdn.bcebos.com/bazel_deps/arm-5.2.1/cache.tar.gz)
