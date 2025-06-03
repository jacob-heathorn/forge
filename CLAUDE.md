# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

- **Build debug**: `cmake --workflow --preset native-debug`
- **Build release**: `cmake --workflow --preset native-release`
- **Clean**: `rip -c` (deletes .bin folder and VS Code launch.json)
- **Run tests**: `tox` (runs pytest and linting)
- **Run single test**: Use ctest from build directory: `cd .bin/native-release/ && ctest`

## Development Workflow

- **Run application**: `rip -r native-debug:hello-world` (format: `<preset>:<target>`)
- **Debug application**: `rip -d native-debug:hello-world`
- **Python linting**: Uses flake8 and mypy via tox
- **Setup environment**: Run `setup` script after clone

## Architecture Overview

This is an embedded systems project with a custom template library (FTL) and Python tooling:

### Core Components

- **FTL (Forge Template Library)**: Header-only C++17 library in `firmware/ftl/` providing embedded-specific utilities like memory pools, networking abstractions, and threading primitives
- **Native Implementation**: Platform-specific implementations in `firmware/native/` for running on host systems
- **ThreadX Support**: RTOS-specific implementations in `firmware/threadx/`
- **Python Tooling**: Build and debug utilities in `scripts/package/forge/` including VS Code integration

### Build System

- Uses CMake with presets for native-debug and native-release configurations
- Environment variables: `FORGE_ROOT`, `PROJECT_ROOT`, `ETL_ROOT` must be set
- Depends on ETL (Embedded Template Library) as external dependency
- Custom `rip.py` script wraps common development tasks

## Coding Style Guidance

This project follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with these specific modifications:

### Naming Conventions
- **Method names**: Use lowerCamelCase (e.g., `publishMessage()`, `getNodeId()`)
- **Regular/standalone functions**: Use UpperCamelCase (e.g., `WriteU16LE()`, `ReadU32BE()`)
- **Member variables**: Use snake_case (e.g., `node_id_`, `transfer_count_`)
- **Accessors/mutators**: May be named like variables
  - Example: `int count()` and `void set_count(int count)`

### Comments
- Use `//` for single-line comments instead of `/* */` style comments
- Follow Google style guide recommendations for documentation comments

### Key Libraries

- FTL provides abstractions for: buffers/memory pools, networking (IPv4/UDP), threading/mutex, singleton patterns
- Integrates with pw_unit_test framework for testing
- Uses Jinja2 templates for code generation (CMSIS-SVD register definitions)

## Testing Strategy

- C++ unit tests in `test/native/` using custom unit test framework
- Python tests in `test/pytest/` using pytest
- Both accessed via `tox` command which runs pytest and linting