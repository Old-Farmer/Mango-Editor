# Keymaps

A keymap is a key sequence which can trigger some operations. Sometime keymaps are called Shortcuts in other editors.

NOTE: Don't support self-defined keymaps now.

`<c-...>` means ctrl + ...

`<a-...>` means alt + ...

Use `<c-k>` as a general leader key.

## General

- `<c-q>`
    quit

- `<c-e>`
    go to peel(to execute command)

## Navigation

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

- `<c-k><c-g>`
    goto line.

- `<a-left>`
    jump backward.

- `<a-right>`
    jump forward.

## Edit

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
    cut (copy/paste will use OS clipboard if available, install xsel if you want this feature)

- `<c-a>` or `<c-k><c-l>`
    select all

## Code Completion

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