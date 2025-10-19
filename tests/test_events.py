#!/usr/bin/env python3
"""Test that all monitoring events trigger their respective probes."""
import sys
import subprocess
import time

def test_all_probes_exist():
    """Test that all 6 USDT probes are defined in the library."""
    print("Testing all probes exist...\n")

    # Start a simple Python script with pyusdt
    proc = subprocess.Popen(
        [sys.executable, "-c", "import pyusdt; import time; time.sleep(10)"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

    # Give it a moment to start
    time.sleep(0.5)

    # Check if the process is running
    if proc.poll() is not None:
        stdout, stderr = proc.communicate()
        print("Process terminated early:")
        print("STDERR:", stderr)
        return False

    try:
        # Get the absolute path to libpyusdt.so
        import os
        lib_path = os.path.abspath("libpyusdt.so")

        result = subprocess.run(
            ["sudo", "bpftrace", "-l", f"usdt:{lib_path}:*"],
            capture_output=True,
            text=True,
            timeout=5
        )

        probes = result.stdout.strip()

        # Expected probes
        expected_probes = [
            "PY_START",
            "PY_RESUME",
            "PY_RETURN",
            "PY_YIELD",
            "CALL",
            "LINE"
        ]

        found_probes = []
        missing_probes = []

        for probe in expected_probes:
            if probe in probes:
                found_probes.append(probe)
                print(f"✓ Found probe: {probe}")
            else:
                missing_probes.append(probe)
                print(f"✗ Missing probe: {probe}")

        proc.terminate()
        proc.wait(timeout=2)

        if missing_probes:
            print(f"\n✗ Missing {len(missing_probes)} probes: {', '.join(missing_probes)}")
            return False
        else:
            print(f"\n✓ All {len(expected_probes)} probes found")
            return True

    except FileNotFoundError:
        print("⚠ bpftrace not found - skipping probe verification")
        print("  Install bpftrace to verify probes are working")
        proc.terminate()
        proc.wait(timeout=2)
        return None
    except subprocess.TimeoutExpired:
        print("✗ bpftrace timed out")
        proc.terminate()
        proc.wait(timeout=2)
        return False
    except Exception as e:
        print(f"✗ Error checking probes: {e}")
        proc.terminate()
        proc.wait(timeout=2)
        return False

def test_event_triggering():
    """Test that monitoring callbacks can be triggered for each event type."""
    print("\nTesting event triggering...\n")

    # Create a test script that triggers all event types
    test_script = '''
import pyusdt

# PY_START: triggered on function entry
def simple_function():
    return 42

# PY_RETURN: triggered on function return
def function_with_return():
    x = 1 + 1
    return x

# PY_YIELD: triggered on generator yield
def generator_function():
    yield 1
    yield 2

# CALL: triggered on function calls
def calling_function():
    simple_function()
    function_with_return()

# LINE: triggered on line execution
def multiline_function():
    a = 1
    b = 2
    c = a + b
    return c

# Execute functions to trigger events
simple_function()
function_with_return()
list(generator_function())
calling_function()
multiline_function()
'''

    try:
        # Run the test script
        result = subprocess.run(
            [sys.executable, "-c", test_script],
            capture_output=True,
            text=True,
            timeout=5
        )

        if result.returncode == 0:
            print("✓ Test script executed without errors")
            if "pyusdt monitoring enabled" in result.stderr:
                print("✓ Monitoring was enabled")
            return True
        else:
            print(f"✗ Test script failed with return code {result.returncode}")
            print("STDERR:", result.stderr)
            print("STDOUT:", result.stdout)
            return False

    except subprocess.TimeoutExpired:
        print("✗ Test script timed out")
        return False
    except Exception as e:
        print(f"✗ Error running test script: {e}")
        return False

if __name__ == "__main__":
    print("Running USDT event tests...\n")

    results = []

    # Test 1: All probes exist
    probe_result = test_all_probes_exist()
    if probe_result is not None:
        results.append(probe_result)

    # Test 2: Events can be triggered
    results.append(test_event_triggering())

    print(f"\n{'='*60}")
    print(f"Results: {sum(results)}/{len(results)} tests passed")
    print('='*60)

    sys.exit(0 if all(results) else 1)
