import sys
import runpy

if len(sys.argv) < 2:
    print("Usage: python -m pyusdt <script.py> [args...]", file=sys.stderr)
    sys.exit(1)

script_path = sys.argv[1]
sys.argv = sys.argv[1:]
runpy.run_path(script_path, run_name='__main__')
