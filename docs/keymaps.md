# Keymaps

A keymap is a key sequence which can trigger some operations. Sometime keymaps are called Shortcuts in other editors.

NOTE: Don't support self-defined keymaps now.

## Notation

- `<c-...>` means Ctrl + ...
- `<a-...>` means Alt + ...
- `<enter>` means Enter key
- `<esc>` means Escape key
- `<bs>` means Backspace key
- `<home>` means Home key
- `<end>` means End key
- `<left>` means Left arrow key
- `<right>` means Right arrow key
- `<character>` means any character

## Mode Reference

- **Normal**: Normal editing mode, like Vim's Normal mode
- **Insert**: Insert mode, for typing text
- **Select**: Character-wise selection mode
- **Select-L**: Line-wise selection mode
- **Op-Pend**: Operator pending mode
- **Command**: Command input mode (`:` prompt)
- **Search**: Search input mode (`/` or `?` prompt)
- **Show**: Peel show mode (multirow output display)

## How Operator Pending Works

Operator pending is the "waiting for a motion" state after you press an operator key in Normal mode.

Think of it as:

`operator + motion/text-object`

Examples:

- `dw` deletes one word
- `y$` yanks to end of line
- `>j` indents the current line and the next line
- `di(` deletes inside parentheses
- `2d2w` becomes `d4w` because counts multiply

---

## General

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `<esc>` / `<c-[>` | Exit from current mode / close Peel | All modes | All |

## Navigation

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `h` | Move cursor left | Normal, Select, Select-L, Show | Editor |
| `l` | Move cursor right | Normal, Select, Select-L, Show | Editor |
| `k` | Move cursor up | Normal, Select, Select-L, Show | All |
| `j` | Move cursor down | Normal, Select, Select-L, Show | All |
| `b` | Move to beginning of word | Normal, Select, Select-L, Show | Editor |
| `e` | Move to end of word | Normal, Select, Select-L, Show | Editor |
| `w` | Move to beginning of next word | Normal, Select, Select-L, Show | Editor |
| `0` | Move to beginning of line | Normal, Select, Select-L, Show | Editor |
| `^` | Move to the first non-blank character of the line | Normal, Select, Select-L | Editor |
| `$` | Move to end of line | Normal, Select, Select-L, Show | Editor |
| `%` | Move to the other bracket in a bracket pair(or go to {count} %) | Normal, Select, Select-L, Show | Editor |
| `<c-f>` | Move down one page | Normal, Select, Select-L, Show | All |
| `<c-b>` | Move up one page | Normal, Select, Select-L, Show | All |
| `<c-d>` | Move down half page | Normal, Select, Select-L, Show | All |
| `<c-u>` | Move up half page | Normal, Select, Select-L, Show | All |
| `gg` | Move to beginning | Normal, Select, Select-L | Editor |
| `G` | Move to end of file (or go to line {count}) | Normal, Select, Select-L | Editor |
| `gf` | Go to file at cursor | Normal, Select, Select-L | Editor |
| `f<character>` | Go to the next positon of the character in the current line | Normal, Select, Select-L | Editor |
| `F<character>` | Go to the prev positon of the character in the current line | Normal, Select, Select-L | Editor |
| `<c-o>` | Jump to previous cursor position | Normal | Editor |
| `<c-i>` | Jump to next cursor position | Normal | Editor |
| `]b` | Go to next buffer | Normal | Editor |
| `[b` | Go to previous buffer | Normal | Editor |

## Selection

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `s` | Start line-wise selection | Normal | Editor |
| `S` | Start character-wise selection | Normal | Editor |

