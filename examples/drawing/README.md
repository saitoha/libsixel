Drawing Example
===============

This example suggests how to implement the interaction among SIXEL terminals and pointer devices.

  [![drawing](https://raw.githubusercontent.com/saitoha/libsixel/data/data/drawing.png)](https://youtu.be/2-2FnoZp4Z0)


Requirements
------------
To play this demo application, your terminal have to implement SIXEL graphics and DEC Locator mode.

- XTerm (with --enable-dec-locator --enable-sixel-graphics --with-terminal-id=VT340 configure option)
- RLogin
- mlterm


How to Build
------------

  $ make


Run (only works on SIXEL terminals)
-----------------------------------

  $ ./demo


License
--------
Hayaki Saito <saitoha@me.com>

I declared main.c is in Public Domain (CC0 - "No Rights Reserved").
This example is offered AS-IS, without any warranty.
