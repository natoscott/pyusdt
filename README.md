# pyusdt

A Python profiler using USDT (User-level Statically Defined Tracing) probes for low-overhead performance monitoring.

## Overview

pyusdt instruments Python code execution with USDT probes that can be traced using tools like `bpftrace`, `perf`, or `systemtap`. It uses Python's `sys.monitoring` API for efficient function-level tracing.

## Requirements

- Python 3.12+ (for `sys.monitoring` API)
- Linux with USDT support
- `systemtap-sdt-devel` package (provides `sys/sdt.h` header)
- bpftrace (for tracing)

### Installing Build Dependencies

On Fedora/RHEL:
```bash
sudo dnf install systemtap-sdt-devel
```

On Debian/Ubuntu:
```bash
sudo apt-get install systemtap-sdt-dev
```

## Building

Compile the USDT probe library:

```bash
make
```

This creates `libpyusdt.so` with embedded USDT probes.

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

1. `libpyusdt.so` - C library with USDT probe definitions
2. `pyusdt/__init__.py` - Loads the library and sets up `sys.monitoring` callbacks
3. `pyusdt/__main__.py` - Entry point for `python -m pyusdt` execution
4. `sample.bt` - bpftrace script to display traced function calls

The `PY_START` monitoring event fires when each Python function begins execution, triggering the USDT probe with the function name, filename, and line number.
