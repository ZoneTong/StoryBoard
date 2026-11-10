#!/usr/bin/env bash
#
# Cross-platform portable packager for StoryBoard.
#
# Supported hosts:
#   Windows : Git Bash / MSYS2 / Cygwin, packages a win64 zip with windeployqt
#   macOS   : packages StoryBoard.app with macdeployqt
#   Linux   : packages a tar.gz with Qt libraries/plugins
#
# The script never terminates a running StoryBoard process. It refuses to
# overwrite a package directory that is currently in use.
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION=""
BUILD_DIR=""
QT_DIR_ARG=""
OUTPUT_DIR="$ROOT"
PACKAGE_NAME=""
PLATFORM=""
DO_BUILD=0
SKIP_SELF_TEST=0
EXPECTED_PASSED=""

usage() {
    cat <<'EOF'
Usage: package-portable.sh [options]

Options:
  --build                 Build the configured build directory before packaging
  --build-dir <dir>      Build directory (default: out)
  --qt-dir <dir>         Qt installation directory (default: auto-detect qmake/Qt roots)
  --version <version>    Assert version matches CMakeLists.txt (optional)
  --output-dir <dir>     Directory that receives the package/zip (default: project root)
  --package-name <name>  Override the package directory name
  --platform <name>      windows|macos|linux (default: detect from uname)
  --expected-passed <n>  Require exactly n passing checks (0 = accept current count)
  --skip-self-test       Skip --self-test (not recommended)
  -h, --help             Show this help

Examples:
  bash scripts/package-portable.sh
  bash scripts/package-portable.sh --build
  bash scripts/package-portable.sh --qt-dir /opt/Qt/6.8.0/gcc_64
EOF
}

die() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

to_posix_path() {
    local path="$1"
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -u "$path"
    else
        printf '%s\n' "$path"
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build) DO_BUILD=1; shift ;;
        --build-dir) [ "$#" -ge 2 ] || die "--build-dir needs a value"; BUILD_DIR="$2"; shift 2 ;;
        --qt-dir) [ "$#" -ge 2 ] || die "--qt-dir needs a value"; QT_DIR_ARG="$2"; shift 2 ;;
        --version) [ "$#" -ge 2 ] || die "--version needs a value"; VERSION="$2"; shift 2 ;;
        --output-dir) [ "$#" -ge 2 ] || die "--output-dir needs a value"; OUTPUT_DIR="$2"; shift 2 ;;
        --package-name) [ "$#" -ge 2 ] || die "--package-name needs a value"; PACKAGE_NAME="$2"; shift 2 ;;
        --platform) [ "$#" -ge 2 ] || die "--platform needs a value"; PLATFORM="$2"; shift 2 ;;
        --expected-passed) [ "$#" -ge 2 ] || die "--expected-passed needs a value"; EXPECTED_PASSED="$2"; shift 2 ;;
        --skip-self-test) SKIP_SELF_TEST=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1" ;;
    esac
done

[ -n "$QT_DIR_ARG" ] && QT_DIR_ARG="$(to_posix_path "$QT_DIR_ARG")"
[ -n "$BUILD_DIR" ] && BUILD_DIR="$(to_posix_path "$BUILD_DIR")"
[ -n "$OUTPUT_DIR" ] && OUTPUT_DIR="$(to_posix_path "$OUTPUT_DIR")"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) HOST_OS=windows ;;
    Darwin) HOST_OS=macos ;;
    Linux) HOST_OS=linux ;;
    *) HOST_OS=unknown ;;
esac

if [ -z "$PLATFORM" ]; then
    PLATFORM="$HOST_OS"
fi
case "$PLATFORM" in
    windows|macos|linux) ;;
    *) die "unsupported platform: $PLATFORM" ;;
esac

CMAKE_VERSION="$(sed -n 's/.*project(StoryBoard VERSION \([0-9][0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt" | head -n 1)"
[ -n "$CMAKE_VERSION" ] || die "cannot read project version from CMakeLists.txt"
if [ -n "$VERSION" ] && [ "$VERSION" != "$CMAKE_VERSION" ]; then
    die "--version $VERSION does not match CMakeLists.txt project version $CMAKE_VERSION; bump CMakeLists.txt instead"
