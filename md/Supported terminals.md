# Supported terminals

## Hardware

- DEC VT series, VT240/VT241/VT330/VT340/VT282/VT284/VT286/VT382

## Software

- DECterm(dxterm)

- Kermit

- ZSTEM 340

- WRQ Reflection

- RLogin (Japanese terminal emulator)

  [https://nanno.dip.jp/softlib/man/rlogin/](https://nanno.dip.jp/softlib/man/rlogin/)

- mlterm

  [https://mlterm.sourceforge.net/](https://mlterm.sourceforge.net/)

  Works on each of X, WIN32 GDI, framebuffer, Android, Cocoa version.

- XTerm (compiled with `--enable-sixel-graphics` option)

  [https://invisible-island.net/xterm/](https://invisible-island.net/xterm/)

  You should launch xterm with "`-ti vt340`" option.
  The SIXEL palette is limited to a maximum of 16 colors.
  To avoid this limitation, Try

```sh
$ echo "XTerm*decTerminalID: vt340" >> $HOME/.Xresources
$ echo "XTerm*numColorRegisters: 256" >>  $HOME/.Xresources
$ xrdb $HOME/.Xresources
$ xterm
```

  or

```sh
$ xterm -xrm "XTerm*decTerminalID: vt340" -xrm "XTerm*numColorRegisters: 256"
```

- yaft

  [https://github.com/uobikiemukot/yaft](https://github.com/uobikiemukot/yaft)

- recterm (ttyrec to GIF converter)

  [https://github.com/uobikiemukot/recterm](https://github.com/uobikiemukot/recterm)

- seq2gif (ttyrec to GIF converter)

  [https://github.com/saitoha/seq2gif](https://github.com/saitoha/seq2gif)

- Mintty (>= 2.6.0)

  [https://mintty.github.io/](https://mintty.github.io/)

- cancer
  [https://github.com/meh/cancer/](https://github.com/meh/cancer)

- MacTerm
  [https://github.com/kmgrant/macterm](https://github.com/kmgrant/macterm)

- WezTerm
  [https://github.com/wez/wezterm](https://github.com/wez/wezterm)

- Darktile
  [https://github.com/liamg/darktile](https://github.com/liamg/darktile)

- iTerm2 (>= 3.0.0)
  [https://gitlab.com/gnachman/iterm2](https://gitlab.com/gnachman/iterm2)

- st-sixel
  [https://github.com/galatolofederico/st-sixel](https://github.com/galatolofederico/st-sixel)

- DomTerm
  [https://github.com/PerBothner/DomTerm](https://github.com/PerBothner/DomTerm)

- yaft-cocoa
  [https://github.com/uobikiemukot/yaft-cocoa](https://github.com/uobikiemukot/yaft-cocoa)

- toyterm
  [https://github.com/algon-320/toyterm](https://github.com/algon-320/toyterm)
