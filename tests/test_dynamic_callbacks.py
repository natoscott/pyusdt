#!/usr/bin/env python3
"""Test that sys.monitoring callbacks are dynamically enabled/disabled based on tracer attachment."""
import sys
import subprocess
import time
import threading
import os

def test_callback_lifecycle():
    """Test the complete lifecycle: ready -> enabled -> disabled."""
    print("Testing callback enable/disable lifecycle...\n")

    # Create a test script that will run and generate output we can monitor
    test_script = '''
import pyusdt
import sys
import time

# Force stderr to be unbuffered
sys.stderr.reconfigure(line_buffering=True)

def test_function():
    """A simple function to trace."""
    return 42

print("READY", flush=True)

# Run for 15 seconds, calling functions periodically
for i in range(30):
    test_function()
    time.sleep(0.5)

print("DONE", flush=True)
'''

    # Start the test script
    proc = subprocess.Popen(
        [sys.executable, "-u", "-c", test_script],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=0  # Unbuffered
    )

    stderr_output = []
    stdout_output = []

    def read_stderr():
        """Read stderr in background to avoid blocking."""
        for line in proc.stderr:
            stderr_output.append(line.strip())

    def read_stdout():
        """Read stdout in background."""
        for line in proc.stdout:
            stdout_output.append(line.strip())

    # Start background threads to read output
    stderr_thread = threading.Thread(target=read_stderr, daemon=True)
    stdout_thread = threading.Thread(target=read_stdout, daemon=True)
    stderr_thread.start()
    stdout_thread.start()

    # Wait for the process to be ready
    timeout = time.time() + 5
    while time.time() < timeout:
        if "READY" in stdout_output:
            break
        time.sleep(0.1)
    else:
        print("✗ Process never became ready")
        proc.terminate()
        proc.wait(timeout=2)
        return False

    # Give a moment for stderr to be captured
    time.sleep(0.5)

    # Check initial state - should see "ready" message but NOT "enabled"
    print("Phase 1: Initial state (no tracer)")
    print(f"  stderr messages: {stderr_output}")

    if not any("ready" in line.lower() for line in stderr_output):
        print("✗ Did not see 'ready' message on startup")
        proc.terminate()
        proc.wait(timeout=2)
        return False

    if any("monitoring enabled" in line for line in stderr_output):
        print("✗ Monitoring was enabled too early (before tracer attached)")
        proc.terminate()
        proc.wait(timeout=2)
        return False

    print("✓ Module loaded but monitoring not yet enabled")

    # Now attach bpftrace
    print("\nPhase 2: Attaching tracer...")
    try:
        # Get the absolute path to libpyusdt.so
        lib_path = os.path.abspath("libpyusdt.so")

        # Use a simple bpftrace script that traces PY_START
        bpftrace_script = f'''
usdt:{lib_path}:pyusdt:PY_START {{
    @starts = count();
}}
'''
        bpftrace_proc = subprocess.Popen(
            ["sudo", "bpftrace", "-e", bpftrace_script, "-p", str(proc.pid)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Wait for monitoring to be enabled (poll thread checks every 100ms)
        print("  Waiting for monitoring to enable...")
        timeout = time.time() + 5
        monitoring_enabled = False
        while time.time() < timeout:
            if any("monitoring enabled" in line for line in stderr_output):
                monitoring_enabled = True
                break
            time.sleep(0.2)

        if not monitoring_enabled:
            print(f"✗ Monitoring was not enabled within 5 seconds")
            print(f"  stderr so far: {stderr_output}")
            bpftrace_proc.terminate()
            proc.terminate()
            proc.wait(timeout=2)
            return False

        print("✓ Monitoring enabled after tracer attached")

        # Let it trace for a bit
        time.sleep(1)

        # Now detach the tracer
        print("\nPhase 3: Detaching tracer...")
        bpftrace_proc.terminate()
        bpftrace_proc.wait(timeout=5)

        # Wait for monitoring to be disabled (poll thread checks every 100ms)
        print("  Waiting for monitoring to disable...")
        timeout = time.time() + 5
        monitoring_disabled = False
        while time.time() < timeout:
            if any("monitoring disabled" in line for line in stderr_output):
                monitoring_disabled = True
                break
            time.sleep(0.2)

        if not monitoring_disabled:
            print(f"✗ Monitoring was not disabled within 5 seconds")
            print(f"  stderr so far: {stderr_output}")
            proc.terminate()
            proc.wait(timeout=2)
            return False

        print("✓ Monitoring disabled after tracer detached")

        # Verify process is still running normally
        if proc.poll() is not None:
            stdout, stderr = proc.communicate()
            print(f"✗ Process crashed: {stderr}")
            return False

        print("✓ Process still running normally after disable")

        # Clean up
        proc.terminate()
        proc.wait(timeout=2)

        print("\n✓ Complete enable/disable lifecycle verified")
        return True

    except FileNotFoundError:
        print("\n⚠ bpftrace not found - skipping test")
        print("  Install bpftrace to verify dynamic callbacks")
        proc.terminate()
        proc.wait(timeout=2)
        return None
    except subprocess.TimeoutExpired:
        print("\n✗ bpftrace timed out")
        proc.terminate()
        proc.wait(timeout=2)
        if 'bpftrace_proc' in locals():
            try:
                bpftrace_proc.terminate()
            except:
                pass
        return False
    except Exception as e:
        print(f"\n✗ Error during test: {e}")
        import traceback
        traceback.print_exc()
        proc.terminate()
        proc.wait(timeout=2)
        if 'bpftrace_proc' in locals():
            try:
                bpftrace_proc.terminate()
            except:
                pass
        return False

if __name__ == "__main__":
    print("Running dynamic callback lifecycle test...\n")
    print("=" * 60)

    result = test_callback_lifecycle()

    print("=" * 60)
    if result is True:
        print("✓ Dynamic callback test passed")
        sys.exit(0)
    elif result is None:
        print("⚠ Test skipped (bpftrace not available)")
        sys.exit(0)
    else:
        print("✗ Dynamic callback test failed")
        sys.exit(1)
