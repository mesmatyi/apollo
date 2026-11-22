# Apollo Coordinate Systems

We welcome every developer to the Apollo development platform. The Apollo system involves multiple coordinate systems. In this document, we discuss the definitions of the coordinate systems used in the Apollo system.

## 1. Global geographic coordinate system

In the Apollo system, we use the global geographic coordinate system to represent the geographic location of each element in the HD Map. The global geographic coordinate system is commonly used to represent latitude, longitude and altitude. Apollo uses WGS84 (World Geodetic System 1984) as the standard coordinate system to represent the latitude and longitude of objects. By using this standard coordinate system, we can uniquely determine all points on the Earth's surface except the North Pole using 2 numbers: the x coordinate and the y coordinate, where the x coordinate represents longitude and the y coordinate represents latitude. WGS-84 is commonly used in GIS services such as map drawing, localization and navigation. The definition of the global geographic coordinate system is shown in the figure below.

![Image](images/coordination_01.png)

## 2. Local coordinate system - East-North-Up (ENU)

In the Apollo system, the local coordinate system is defined as:

z-axis - points up (in line with the gravity line)

y-axis - points north

x-axis - points east

![Image](images/coordination_02.png)

The ENU local coordinate system relies on a 3D Cartesian coordinate system established on the Earth's surface.
The Universal Transverse Mercator (UTM) uses a 2D Cartesian coordinate system to give the positions of points on the Earth's surface. This is more than just a map projection. The coordinate system divides the Earth into 60 zones, each representing a 6-degree longitude band, and uses a secant transverse Mercator projection in each zone. In the Apollo system, the UTM coordinate system is used as the local coordinate system in modules such as localization and Planning.

![Image](images/coordination_03.png)

For the use of the UTM coordinate system, we follow international standard specifications. Developers can refer to the following websites for more details:

[http://geokov.com/education/utm.aspx](http://geokov.com/education/utm.aspx)

[https://en.wikipedia.org/wiki/Universal_Transverse_Mercator_coordinate_system](https://en.wikipedia.org/wiki/Universal_Transverse_Mercator_coordinate_system)

## 3. Vehicle coordinate system - Right-Forward-Up (RFU)

The vehicle coordinate system is defined as:

z-axis - passes through the roof, perpendicular to the ground, pointing up

y-axis - points to the front of the vehicle in the direction of travel

x-axis - points to the right side of the vehicle when facing forward

The origin of the vehicle coordinate system is at the center of the vehicle's rear axle.

![Image](images/coordination_04.png)
