#! /usr/bin/python3

EC_IO_FILE = '/sys/kernel/debug/ec/ec0/io'
EC_FILE_1 = '/etc/modprobe.d/ec_sys.conf'
EC_FILE_2 = '/etc/modules-load.d/ec_sys.conf'

# Adding EC_SYS support to the OS 

def check():
    FLAG = 0
    try:
        FILE = open(EC_FILE_1, 'r')
    except FileNotFoundError:
        FILE = open(EC_FILE_1, 'w')
        FILE.write("options ec_sys write_support=1")
        FILE.close()
        FLAG = 1
        
    try:
        FILE = open(EC_FILE_2, 'r')
    except FileNotFoundError:
        FILE = open(EC_FILE_2, 'w')
        FILE.write("ec_sys")
        FILE.close()
        FLAG = 1

    return FLAG

# Universal EC Byte writing

def write(BYTE, VALUE):
    with open(EC_IO_FILE,'w+b') as file:
        file.seek(BYTE)
        file.write(bytes((VALUE,)))

# Universal EC Byte reading

def read(BYTE, SIZE):
    with open(EC_IO_FILE,'r+b') as file:
        file.seek(BYTE)
        if SIZE == 1:
            VALUE = int(file.read(1).hex(),16)
        elif SIZE == 2:
            VALUE = int(file.read(2).hex(),16)
    return int(VALUE)

def fan_profile(PROFILE, ONOFF, ADDRESS = 0, SPEED = 0):
    # Setting up fan profiles
    if PROFILE == 0:
        write(ONOFF[0][0], ONOFF[0][1])      # Switching Auto/Adv/Basic fan curve on
        write(ONOFF[1][0], ONOFF[1][1])      # Switching Cooler Booster off, if enabled
        for CPU_GPU_ROWS in range (0, 2):
            for FAN_SPEEDS in range (0, 7):
                write(ADDRESS[CPU_GPU_ROWS][FAN_SPEEDS], SPEED[CPU_GPU_ROWS][FAN_SPEEDS])
    elif PROFILE == 1:
        write(ONOFF[0], ONOFF[1])      # Switching Cooler Booster on
