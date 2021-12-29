from SCons.Script import *
from filelock import FileLock, Timeout
import os

# Global symbols that should be exposed
from .util import BUILD_ROOT
from .BuildTarget import RulosBuildTarget
from .ArmBuildRules import ArmPlatform, ArmStmPlatform
from .AvrBuildRules import AvrPlatform
from .SimBuildRules import SimulatorPlatform

if not os.path.exists(BUILD_ROOT):
    os.makedirs(BUILD_ROOT)
sconsign = f".sconsign.{SCons.__version__}.dblite"
lock = FileLock(os.path.join(BUILD_ROOT, sconsign+".lock"))
try:
    lock.acquire(timeout=0.1)
except:
    print("Scons already running against this sconsign file; concurrency will confuse it.")
    sys.exit(-1)
SConsignFile(os.path.join(BUILD_ROOT, sconsign))

