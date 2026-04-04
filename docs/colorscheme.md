# Colorscheme

A colorscheme is a mapping that map a colorscheme type to a color.

You can configure it in config.

Key is a colorscheme type(normal, ui element, or language syntax type), value is a json object with a fg(foreground) and bg(background) keys.

For every fg and bg key, value is an array of strings, which represents color and styles of text. For truecolor, you can specify an rgb value, like "#cdd6g4"; For 8 color, you can specify only 8 colors + default: ("default", "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white").

You can also specify >= 0 style with a color, ("bold", "underline", "reverse", "italic", "blink", "brighter", "dim", 
"strikeout", "underline2", "overline", "invisible")

Notice that you can only specify "fg" or "bg" to one colorscheme type, but following types should have both:

- "normal"
- "menu"
- "menu_selection"
- "sidbar"
- "statusline"

Below is a truecolor colorscheme example, all colorscheme type is showed.

You can check [colorscheme.json](../resource/colorscheme.json) to have a look at all provided colorscheme.

```json
  "default_truecolor": {
    "normal": { "fg": [ "#cdd6f4" ], "bg": [ "#1e1e2e" ] },
    "selection": { "bg": [ "#4a4a68" ] },
    "menu": { "fg": [ "#bac2de" ], "bg": [ "#45475a" ] },
    "menu_selection": { "fg": [ "#1e1e2e" ], "bg": [ "#cba6f7" ] },
    "sidebar": { "fg": [ "#7f849c" ], "bg": [ "#1e1e2e" ] },
    "statusline": { "fg": [ "#cdd6f4" ], "bg": [ "#313244" ] },
    "search": { "fg": [ "#1e1e2e" ], "bg": [ "#f9e2af" ] },

    "keyword": { "fg": [ "#b4befe" ] },
    "typebuiltin": { "fg": [ "#f9e2af" ] },
    "operator": { "fg": [ "#94e2d5" ] },
    "string": { "fg": [ "#a6e3a1" ] },
    "comment": { "fg": [ "#6c7086", "italic" ] },
    "number": { "fg": [ "#fab387" ] },
    "constant": { "fg": [ "#f2cdcd" ] },
    "function": { "fg": [ "#89b4fa" ] },
    "type": { "fg": [ "#89b4fa" ] },
    "variable": { "fg": [ "#cdd6f4" ] },
    "delimiter": { "fg": [ "#f5c2e7" ] },
    "property": { "fg": [ "#eba0ac" ] },
    "label": { "fg": [ "#89dceb" ] }
  }
```

## Config

Now we only have 2 default colorscheme: one for 8 colors, one for truecolor. Users can create a `colorscheme.json` in the config directory, provide a colorscheme like above, and set "colorscheme" to your colorscheme name. Note that "default8" and "default_truecolor" is reserved.