fi
VERSION="$CMAKE_VERSION"

if [ -z "$BUILD_DIR" ]; then
    BUILD_DIR="$ROOT/out"
fi
[ -n "$OUTPUT_DIR" ] || die "output directory must not be empty"
BUILD_DIR="$(cd "$(dirname "$BUILD_DIR")" 2>/dev/null && pwd)/$(basename "$BUILD_DIR")" || die "invalid build directory"

case "$PLATFORM" in
    windows) SUFFIX="win64" ;;
    macos) SUFFIX="macos" ;;
    linux) SUFFIX="linux-$(uname -m)" ;;
esac
if [ -z "$PACKAGE_NAME" ]; then
    PACKAGE_NAME="StoryBoard-$VERSION-$SUFFIX-portable"
fi
[ -n "$PACKAGE_NAME" ] || die "package name must not be empty"
case "$PACKAGE_NAME" in
    */*|*\\*|*..*) die "invalid package name: $PACKAGE_NAME" ;;
esac
PACKAGE_DIR="$OUTPUT_DIR/$PACKAGE_NAME"
case "$PLATFORM" in
    windows|macos) ZIP_PATH="$PACKAGE_DIR.zip" ;;
    linux) ZIP_PATH="$PACKAGE_DIR.tar.gz" ;;
esac
REPORT_NAME="portable-selftest.txt"

mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR="$(cd "$OUTPUT_DIR" && pwd)"
PACKAGE_DIR="$OUTPUT_DIR/$PACKAGE_NAME"
case "$PLATFORM" in
    windows|macos) ZIP_PATH="$PACKAGE_DIR.zip" ;;
    linux) ZIP_PATH="$PACKAGE_DIR.tar.gz" ;;
esac

# Only clean a directory that is directly under the chosen output directory.
case "$PACKAGE_DIR" in
    "$OUTPUT_DIR"/*) ;;
    *) die "refusing to clean a path outside the output directory: $PACKAGE_DIR" ;;
esac

if [ -n "$EXPECTED_PASSED" ]; then
    case "$EXPECTED_PASSED" in
        ''|*[!0-9]*) die "--expected-passed must be a number" ;;
    esac
elif [ "$PLATFORM" = windows ]; then
    EXPECTED_PASSED=561
else
    EXPECTED_PASSED=0
fi

qt_tool_exists() {
    [ -x "$1" ] || [ -x "$1.exe" ]
}

find_qt_dir() {
    if [ -n "$QT_DIR_ARG" ]; then
        qt_tool_exists "$QT_DIR_ARG/bin/qmake" || \
        qt_tool_exists "$QT_DIR_ARG/bin/qmake6" || \
        qt_tool_exists "$QT_DIR_ARG/bin/qtpaths" || \
            die "invalid Qt directory: $QT_DIR_ARG"
        (cd "$QT_DIR_ARG" && pwd)
        return
    fi

    # 这台机器上可能同时装着 Qt 5 和 Qt 6，PATH 里的 qmake 未必是 Qt 6，所以逐个校验大版本。
    local tool prefix version
    for tool in qmake6 qtpaths6 qmake qtpaths; do
        command -v "$tool" >/dev/null 2>&1 || continue
        version="$("$tool" -query QT_VERSION 2>/dev/null || true)"
        [ "${version%%.*}" = 6 ] || continue
        prefix="$("$tool" -query QT_INSTALL_PREFIX 2>/dev/null || true)"
        if [ -n "$prefix" ] && [ -d "$prefix/bin" ]; then
            (cd "$prefix" && pwd)
            return
        fi
    done

    local candidates=()
    shopt -s nullglob
    case "$PLATFORM" in
        windows)
            candidates=(/d/Qt/*/mingw*_64 /c/Qt/*/mingw*_64 "$HOME"/Qt/*/mingw*_64)
            ;;
        macos)
            candidates=("$HOME"/Qt/*/macos /opt/Qt/*/macos /usr/local/Qt/*/macos)
            ;;
        linux)
            candidates=("$HOME"/Qt/*/gcc_64 /opt/Qt/*/gcc_64 /usr/lib/qt6 /usr/lib/x86_64-linux-gnu/qt6)
            ;;
    esac
    shopt -u nullglob
    [ "${#candidates[@]}" -gt 0 ] || die "Qt 6 installation not found; use --qt-dir <dir>"

    # 按版本倒序排，跳过 Qt 5 之类的大版本，取最新的 Qt 6。
    local sorted=() candidate
    while IFS= read -r candidate; do
        [ -n "$candidate" ] || continue
        qt_tool_exists "$candidate/bin/qmake" || \
        qt_tool_exists "$candidate/bin/qmake6" || \
        qt_tool_exists "$candidate/bin/qtpaths" || continue
        version="$("$candidate/bin/qmake" -query QT_VERSION 2>/dev/null || \
                  "$candidate/bin/qmake6" -query QT_VERSION 2>/dev/null || true)"
        [ "${version%%.*}" = 6 ] || continue
        sorted+=("$candidate")
    done < <(printf '%s\n' "${candidates[@]}" | sort -rV)

    [ "${#sorted[@]}" -gt 0 ] || die "Qt 6 installation not found; use --qt-dir <dir>"
    printf '%s\n' "${sorted[0]}"
}

QT_DIR="$(find_qt_dir)"
QT_BIN="$QT_DIR/bin"
[ -d "$QT_BIN" ] || die "Qt bin directory not found: $QT_BIN"

qt_query() {
    local key="$1"
    local tool path out
    for tool in qmake qmake6; do
        for path in "$QT_BIN/$tool" "$QT_BIN/$tool.exe"; do
            if [ -x "$path" ]; then
                out="$("$path" -query "$key" 2>/dev/null || true)"
                if [ -n "$out" ]; then
                    printf '%s\n' "$out"
                    return 0
                fi
            fi
        done
        path="$(command -v "$tool" 2>/dev/null || command -v "$tool.exe" 2>/dev/null || true)"
        if [ -n "$path" ]; then
            out="$("$path" -query "$key" 2>/dev/null || true)"
            if [ -n "$out" ]; then
                printf '%s\n' "$out"
                return 0
            fi
        fi
    done
    return 1
}

find_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        command -v cmake
        return
    fi
    local qt_root
    qt_root="$(cd "$QT_DIR/../.." && pwd)"
    for candidate in "$qt_root/Tools"/CMake*/bin/cmake.exe "$qt_root/Tools"/CMake*/bin/cmake; do
        [ -x "$candidate" ] && { printf '%s\n' "$candidate"; return; }
    done
    die "cmake not found; add it to PATH or use Qt's Tools/CMake"
}
CMAKE="$(find_cmake 2>/dev/null || true)"
if [ "$DO_BUILD" = 1 ] && [ -z "$CMAKE" ]; then
    die "cmake is required for --build"
