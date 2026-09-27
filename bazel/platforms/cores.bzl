"""The Cortex-M cores forge builds for."""

CORES = ["cm4", "cm7"]

# target_compatible_with values that restrict a target to one core.
CM4 = [Label("//bazel/platforms:cm4_core")]
CM7 = [Label("//bazel/platforms:cm7_core")]
