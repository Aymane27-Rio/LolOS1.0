#!/usr/bin/env python3
import argparse
import os
import re
import select
import subprocess
import sys
import time


REGULAR_COMMANDS = [
    ("help", b"Commands:"),
    ("info", b"Memory-map entries retained:"),
    ("memmap usable", b"The allocator uses ranges below 1 GiB;"),
    ("pmm", b"Physical page allocator: initialized"),
    ("pmm selftest", b"PMM self-test passed; pages returned and reallocation zeroed."),
    ("echo ci-console-ok", b"ci-console-ok"),
    ("halt", b"CPU halted. Reset the VM to continue."),
]

MODULE_COMMANDS = [
    ("info", b"Boot modules reserved: 1"),
    ("modules", b"module[0] base="),
    ("memmap usable", b"The allocator uses ranges below 1 GiB;"),
    ("pmm selftest", b"PMM self-test passed; pages returned and reallocation zeroed."),
    ("halt", b"CPU halted. Reset the VM to continue."),
]


def wait_for_output(process, output, marker, start, deadline):
    while True:
        position = output.find(marker, start)
        if position >= 0:
            return position + len(marker)
        if process.poll() is not None:
            raise RuntimeError("QEMU exited before producing {!r}".format(marker))

        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("timed out waiting for {!r}".format(marker))
        readable, _, _ = select.select([process.stdout], [], [], remaining)
        if not readable:
            raise TimeoutError("timed out waiting for {!r}".format(marker))
        chunk = os.read(process.stdout.fileno(), 4096)
        if not chunk:
            raise RuntimeError("QEMU closed its serial output")
        output.extend(chunk)


def verify_module_pages_are_reserved(output):
    text = output.decode("ascii", errors="replace")
    modules = [
        (int(base, 16), int(length, 16))
        for base, length in re.findall(
            r"module\[\d+\] base=(0x[0-9a-fA-F]+) length=(0x[0-9a-fA-F]+)",
            text,
        )
    ]
    usable = [
        (int(base, 16), int(length, 16))
        for base, length in re.findall(
            r"^\[\d+\] base=(0x[0-9a-fA-F]+) length=(0x[0-9a-fA-F]+)\r?$",
            text,
            re.MULTILINE,
        )
    ]
    if not modules or not usable:
        raise RuntimeError("module or normalized-range output is missing")

    page_mask = 4096 - 1
    for base, length in modules:
        reserved_start = base & ~page_mask
        reserved_end = (base + length + page_mask) & ~page_mask
        for usable_base, usable_length in usable:
            usable_end = usable_base + usable_length
            if reserved_start < usable_end and usable_base < reserved_end:
                raise RuntimeError("a Multiboot module page remains usable")


def stop_qemu(process):
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()


def run_smoke(iso_path, mode, timeout):
    commands = REGULAR_COMMANDS if mode == "regular" else MODULE_COMMANDS
    process = subprocess.Popen(
        [
            "qemu-system-x86_64",
            "-m",
            "128M",
            "-cdrom",
            iso_path,
            "-serial",
            "stdio",
            "-monitor",
            "none",
            "-display",
            "none",
            "-no-reboot",
            "-no-shutdown",
        ],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=0,
    )
    output = bytearray()
    deadline = time.monotonic() + timeout

    try:
        prompt_end = wait_for_output(process, output, b"lolos> ", 0, deadline)
        cursor = prompt_end
        for command, expected in commands:
            process.stdin.write(command.encode("ascii") + b"\r")
            process.stdin.flush()
            cursor = wait_for_output(process, output, expected, cursor, deadline)
            if command != "halt":
                cursor = wait_for_output(process, output, b"lolos> ", cursor, deadline)

        if mode == "regular":
            required = [
                b"LolOS x86_64: boot handoff received.",
                b"LolOS x86_64: memory map normalized.",
                b"LolOS x86_64: boot map valid.",
                b"Boot modules reserved: 0",
                b"Normalized usable page ranges:",
                b"Physical page allocator: initialized",
                b"ci-console-ok",
            ]
        else:
            required = [
                b"LolOS x86_64: boot handoff received.",
                b"LolOS x86_64: memory map normalized.",
                b"LolOS x86_64: boot map valid.",
                b"Boot modules reserved: 1",
                b"module[0] base=",
            ]
        for marker in required:
            if marker not in output:
                raise RuntimeError("expected serial output missing: {!r}".format(marker))

        if mode == "module":
            verify_module_pages_are_reserved(output)
    except Exception:
        sys.stdout.buffer.write(output)
        raise
    finally:
        stop_qemu(process)
        if process.stdin:
            process.stdin.close()
        if process.stdout:
            process.stdout.close()

    sys.stdout.buffer.write(output)
    sys.stdout.buffer.write(
        "\nQEMU serial smoke passed ({}).\n".format(mode).encode("ascii")
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("iso")
    parser.add_argument("mode", choices=("regular", "module"))
    parser.add_argument("--timeout", type=float, default=20.0)
    args = parser.parse_args()

    try:
        run_smoke(args.iso, args.mode, args.timeout)
    except Exception as error:
        print("QEMU serial smoke failed: {}".format(error), file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
