#!/bin/sh

set -eu

repository="xinghao-wu/epubworm"

fail() {
    printf 'epubworm installer: %s\n' "$1" >&2
    exit 1
}

command -v uname >/dev/null 2>&1 || fail 'required command not found: uname'

if [ -n "${EPUBWORM_PREFIX:-}" ]; then
    prefix=$EPUBWORM_PREFIX
elif [ -n "${HOME:-}" ]; then
    prefix="$HOME/.local"
else
    fail 'HOME is not set; set EPUBWORM_PREFIX to an absolute path'
fi

case $prefix in
    /*) ;;
    *) fail 'EPUBWORM_PREFIX must be an absolute path' ;;
esac

case $(uname -s) in
    Linux) platform=linux ;;
    Darwin) platform=macos ;;
    *) fail "unsupported operating system: $(uname -s)" ;;
esac

if [ "$platform" = linux ]; then
    for musl_loader in /lib/ld-musl-*.so.1 /usr/lib/ld-musl-*.so.1; do
        [ ! -e "$musl_loader" ] || fail "musl-based Linux distributions are not supported by the prebuilt binaries; build from source at https://github.com/$repository#build-from-source"
    done

    if command -v ldd >/dev/null 2>&1; then
        ldd_version=$(ldd --version 2>&1 || :)
        case $ldd_version in
            *musl* | *Musl* | *MUSL*) fail "musl-based Linux distributions are not supported by the prebuilt binaries; build from source at https://github.com/$repository#build-from-source" ;;
        esac
    fi
fi

for command in curl install mktemp tar; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done

case $(uname -m) in
    x86_64 | amd64) architecture=x86_64 ;;
    arm64 | aarch64) architecture=arm64 ;;
    *) fail "unsupported architecture: $(uname -m); build from source at https://github.com/$repository#build-from-source" ;;
esac

package="epubworm-$platform-$architecture"
archive="$package.tar.gz"
url="https://github.com/$repository/releases/download/rolling/$archive"
temporary_directory=$(mktemp -d 2>/dev/null || mktemp -d -t epubworm) || \
    fail 'could not create a temporary directory'
trap 'rm -rf "$temporary_directory"' 0
trap 'exit 1' HUP INT TERM

printf 'Downloading %s...\n' "$archive"
curl --fail --location --silent --show-error \
    --output "$temporary_directory/$archive" "$url"
tar -xzf "$temporary_directory/$archive" -C "$temporary_directory"

package_directory="$temporary_directory/$package"
[ -x "$package_directory/epubworm" ] || fail 'release archive does not contain the epubworm executable'

install -d \
    "$prefix/bin" \
    "$prefix/share/doc/epubworm" \
    "$prefix/share/bash-completion/completions" \
    "$prefix/share/zsh/site-functions" \
    "$prefix/share/fish/vendor_completions.d"
install -m 755 "$package_directory/epubworm" "$prefix/bin/epubworm"
install -m 644 "$package_directory/LICENSE" "$package_directory/THIRD_PARTY_LICENSES" \
    "$prefix/share/doc/epubworm"
install -m 644 "$package_directory/completions/bash/epubworm" \
    "$prefix/share/bash-completion/completions/epubworm"
install -m 644 "$package_directory/completions/zsh/_epubworm" \
    "$prefix/share/zsh/site-functions/_epubworm"
install -m 644 "$package_directory/completions/fish/epubworm.fish" \
    "$prefix/share/fish/vendor_completions.d/epubworm.fish"

printf 'Installed epubworm to %s/bin/epubworm\n' "$prefix"
if [ "$platform" = macos ]; then
    printf 'Ensure %s/bin is in PATH and %s/share/zsh/site-functions is in Zsh fpath.\n' \
        "$prefix" "$prefix"
fi
