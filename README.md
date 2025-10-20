# 🐍 pyusdt 🐝

A Python profiler using USDT (User-level Statically Defined Tracing) probes for low-overhead performance monitoring.

## Overview

pyusdt instruments Python code execution with USDT probes that can be traced using `bpftrace`. It uses Python's `sys.monitoring` API for efficient function-level tracing.

This tool is particularly designed to enable bpftrace workflows where traces need to span both kernel and userspace, allowing you to correlate Python function calls with kernel events in a single trace session.

**Zero-overhead when not traced**: pyusdt uses dynamic callback registration combined with USDT semaphores. When no tracer is attached, monitoring callbacks are not registered with `sys.monitoring`, resulting in essentially zero performance impact on your Python code. When bpftrace or another tracer attaches, callbacks are automatically enabled within ~100ms.

## Requirements

- Python 3.12+ (for `sys.monitoring` API)
- Linux with USDT support
- bpftrace

## Building

Make sure gcc and Python development headers are installed.
Compile the USDT probe extension:

```bash
make
```

This creates `libpyusdt.so`, a Python C extension module with embedded USDT probes.

## Usage

Run any Python script with USDT monitoring:

```bash
python -m pyusdt <script.py> [args...]
```

Example:

```bash
python -m pyusdt sleep.py
```

### Configuration

Adjust the polling interval for tracer detection (default: 100ms):

```bash
# Check for attached tracers every 50ms
PYUSDT_CHECK_MSEC=50 python -m pyusdt sleep.py

# Check every 500ms (lower overhead, slower tracer detection)
PYUSDT_CHECK_MSEC=500 python -m pyusdt sleep.py
```

## Tracing with bpftrace

Use the included bpftrace script to trace function calls:

```bash
sudo bpftrace sample.bt -c "python -m pyusdt sleep.py"
```

Or attach to a running process:

```bash
# In terminal 1:
python -m pyusdt sleep.py

# In terminal 2:
sudo bpftrace sample.bt -p $(pgrep -f "python -m pyusdt")
```

### Example bpftrace Script

Here's a simple one-liner to trace Python function entries:

```bash
sudo bpftrace -e 'usdt:./libpyusdt.so:pyusdt:PY_START { printf("%s (%s:%d)\n", str(arg0), str(arg1), arg2); }' -c "python -m pyusdt sleep.py"
```

**Note:** When using `pip install pyusdt`, the library path will be different (installed in your Python site-packages). Use `-p <PID>` to attach to a running process instead of specifying the library path, and bpftrace will automatically find the loaded library.

**Available USDT probes:**
- `PY_START` - Function entry: `(function, file, line, offset)`
- `PY_RESUME` - Generator/coroutine resume: `(function, file, line, offset)`
- `PY_RETURN` - Function return: `(function, file, line, offset, retval)`
- `PY_YIELD` - Generator yield: `(function, file, line, offset, yieldval)`
- `CALL` - Function call: `(function, file, line, offset, callable)`
- `LINE` - Line execution: `(function, file, line)`

The included `sample.bt` script traces all 6 probe types with detailed output.

## Testing

Run the test suite:

```bash
make test
```

## How it Works

1. `libpyusdt.so` - Python C extension module with USDT probe definitions and `sys.monitoring` integration
2. `pyusdt/__init__.py` - Minimal Python wrapper that imports the C extension
3. `pyusdt/__main__.py` - Entry point for `python -m pyusdt` execution
4. `sample.bt` - bpftrace script to display traced function calls
5. `usdt.h` - Header-only USDT library from [libbpf/usdt](https://github.com/libbpf/usdt)

When the `pyusdt` module is imported, the C extension starts a background thread that polls USDT semaphores to detect when a tracer (like bpftrace) attaches. Only when a tracer is active does pyusdt register callbacks with Python's `sys.monitoring` API (see [PEP 669](https://peps.python.org/pep-0669/)). When the tracer detaches, callbacks are automatically unregistered.

The following monitoring events are captured and exposed as USDT probes when tracing is active:

- **PY_START** - Function entry
- **PY_RESUME** - Generator/coroutine resumption
- **PY_RETURN** - Function return with return value
- **PY_YIELD** - Generator yield with yielded value
- **CALL** - Function calls
- **LINE** - Line-by-line execution

Each event triggers its corresponding USDT probe with relevant context (function name, filename, line number, and event-specific data).

### Zero-Overhead Design

pyusdt achieves true zero overhead when not being traced through:

1. **USDT Semaphores**: The libbpf/usdt library uses kernel-managed semaphores that are incremented when tracers attach
2. **Dynamic Callback Registration**: A background thread polls semaphores every 100ms and only registers `sys.monitoring` callbacks when needed
3. **Automatic Enable/Disable**: When bpftrace attaches, monitoring activates within ~100ms; when it detaches, monitoring stops immediately

This means you can leave pyusdt enabled in production with virtually no performance impact until you need to trace.

## References

- [PEP 669 - Low Impact Monitoring for CPython](https://peps.python.org/pep-0669/)
- [sys.monitoring documentation](https://docs.python.org/3/library/sys.monitoring.html)
- [libbpf/usdt - Header-only USDT library](https://github.com/libbpf/usdt)