fi

find_mingw() {
    local qt_root="$(cd "$QT_DIR/../.." && pwd)"
    for candidate in "$qt_root/Tools"/mingw*_64; do
        [ -x "$candidate/bin/g++.exe" ] && { printf '%s\n' "$candidate"; return; }
    done
    if command -v g++.exe >/dev/null 2>&1; then
        dirname "$(command -v g++.exe)"
        return
    fi
    die "MinGW toolchain not found under $qt_root/Tools"
}

join_jobs() {
    if command -v nproc >/dev/null 2>&1; then nproc
    elif command -v sysctl >/dev/null 2>&1; then sysctl -n hw.ncpu
    else printf '4\n'; fi
}

if [ "$DO_BUILD" = 1 ]; then
    build_arg="$BUILD_DIR"
    root_arg="$ROOT"
    qt_arg="$QT_DIR"
    mingw_arg=""
    if [ "$PLATFORM" = windows ] && command -v cygpath >/dev/null 2>&1; then
        build_arg="$(cygpath -m "$BUILD_DIR")"
        root_arg="$(cygpath -m "$ROOT")"
        qt_arg="$(cygpath -m "$QT_DIR")"
    fi
    if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
        printf 'Configuring %s ...\n' "$BUILD_DIR"
        if [ "$PLATFORM" = windows ]; then
            mingw="$(find_mingw)"
            mingw_arg="$mingw"
            if command -v cygpath >/dev/null 2>&1; then
                mingw_arg="$(cygpath -m "$mingw")"
            fi
            "$CMAKE" -S "$root_arg" -B "$build_arg" -G "MinGW Makefiles" \
                "-DCMAKE_BUILD_TYPE=Release" \
                "-DCMAKE_PREFIX_PATH=$qt_arg" \
                "-DCMAKE_C_COMPILER=$mingw_arg/bin/gcc.exe" \
                "-DCMAKE_CXX_COMPILER=$mingw_arg/bin/g++.exe" \
                "-DCMAKE_MAKE_PROGRAM=$mingw_arg/bin/mingw32-make.exe"
        else
            "$CMAKE" -S "$root_arg" -B "$build_arg" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_PREFIX_PATH=$qt_arg"
        fi
    fi
    printf 'Building %s ...\n' "$BUILD_DIR"
    "$CMAKE" --build "$build_arg" --parallel "$(join_jobs)"
