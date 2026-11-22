# Using the HD map
## Generating an HD map:
Because Apollo's HD map production is not open, generating an HD map currently requires making a request to Baidu.
If you want to produce one yourself, Apollo offers the following suggestions:
 * Raw data collection (vision, LiDAR, GPS, etc.) and processing.
 * Map data generation. Obtain map data from the data generated in step one, through algorithms or manually.
 * Map format organization. Convert the map data into Apollo's HD map format (you can refer to the base_map.xml format; all other maps can be generated from base_map.xml).
 * Note: the tools for all three of these steps need to be developed by yourself. For a small-scale simple test, you can also organize the data manually by following the base_map.xml format.

## Adding an HD map to apollo1.5:
There are two methods: one is to add a new directory and use it with the Apollo system; the other is to replace the map files in an existing directory.
### Adding a new HD map:
 * Create a directory new_map under the /apollo/modules/map/data directory.
 * Put the generated HD map into new_map; if there are configuration files, you can refer to the configuration files in the sunnyvale_office directory.
 * Build by running bash apollo.sh build.
 * Then run bash scripts/hmi.sh.
 * Open ip:8887, and the newly added HD map can be seen in the map selection drop-down box.
 * Tested by directly copying new_garage and renaming it new_garage_2; the test passed.
 * Note 1: building should be equivalent to registering /apollo/modules/map/data/new_map in the system, so that when the HMI is started, the front-end web page can locate the /apollo/modules/map/data/new_map directory and load the files in it. This is also why there is a second method for adding an HD map.

### Adding a map using an existing map directory:
 * Assuming new_map has already been added in apollo1.5, you only need to replace all the HD map files in that directory, and the new HD map can then be used without building.
