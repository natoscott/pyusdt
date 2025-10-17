"""
pyusdt - Python USDT (User-level Statically Defined Tracing) profiler

This module sets up monitoring hooks to emit USDT probes for Python code execution.
"""
import sys
import ctypes
import os

# Find and load the libpyusdt.so library
_lib_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'libpyusdt.so')
if not os.path.exists(_lib_path):
    # Try current directory as fallback
    _lib_path = os.path.join(os.getcwd(), 'libpyusdt.so')

try:
    _pyusdt_lib = ctypes.CDLL(_lib_path)

    # Define the function signature for pyusdt_PY_START
    # void pyusdt_PY_START(const char *code, const char *file, long long line)
    _pyusdt_lib.pyusdt_PY_START.argtypes = [
        ctypes.c_char_p,  # code
        ctypes.c_char_p,  # file
        ctypes.c_longlong  # line
    ]
    _pyusdt_lib.pyusdt_PY_START.restype = None

    _library_loaded = True
except (OSError, AttributeError) as e:
    print(f"Warning: Could not load pyusdt library: {e}", file=sys.stderr)
    _library_loaded = False


def _monitor_callback(code, _):
    """
    Monitoring callback that gets called for PY_START events.

    Args:
        code: Code object being executed
        _: Instruction offset (unused)
    """
    if not _library_loaded:
        return

    # Get information from the code object
    function_name = code.co_name
    filename = code.co_filename
    line_number = code.co_firstlineno

    # Fire the USDT probe
    try:
        _pyusdt_lib.pyusdt_PY_START(
            function_name.encode('utf-8'),
            filename.encode('utf-8'),
            ctypes.c_longlong(line_number)
        )
    except Exception as e:
        # Don't let monitoring errors break the program
        print(f"pyusdt monitor error: {e}", file=sys.stderr)


# Install the monitoring callback when the module is imported
if _library_loaded:
    mon = sys.monitoring
    mon.use_tool_id(mon.PROFILER_ID, "pyusdt-profiling")
    mon.set_events(mon.PROFILER_ID, mon.events.PY_START)
    mon.register_callback(mon.PROFILER_ID, mon.events.PY_START, _monitor_callback)
    print("pyusdt monitoring enabled", file=sys.stderr)
