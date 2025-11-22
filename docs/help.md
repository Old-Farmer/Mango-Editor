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

### Code Completion(demo)

- `<c-k><c-c>`
    trigger code completion

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

## Configuration

Mango currently uses json for user configuration: 
    All config files are in `$XDG_CONFIG_HOME/mango-editor/`.
    Current support files: `config.json`, `colorscheme.json`.


In `config.json`, Support options:

1. tab_stop  
    scope: buffer,
    type: int,
    default: 4,
    desc: Tab aligment.

2. tab_space  
    scope: buffer,
    type: bool,
    default: true,
    desc: When hit tab, insert space or just tab.

3. auto_indent  
    scope: buffer,
    type: bool,
    default: true,
    desc: Auto indentation.

4. auto_pair  
    scope: buffer,
    type: bool,
    default: true,
    desc: Auto pair for brackets, etc.

5. line_number  
    scope: window,
    type: int,
    available: 0 = none, 1 = absolute,
    default: 1,
    desc: Show line number.

6. basic_word_completion  
    scope: global,
    type: bool,
    default: true,
    desc: A baisc ascii word completion.

7. truecolor  
    scope: global,
    type: bool,
    default: true,
    desc: Truecolor support. If enabled, all ui will use the terminal 
    truecolor ability, otherwise all ui use the terminal 8 color.

8. colorscheme:  
    scope: global,
    type: string,
    default: "default",
    desc: A colorscheme. When truecolor not enabled, 
    default is a clean 8 color colorscheme; 
    Otherwise, Default is a Catpuccin-like colorscheme(See colorscheme.md for more info).


Note that you can set per filetype buffer options like:

```json
{
    "c": {
        "auto_pair": false
    }
}
```

## Logging

The path of logging file is `%XDG_CACHE_HOME/mango.log`.

## NOTE

### Unicode && UTF-8

Unicode is a complex topic, and I support it carefully.
However, bugs may be occured due to **lack of knowledgement, poor support**
**of low-level library or terminal issues/limitaions**.
Also, sometimes I loose the constraint of unicode to gain some performance benifit. 
Please feel free to report any issue that you have encountered.

References:

- [UAX#11](http://www.unicode.org/reports/tr11/)
- [UAX#29](http://www.unicode.org/reports/tr29/)
- [UTX#51](https://unicode.org/reports/tr51/)
