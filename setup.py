from setuptools import setup, Extension
import subprocess
import sys

# Get Python config flags
def get_python_config(flag):
    """Get Python configuration flags using python3-config."""
    try:
        result = subprocess.run(
            ['python3-config', flag],
            capture_output=True,
            text=True,
            check=True
        )
        return result.stdout.strip().split()
    except subprocess.CalledProcessError:
        print(f"Error: python3-config {flag} failed", file=sys.stderr)
        sys.exit(1)

# Get include and library flags
include_flags = get_python_config('--includes')
ldflags = get_python_config('--ldflags')

# Extract include directories
include_dirs = [flag[2:] for flag in include_flags if flag.startswith('-I')]

# Create the extension module
libpyusdt_extension = Extension(
    'libpyusdt',
    sources=['pyusdt.c'],
    include_dirs=include_dirs + ['.'],  # Add current dir for usdt.h
    extra_compile_args=['-fPIC'],
    extra_link_args=['-lpthread'],
)

setup(
    ext_modules=[libpyusdt_extension],
)
