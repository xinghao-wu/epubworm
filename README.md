![Epubworm mascot and banner](assets/mascot_and_banner.png)
An image-capable, minimalistic TUI EPUB reader written in C++.

# Overview
https://github.com/user-attachments/assets/76c2db37-3419-4fe6-8e85-54b3e5678cf4

## Notable Features
- Inline image display with multiplexer support via Kitty Graphics Protocol
- Compliant text styling and formatting: *italic*, **bold**, centered, and right aligned text
- Wide character (e.g. emojis and Japanese text) support
- Mouse support + vim-like keybinds
- Configurable text width
- Responsive to window resize
- Persistent reading progress and last-read book
- Being really, really fast in loading and rendering content
- Uses your terminal's colorscheme

## Known Limitations
- No search (yet)
- No annotations or bookmarks
- No hyperlink support
- No whole book reading progress percentage
- No CSS resolver: a rare minority of EPUB files' text styling and formatting will not be rendered
- No MathML support: equations not stored as images will be displayed incorrectly
- No TOC fragment id support: some TOC entries will point to the same place (the start of the chapter file)
- No in-app dictionary
- No text-to-speech

Epubworm uses the relatively simple [TinyXML-2](https://github.com/leethomason/tinyxml2) for parsing, and as such is not very tolerant of malformed EPUB files. If a book crashes the application, converting the EPUB file to AZW3 and back to EPUB is an easy fix (this can be done via [Calibre](https://calibre-ebook.com/)'s [ebook-convert](https://manual.calibre-ebook.com/generated/en/ebook-convert.html) or its online hosted version at [cloudconvert](https://cloudconvert.com/)).

# Install
Epubworm supports Linux and macOS.

## Build From Source
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

# Quickstart
```bash
epubworm -h
epubworm add <file>...
epubworm open <id hash>
```

Tip: Since `epubworm add` accepts multiple arguments, file globbing can be used to quickly add all your books to Epubworm's library.
```bash
shopt -s globstar # Enables globbing on bash, not necessary if you're using zsh or fish
epubworm add books_parent_dir/**/*.epub
```

# Image Support
Epubworm uses the unicode placeholders feature of the [Kitty Graphics Protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) to display images. This requires a terminal with Kitty Graphics Protocol unicode placeholders support, and (if you use a multiplexer) a multiplexer with escape sequence passthrough support. Epubworm doesn't display images in terminals and multiplexers without these features.

## Terminals
Unicode placeholder graphics are supported by:
- [Kitty](https://sw.kovidgoyal.net/kitty/)
- [Ghostty](https://ghostty.org/)
- [iTerm2](https://iterm2.com/)
- [st-graphics](https://github.com/sergei-grechanik/st-graphics)

Unicode placeholder graphics could be implemented in the near future by:
- WezTerm: [PR](https://github.com/wezterm/wezterm/pull/7924)

## Multiplexers
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

### Unsupported
- Zellij does not support passthrough, and [doesn't support unicode placeholder graphics yet](https://github.com/zellij-org/zellij/pull/5428#issue-5031592643).
- Images don't currently work in GNU Screen and Byubu's Screen backend due to their true color limitations. A workaround is planned.

# License
MIT