fi

case "$PLATFORM" in
    windows) EXE="$BUILD_DIR/bin/StoryBoard.exe" ;;
    linux) EXE="$BUILD_DIR/bin/StoryBoard" ;;
    macos) EXE="" ;;
esac
if [ "$PLATFORM" != macos ]; then
    [ -f "$EXE" ] || die "executable not found: $EXE (run with --build or build first)"
fi

if [ "$PLATFORM" = windows ]; then
    WINDEPLOYQT="$QT_BIN/windeployqt.exe"
    [ -x "$WINDEPLOYQT" ] || WINDEPLOYQT="$(command -v windeployqt 2>/dev/null || command -v windeployqt.exe 2>/dev/null || true)"
    [ -n "$WINDEPLOYQT" ] || die "windeployqt not found"
fi

if [ "$PLATFORM" = macos ]; then
    MACDEPLOYQT="$QT_BIN/macdeployqt"
    [ -x "$MACDEPLOYQT" ] || die "macdeployqt not found: $MACDEPLOYQT"
fi

# Refuse to delete a package that is currently running.
if [ -d "$PACKAGE_DIR" ]; then
    if [ "$PLATFORM" = linux ] || [ "$PLATFORM" = macos ]; then
        if command -v pgrep >/dev/null 2>&1 && pgrep -f "$PACKAGE_DIR" >/dev/null 2>&1; then
            die "package is currently running: $PACKAGE_DIR"
        fi
    elif [ "$PLATFORM" = windows ] && command -v powershell.exe >/dev/null 2>&1; then
        win_pkg="$(command -v cygpath >/dev/null 2>&1 && cygpath -w "$PACKAGE_DIR" || printf '%s' "$PACKAGE_DIR")"
        if powershell.exe -NoProfile -Command "Get-Process StoryBoard -ErrorAction SilentlyContinue | Where-Object { \$_.Path -like '$win_pkg\\*' } | Select-Object -First 1 -ExpandProperty Id" | grep -q '[0-9]'; then
            die "package is currently running: $PACKAGE_DIR"
        fi
    fi
    rm -rf "$PACKAGE_DIR"
fi
mkdir -p "$PACKAGE_DIR"

render_readme() {
    local dest="$1"
    local template="$SCRIPT_DIR/portable-readme-$PLATFORM.txt"
    if [ -f "$template" ]; then
        awk -v version="$VERSION" '{ gsub(/@VERSION@/, version); print }' "$template" > "$dest"
    else
        printf 'StoryBoard %s portable package\n' "$VERSION" > "$dest"
    fi
}

