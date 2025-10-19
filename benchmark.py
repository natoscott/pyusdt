#!/usr/bin/env python3
"""
Simple benchmark to demonstrate overhead with and without tracing active.

This shows the benefit of USDT semaphores - when no tracer is attached,
the USDT_IS_ACTIVE() check returns false immediately and we skip all
the expensive work.
"""
import time
import sys

def fibonacci(n):
    """Simple recursive fibonacci to generate function calls."""
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

def benchmark_with_monitoring():
    """Run with pyusdt monitoring enabled but no tracer attached."""
    import pyusdt

    start = time.time()
    result = fibonacci(20)
    elapsed = time.time() - start

    print(f"With monitoring (no tracer): {elapsed:.4f}s, result={result}")
    return elapsed

def benchmark_without_monitoring():
    """Run without any monitoring."""
    start = time.time()
    result = fibonacci(20)
    elapsed = time.time() - start

    print(f"Without monitoring:          {elapsed:.4f}s, result={result}")
    return elapsed

if __name__ == "__main__":
    print("Benchmarking USDT overhead with semaphores...")
    print("=" * 60)
    print()

    # Run without monitoring first
    without = benchmark_without_monitoring()

    # Run with monitoring (but no tracer attached)
    with_monitoring = benchmark_with_monitoring()

    overhead = ((with_monitoring - without) / without) * 100

    print()
    print("=" * 60)
    print(f"Overhead with semaphores: {overhead:.2f}%")
    print()
    print("With USDT_IS_ACTIVE() checks, the overhead when not being")
    print("traced should be minimal (just checking a volatile short).")
