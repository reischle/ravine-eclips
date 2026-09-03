# ravine-eclips
Insights on the Canyon Eclips lighting system. Attempt to de-APP-ify the system into an ESP32 based single-purpose-device.
<img src="https://github.com/reischle/ravine-eclips/blob/main/images/ravine-eclips.png" width="400" alt="An Eclipse over a Canyon">

Here is what we know:
* The electronics were designed and manufactured by [IK-Elektronik](https://www.ik-elektronik.de/eclips/) in Germany. 
* About a year after first selling the "Blackbox", a "Booster" was sent out to the customers. From what we see here [Reddit-Link](https://www.reddit.com/media?url=https%3A%2F%2Fcf.preview.redd.it%2Feclips-power-supply-booster-impressions-v0-b2oyu596bdmh1.jpeg%3Fwidth%3D2596%26format%3Dpjpg%26auto%3Dwebp%26s%3D56cb17bd90e97a76a262fdf4d6dc023081d4e476) most likely an AC voltage multiplier circuit. This should fix the problem that a discharged system could not start from the generator and improve the overall performance at lower speeds.
* From the obeservable telemetry (e.g. with nRF Connect) on an already connected smartphone running the Canyon App, we can conclude:
  * The connection runs on BLE (Bluetooth Low Energy)
  * It sends clear text ascii telemetry over a standard Nordic UART TX service
  * It also announces a Zephyr SMP Service, which makes it most likely that the Blackbox is running on [Zephyr OS](https://docs.zephyrproject.org/latest/introduction/index.html). This is well documented, so there are probably no exotic surprises.

Here is the telemetry output of a stationary bike as produced by the ravine-eclips-telemetry-serial-v1.ino sketch:
```
UID: cps_02
Chg: 0, 0
iBat: -1 mA
VBat: 7909 mV
SOC: 74%
USB: 0
FL: 0
RL: 0
BL: 1
Speed: 0.0
FW: 0.1.2
Power State: 1
Count: 9735
TPS_COMM_Fail: 0
EEPROM_Ver: 0006
Paired_Conn: 5
VAC1: 0
VAC2: 0
```
What does stand out is, that there is a Speed output. I wonder what that is derived from. Possibly the frequency of the AC input, which immediately raises the question why there are two VAC readings and if the DC-ripple of the suspected voltage multiplier is pronounced enough to register properly to calculate the speed.
The BlackBox might sill have some tricks up it's sleeve that don't show in the App yet.

If you have a 12-LED Neopixel ring, try the ravine-eclips-telemetry-neopixel-v1.ino sketch for life charge/discharge monitoring of the battery
<img src="https://github.com/reischle/ravine-eclips/blob/main/images/tempSetup.JPG" width="400" alt="Temporary setup on my Bike">
<img src="https://github.com/reischle/ravine-eclips/blob/main/images/ring.JPG" width="400" alt="WorkbenchSetup">

Super important for successful compilation of the sketches is the right NimBLE Version: NimBLE-Arduino 2.5.1
