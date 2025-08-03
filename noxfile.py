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
    
    # Run pytest
    session.run(
        "pytest",
        "../../test/pytest/",
        "--cov=forge",
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
    
    # Run flake8
    session.run(
        "flake8",
        "--config=../../tox.ini",
        "."
    )
    
    # Run mypy
    session.run(
        "mypy",
        "--config-file=../../tox.ini",
        "--package=forge"
    )


@nox.session
def dev(session):
    """Create a development environment with all dependencies."""
    # This session is useful for creating a dev environment similar to tox devenv
    session.chdir("scripts/package")
    session.install("-e", ".")
    session.install("pytest", "pytest-cov", "flake8", "mypy")
    
    session.log(f"Development environment created at: {session.bin}")