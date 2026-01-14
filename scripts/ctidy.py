#!/usr/bin/env python3
"""Run clang-tidy on all project C files."""

import shutil
import subprocess
import sys
from pathlib import Path

CHECKS = ",".join([
    "-*",
    "clang-analyzer-*",
    "bugprone-*",
    "performance-*",
    "portability-*",
    "-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling",
    "-bugprone-easily-swappable-parameters",
])

CLANG_TIDY_PATHS = [
    # macOS Homebrew (Apple Silicon)
    "/opt/homebrew/opt/llvm/bin/clang-tidy",
    # macOS Homebrew (Intel)
    "/usr/local/opt/llvm/bin/clang-tidy",
    # Linux/Windows common
    "/usr/bin/clang-tidy",
    "/usr/local/bin/clang-tidy",
    # Versioned (Linux)
    "/usr/bin/clang-tidy-18",
    "/usr/bin/clang-tidy-17",
    "/usr/bin/clang-tidy-16",
]


def find_clang_tidy() -> str | None:
    # Check PATH first
    if path := shutil.which("clang-tidy"):
        return path
    # Check common locations
    for candidate in CLANG_TIDY_PATHS:
        if Path(candidate).is_file():
            return candidate
    return None


def find_local_include() -> Path | None:
    # Try brew --prefix on macOS
    if shutil.which("brew"):
        try:
            result = subprocess.run(
                ["brew", "--prefix"],
                capture_output=True, text=True, check=True
            )
            inc = Path(result.stdout.strip()) / "include"
            if inc.is_dir():
                return inc
        except subprocess.CalledProcessError:
            pass
    # Fallback
    if Path("/usr/local/include").is_dir():
        return Path("/usr/local/include")
    return None

def main() -> int:
    ctidy = find_clang_tidy()
    if not ctidy:
        print("Error: clang-tidy not found. Install LLVM/Clang tools.", file=sys.stderr)
        print("  macOS:  brew install llvm", file=sys.stderr)
        print("  Ubuntu: apt install clang-tools", file=sys.stderr)
        print("  Fedora: dnf install clang-tools-extra", file=sys.stderr)
        return 1

    print(f"Using: {ctidy}")

    project_dir = Path(__file__).resolve().parent.parent
    src_dir = project_dir / "src"
    thirdparty_dir = project_dir / "thirdparty"
    include_dir = project_dir / "include"

    c_sources = list(src_dir.glob("**/*.c"))
    cpp_sources = list(src_dir.glob("**/*.cpp"))

    if not c_sources and not cpp_sources:
        print("No source files found.")
        return 1

    includes = [include_dir, src_dir]
    for lib in thirdparty_dir.iterdir():
        if lib.is_dir():
            includes.append(lib)
            for path in lib.rglob("include"):
                if path.is_dir():
                    includes.append(path)
            # special cases (libs without include)
            if lib.name == "lua-5.5.0":
                includes.append(lib / "src")
            if lib.name == "TinySoundFont":
                includes.append(lib)
            if lib.name == "miniaudio":
                includes.append(lib)


    local_inc = find_local_include()
    if local_inc:
        includes.append(local_inc)

    def run_clang_tidy(sources: list[Path], standard: str) -> bool:
        if not sources:
            return True

        print(f"\nRunning clang-tidy with {standard}...")

        args = [ctidy, f"--checks={CHECKS}"] + [str(s) for s in sources] + ["--"]
        for inc in includes:
            args.append(f"-I{inc}")
        args.append(f"-std={standard}")

        result = subprocess.run(args, capture_output=True, text=True)
        output = result.stdout + result.stderr

        # Filter to only warnings from this project
        warnings = [
            line for line in output.splitlines()
            if project_dir.name in line and ": note:" not in line
        ]

        if warnings:
            print("-"*80)
            print("\n".join(warnings))
        else:
            print("No warnings found")

        return "error:" not in output

    ok = run_clang_tidy(c_sources, "c11")
    if not run_clang_tidy(cpp_sources, "c++11"):
        ok = False

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
