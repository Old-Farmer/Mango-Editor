# Mango editor

## Keymaps

`<c-...>` means ctrl + ...

Use `<c-k>` as a general leader key.

### General

- `<c-q>`
    quit

- `<c-e>`
    go to peel(to execute command)

### Navigation

- `<home>`
    line home

- `<end>`
    line end

- `<pgdn>`
    page down

- `<pgup>`
    page up

- `<left>`
    move cursor left

- `<right>`
    move cursor right

- `<c-left>`
    move cursor to the previous word

- `<c-right>`
    move cursor to the next word

- `<up>` or `<c-p>`
    move cursor up

- `<down>` or `<c-n>`
    move cursor down

- `<c-b><c-n>` or `<c-pgdn>`
    next buffer

- `<c-b><c-p>` or `<c-pgup>`
    previous buffer

- `<c-b><c-d>`
    delete buffer

- `<c-b><c-b>`
    search buffer

- `<c-b><c-e>`
    open file

- `<c-f>`
    search. `<c-k><c-n>` or `<c-k><c-p>` to jump to search result when in search mode

### Edit

- `<bs>`
    delelte a character / selection

- `<c-w>`
    delete a word

- `<enter>`
    insert a new line

- `<tab>`
    indent

- `<c-s>`
    save file to disk

- `<c-z>`
    undo

- `<c-y>`
    redo

- `<c-c>`
    copy

- `<c-v>`
    paste

- `<c-x>`
    cut (copy/paste will use OS clipboard if available,
    install xsel if you want this feature)

- `<c-a>` or `<c-k><c-l>`
    select all

### Code Completion

- `<c-k><c-c>`
    trigger code completion

- `<tab>`
    trigger completion in peel.

- `<c-n>` or `<down>`
    select next cmp entry

- `<c-p>` or `<up>`
    select prev cmp entry

- `<enter>`
    accept current entry

- `<esc>`
    cancel completion

## Mouse

- left click
- scroll up/down
- selection

## Commands

- "s `<search-pattern>`"
    search string

- "h"
    goto help

- "e `<path>`"
    goto and edit the file

- "b `<path>`"
    goto and edit the buffer

## Configuration

Mango currently uses json for user configuration: 
    All config files are in `$XDG_CONFIG_HOME/mango-editor/`.
    Current support files: `config.json`, `colorscheme.json`.


In `config.json`, Support options:

Buffer Scope:

1. tab_stop  
    type: int,
    default: 4,
    desc: Tab aligment.

2. tab_space  
    type: bool,
    default: true,
    desc: When hit tab, insert space or just tab.

3. auto_indent  
    type: bool,
    default: true,
    desc: Auto indentation.

4. auto_pair  
    type: bool,
    default: true,
    desc: Auto pair for brackets, etc.

Note that you can set per filetype buffer options like:

```json
{
    "c": {
        "auto_pair": false
    }
}
```

Window Scope:

1. line_number  
    type: int,
    available: 0 = none, 1 = absolute,
    default: 1,
    desc: Show line number.

Global Scope:

1. basic_word_completion  
    type: bool,
    default: true,
    desc: A baisc word completion.

2. truecolor  
    type: bool,
    default: true,
    desc: Truecolor support. If enabled, all ui will use the terminal 
    truecolor ability, otherwise all ui use the terminal 8 color.

3. colorscheme:  
    type: string,
    default: "default",
    desc: A colorscheme. When truecolor not enabled, 
    default is a clean 8 color colorscheme; 
    Otherwise, Default is a Catpuccin-like colorscheme(See colorscheme.md for more info).

4. cursor_blinking:  
    type: bool,
    default: true,
    desc: Is cursor blinking?

5. cursor_blinking_show_interval:  
    type: integer,
    default: 600,
    desc: When cursor_blinking is enabled, this option controls 
    how long the cursor will show in a blinking loop(unit: ms).

6. cursor_blinking_hide_interval:  
    type: integer,
    default: 600,
    desc: When cursor_blinking is enabled, this option controls 
    how long the cursor will hide in a blinking loop(unit: ms).

7. auto_cmp_timeout:  
    type: integer,
    default: 100,
    desc: Auto-completion is triggered after a timeout following the user’s input.


## Logging

The path of logging file is `%XDG_CACHE_HOME/mango.log`.

## NOTE

### Unicode && UTF-8

Unicode is a complex and 💩, but I support it as much as I can.
However, bugs may be occured due to **lack of knowledgement, poor support**
**of low-level library or terminal issues/limitaions**.
Also, sometimes I loose the constraint of unicode **which will not effect the normal** **circumstance** to just simplify the implementaion or to gain some performance benifit.
Please feel free to report any issue that you have encountered.

References:

- [UAX#11](http://www.unicode.org/reports/tr11/)
- [UAX#15](https://unicode.org/reports/tr15/)
- [UAX#29](http://www.unicode.org/reports/tr29/)
- [UTX#51](https://unicode.org/reports/tr51/)

- <https://www.unicode.org/L2/L2023/23140-graphemes-expectations.pdf>

- <https://sw.kovidgoyal.net/kitty/text-sizing-protocol>