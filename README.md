# epubworm
An image-capable, minimalistic TUI epub reader written in C++.

## Install
epubworm supports Linux and macOS.

### Build From Source
First, clone the repo and `cd` into it.

#### Linux
Linux builds require `make` and `g++` installed.

Install system-wide:
```bash
sudo make install
```

Or, install for only the current user:
```bash
make install-user
```

#### macOS
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

## Image Support
epubworm uses the unicode placeholders feature of the [Kitty Graphics Protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) to display images. This requires a terminal with Kitty Graphics Protocol unicode placeholders support, and (if you use a multiplexer) a multiplexer with escape sequence passthrough support. epubworm displays only text in terminals and multiplexers without these features.

### Terminals
Unicode placeholder graphics are supported by:
- [Kitty](https://sw.kovidgoyal.net/kitty/)
- [Ghostty](https://ghostty.org/)
- [iTerm2](https://iterm2.com/)
- [st-graphics](https://github.com/sergei-grechanik/st-graphics)

Unicode placeholder graphics could be implemented in the near future by:
- WezTerm: [PR](https://github.com/wezterm/wezterm/pull/7924)
- xterm.js: [issue](https://github.com/xtermjs/xterm.js/issues/5711)
- Warp: [issue](https://github.com/warpdotdev/warp/issues/6210), [PR](https://github.com/warpdotdev/warp/pull/15001)

### Multiplexers
#### tmux
Make sure your `~/.tmux.conf` contains the following:
```
# Enables 256 colors and italics
set -g default-terminal "tmux-256color"

# Allows escape sequences to pass through tmux to the terminal emulator
set -g allow-passthrough on
```

#### Byobu (tmux backend)
Make sure your `~/.byobu/.tmux.conf` contains the following:
```
# Enables 256 colors and italics
set -g default-terminal "tmux-256color"

# Allows escape sequences to pass through Byobu to the terminal emulator
set -g allow-passthrough on
```

#### Unsupported
- Zellij does not support passthrough, and [doesn't support unicode placeholder graphics yet](https://github.com/zellij-org/zellij/pull/5428#issue-5031592643).
- Images don't currently work in GNU Screen and Byubu's Screen backend due to their true color limitations. A workaround is planned.

## License
MIT
