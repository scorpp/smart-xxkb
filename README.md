# smart-xxkb
Per-window-tab keyboard layout manager

## Problem Statement
Modern PC desktops often involve multiple levels of modality: virtual desktops, windows, tabs, and more.

It's also common to communicate with people from around the world, using 
different languages.

And often, all of this happens simultaneously!

Most keyboard layout managers only remember the layout per application or, at best, per window.

Imagine participating in several conversations on Slack, each in a different 
language. Or writing a work email in English while chatting with your team in their native language (e.g., Ukrainian)
in the same browser window.

## Working Principle
**smart-xxkb** tracks changes to keyboard layouts and window titles, associating the two.

When you switch keyboard layouts, it remembers the layout for your current 
window and its title (e.g., a browser tab).

If the window title changes and a layout is already associated with that title, 
the keyboard layout is automatically switched to the remembered one.

## Limitations
- Unfortunately, not all applications expose their internal context in the window title. In such cases, you will
  experience a standard per-window layout behavior.