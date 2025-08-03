"""Nox configuration for running tests and linting."""
import nox
import os

# Configure nox to use uv
nox.options.default_venv_backend = "uv"

# Set environment variables that need to be passed through
ENV_VARS = [
    "PROJECT_ROOT",
    "FORGE_ROOT",
    "ETL_ROOT",
    "CMAKE_PREFIX_PATH",
    "GTEST_INCLUDE_DIR",
    "PYTHONPYCACHEPREFIX",
]


@nox.session
def tests(session):
  """Run the pytest test suite."""
  # Pass through environment variables
  for var in ENV_VARS:
    if var in os.environ:
      session.env[var] = os.environ[var]

  # Change to package directory
  session.chdir("scripts/package")

  # Install test dependencies
  session.install("pytest", "pytest-cov")

  # Install the package in editable mode
  session.install("-e", ".")

  # Set coverage file location
  session.env["COVERAGE_FILE"] = "../../.pycache/.coverage"

  # Run pytest with cache in .pycache
  session.run(
      "pytest",
      "../../test/pytest/",
      "--cov=forge",
      "-o", f"cache_dir={os.environ.get('PROJECT_ROOT', '../..')}/.pycache",
      *session.posargs
  )


@nox.session
def lint(session):
  """Run flake8 and mypy linting."""
  # Pass through environment variables
  for var in ENV_VARS:
    if var in os.environ:
      session.env[var] = os.environ[var]

  # Change to package directory
  session.chdir("scripts/package")

  # Install lint dependencies
  session.install("flake8", "mypy")

  # Install the package in editable mode
  session.install("-e", ".")

  # Run flake8 with configuration
  session.run(
      "flake8",
      "--ignore=E126",
      "--max-line-length=100",
      "--indent-size=2",
      "--exclude=.venv",
      "."
  )

  # Run mypy with configuration
  session.run(
      "mypy",
      "--ignore-missing-imports",
      "--check-untyped-defs",
      "--cache-dir=../../.pycache",
      "--package=forge"
  )


@nox.session
def dev(session):
  """Create a development environment with all dependencies."""
  session.chdir("scripts/package")
  session.install("-e", ".")
  session.install("pytest", "pytest-cov", "flake8", "mypy")

  session.log(f"Development environment created at: {session.bin}")
