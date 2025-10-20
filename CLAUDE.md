# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

pyusdt is a Python profiler that uses USDT (User-level Statically Defined Tracing) probes for low-overhead performance monitoring. It bridges Python execution with bpftrace, enabling kernel/userspace trace correlation. The project is built as a Python C extension module that integrates with Python's `sys.monitoring` API (PEP 669) and uses the libbpf/usdt header-only library for USDT probe definitions.

**Key Resources:**
- [PEP 669 - Low Impact Monitoring for CPython](https://peps.python.org/pep-0669/)
- [sys.monitoring documentation](https://docs.python.org/3/library/sys.monitoring.html)
- [libbpf/usdt](https://github.com/libbpf/usdt)

## Build System

Build the C library with embedded USDT probes:
```bash
make
```

This compiles `pyusdt.c` into `libpyusdt.so` as a Python C extension module with:
- `-fPIC`: Position-independent code for shared library
- Python include paths and link flags from `python3-config`
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
PYTHONPATH=. python tests/load.py         # Test module loading and monitoring setup
PYTHONPATH=. python tests/probe.py        # Test USDT probe functionality (requires sudo/bpftrace)
PYTHONPATH=. python tests/test_events.py  # Test all 6 probe types exist and trigger
PYTHONPATH=. python tests/test_missing.py # Test sys.monitoring.MISSING handling
```

Note: `tests/probe.py` and `tests/test_events.py` require bpftrace and sudo privileges.

## Architecture

### Core Components

1. **C Extension Module (`pyusdt.c`)**: Python C extension with USDT probes
   - `PyInit_libpyusdt()`: Module initialization function
   - Uses dynamic callback registration: monitoring callbacks are only registered when a tracer is attached
   - Background thread polls USDT semaphores every 100ms by default (configurable via `PYUSDT_CHECK_MSEC` env var)
   - When bpftrace/tracer attaches, callbacks auto-enable; when detached, callbacks auto-disable
   - Registers 6 monitoring event callbacks with `sys.monitoring` when active
   - Each callback fires corresponding USDT probe: PY_START, PY_RESUME, PY_RETURN, PY_YIELD, CALL, LINE
   - Handles `sys.monitoring.MISSING` sentinel value for missing code objects
   - Built into `libpyusdt.so`
   - Uses `usdt.h` from https://github.com/libbpf/usdt (header-only library)

2. **Python Wrapper (`pyusdt/__init__.py`)**: Minimal wrapper
   - Single line: `import libpyusdt`
   - Triggers C extension load and `sys.monitoring` registration

3. **Entry Point (`pyusdt/__main__.py`)**: Script runner
   - Usage: `python -m pyusdt <script.py> [args...]`
   - Uses `runpy.run_path()` to execute target script
   - Monitoring auto-enabled via `pyusdt/__init__.py` import

4. **USDT Header (`usdt.h`)**: Header-only library from libbpf/usdt
   - Provides `USDT()` macro for defining zero-overhead probes
   - No runtime dependencies, pure compile-time implementation

### Data Flow

When Python code executes, the following events are monitored and exposed as USDT probes:

**PY_START / PY_RESUME** (function entry / resumption):
1. `sys.monitoring` fires event with `(code, instruction_offset)`
2. C callback extracts: `co_name`, `co_filename`, `co_firstlineno`
3. Fires USDT probe with: function, file, line, offset

**PY_RETURN / PY_YIELD** (function return / generator yield):
1. `sys.monitoring` fires event with `(code, instruction_offset, retval)`
2. C callback extracts code info and converts retval to string via `PyObject_Repr()`
3. Fires USDT probe with: function, file, line, offset, retval_string

**CALL** (function calls):
1. `sys.monitoring` fires event with `(code, instruction_offset, callable, arg0)`
2. C callback extracts code info and converts callable to string
3. Fires USDT probe with: function, file, line, offset, callable_string

**LINE** (line execution):
1. `sys.monitoring` fires event with `(code, line_number)`
2. C callback extracts code info (or handles MISSING)
3. Fires USDT probe with: function, file, line

### Key Design Decisions

- **Why C extension?**: USDT probes require C macros (from libbpf/usdt); cannot be created in pure Python
- **sys.monitoring vs sys.settrace**: `sys.monitoring` is faster and designed for production use (Python 3.12+, see PEP 669)
- **Dynamic callback registration**: Combined with USDT semaphores for true zero-overhead when not tracing - callbacks are not registered until a tracer attaches
- **All logic in C**: Minimizes Python overhead in monitoring callbacks for better performance
- **MISSING handling**: Checks for `sys.monitoring.MISSING` sentinel to avoid crashes when code object unavailable
- **Error handling**: `PyObject_Repr()` failures are caught and cleared to avoid breaking traced programs
- **String representations**: Return values and callables converted to strings for USDT probe compatibility

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

Adjust polling interval (for faster/slower tracer detection):
```bash
# Check every 50ms
PYUSDT_CHECK_MSEC=50 python -m pyusdt sleep.py

# Check every 500ms
PYUSDT_CHECK_MSEC=500 python -m pyusdt sleep.py
```

## Requirements

- Python 3.12+ (requires `sys.monitoring` API)
- Linux with USDT support
- gcc compiler (no external USDT header dependencies - uses bundled `usdt.h`)
- bpftrace (for tracing)

## Important Notes

- The `libpyusdt.so` must be present in the repository root or discoverable from the module path
- USDT probes are only visible to bpftrace when the library is loaded into a running process
- The `sample.bt` script traces all 6 probe types from `libpyusdt.so`
- Tests assume you're running from the repository root with `PYTHONPATH=.`
- All monitoring events are registered simultaneously on module import (event mask: 0x3F for all 6 events)
