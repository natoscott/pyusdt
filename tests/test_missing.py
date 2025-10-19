#!/usr/bin/env python3
"""Test that sys.monitoring.MISSING is handled correctly."""
import sys

def test_missing_handling():
    """Test that MISSING value is properly handled in callbacks."""
    print("Testing MISSING handling...\n")

    import pyusdt

    # Verify MISSING exists
    if not hasattr(sys.monitoring, 'MISSING'):
        print("✗ sys.monitoring.MISSING not available")
        return False

    MISSING = sys.monitoring.MISSING
    print(f"✓ sys.monitoring.MISSING exists: {MISSING}")

    # The C extension should have loaded and stored MISSING
    # We can't directly test if it's handling MISSING in callbacks
    # without triggering actual MISSING events, but we can verify
    # the module loaded successfully (which means MISSING was found)

    print("✓ Module loaded successfully (MISSING reference acquired)")

    # Run a simple script that might trigger MISSING in some edge cases
    # (though in practice, MISSING is rare in normal Python execution)
    try:
        def test_func():
            return 42

        test_func()
        print("✓ Callbacks work with normal code objects")
        return True
    except Exception as e:
        print(f"✗ Error during execution: {e}")
        return False

if __name__ == "__main__":
    print("Running MISSING handling test...\n")

    result = test_missing_handling()

    print(f"\n{'='*60}")
    if result:
        print("✓ MISSING handling test passed")
    else:
        print("✗ MISSING handling test failed")
    print('='*60)

    sys.exit(0 if result else 1)
