# 🐍 pyusdt 🐝

A Python profiler using USDT (User-level Statically Defined Tracing) probes for low-overhead performance monitoring.

## Overview

pyusdt instruments Python code execution with USDT probes that can be traced using `bpftrace`. It uses Python's `sys.monitoring` API for efficient function-level tracing.

This tool is particularly designed to enable bpftrace workflows where traces need to span both kernel and userspace, allowing you to correlate Python function calls with kernel events in a single trace session.

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

When the `pyusdt` module is imported, the C extension automatically registers callbacks with Python's `sys.monitoring` API (see [PEP 669](https://peps.python.org/pep-0669/)). The following monitoring events are captured and exposed as USDT probes:

- **PY_START** - Function entry
- **PY_RESUME** - Generator/coroutine resumption
- **PY_RETURN** - Function return with return value
- **PY_YIELD** - Generator yield with yielded value
- **CALL** - Function calls
- **LINE** - Line-by-line execution

Each event triggers its corresponding USDT probe with relevant context (function name, filename, line number, and event-specific data).

## References

- [PEP 669 - Low Impact Monitoring for CPython](https://peps.python.org/pep-0669/)
- [sys.monitoring documentation](https://docs.python.org/3/library/sys.monitoring.html)
- [libbpf/usdt - Header-only USDT library](https://github.com/libbpf/usdt)
