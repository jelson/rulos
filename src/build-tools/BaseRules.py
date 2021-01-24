import os

PROJECT_ROOT = os.path.relpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC_ROOT = os.path.join(PROJECT_ROOT, "src")
BUILD_ROOT = os.path.join(PROJECT_ROOT, "build")

def cwd_to_project_root(inp):
    if type(inp)==type([]):
        return [os.path.relpath(s, PROJECT_ROOT) for s in inp]
    else:
        assert(type(inp)==type(""))
        return os.path.relpath(inp, PROJECT_ROOT)

