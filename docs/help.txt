# Mango editor

## Keymaps

Use <c-k> as a general leader key.

### General

- <ctrl-q> quit
- <c-p> to go to peel(command)

### Navigation

- <home> line home
- <end> line end
- <pgdn> page down
- <pgup> page up
- <left> left arrow to move cursor left
- <right> right to move cursor right
- <up> up arrow to move cursor up
- <down> down arrow to move cursor down
- <c-b><c-n> or <c-pgdn> next buffer
- <c-b><c-p> or <c-pgup> previous buffer
- <c-b><c-d> delete buffer
- <c-f> to search, <c-n> <c-p> to jump to search result when in search mode
- <esc> to escape to edit mode

### Edit

- <bs> backspace to del a character before cursor
- <enter> insert a new line
- <tab> tab to insert tab/spaces (see src/options.h)
- <ctrl-s> save
- <c-z> undo
- <c-y> redo

### Code Completion(demo)

- <c-k><c-i> trigger code completion
- <c-n> or <down> select next cmp entry
- <c-p> or <up> select prev cmp entry
- <enter> accept this entry
- <esc> cancel completion

## Mouse

- left click
- scroll up/down

## Commands

- "s <search-pattern>" search string
- "h" goto help
- "e <path>" goto and edit the file