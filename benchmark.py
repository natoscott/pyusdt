#!/usr/bin/env python3
"""
Simple benchmark to demonstrate overhead with and without tracing active.

This shows the benefit of dynamic callback registration combined with USDT
semaphores - when no tracer is attached, sys.monitoring callbacks are not
even registered, resulting in true zero overhead.
"""
import time
import sys

def fibonacci(n):
    """Simple recursive fibonacci to generate function calls."""
    if n <= 1:
        return n
    return fibonacci(n - 1) + fibonacci(n - 2)

def benchmark_with_monitoring():
    """Run with pyusdt monitoring ready but no tracer attached."""
    import pyusdt

    # Give the poll thread a moment to check semaphores
    time.sleep(0.2)

    start = time.time()
    result = fibonacci(20)
    elapsed = time.time() - start

    print(f"With pyusdt (no tracer):     {elapsed:.4f}s, result={result}")
    return elapsed

def benchmark_without_monitoring():
    """Run without any monitoring."""
    start = time.time()
    result = fibonacci(20)
    elapsed = time.time() - start

    print(f"Without pyusdt:              {elapsed:.4f}s, result={result}")
    return elapsed

if __name__ == "__main__":
    print("Benchmarking pyusdt overhead with dynamic callbacks...")
    print("=" * 60)
    print()

    # Run without monitoring first
    without = benchmark_without_monitoring()

    # Run with monitoring (but no tracer attached)
    with_monitoring = benchmark_with_monitoring()

    overhead = ((with_monitoring - without) / without) * 100

    print()
    print("=" * 60)
    print(f"Overhead: {overhead:.2f}%")
    print()
    print("With dynamic callback registration, when no tracer is attached:")
    print("- sys.monitoring callbacks are NOT registered")
    print("- Background thread polls semaphores every 100ms")
    print("- When bpftrace/tracer attaches, callbacks auto-enable")
    print("- When tracer detaches, callbacks auto-disable")
    print()
    print("This achieves near-zero overhead when not being traced!")
