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
sudo make install CXX=clang++
```

Or, install for only the current user:
```bash
make install-user CXX=clang++
```
When on macOS and installing for only the current user, ensure `~/.local/bin` is in `$PATH` and `~/.local/share/zsh/site-functions` is in Zsh's `$fpath`.

## Image Support
epubworm uses the unicode placeholders feature of the [Kitty Graphics Protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) to display images. Any combination of supported terminal + supported multiplexer (or no multiplexer at all) works.

### Terminals
Unicode placeholder graphics are currently supported by:
- [Kitty](https://sw.kovidgoyal.net/kitty/)
- [Ghostty](https://ghostty.org/)
- [iTerm2](https://iterm2.com/)
- [st-graphics](https://github.com/sergei-grechanik/st-graphics)

Unicode placeholder graphics could be implemented in the near future by:
- WezTerm: [PR](https://github.com/wezterm/wezterm/pull/7924)
- xterm.js: [issue](https://github.com/xtermjs/xterm.js/issues/5711)
- Warp: [issue](https://github.com/warpdotdev/warp/issues/6210), [PR](https://github.com/warpdotdev/warp/pull/15001)

epubworm still works in terminals without kitty graphics protocol unicode placeholders support, just without images displayed.

### Multiplexers
epubworm can display images inside terminal multiplexers who have escape sequence passthrough support. Notably, Zellij does not currently support passthrough. epubworm displays no images in unsupported multiplexers like Zellij.

#### tmux
Make sure your `~/.tmux.conf` contains the following:
```
# Enables 256 colors and italics
set -g default-terminal "tmux-256color"

# Allows escape sequences to pass through tmux to the terminal emulator
set -g allow-passthrough on
```

#### Byobu
For now, only Byobu's tmux backend is supported; the Screen backend isn't.

Make sure your `~/.byobu/.tmux.conf` contains the following:
```
# Enables 256 colors and italics
set -g default-terminal "tmux-256color"

# Allows escape sequences to pass through Byobu to the terminal emulator
set -g allow-passthrough on
```

## License
[MIT](LICENSE)
