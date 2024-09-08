# MsiFanControl
Linux solution to control fans on MSI laptops.


More to read:

https://github.com/YoyPa/isw/blob/master/wiki/msi%20ec.png

(and the whole repo there)

# How to run

Compile.
Install daemon using systemd (edit supplied .service). You need to allow debug EC access. How to do you can check here:
https://github.com/YoyPa/isw

Reboot, so everything will apply. On 1st run after reboot daemon will make the backup of settings. If you'll stop daemon it will restore from backup any changes.

Run GUI application, tick checkbox "Game Mode Automatic Boost Control", minimize it to the tray (gui program must run), go play your games. It will take care of the fans.

# Dependencies

You will need installed system wide: g++ (latest), cmake, boost 1.8+, cereal (C++ headers only serialization library), libcpuid, qt5 widgets (for GUI).

# Warning
Use at your own risk, no liability for me. This program is based on researchings of the other peoples (see the links). It must be used ONLY on MSI laptops with Intel CPUs. Daemon will try to check if proper CPU and laptop are used.

Additionaly, acpi irq must be enabled, i.e. booting kernel with `acpi=off` or `acpi=noirq`, or masking/disabling separated irqs will make this program broken.

# Things to note
It appears that checking temperature (reading values over debug interface) raises electricty usage and temperature itself. So this app was redesigned in the such way, so lower your current temp is, bigger time between 2 updates will be. So on the cold cpu program will update values 1-2-3 times per minute. On the hot cpu it can be once per 2 seconds.

# TODO:
1. Automate what I wrote above by installer.
2. Implement more functions, like fan's curves in GUI.
