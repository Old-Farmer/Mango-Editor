# Configuration

Mango currently uses json for user configuration: 
    All config files are in `$XDG_CONFIG_HOME/mango-editor/`.
    Current support files: `config.json`, `colorscheme.json`.

## Options

You can set options in `config.json` as key value pairs.

There are 3 option scopes: global, buffer and window. You can set all options in `config.json` globally, but buffer & window options can be overridden per buffer & window. A buffer is memory presentation of a file and window is where to show a buffer.

Currently, you can set per filetype buffer scope options like:

```json
{
    "c": {
        "auto_pair": false
    }
}
```

Window scope options can't specified per window now.

In `config.json`, Support options:

### Buffer Scope

- auto_indent  
    type: bool,
    default: true,
    desc: Auto indentation.

- auto_pair  
    type: bool,
    default: true,
    desc: Auto pair for brackets, etc.

- max_edit_history:  
    type: integer,
    default: 100,
    desc: The maximum number of edit history records kept for a buffer.

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

### Window Scope

- end_of_buffer_mark  
    type: bool
    default: false,
    desc: Display markers after the end of the buffer.

- line_number  
    type: int,
    available: 0 = none, 1 = absolute, 2 = relative,
    default: 1,
    desc: Show line number.

- trailing_white  
    type: bool
    default: true
    desc: Show trailing white character.

### Global Scope

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
    default: 100,
    desc: Timeout after the user stops typing before triggering deferred behaviors, etc. auto cmp or searching on typing.

- logverbose:  
    type: bool,
    default: false,
    desc: Verbose Logging.

- max_jump_history:  
    type: integer,
    default: 100,
    desc: The maximum number of edit history records kept for each window.

- search_ignore_case:  
    type: bool
    default: true
    desc: Ignore case when do search unless using uppercase character.

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

## Colorscheme

See [colorscheme](./colorscheme.md)
