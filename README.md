# MsiFanControl
Linux solution to control fans on MSI laptops.


More to read:

https://github.com/YoyPa/isw/blob/master/wiki/msi%20ec.png

(and the whole repo there)

# How to run

Compile.
Install daemon using systemd (edit supplied .service). You need to allow debug EC access. How to do you can check here:
https://github.com/YoyPa/isw

Reboot, so everything will apply. On 1st run after reboot daemon will make backup of settings. If you'll stop daemon it will restore from backup any changes.

Run GUI application, tick checkbox "automatic boost control", minimize it to tray (program must run), go play your games. It will take care of the fans.

# Dependencies

You will need installed system wide: g++ (latest), cmake, boost 1.8+, cereal (C++ headers only serialization library), libcpuid, qt5 widgets (for GUI).

# Warning
Use at your own risk, no liability for me. This program is based on researchings of the other peoples (see the links). It must be used ONLY on MSI laptops with Intel CPUs.

# TODO:
1. Automate what I wrote above by installer.
2. Implement more functions, like fan's curves in GUI.
