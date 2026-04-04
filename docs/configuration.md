# Configuration

Mango currently uses json for user configuration: 
    All config files are in `$XDG_CONFIG_HOME/mango-editor/`.
    Current support files: `config.json`, `colorscheme.json`.

In `config.json`, Support options:

Buffer Scope:

- auto_indent  
    type: bool,
    default: true,
    desc: Auto indentation.

- auto_pair  
    type: bool,
    default: true,
    desc: Auto pair for brackets, etc.

- tab_space  
    type: bool,
    default: true,
    desc: When hit tab, insert space or just tab.

- tab_stop  
    type: int,
    default: 4,
    desc: Tab aligment.

- wrap  
    type: bool,
    default: false,
    desc: Wrap the file content.

Note that you can set per filetype buffer options like:

```json
{
    "c": {
        "auto_pair": false
    }
}
```

Window Scope:

- line_number  
    type: int,
    available: 0 = none, 1 = absolute, 2 = relative,
    default: 1,
    desc: Show line number.

Global Scope:

- basic_word_completion  
    type: bool,
    default: true,
    desc: A baisc word completion.

- cmp_menu_max_height:  
    type: integer
    default: 15
    desc: Max height(columns) of a completion menu.

- cmp_menu_max_width:  
    type: integer
    default: 40
    desc: Max width(rows) of a completion menu.

- colorscheme:  
    type: string,
    default: "default",
    desc: A colorscheme. When truecolor not enabled, default is a clean 8 color colorscheme; Otherwise, Default is a Catpuccin-like colorscheme(See [Colorscheme](./colorscheme.md) for more info).

- highlight_on_search:  
    type: bool,
    default: true,
    desc: highlight all searched strings.

- input_indle_timeout:  
    type: integer,
    default: 150,
    desc: Timeout after the user stops typing before triggering deferred behaviors, etc. auto cmp or searching on typing.

- logverbose:  
    type: bool,
    default: false,
    desc: Verbose Logging.

- max_edit_history:  
    type: integer,
    default: 100,
    desc: The maximum number of edit history records kept for each buffer.

- max_jump_history:  
    type: integer,
    default: 100,
    desc: The maximum number of edit history records kept for each window.

- scroll_rows:  
    type: integer,
    default: 3,
    desc: The number of rows scrolled each time.

- truecolor  
    type: bool,
    default: true,
    desc: Truecolor support. If enabled, all ui will use the terminal truecolor ability, otherwise all ui use the terminal 8 color.

- vim  
    type: bool,
    default: false,
    desc: Vim mode support. If enabled, Mango will switch to Vim mode.
