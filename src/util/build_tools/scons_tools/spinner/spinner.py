"""Implementation of the spinner build output tool for SCons.

Replaces verbose build command output with compact, single-line status
messages that overwrite each other in-place using ANSI escape sequences.
"""

import os
import shutil
import sys

from SCons.Action import Action
from SCons.Script import Progress

_progress_set = False


# Internal marker protocol. These strings are placed into *COMSTR
# variables and recognized by the print function to trigger spinner
# behavior.
_MARKER = '<<spinner>>'
_SOURCE = 'src'
_TARGET = 'trg'


def spinner_comstr(action_name, use_source=True):
    """Create a *COMSTR value that triggers the spinner for a builder.

    Args:
        action_name: Human-readable action name, e.g. "Compiling".
        use_source: If True, display the source filename (appropriate
            for compile-like steps). If False, display the target
            filename (appropriate for link/archive steps that have
            many sources and one target).

    Returns:
        A string suitable for assignment to a *COMSTR construction
        variable, e.g. env['CCCOMSTR'] = spinner_comstr('Compiling')
    """
    which = _SOURCE if use_source else _TARGET
    return _MARKER + which + action_name


class SpinnerPrinter:
    """Replaces default SCons build output with a compact spinner.

    When stdout is a TTY, each build step's output overwrites the
    previous line using ANSI escape sequences. When stdout is not a
    TTY (e.g. in CI), output is printed one line per step without
    ANSI codes.
    """

    def __init__(self, project_root):
        """Initialize the SpinnerPrinter.

        Args:
            project_root: Absolute path to the project root directory.
                File paths will be displayed relative to this directory.
        """
        self._project_root = project_root
        is_tty = sys.stdout.isatty()
        pretend = os.environ.get('PRETEND_ISATTY', '').lower() not in ('', '0', 'false', 'no')
        self._use_ansi = is_tty or pretend

    def __call__(self, s, target, source, env):
        """Print function compatible with SCons' PRINT_CMD_LINE_FUNC.

        This method has the signature (s, target, source, env) as
        required by SCons.
        """
        # Non-spinner strings pass through unchanged
        if not s.startswith(_MARKER):
            sys.stdout.write(s + "\n")
            return

        # Parse the marker: <<spinner>>{src|trg}{ActionName}
        rest = s[len(_MARKER):]
        which = rest[0:3]
        action_name = rest[3:]

        # Build the prefix: optional [platform] + action name
        prefix = action_name + ' '
        spinner_prefix = env.get('SPINNER_PREFIX', '')
        if spinner_prefix:
            prefix = f'[{spinner_prefix}] {prefix}'

        # Choose source or target filename to display. Compile-like
        # steps show the source; link/archive steps show the target.
        if which == _SOURCE and source:
            file_node = source[0]
        elif target:
            file_node = target[0]
        else:
            sys.stdout.write(prefix.rstrip() + "\n")
            return

        fn_abs = os.path.realpath(str(file_node))

        # Make path relative to the project root
        fn_display = os.path.relpath(fn_abs, self._project_root)

        # If the file is outside the project tree, show the absolute path
        if fn_display.startswith('..'):
            fn_display = fn_abs

        if not self._use_ansi:
            # Non-TTY: simple one-line output
            sys.stdout.write(f"{prefix}{fn_display}\n")
            return

        # TTY: overwrite the previous line
        terminal_width = shutil.get_terminal_size().columns
        spaces_avail = terminal_width - len(prefix) - 1

        # Keep truncating pathname parts from the left until the
        # filename fits on the screen
        while len(fn_display) > spaces_avail and '/' in fn_display:
            fn_display = fn_display[fn_display.index('/') + 1:]

        out = f"{prefix}{fn_display}"

        # \x1b[K  = erase to end of line
        # \n      = newline
        # \x1b[1A = cursor up one line
        # Net effect: overwrite this line with the next step's output
        sys.stdout.write(f"\x1b[K{out}\n\x1b[1A")
        sys.stdout.flush()



def _spinner_action(env, cmd, action_name, use_source=True):
    """Environment method to create an Action with spinner output.

    Use this when defining custom Builders. Returns an Action whose
    display string is the spinner output instead of the raw command.

    When SPINNER_DISABLE is truthy, returns a plain Action without
    a display string override, so SCons prints the raw command.

    Usage::

        env.Append(BUILDERS = {
            "HexFile": Builder(
                action = env.SpinnerAction(
                    "objcopy $SOURCE -O ihex $TARGET",
                    'Creating hex',
                    use_source=False,
                ),
            ),
        })

    Args:
        env: SCons construction environment (passed automatically).
        cmd: The shell command string for the action.
        action_name: Human-readable name, e.g. 'Creating hex'.
        use_source: If True, display source filename; if False, target.
    """
    if env.get('SPINNER_DISABLE'):
        return Action(cmd)
    return Action(cmd, spinner_comstr(action_name, use_source))


