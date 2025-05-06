# smart-xxkb
Per-window-tab keyboard layout manager

## Problem Statement
On modern PC desktop there are multiple modality levels: virtual desktops, 
windows, tabs, etc.

It's also quite common to communicate with diff people around the globe, use
diff languages.

And all that in the same time!

Usual keyboard layout managers only remember layout per app or per window at
best.

Imaging taking part in several conversations in slack, all using it's own
language. Or editing a work email (in English) and chatting with your team in
their native languae (say, Ukrainian) in same browser window.

## Working principle
**smart-xxkb** tracks changes to keyboard layouts and window titles and 
associates the two.

Once you switch keyboard layout it's associated with your current window AND
it's title (e.g. tab).

If window title changes and there's a known layout associated with that title
the keyboard layout is automatically switched to the remembered one.

## Limitations
- Unfortunately, not all apps expose their inner context to window title. In
  such cases you will get a regular per-window layout experience.

