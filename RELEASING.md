# Release Process

## Prerequisites

1. Ensure you have push access to the GitHub repository
2. Configure PyPI trusted publishing in the repository settings:
   - Go to https://github.com/natoscott/pyusdt/settings/environments
   - Create a new environment named `pypi`
   - Add PyPI as a trusted publisher at https://pypi.org/manage/account/publishing/

## Creating a Release

1. Update the version number in `pyproject.toml`

2. Commit and push the version change:
   ```bash
   git add pyproject.toml
   git commit -m "Bump version to X.Y.Z"
   git push
   ```

3. Create and push a git tag:
   ```bash
   git tag -a vX.Y.Z -m "Release vX.Y.Z"
   git push origin vX.Y.Z
   ```

4. Create a GitHub release:
   - Go to https://github.com/natoscott/pyusdt/releases/new
   - Select the tag you just created
   - Write release notes describing the changes
   - Click "Publish release"

5. The GitHub Actions workflow will automatically:
   - Build wheels for x86_64 and aarch64 architectures
   - Build wheels for Python 3.12, 3.13, and 3.14
   - Build the source distribution
   - Publish to PyPI using trusted publishing

## Manual Release (if needed)

If you need to publish manually:

```bash
# Build the package
python -m build

# Upload to PyPI (requires PyPI credentials)
python -m twine upload dist/*
```

## Testing the Package

After release, test the installation:

```bash
# Create a fresh virtual environment
python -m venv test-env
source test-env/bin/activate

# Install from PyPI
pip install pyusdt

# Test basic functionality
python -m pyusdt --help
```
