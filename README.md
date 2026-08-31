# ravine-eclips
Insights on the Canyon Eclips lighting system. Attempt to de-APP-ify the system into an ESP32 based single-purpose-device.
<img src="https://github.com/reischle/ravine-eclips/blob/main/ravine-eclips.png" width="400" alt="An Eclipse over a Canyon">

Here is what we know:
* The electronics were designed and manufactured by [IK-Elektronik](https://www.ik-elektronik.de/eclips/) in Germany. 
* About a year after first selling the "Blackbox", a "Booster" was sent out to the customers. From what we see here [Reddit-Link](https://www.reddit.com/media?url=https%3A%2F%2Fcf.preview.redd.it%2Feclips-power-supply-booster-impressions-v0-b2oyu596bdmh1.jpeg%3Fwidth%3D2596%26format%3Dpjpg%26auto%3Dwebp%26s%3D56cb17bd90e97a76a262fdf4d6dc023081d4e476) most likely an AC voltage multiplier circuit.
* From the obeservable telemetry (e.g. with nRF Connect) on an already connected smartphone running the Canyon App, we can conclude:
* The connection runs on BLE (Bluetooth Low Energy)
* It sends clear text ascii telemetry over a standard Nordic UART TX service
* It also announces a Zephyr SMP Service, which makes it most likely that the Blackbox is running on [Zephy OS] (https://docs.zephyrproject.org/latest/introduction/index.html)