win_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

run_self_test() {
    local package_dir="$1"
    local report="$package_dir/$REPORT_NAME"
    if [ "$PLATFORM" = windows ]; then
        (
            cd "$package_dir"
            env -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH -u STORYBOARD_LANG \
                ./StoryBoard.exe --self-test "$REPORT_NAME"
        )
    elif [ "$PLATFORM" = macos ]; then
        env -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH -u STORYBOARD_LANG \
            QT_QPA_PLATFORM=offscreen \
            "$package_dir/StoryBoard.app/Contents/MacOS/StoryBoard" --self-test "$report"
    else
        env -u QT_QPA_PLATFORM_PLUGIN_PATH -u STORYBOARD_LANG \
            LD_LIBRARY_PATH="$package_dir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
            QT_PLUGIN_PATH="$package_dir/plugins" \
            QT_QPA_PLATFORM=offscreen \
            "$package_dir/StoryBoard" --self-test "$report"
    fi

    [ -f "$report" ] || die "self-test report not found: $report"
    local summary
    summary="$(awk '/^SUMMARY/{ p=$2; f=$4 } END { gsub(",", "", p); gsub(",", "", f); print p " " f }' "$report")"
    local passed failed
    read -r passed failed <<< "$summary"
    [ -n "$passed" ] && [ -n "$failed" ] || die "self-test summary not found in $report"
    printf 'Self-test: %s passed, %s failed\n' "$passed" "$failed"
    [ "$failed" -eq 0 ] || die "self-test failed: $failed case(s)"
    if [ "$EXPECTED_PASSED" -gt 0 ] && [ "$passed" -ne "$EXPECTED_PASSED" ]; then
        die "expected $EXPECTED_PASSED passing checks, got $passed (use --expected-passed 0 to accept)"
    fi
}

bundle_linux() {
    cp "$EXE" "$PACKAGE_DIR/StoryBoard"
    local qt_libs qt_plugins
    qt_libs="$(qt_query QT_INSTALL_LIBS || true)"
    qt_plugins="$(qt_query QT_INSTALL_PLUGINS || true)"
    [ -n "$qt_libs" ] || die "cannot query QT_INSTALL_LIBS from Qt"
    [ -n "$qt_plugins" ] || die "cannot query QT_INSTALL_PLUGINS from Qt"
    shopt -s nullglob
    for lib in "$qt_libs"/libQt6*.so* "$qt_libs"/libicu*.so*; do
        [ -e "$lib" ] && cp -a "$lib" "$PACKAGE_DIR/"
    done
    shopt -u nullglob
    mkdir -p "$PACKAGE_DIR/plugins"
    for sub in platforms imageformats iconengines styles tls networkinformation generic; do
        if [ -d "$qt_plugins/$sub" ]; then
            cp -R "$qt_plugins/$sub" "$PACKAGE_DIR/plugins/"
        fi
    done
    cat > "$PACKAGE_DIR/qt.conf" <<'EOF'
[Paths]
Plugins = plugins
EOF
    if [ -d "$ROOT/examples" ]; then
        mkdir -p "$PACKAGE_DIR/examples"
        cp -R "$ROOT/examples/." "$PACKAGE_DIR/examples/"
    fi
    render_readme "$PACKAGE_DIR/README.txt"
}

bundle_macos() {
    local app_src
    app_src="$(find "$BUILD_DIR/bin" -maxdepth 1 -name '*.app' -type d | head -n 1)"
    [ -n "$app_src" ] || die "no .app bundle found under $BUILD_DIR/bin (build a macOS app first)"
    cp -R "$app_src" "$PACKAGE_DIR/StoryBoard.app"
    "$MACDEPLOYQT" "$PACKAGE_DIR/StoryBoard.app" -always-overwrite
    local app="$PACKAGE_DIR/StoryBoard.app"
    mkdir -p "$app/Contents/PlugIns/platforms" "$app/Contents/MacOS/examples"
    if [ -f "$QT_DIR/plugins/platforms/libqoffscreen.dylib" ]; then
        cp "$QT_DIR/plugins/platforms/libqoffscreen.dylib" "$app/Contents/PlugIns/platforms/"
    fi
    if [ -d "$ROOT/examples" ]; then
        cp -R "$ROOT/examples/." "$app/Contents/MacOS/examples/"
        mkdir -p "$PACKAGE_DIR/examples"
        cp -R "$ROOT/examples/." "$PACKAGE_DIR/examples/"
    fi
    render_readme "$PACKAGE_DIR/README.txt"
}

