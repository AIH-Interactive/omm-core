"""Validate the public C ABI's six sections of the own current-schema fixture.

Usage: python -I Validate-SectionSchema.py <library> <fixture> <schema>
Requires the test-only jsonschema Python package; never opens third-party assets.
"""

import ctypes
import json
import sys
from pathlib import Path

import jsonschema


class RuntimeDesc(ctypes.Structure):
    _fields_ = [
        ("struct_size", ctypes.c_uint32),
        ("version", ctypes.c_uint32),
        ("abi_major", ctypes.c_uint32),
        ("abi_minor", ctypes.c_uint32),
        ("cpu_mode", ctypes.c_uint32),
        ("workers", ctypes.c_uint32),
        ("memory_budget", ctypes.c_uint64),
        ("file_limit", ctypes.c_uint64),
        ("expanded_limit", ctypes.c_uint64),
    ]


class Bytes(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("size", ctypes.c_uint64)]


def require(status):
    if status != 0:
        raise RuntimeError(f"C ABI call failed: {status}")


def main():
    library = ctypes.CDLL(str(Path(sys.argv[1]).resolve()))
    handle = ctypes.c_uint64
    pointer = ctypes.POINTER(handle)
    library.omm_runtime_create.argtypes = [ctypes.POINTER(RuntimeDesc), pointer, ctypes.c_void_p]
    library.omm_ysm_parse.argtypes = [handle, Bytes, ctypes.c_void_p, pointer, ctypes.c_void_p]
    library.omm_asset_read_section.argtypes = [handle, ctypes.c_uint32, Bytes, pointer, ctypes.c_void_p]
    library.omm_asset_release.argtypes = [handle, ctypes.c_void_p]
    library.omm_runtime_destroy.argtypes = [handle, ctypes.c_void_p]

    schema = json.loads(Path(sys.argv[3]).read_text(encoding="utf-8"))
    jsonschema.Draft202012Validator.check_schema(schema)
    validator = jsonschema.Draft202012Validator(schema)
    desc = RuntimeDesc(ctypes.sizeof(RuntimeDesc), 1, 0, 2, 0, 1, 0, 0, 0)
    runtime = handle()
    require(library.omm_runtime_create(ctypes.byref(desc), ctypes.byref(runtime), None))
    asset = handle()
    try:
        contents = Path(sys.argv[2]).read_bytes()
        data = ctypes.create_string_buffer(contents)
        require(library.omm_ysm_parse(runtime, Bytes(ctypes.addressof(data), len(contents)), None,
                                      ctypes.byref(asset), None))
        for section in range(1, 7):
            length = handle()
            require(library.omm_asset_read_section(asset, section, Bytes(), ctypes.byref(length), None))
            output = ctypes.create_string_buffer(length.value)
            require(library.omm_asset_read_section(asset, section,
                                                  Bytes(ctypes.addressof(output), length.value),
                                                  ctypes.byref(length), None))
            validator.validate(json.loads(output.raw.decode("utf-8")))
            print(f"section {section}: schema PASS")
    finally:
        if asset.value:
            require(library.omm_asset_release(asset, None))
        require(library.omm_runtime_destroy(runtime, None))


if __name__ == "__main__":
    main()
