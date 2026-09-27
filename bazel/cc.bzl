"""cc rules for forge-owned code: cc_library and friends with forge's warnings applied."""

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load(":copts.bzl", "FORGE_COPTS", "FORGE_CXXOPTS")

def forge_cc_library(name, copts = [], cxxopts = [], **kwargs):
    cc_library(name = name, copts = FORGE_COPTS + copts, cxxopts = FORGE_CXXOPTS + cxxopts, **kwargs)

def forge_cc_binary(name, copts = [], cxxopts = [], **kwargs):
    cc_binary(name = name, copts = FORGE_COPTS + copts, cxxopts = FORGE_CXXOPTS + cxxopts, **kwargs)

def forge_cc_test(name, copts = [], cxxopts = [], **kwargs):
    cc_test(name = name, copts = FORGE_COPTS + copts, cxxopts = FORGE_CXXOPTS + cxxopts, **kwargs)
