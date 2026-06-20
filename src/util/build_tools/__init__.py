from SCons.Script import *
import os

AddOption('-V', '--verbose',
          dest='verbose',
          action='store_true',
          default=False,
          help='Show full build commands instead of spinner output')

# Define a preprocessor constant from the command line, repeatable:
#   scons --define FOO=1 --define 'BAR=\"baz\"'
# Each becomes a -DKEY=VAL cflag appended after the SConstruct's own
# flags (so it overrides any default). Same quoting rules as an
# extra_cflags entry: a string value needs escaped quotes.
AddOption('--define',
          dest='rulos_defines',
          action='append',
          default=[],
          metavar='KEY[=VAL]',
          help='Add a -DKEY=VAL to the build (repeatable)')

# Redirect the build tree (objects, .elf/.bin, sconsign, lock) so two
# configurations can build side by side without colliding:
#   scons --build-dir=build-A --define 'VERSION=\"A\"'
#   scons --build-dir=build-B --define 'VERSION=\"B\"'
AddOption('--build-dir',
          dest='rulos_build_dir',
          default=None,
          metavar='DIR',
          help='Build tree root (default: ./build)')

try:
    from filelock import FileLock, Timeout
except ModuleNotFoundError as e:
    print("Can't find filelock module. Try apt-get install python3-filelock.")
    sys.exit(-1)

# Apply --build-dir before anything binds BUILD_ROOT. util defines it at
# import; override the module global here -- before importing the rules
# modules (which do `from .util import *`) and before the sconsign/lock
# below -- so the redirected tree is fully self-contained.
from . import util
_build_dir = GetOption('rulos_build_dir')
if _build_dir:
    util.BUILD_ROOT = os.path.abspath(_build_dir)
    os.makedirs(util.BUILD_ROOT, exist_ok=True)
    util.ensure_cachedir_tag()

# Global symbols that should be exposed
from .util import BUILD_ROOT
from .BuildTarget import RulosBuildTarget
from .ArmBuildRules import ArmPlatform, ArmStmPlatform, ArmNxpPlatform
from .AvrBuildRules import AvrPlatform
from .SimBuildRules import SimulatorPlatform
from .Esp32BuildRules import Esp32Platform
from .Canary import canary_platforms

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