bundle_windows() {
    cp "$EXE" "$PACKAGE_DIR/StoryBoard.exe"
    "$WINDEPLOYQT" --release --no-translations --compiler-runtime \
        --dir "$(win_path "$PACKAGE_DIR")" "$(win_path "$PACKAGE_DIR/StoryBoard.exe")"
    if [ -f "$QT_DIR/plugins/platforms/qoffscreen.dll" ]; then
        mkdir -p "$PACKAGE_DIR/platforms"
        cp "$QT_DIR/plugins/platforms/qoffscreen.dll" "$PACKAGE_DIR/platforms/"
    fi
    if [ -d "$ROOT/examples" ]; then
        mkdir -p "$PACKAGE_DIR/examples"
        cp -R "$ROOT/examples/." "$PACKAGE_DIR/examples/"
    fi
    render_readme "$PACKAGE_DIR/README.txt"
}

create_zip() {
    [ -e "$ZIP_PATH" ] && rm -f "$ZIP_PATH"
    case "$PLATFORM" in
        windows)
            if command -v zip >/dev/null 2>&1; then
                (cd "$OUTPUT_DIR" && zip -qr "$(basename "$ZIP_PATH")" "$PACKAGE_NAME")
            else
                [ -n "$CMAKE" ] || die "zip is unavailable and cmake was not found"
                (cd "$OUTPUT_DIR" && "$CMAKE" -E tar cf "$(basename "$ZIP_PATH")" --format=zip "$PACKAGE_NAME")
            fi
            ;;
        macos)
            if command -v ditto >/dev/null 2>&1; then
                ditto -c -k --keepParent "$PACKAGE_DIR" "$ZIP_PATH"
            else
                (cd "$OUTPUT_DIR" && zip -qr "$(basename "$ZIP_PATH")" "$PACKAGE_NAME")
            fi
            ;;
        linux)
            tar -C "$OUTPUT_DIR" -czf "$ZIP_PATH" "$PACKAGE_NAME"
            ;;
    esac
    [ -f "$ZIP_PATH" ] || die "zip was not created: $ZIP_PATH"
}

hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    elif command -v certutil >/dev/null 2>&1; then
        certutil -hashfile "$1" SHA256 | sed -n '2p' | tr -d ' \r'
    elif command -v certutil.exe >/dev/null 2>&1; then
        certutil.exe -hashfile "$1" SHA256 | sed -n '2p' | tr -d ' \r'
    else
        printf 'unavailable\n'
    fi
}

case "$PLATFORM" in
    windows) bundle_windows ;;
    linux) bundle_linux ;;
    macos) bundle_macos ;;
esac

if [ "$SKIP_SELF_TEST" -eq 0 ]; then
    run_self_test "$PACKAGE_DIR"
    work_dir="$PACKAGE_DIR/storyboard-selftest-work"
    [ -d "$work_dir" ] && rm -rf "$work_dir"
fi

create_zip

printf '\nPortable package: %s\n' "$PACKAGE_DIR"
printf 'Archive:          %s\n' "$ZIP_PATH"
printf 'Archive size:     %s bytes\n' "$(wc -c < "$ZIP_PATH" | tr -d '[:space:]')"
printf 'Archive SHA256:   %s\n' "$(hash_file "$ZIP_PATH")"
if [ "$SKIP_SELF_TEST" -eq 0 ]; then
    printf 'Self-test report: %s\n' "$PACKAGE_DIR/$REPORT_NAME"
fi
