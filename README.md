# epubworm
An image-capable, minimalistic TUI epub reader written in C++.

## Image Support
epubworm uses the unicode placeholders feature of the [Kitty Graphics Protocol](https://sw.kovidgoyal.net/kitty/graphics-protocol/) to display images.

### Terminals
This way of displaying images is currently supported by:
- [Kitty](https://sw.kovidgoyal.net/kitty/)
- [Ghostty](https://ghostty.org/)
- [iTerm2](https://iterm2.com/)
- [st-graphics](https://github.com/sergei-grechanik/st-graphics)

and could be implemented in the near future by:
- WezTerm [PR](https://github.com/wezterm/wezterm/pull/7924)
- xterm.js [issue](https://github.com/xtermjs/xterm.js/issues/5711)
- Warp [issue](https://github.com/warpdotdev/warp/issues/6210), [PR](https://github.com/warpdotdev/warp/pull/15001)

epubworm still works in terminals without unicode placeholder graphics support, only without image capabilities.

## License
[MIT](LICENSE)
