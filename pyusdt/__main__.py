"""
Run a Python script with pyusdt monitoring enabled.

Usage: python -m pyusdt <script.py> [args...]
"""
import sys
import runpy

if len(sys.argv) < 2:
    print("Usage: python -m pyusdt <script.py> [args...]", file=sys.stderr)
    sys.exit(1)

# Get the script to run
script_path = sys.argv[1]

# Remove the module invocation from sys.argv so the target script
# sees its own arguments correctly
sys.argv = sys.argv[1:]

# Run the target script with monitoring already enabled
# (monitoring was enabled when pyusdt.__init__ was imported)
runpy.run_path(script_path, run_name='__main__')
