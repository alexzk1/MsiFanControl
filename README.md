# MsiFanControl
Linux solution to control fans on MSI laptops.


More to read:

https://github.com/YoyPa/isw/blob/master/wiki/msi%20ec.png
(and the whole repo there)

https://github.com/BeardOverflow/msi-ec

https://github.com/dmitry-s93/MControlCenter

# Why this one ?

"Selling point" is - GUI aplication part controls fan's boost by algorithm which gives perfect gaming experience. Also quering ACPI for details like temperatures is done in smart way, so it does not distrub CPU too much. It gets down to 34 C if left alone, while other apps will keep it at 40-50 C.

Implemented self-restriction via `libseccomp` so this one is much safer to run as `root` than anything else. Restriction is enabled if `--restrict` command line parameter is given. Note, it may add some heat to your CPU.

Everthing else (present into other solutions) does not look too much important for me.

# How to run

Compile.
Install daemon using systemd (edit supplied .service). You need to allow debug EC access. How to do you can check here:
https://github.com/YoyPa/isw

Reboot, so everything will apply. On 1st run after reboot daemon will make the backup of settings. If you'll stop daemon it will restore from backup any changes.

Run GUI application, tick checkbox "Game Mode Automatic Boost Control", minimize it to the tray (gui program must run), go play your games. It will take care of the fans.

## Arch Linux
You can download all files from `linux` subfolder and run `makepkg -is`. Restriction is enabled by default in file `linux/msifancontrol.service`.

# Stress test "smart logic" of the "game mode"
Install `stress-ng` (https://www.tecmint.com/linux-cpu-load-stress-test-with-stress-ng-tool/).

Run `sudo stress-ng --cpu 8 --timeout 90`.

# Dependencies

You will need installed system wide: g++ (latest), cmake, boost 1.8+, cereal (C++ headers only serialization library), libcpuid, qt5 widgets (for GUI), libseccomp.

Optionally you may try to install https://github.com/BeardOverflow/msi-ec

msi-ec allows to control battery maximum charge (and many other things outside scope of this program).

# Warning
Use at your own risk, no liability for me. This program is based on researchings of the other peoples (see the links). It must be used ONLY on MSI laptops with Intel CPUs. Daemon will try to check if proper CPU and laptop are used.

Additionaly, acpi irq must be enabled, i.e. booting kernel with `acpi=off` or `acpi=noirq`, or masking/disabling separated irqs will make this program broken.

# Things to note
It appears that checking temperature (reading values over debug interface) raises electricty usage and temperature itself. So this app was redesigned in the such way, so lower your current temp is, bigger time between 2 updates will be. So on the cold cpu program will update values 1-2-3 times per minute. On the hot cpu it can be once per 2 seconds.

# TODO:
1. ~~Automate what I wrote above by installer.~~ Done for ArchLinux.
2. Implement more functions, like fan's curves in GUI.
