## nano_radar
This driver is developed on Apollo Cyber and supports the Nanoradar SR73F millimeter-wave radar.

### Configuration
Default radar configuration: [conf/nano_radar__front_conf.pb.txt]
When the radar starts, it first sends commands to the CAN card according to the configuration file above to configure the radar. Only when the received radar status information matches the user configuration does it begin parsing data and publishing messages.

### lanuch
dag_files: "/apollo/modules/drivers/radar/nano_radar/dag/nano_radar.dag"

### Topic
**topic name**: /apollo/sensor/nanoradar/front
**data type**:  apollo::drivers::NanoRadar
**channel ID**: CHANNEL_ID_ONE
