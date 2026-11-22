# Apollo on ROS 2 Jazzy (Ubuntu 24.04).
#
#   docker build -f docker/ros2/Dockerfile -t apollo:ros2-jazzy .
#   docker run --rm -it --net=host -v $PWD:/apollo apollo:ros2-jazzy
#
# Stage "dev" holds every build dependency; stage "build" compiles the colcon
# workspace into /opt/apollo/install.
ARG ROS_DISTRO=jazzy
FROM ros:${ROS_DISTRO}-ros-base AS dev

ARG ROS_DISTRO
ENV DEBIAN_FRONTEND=noninteractive \
    ROS_DISTRO=${ROS_DISTRO} \
    APOLLO_ROOT=/apollo

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git pkg-config ccache \
      python3-colcon-common-extensions python3-rosdep python3-pip \
      python3-protobuf python3-numpy python3-yaml \
      ros-${ROS_DISTRO}-rclcpp-components \
      ros-${ROS_DISTRO}-sensor-msgs ros-${ROS_DISTRO}-sensor-msgs-py \
      ros-${ROS_DISTRO}-nav-msgs ros-${ROS_DISTRO}-tf2-msgs \
      ros-${ROS_DISTRO}-visualization-msgs ros-${ROS_DISTRO}-rosbag2 \
      ros-${ROS_DISTRO}-launch-ros \
      ros-${ROS_DISTRO}-rmw-cyclonedds-cpp ros-${ROS_DISTRO}-rosidl-runtime-py \
      ros-${ROS_DISTRO}-rosgraph-msgs ros-${ROS_DISTRO}-ros2cli-common-extensions \
      libprotobuf-dev libprotoc-dev protobuf-compiler \
      libgoogle-glog-dev libgflags-dev libgtest-dev libgmock-dev \
      nlohmann-json3-dev libtinyxml2-dev uuid-dev libncurses-dev \
      libeigen3-dev libboost-all-dev libabsl-dev libyaml-cpp-dev \
      libopencv-dev libpcl-dev libproj-dev libfftw3-dev libsqlite3-dev \
      coinor-libipopt-dev libadolc-dev libcurl4-openssl-dev \
      libcivetweb-dev libpcap-dev portaudio19-dev libleveldb-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Third-party libraries Ubuntu does not package (OSQP, ADOL-C, ...).
COPY scripts/ros2/install_third_party.sh /tmp/install_third_party.sh
RUN bash /tmp/install_third_party.sh && rm /tmp/install_third_party.sh

WORKDIR /apollo
RUN echo "source /opt/ros/${ROS_DISTRO}/setup.bash" >> /root/.bashrc && \
    echo "[ -f /opt/apollo/install/setup.bash ] && source /opt/apollo/install/setup.bash" >> /root/.bashrc

FROM dev AS build
COPY . /apollo
RUN . /opt/ros/${ROS_DISTRO}/setup.sh && \
    rosdep update --rosdistro ${ROS_DISTRO} && \
    rosdep install --rosdistro ${ROS_DISTRO} --from-paths ros2 cyber modules \
      --ignore-src -y -r && \
    APOLLO_INSTALL_BASE=/opt/apollo/install \
    APOLLO_BUILD_BASE=/opt/apollo/build \
    bash scripts/ros2/build.sh && \
    rm -rf /opt/apollo/build
CMD ["bash"]
