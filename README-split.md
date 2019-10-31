# Split PX4 for Secure Flight Control
This enabled running PX4 split into to parts: a _secure_ part that runs the `sensors` and `pwm_out_sim` modules, and the _main_ part running the rest of the modules. For now, the secure part is just a separate process, but it could eventually be a hyperapp.

Even though these instructions show how to run PX4 and the jMAVSim simulator in a single computer, the split is suitable for using it in HIL-simulation (e.g., PX4 running on a Raspberry Pi and connected to a simulator). Furthermore, it could be adapted for running in a drone, but that would require reconfiguration to run the actual drivers that feed the `sensors` module.

Note that no source code changes have been made to PX4 for this. The changes consist of new builds, launch scripts and configuration files.

The communication between the two parts is done via microRTPS, which allows exchanging uORB messages between the two processess. (see [mircoRTPS](https://dev.px4.io/v1.9.0/en/middleware/micrortps.html), but keep in mind that ROS is not used at all here)

In this case microRTPS is configured to work over UDP, however, it would be a matter of configuration to use a serial port instead. Furthermore, it would be relatively simple to use a different communication mechanism between the two parts, which may be needed if the secure part runs as a hyperapp.

In the rest of this guide, all paths are relative to Firmware (or the root of the git repo).

The first step is to build the binaries for each part.
```
make px4_sitl_smain
make px4_sitl_ssec
```

To run it, first execute the following command to start the secure part and the simulator.
```
Tools/run_split_sec.sh
```

Then, in a separate terminal execute the following command to launch the main part.
```
Tools/run_split_main.sh
```

Once the GPS fusion message is seen in the main part, it should be possible to execute `commander takeoff` in the PX4 prompt of the main part.

## microRTPS
Two different binaries are needed because the `micrortps_client` in each sends and receives a complementary set of topics. For example, the main part sends the actuator_armed uORB topic, and the secure part receives it. The topics sent and received by each part are specified in the following files:
```
msg/tools/uorb_rtps_message_ids_main.yaml
msg/tools/uorb_rtps_message_ids_sec.yaml
```

If changes have to be made (e.g., to exchange another uORB, or a new uORB message) take into account the following:
- If a multi instance uORB topic has to be sent (or received), both the sigle instance and the multi instance have to sent (or received).
- The files are complementary, so it's best to change `uorb_rtps_message_ids_main.yaml` and then generate the other file with:
    ```
    cd msg/tools
    ./complement.sh uorb_rtps_message_ids_main.yaml > uorb_rtps_message_ids_sec.yaml
    ```
- To force make to rebuild the micrortps_client do:
    ```
    touch src/modules/micrortps_bridge/CMakeLists.txt
    ```

## Running on Docker
It is possible to run this in Docker. First execute this in one terminal:
```
Tools/docker_run.sh 'bash'
```
In another terminal, execute this:
```
Tools/docker_attach.sh
```

## Doing actual HIL
The mavlink configuration in `ROMFS/px4fmu_common/init.d-posix/rcS-hil-sec` would need to be changed to use the serial port that connects the Raspberry Pi to the PC running jMAVSim. jMAVSim would have to be started independently configure to use the serial port.

## Things not tested yet
The following things haven't been tested:
- Connecting with QGroundControl
- Using input from the radio controller
