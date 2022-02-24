#!/usr/bin/env python3

# This is a configuration file for users of the "st" command line search tool.
#
# Tool can be found here: https://github.com/djacobow/st
#
# If you'd like to use it, simply:
#     1. pull that repo somewhere convenient
#     2. add that repo to your path or add an alias to point write to the script st.py

import os, sys

import stlib.commands.shell
import stlib.commands.pb_decode
import stlib.shell

CONFIG = {
    'code_search_paths': {
        'rulos': {
            'lib': {
                'default': True,
                'include': ['src/lib'],
                'exclude': 'arm/common/CMSIS|periph/fat_sd|fatfs/ff.c|vendor_libraries|periph/libusb_stm32',
            },
            'app': {
                'default': True,
                'include': ['src/app'],
            },
            'util': {
                'default': True,
                'include': ['src/util'],
            },
        },
    }
}

