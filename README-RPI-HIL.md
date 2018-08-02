# HIL Simulation with PX4 + RPI + Navio2

## Hardware Setup
- Connect RC receiver to the Navio2 as described in [these instructions](https://docs.emlid.com/navio2/ardupilot/hardware-setup/#rc-input)
- Connect Navio2 to machine that will run the simulator
  - Use one [6-pin cable](https://store.emlid.com/product/wire-pack-for-navio2/) connected to the UART port on the Navio2
  - Use a USB to TTL Serial cable (e.g., [this one](https://www.adafruit.com/product/954)), making the following connections to the Navio2 cable using [male jumper wires](https://www.adafruit.com/product/1956). **Do not connect the red wire**
  
  | Navio2 UART | USB TTL Serial |
  |-------------|----------------|
  | GND         | GND            |
  | RX          | TX             |
  | TX          | RX             |

- Connect USB cable to the machine that will run the simulator

## PX4 Software Setup
- Configure RPI with Navio2 as described [here](https://docs.emlid.com/navio2/common/ardupilot/configuring-raspberry-pi/) or more specifically for PX4 [here](https://docs.px4.io/en/flight_controller/raspberry_pi_navio2.html)
  - If following the former, follow the [instructiona to disable the Navio RGB overlay](https://docs.px4.io/en/flight_controller/raspberry_pi_navio2.html#disable-navio-rgb-overlay)
  - Note that `wpa_suplicant.conf` is not in `/boot` but in `/etc/wpa_supplicant`
  - To connect to the CMU wifi without password, use the following in `wpa_suplicant.conf`
  ```
  network={
        auth_alg=OPEN
        key_mgmt=NONE
        mode=0
        ssid="CMU"
}
```
- We're going to build PX4 natively on the RPI, so follow these instructions to install all the dependencies
- Disable modem manager
```
sudo apt-get remove modemmanager -y
```
- Install dependencies

```
sudo apt-get update
sudo apt-get install git zip cmake build-essential genromfs ninja-build -y
sudo apt-get install python-empy python-toml python-numpy python-dev python-pip -y
sudo -H pip install --upgrade pip
sudo -H pip install pandas jinja2 pyserial

```
- Get custom PX4 code
```
cd ~
mkdir px4
cd px4
git clone https://github.com/cps-sei/Firmware
```
- Build the code
```
cd Firmware
git checkout rpihil
make posix_rpi_native
```

## Running the Simulation
- Build jMAVSim in the machine that will run the simulator
```
sudo apt install ant -y
git clone https://github.com/PX4/jMAVSim.git
cd jMAVSim/
git submodule update --init --recursive
cd ..
wget https://raw.githubusercontent.com/PX4/Firmware/master/Tools/jmavsim_run.sh
chmod u+x jmavsim_run.sh
```
- Start jMAVSim
```
./jmavsim_run.sh -d /dev/ttyUSB0
```
- Start PX4 on the RPI
```
cd ~/px4/Firmware
sudo ./build_posix_rpi_native/src/firmware/posix/px4 posix-configs/rpi/px4-hil.config
```
Note that the there's a config file with a very similar name (with underscore). Use the one with a hyphen.
- Before the first flight it may be necessary to calibrate the radio control. Follow [these instructions](https://docs.px4.io/en/config/) to calibrate the radio control and configure flight modes. Keep in mind that *Position* is the easiest flight mode.
- Fly
  - Push the two sticks in the RC towards the lower inner position to arm the drone.
  - Push the left stick forward to take off.
  
