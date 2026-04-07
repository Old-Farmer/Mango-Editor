# TODO

## UI

- syntax highlight on more language(bash, go, java, markdown, markdown_inline, rust, etc.)
- Soft line break
- highlight line on cursor
- Multi-window
- File explorer
- Bufferline(low priority)

## LSP

- Json RPC protocal
- Lsp Client

## Vi-Mode

- Operator pending mode
- Text object

## Usability

- Options need a range limitaion
- Jump history should be adjusted when buffer changed.
- File changing detection
- diff algorithm and diff view
- Better auto indent per laguages
- MacOS support
- Windows support
- Low lantency remote development

## Extensibiliy

- JS engine embbeded
- Plugin system
- API Design
    - Buffer API
    - Cursor API
    - UI API: Window
    - Option API
    - Keymap API
    - Command API

## Performance

- Big file support:
    - Refactor buffer data structure (line oriented, but rope like, balanced tree)
    - treesitter background thread parsing
    - File background thread saving
    - long line optimization

## Code Quality

- linter