## Editing Operations

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `<c-s>` | Save the current buffer | Normal, Select, Select-L, Insert | Editor |
| `y` | Copy selection / prepare yank operation | Select, Select-L, Normal | Editor |
| `Y` | Copy to line end | Normal | Editor |
| `d` | Cut selection / prepare delete operation | Select, Select-L, Normal | Editor |
| `D` | Delete to line end | Normal | Editor |
| `p` | Paste just after cursor | Normal | Editor |
| `P` | Paste at cursor | Normal | Editor |
| `p`,`P` | Replace selection | Select, Select-L | Editor |
| `u` | Undo | Normal | Editor |
| `<c-r>` | Redo | Normal | Editor |
| `i` | Enter insert mode at cursor | Normal | Editor |
| `I` | Enter insert mode at first non-blank character | Normal | Editor |
| `a` | Enter insert mode after cursor | Normal | Editor |
| `A` | Enter insert mode at end of line | Normal | Editor |
| `o` | Create new line below and enter insert mode | Normal | Editor |
| `O` | Create new line above and enter insert mode | Normal | Editor |
| `<enter>` | Add newline | Insert | Editor |
| `<bs>` | Delete character before cursor | Insert | Editor |
| `<c-w>` | Delete word before cursor | Insert | Editor |
| `<c-r>` | Paste | Insert | Editor |
| `<space>f` | Call clang-format to format the current buffer(really unstable) | Normal | Editor |
| `>` | Indent | Select, Select-L, Op-Pend | Editor |
| `<` | Unindent | Select-L, Select-L, Op-Pend | Editor |

## Operator Pending Motions / Text Objects

operator support:

- `y` copy
- `d` delete
- `>` indent
- `<` unindent

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `y` | Only after `y`, yank lines | - | Editor |
| `d` | Only after `d`, delete lines | - | Editor |
| `>` | Only after `>`, indent lines | - | Editor |
| `<` | Only after `<`, unindent lines | - | Editor |
| `h`, `l`, `b`, `e`, `w`, `0`, `^`, `$` | As if the cursor moves, and do operation in the cursor movement range(character wise, exclusive end character) | - | Editor |
| `%` | As if the cursor moves, and do operation in the cursor movement range(character wise, inclusive end character) | - | Editor |
| `k`, `j`, `gg`, `G` | As if the cursor moves, and do operation in the cursor movement range(line wise, inclusive end line) | - | Editor |
| `ip` | inner content of a pair(bracket/quote), `p` = `{`, `}`, `(`, `)`, `[`, `]`, `"`, `'`, `` ` ``  | - | Editor |
| `ap` | a pair(bracket/quote) | - | Editor |


## Completion & History

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `<c-space>` | Trigger completion | Insert, Command | All |
| `<c-c>` | Trigger completion | Insert, Command | All |
| `<tab>` | Accept completion or insert tab | Insert | Editor |
| `<tab>` | Accept completion | Command | All |
| `<c-n>` | Select next completion | Insert, Command | All |
| `<c-n>` | Select next history | Command, Search | All |
| `<c-p>` | Select prev completion | Insert, Command | All |
| `<c-p>` | Select prev history | Command, Search | All |

## Search & Command

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `/` | Start forward search | Normal | All |
| `?` | Start backward search | Normal | All |
| `n` | Go to next search match | Normal | All |
| `N` | Go to previous search match | Normal | All |
| `:` | Enter command mode | Normal | All |
| `<enter>` | Open Peel show mode | Normal | Editor |

## Peel Input (Command & Search)

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `<left>` | Move cursor left | Command, Search | All |
| `<right>` | Move cursor right | Command, Search | All |
| `<c-left>` | Move to previous word | Command, Search | All |
| `<c-right>` | Move to next word | Command, Search | All |
| `<home>` | Move to beginning | Command, Search | All |
| `<end>` | Move to end | Command, Search | All |
| `<bs>` | Delete character before cursor | Command, Search | All |
| `<c-w>` | Delete word before cursor | Command, Search | All |
| `<c-r>` | Paste from clipboard | Command, Search | All |
| `<c-n>` | Next history item | Command, Search | All |
| `<c-p>` | Previous history item | Command, Search | All |
| `<enter>` | Execute command or search | Command, Search | All |


## Explorer

| Key | Description | Mode(s) | Context |
| --- | --- | --- | --- |
| `<space>e` | Open explorer | Normal | Edit |
| `<enter>` | Expand/Collapse dirs or Open files | Normal | Explorer |
| `q` | Quit explorer | Normal | Explorer |
