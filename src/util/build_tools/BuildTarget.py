import os
from .util import *
from .BaseRules import Platform

class RulosBuildTarget:
    def __init__(self, name, sources, platforms, peripherals = [],
                 extra_cflags = [], extra_converters = []):
        self.name = name
        self.sources = [os.path.relpath(s, PROJECT_ROOT) for s in sources]
        self.platforms = platforms
        assert(type(self.platforms)==type([]))
        for p in self.platforms:
            assert(isinstance(p, Platform))
        self.peripherals = parse_peripherals(peripherals)
        self.extra_cflags = extra_cflags
        self.extra_converters = extra_converters

    def build(self):
        for platform in self.platforms:
            platform.build(self)

    def cflags(self):
        return self.extra_cflags
