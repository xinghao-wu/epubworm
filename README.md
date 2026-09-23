<div align="center">
  <img src="assets/mascot_and_banner.png" alt="Epubworm mascot and banner">
  A fast, image-capable terminal EPUB reader written in C++
</div>

# Overview
https://github.com/user-attachments/assets/76c2db37-3419-4fe6-8e85-54b3e5678cf4

## Notable Features
- Inline image display with multiplexer support via Kitty Graphics Protocol
- Incremental chapter search
- Chapter progress percentage and estimated time left
- Wide character (e.g. emojis and Japanese text) support
- Colorful, using the terminal's existing colorscheme
- Basic HTML text styling and formatting: *italic*, **bold**, centered, and right aligned text
- Mouse support + vim-like keybinds
- Responsive to window resize
- Persistent reading progress and last-read book
- Fast content loading and rendering

## Known Limitations
- No annotations or bookmarks
- No hyperlink support
- No CSS resolver
- No MathML support
- No TOC fragment id support
- No in-app dictionary
- No text-to-speech

Epubworm uses the simple TinyXML-2 to parse EPUBs, and as such is not very tolerant of malformed EPUB files. If a book crashes the application, converting the EPUB file to AZW3 and back to EPUB is an easy fix (this can be done via Calibre's [ebook-convert](https://manual.calibre-ebook.com/generated/en/ebook-convert.html) or its online hosted version at [CloudConvert](https://cloudconvert.com/)).

# Install
Epubworm supports Linux and macOS.

## Fedora COPR
```
sudo dnf copr enable xinghao-wu/epubworm
sudo dnf install epubworm
```

## Homebrew
Tap this repository and install the latest source build:
```bash
brew tap xinghao-wu/epubworm https://github.com/xinghao-wu/epubworm
brew install --HEAD xinghao-wu/epubworm/epubworm
```

To update to the latest commit:
```bash
brew upgrade --fetch-HEAD xinghao-wu/epubworm/epubworm
```

## Nix
<details><summary>Click to expand</summary>

Run Epubworm without installing it:
```bash
nix run github:xinghao-wu/epubworm
```

Or install it into your user profile:
```bash
nix profile install github:xinghao-wu/epubworm
```

### NixOS with flakes
Add Epubworm to your existing flake inputs:
```nix
inputs.epubworm.url = "github:xinghao-wu/epubworm";
```

Then add its package to your NixOS module. Here, `epubworm` is the input bound
in your flake's `outputs` arguments:
```nix
environment.systemPackages = [
  epubworm.packages.${pkgs.system}.default
];
```

Rebuild with your usual `nixos-rebuild switch --flake` command.

### NixOS without flakes
First choose a commit to pin and calculate its source hash:
```bash
commit=<commit-hash>
nix-prefetch-url --unpack "https://github.com/xinghao-wu/epubworm/archive/$commit.tar.gz"
```

Then add the package to `/etc/nixos/configuration.nix`, replacing the commit
and hash with the values above:
```nix
{ pkgs, ... }:

let
  epubwormSource = builtins.fetchTarball {
    url = "https://github.com/xinghao-wu/epubworm/archive/<commit-hash>.tar.gz";
    sha256 = "<source-hash>";
  };
in
{
  environment.systemPackages = [
    (pkgs.callPackage "${epubwormSource}/nix/package.nix" { })
  ];
}
```

Apply the configuration with:
```bash
sudo nixos-rebuild switch
```
</details>

## CURL
Install Epubworm under `~/.local`:
```bash
curl -fsSL https://github.com/xinghao-wu/epubworm/raw/main/install.sh | sh
```

To install under a different location, set `EPUBWORM_PREFIX`:
```bash
curl -fsSL https://github.com/xinghao-wu/epubworm/raw/main/install.sh | EPUBWORM_PREFIX=/path/to/prefix sh
```

Run the command again to update.

The prebuilt binaries and accompanying files which cURL installs can also be manually downloaded from the [latest rolling release](https://github.com/xinghao-wu/epubworm/releases/latest).

## Build From Source
<details><summary>Click to expand</summary>

First, clone the repo and `cd` into it:
```bash
git clone https://github.com/xinghao-wu/epubworm ~/Downloads/epubworm
cd ~/Downloads/epubworm
```

### Linux
Linux builds require `make` and `g++` installed.

Install system-wide:
```bash
sudo make install
```

Or, install for only the current user:
```bash
make install-user
```

To update, `git pull` any changes and run the install command again.

### macOS
macOS builds require the Xcode Command Line Tools:
```bash
xcode-select --install
```

Install system-wide **(recommended)**:
```bash
sudo make install
```

Or, install for only the current user:
```bash
make install-user
```
When on macOS and installing for only the current user, ensure `~/.local/bin` is in `$PATH` and `~/.local/share/zsh/site-functions` is in Zsh's `$fpath`.

To update, `git pull` any changes and run the install command again.
</details>

# Quickstart
```
epubworm -h
epubworm add <file>...
epubworm open <id hash>
```

Tip: Since `epubworm add` accepts multiple arguments, file globbing can be used to quickly add all your books to Epubworm's library.
```bash
shopt -s globstar   # Enables globbing on bash, not necessary if you're using zsh or fish
epubworm add books_parent_dir/**/*.epub
```

# Image Support
## Terminals
Epubworm's image display requires a terminal supporting Kitty Graphics Protocol unicode placeholders. The following terminals have been tested to work:
- [Kitty](https://sw.kovidgoyal.net/kitty/)
- [Ghostty](https://ghostty.org/)
- [iTerm2](https://iterm2.com/)
- [st-graphics](https://github.com/sergei-grechanik/st-graphics)

Unsupporting terminals will work without images displayed.

## Multiplexers
<details><summary>Click to expand</summary>

Epubworm can display images in tmux and Byobu's tmux backend. GNU Screen support is coming soon.

### tmux
Make sure your `~/.tmux.conf` contains the following:
```
# Enables 256 colors and italics
set -g default-terminal "tmux-256color"

# Allows escape sequences to pass through tmux to the terminal emulator
set -g allow-passthrough on
```

### Byobu (tmux backend)
Make sure your `~/.byobu/.tmux.conf` contains the following:
```
# Enables 256 colors and italics
set -g default-terminal "tmux-256color"

# Allows escape sequences to pass through Byobu to the terminal emulator
set -g allow-passthrough on
```
</details>

# Inspiration
[epy](https://github.com/wustho/epy)

# License
MIT
