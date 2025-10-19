# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

pyusdt is a Python profiler that uses USDT (User-level Statically Defined Tracing) probes for low-overhead performance monitoring. It bridges Python execution with bpftrace, enabling kernel/userspace trace correlation. The project has a hybrid C/Python architecture where C defines the USDT probes and Python provides the monitoring integration.

## Build System

Build the C library with embedded USDT probes:
```bash
make
```

This compiles `pyusdt.c` into `libpyusdt.so` with special linker flags:
- `-fPIC`: Position-independent code for shared library
- `-Wl,-init,pyusdt_init`: Auto-execute `pyusdt_init()` on library load
- `-shared`: Build as shared library

Clean build artifacts:
```bash
make clean
```

## Testing

Run all tests (runs every `tests/*.py` file):
```bash
make test
```

Run individual tests:
```bash
PYTHONPATH=. python tests/load.py    # Test library loading and monitoring setup
PYTHONPATH=. python tests/probe.py   # Test USDT probe functionality (requires sudo/bpftrace)
```

Note: `tests/probe.py` requires bpftrace and sudo privileges to verify probes are firing.

## Architecture

### Core Components

1. **C Library (`pyusdt.c`)**: Defines USDT probes using libbpf/usdt macros
   - `pyusdt_PY_START(code, file, line)`: Fires USDT probe with function metadata
   - `pyusdt_init()`: Constructor that runs when library is loaded
   - Built into `libpyusdt.so`
   - Uses `usdt.h` from https://github.com/libbpf/usdt (header-only library)

2. **Python Module (`pyusdt/__init__.py`)**: Monitoring orchestration
   - Loads `libpyusdt.so` using ctypes
   - Registers callback with `sys.monitoring` API (Python 3.12+)
   - Listens for `PY_START` events and calls C probe function
   - Converts Python code objects to function name, filename, line number

3. **Entry Point (`pyusdt/__main__.py`)**: Script runner
   - Usage: `python -m pyusdt <script.py> [args...]`
   - Uses `runpy.run_path()` to execute target script
   - Monitoring auto-enabled via `pyusdt/__init__.py` import

4. **USDT Header (`usdt.h`)**: Header-only library from libbpf/usdt
   - Provides `USDT()` macro for defining zero-overhead probes
   - No runtime dependencies, pure compile-time implementation

### Data Flow

When Python function executes:
1. `sys.monitoring` fires `PY_START` event
2. Python callback `_monitor_callback()` receives code object
3. Extracts metadata: `co_name`, `co_filename`, `co_firstlineno`
4. Calls C function `pyusdt_PY_START()` via ctypes
5. C function fires USDT probe using `USDT()` macro
6. bpftrace attaches to probe and reads arguments

### Key Design Decisions

- **Why C library?**: USDT probes require C macros (from libbpf/usdt); cannot be created in pure Python
- **sys.monitoring vs sys.settrace**: `sys.monitoring` is faster and designed for production use (Python 3.12+)
- **Library loading**: Searches relative to module location first, then CWD as fallback
- **Error handling**: Monitoring callback suppresses exceptions to avoid breaking traced programs

## Usage Patterns

Run script with monitoring:
```bash
python -m pyusdt sleep.py
```

Trace with bpftrace (attached to running process):
```bash
# Terminal 1
python -m pyusdt sleep.py

# Terminal 2
sudo bpftrace sample.bt -p $(pgrep -f "python -m pyusdt")
```

Trace with bpftrace (launch command):
```bash
sudo bpftrace sample.bt -c "python -m pyusdt sleep.py"
```

## Requirements

- Python 3.12+ (requires `sys.monitoring` API)
- Linux with USDT support
- gcc compiler (no external USDT header dependencies - uses bundled `usdt.h`)
- bpftrace (for tracing)

## Important Notes

- The `libpyusdt.so` must be present in the repository root or discoverable from the module path
- USDT probes are only visible to bpftrace when the library is loaded into a running process
- The `sample.bt` script expects probes from `libpyusdt.so` and traces the `pyusdt_PY_START` probe
- Tests assume you're running from the repository root with `PYTHONPATH=.`