# Standard builders and their spinner configurations.
# Each entry is (COMSTR_variable, action_name, use_source).
_STANDARD_BUILDERS = [
    # C / C++
    ('CCCOMSTR',          'Compiling',        True),
    ('CXXCOMSTR',         'Compiling',        True),
    ('SHCCCOMSTR',        'Compiling',        True),
    ('SHCXXCOMSTR',       'Compiling',        True),

    # Assembler
    ('ASCOMSTR',          'Assembling',       True),
    ('ASPPCOMSTR',        'Assembling',       True),

    # Fortran
    ('FORTRANCOMSTR',     'Compiling',        True),
    ('SHFORTRANCOMSTR',   'Compiling',        True),
    ('FORTRANPPCOMSTR',   'Compiling',        True),
    ('SHFORTRANPPCOMSTR', 'Compiling',        True),
    ('F77COMSTR',         'Compiling',        True),
    ('SHF77COMSTR',       'Compiling',        True),
    ('F77PPCOMSTR',       'Compiling',        True),
    ('SHF77PPCOMSTR',     'Compiling',        True),
    ('F90COMSTR',         'Compiling',        True),
    ('SHF90COMSTR',       'Compiling',        True),
    ('F90PPCOMSTR',       'Compiling',        True),
    ('SHF90PPCOMSTR',     'Compiling',        True),
    ('F95COMSTR',         'Compiling',        True),
    ('SHF95COMSTR',       'Compiling',        True),
    ('F95PPCOMSTR',       'Compiling',        True),
    ('SHF95PPCOMSTR',     'Compiling',        True),
    ('F03COMSTR',         'Compiling',        True),
    ('SHF03COMSTR',       'Compiling',        True),
    ('F03PPCOMSTR',       'Compiling',        True),
    ('SHF03PPCOMSTR',     'Compiling',        True),
    ('F08COMSTR',         'Compiling',        True),
    ('SHF08COMSTR',       'Compiling',        True),
    ('F08PPCOMSTR',       'Compiling',        True),
    ('SHF08PPCOMSTR',     'Compiling',        True),

    # Lex / Yacc
    ('LEXCOMSTR',         'Generating lexer', True),
    ('YACCCOMSTR',        'Generating parser', True),

    # Java
    ('JAVACCOMSTR',       'Compiling',        True),
    ('JARCOMSTR',         'Creating JAR',     False),
    ('RMICCOMSTR',        'Generating RMI stubs', True),
    ('JAVAHCOMSTR',       'Generating JNI headers', True),

    # Linker
    ('LINKCOMSTR',        'Linking',          False),
    ('SHLINKCOMSTR',      'Linking',          False),
    ('LDMODULECOMSTR',    'Linking',          False),

    # Archive / library
    ('ARCOMSTR',          'Creating library', False),
    ('RANLIBCOMSTR',      'Indexing library', False),

    # Install
    ('INSTALLSTR',        'Installing',       True),
]


def generate(env):
    """Set up the spinner tool in the given SCons environment.

    Installs a PRINT_CMD_LINE_FUNC that replaces verbose build
    commands with compact, single-line spinner output. Configures
    all standard C/C++ builder COMSTR variables.

    If SPINNER_DISABLE is set to a truthy value in the environment,
    the spinner is not installed and SCons' default verbose output
    is preserved.

    Construction variables used:

        SPINNER_DISABLE: If truthy, skip spinner setup entirely.
            Useful for implementing a --verbose flag.

        SPINNER_PREFIX: Optional string prepended to each line in
            brackets, e.g. set to "arm" to get "[arm] Compiling ...".

        PRINT_CMD_LINE_FUNC: Set to the spinner print function.

    Environment methods added:

        SpinnerAction(cmd, action_name, use_source=True):
            Return an Action with spinner display for use in
            custom Builder() definitions.
    """
    env.AddMethod(_spinner_action, 'SpinnerAction')

    if env.get('SPINNER_DISABLE'):
        return

    global _progress_set
    if not _progress_set:
        def _progress_spinner(node, _chars=['-', '\\', '|', '/'], _count=[0]):
            _count[0] += 1
            ch = _chars[_count[0] % len(_chars)]
            sys.stdout.write(f'\x1b[K{ch}\n\x1b[1A')
            sys.stdout.flush()
        Progress(_progress_spinner, interval=5)
        _progress_set = True

    project_root = env.Dir('#').get_abspath()
    printer = SpinnerPrinter(project_root)
    env['PRINT_CMD_LINE_FUNC'] = printer

    for comstr_var, action_name, use_source in _STANDARD_BUILDERS:
        env[comstr_var] = spinner_comstr(action_name, use_source)


def exists(env):
    """This tool has no external dependencies; it is always available."""
    return True
