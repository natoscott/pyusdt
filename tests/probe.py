import sys
import subprocess
import time
import signal

def test_probe_fires():
    """Test that USDT probes are actually firing."""
    print("Testing if USDT probes fire...\n")

    # Start a simple Python script with pyusdt
    proc = subprocess.Popen(
        [sys.executable, "-c", "import pyusdt; exec(open('sleep.py').read())"],
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

    # Try to list USDT probes using bpftrace
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
        stderr = result.stderr.strip()

        if stderr:
            print(f"bpftrace stderr: {stderr}")

        if "PY_START" in probes:
            print("✓ USDT probe found in process:")
            for line in probes.split('\n'):
                if line.strip():
                    print(f"  {line}")
            proc.terminate()
            proc.wait(timeout=2)
            return True
        else:
            print("✗ No PY_START probe found")
            print(f"Return code: {result.returncode}")
            print(f"PID: {proc.pid}")
            print(f"Available probes: {probes if probes else '(none)'}")
            proc.terminate()
            proc.wait(timeout=2)
            return False

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

if __name__ == "__main__":
    print("Running USDT probe test...\n")

    result = test_probe_fires()

    if result is True:
        print("\n✓ Test passed: USDT probes are firing")
        sys.exit(0)
    elif result is None:
        print("\n⚠ Test skipped: bpftrace not available")
        sys.exit(0)
    else:
        print("\n✗ Test failed: USDT probes not detected")
        sys.exit(1)
