# Colorscheme

A colorscheme is a mapping that map a colorscheme type to a color.

You can configure it in config.

Key is a colorscheme type(normal, ui element, or language syntax type), 
value is a json object with a fg(foreground) and bg(background) keys.

For every fg and bg key, value is an array of strings, 
which represents color and styles of text. 
For truecolor, you can specify an rgb value, like "#cdd6g4"; 
For 8 color, you can specify only 8 colors + default: 
("default", "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white").

You can also specify >= 0 style with a color, ("bold", "underline", "reverse", "italic", "blink", "brighter", "dim", 
"strikeout", "underline2", "overline", "invisible")

Below is a truecolor colorscheme example, all colorscheme type is showed.

```json
"default_truecolor": {
    "normal": { "fg": ["#cdd6f4"], "bg": ["#1e1e2e"] },
    "selection": { "fg": ["#1e1e2e"], "bg": ["#cba6f7"] },
    "menu": { "fg": ["#cdd6f4"], "bg": ["#313244"] },
    "linenumber": { "fg": ["#7f849c"], "bg": ["#1e1e2e"] },
    "statusline": { "fg": ["#cdd6f4"], "bg": ["#313244"] },

    "keyword": { "fg": ["#cba6f7"], "bg": ["#1e1e2e"] },
    "typebuiltin": { "fg": ["#89b4fa"], "bg": ["#1e1e2e"] },
    "operator": { "fg": ["#89dceb"], "bg": ["#1e1e2e"] },
    "string": { "fg": ["#a6e3a1"], "bg": ["#1e1e2e"] },
    "comment": { "fg": ["#6c7086", "italic"], "bg": ["#1e1e2e"] },
    "number": { "fg": ["#f9e2af"], "bg": ["#1e1e2e"] },
    "constant": { "fg": ["#fab387"], "bg": ["#1e1e2e"] },
    "function": { "fg": ["#89b4fa"], "bg": ["#1e1e2e"] },
    "type": { "fg": ["#cba6f7"], "bg": ["#1e1e2e"] },
    "variable": { "fg": ["#cdd6f4"], "bg": ["#1e1e2e"] },
    "delimiter": { "fg": ["#f38ba8"], "bg": ["#1e1e2e"] },
    "property": { "fg": ["#f5c2e7"], "bg": ["#1e1e2e"] },
    "label": { "fg": ["#89b4fa"], "bg": ["#1e1e2e"] }
},
```

## Config

Now we only have 2 default colorscheme: one for 8 colors, one for truecolor.
Users can create a `colorscheme.json` in the config directory, provide a colorscheme like above,
and set "colorscheme" to your colorscheme name. Note that "default8" and "default_truecolor" is reserved.