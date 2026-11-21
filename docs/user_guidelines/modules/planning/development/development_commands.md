# Development by Publishing Commands

- You can use the planning module by publishing commands to it, when your scenario requires multiple planning tasks to complete one job, or when you need to dynamically change the task state while a planning task is executing.

- You can orchestrate planning tasks yourself at the business layer according to your business needs and publish commands to the planning module.

Apollo currently supports the following commands:

<table style="undefined;table-layout: fixed; width: 507px">
<thead>
  <tr>
    <th colspan="2">Command Name</th>
    <th>Layer</th>
    <th>Function</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td colspan="2">LaneFollowCommand</td>
    <td>core</td>
    <td>Point-to-point driving along the road</td>
  </tr>
  <tr>
    <td colspan="2">ValetParkingCommand</td>
    <td>core</td>
    <td>Parking</td>
  </tr>
  <tr>
    <td rowspan="6">ActionCommand</td>
    <td>PULL_OVER</td>
    <td rowspan="6">core</td>
    <td>Emergency pull over</td>
  </tr>
  <tr>
    <td>STOP</td>
    <td>Emergency stop</td>
  </tr>
  <tr>
    <td>START</td>
    <td>Continue driving</td>
  </tr>
  <tr>
    <td>SWITCH_TO_MANUAL</td>
    <td>Switch to manual mode</td>
  </tr>
  <tr>
    <td>SWITCH_TO_AUTO</td>
    <td>Switch to auto mode</td>
  </tr>
  <tr>
    <td>VIN_REQ</td>
    <td>VIN code verification</td>
  </tr>
  <tr>
    <td colspan="2">ChassisCommand</td>
    <td>campus</td>
    <td>Chassis control command</td>
  </tr>
  <tr>
    <td colspan="2">FreeSpaceCommand</td>
    <td>campus</td>
    <td>Driving on unstructured roads</td>
  </tr>
  <tr>
    <td colspan="2">PathFollowCommand</td>
    <td>campus</td>
    <td>Record-and-replay driving</td>
  </tr>
  <tr>
    <td colspan="2">SpeedCommand</td>
    <td>campus</td>
    <td>Speed limit command</td>
  </tr>
</tbody>
</table>

Commands are sent by the user's business layer through the `server-client` method<!--, for details see the document "External Interface User Guide"-->.

You can also create new custom commands based on your scenario and send them to the planning module, but Apollo's planning module will not respond to them. You can use the next chapter on secondary development of the planning module to create new scenarios, stages, and tasks to handle your custom commands.
