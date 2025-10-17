import sys
import importlib

def test_pyusdt_import():
    """Test that the pyusdt module can be imported."""
    try:
        import pyusdt
        print("✓ pyusdt module imported successfully")
        return True
    except ImportError as e:
        print(f"✗ Failed to import pyusdt: {e}")
        return False

def test_monitoring_enabled():
    """Test that sys.monitoring is active."""
    import pyusdt
    mon = sys.monitoring

    # Check if PROFILER_ID is being used
    tool_name = mon.get_tool(mon.PROFILER_ID)
    if tool_name == "pyusdt-profiling":
        print(f"✓ Monitoring tool registered: {tool_name}")
        return True
    else:
        print(f"✗ Expected 'pyusdt-profiling', got: {tool_name}")
        return False

def test_library_loaded():
    """Test that libpyusdt.so was loaded successfully."""
    import pyusdt
    if pyusdt._library_loaded:
        print("✓ libpyusdt.so loaded successfully")
        return True
    else:
        print("✗ libpyusdt.so failed to load")
        return False

if __name__ == "__main__":
    print("Running pyusdt tests...\n")

    results = []
    results.append(test_pyusdt_import())
    results.append(test_library_loaded())
    results.append(test_monitoring_enabled())

    print(f"\nResults: {sum(results)}/{len(results)} tests passed")
    sys.exit(0 if all(results) else 1)
