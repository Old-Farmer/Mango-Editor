#pragma once

#include <memory>

#include "buffer.h"
#include "buffer_manager.h"
#include "cmp_menu.h"
#include "command_manager.h"
#include "cursor.h"
#include "editor_event_manager.h"
#include "event_loop.h"
#include "explorer.h"
#include "fs_monitor.h"
#include "keyseq_manager.h"
#include "layout_manager.h"
#include "mango_peel.h"
#include "mouse.h"
#include "state.h"
#include "status_line.h"
#include "syntax.h"
#include "text_window.h"
#include "timer_manager.h"
#include "utils.h"

namespace charxed {

struct InitOpts;
struct Options;

class Editor {
   private:
    CHX_DEFAULT_CONSTRUCT_DESTRUCT(Editor);

   public:
    CHX_DELETE_COPY(Editor);
    CHX_DELETE_MOVE(Editor);

    // Make sure that options is static lifetime
    void Init(std::unique_ptr<GlobalOpts> global_opts,
              std::unique_ptr<InitOpts> init_opts);

    // throws TermException
    void Loop();

    static Editor& GetInstance();

    void Help(const std::string& doc_name);
    void Quit(bool force);

    // require: not in peel
    void GotoPeel(Mode mode);

    using PromptHandler = std::function<void(std::string_view)>;
    // goto peel command mode and emmit a prompt.
    // Useful when we need a simple yes/no.
    // require: not in peel
    void Prompt(const std::string& prefix, const PromptHandler& handler);

    void ExitFromMode();
    void GotoMode(Mode mode);
    void TriggerCompletion(bool autocmp);
    void CancellCompletion();
    bool CompletionTriggered();
    void SearchCurrentWindow(const std::string& pattern);
    void CommandHitEnter();
    void SearchHitEnter();

    void CursorGoSearch(bool next, size_t count, bool keep_current_if_one);

    void RemoveCurrentBuffer();
    void SaveCurrentBuffer();
    void SaveCurrentBufferAs(const Path& path);

    void NotifyUser(std::string_view str);

    void StartupScreen();

    void Edit(const std::string& path);

    // Make sure not in peel
    void OpenExplorer();
    void QuitExplorer();

   private:
    // Editor Lifetime
    void InitKeymaps();
    void InitCommands();
    void RegisterEditorEventHandlers();

    void HandleBracketedPaste(std::string& bracketed_paste_buffer);
    void HandleKey();
    void HandleLeftClick(int s_row, int s_col);
    void HandleRelease(int s_row, int s_col);
    void HandleMouse();
    void HandleResize();

    void StartAutoCompletionTimer();
    void StartSearchOnTypeTimer();
    void TrySearchOnType();

    // A nice method that combine codepoints in the input stream to a grapheme.
    // But due to limitation of terminal protocal, we don't know whether it's
    // really a grapheme produced by users once a time. So best effort.
    Character CombineACharacterFromInput(Codepoint init_cp);

    void Draw();
    void PreProcess();

    // Count is at least 1.
    size_t Count() { return count_ == 0 ? 1 : count_; }
    // OpPendingCount is at least 1.
    size_t OpPendingCount() {
        return Count() *
               (op_pending_stored_count_ == 0 ? 1 : op_pending_stored_count_);
    }

    // helper methods
    void PrintKey(const Terminal::KeyInfo& key_info);
    Window* LocateWindow(int s_col, int s_row);
    // Make sure not in peel
    void EnsureInEditorContext();

   private:
    std::unique_ptr<EventLoop> loop_;

    Mode mode_ = Mode::kNormal;
    Context context_ = Context::kEditor;
    bool command_prompt_ = false;  // Is in prompt sub mode when command mode?
    PromptHandler prompt_handler_;

    std::unique_ptr<BufferManager> buffer_manager_;
    KeyseqManager keymap_manager_{mode_, context_};
    CommandManager command_manager_;
    std::unique_ptr<SyntaxParser> syntax_parser_;
    EditorEventManager editor_event_manager_;
    std::unique_ptr<LayoutManager> layout_manager_;
    std::unique_ptr<BufferFSMonitor> buffer_monitor_;

    enum class ContextID : int {};
    class ContextManager {
        std::unordered_map<ContextID, void*> contexts_;

       public:
        CHX_DEFAULT_CONSTRUCT_DESTRUCT(ContextManager);
        CHX_DELETE_COPY(ContextManager);
        CHX_DELETE_MOVE(ContextManager);

        void*& GetContext(ContextID id);
        void FreeContext(ContextID id);
    };
    ContextManager contexts_manager_;

    std::unique_ptr<ClipBoard> clipboard_;

    Mouse mouse_;
    Cursor cursor_;
    // Now only support one window in the screen
    // TODO: mutiple window logic
    std::unique_ptr<TextWindow> text_window_;
    std::unique_ptr<StatusLine> status_line_;
    std::unique_ptr<MangoPeel> peel_;
    std::unique_ptr<CmpMenu> cmp_menu_;
    std::unique_ptr<Explorer> explorer_;

    // Cmp context
    Completer* completer_ = nullptr;
    bool show_cmp_menu_ = false;  // if false, hide cmp menu.

    bool multirow_peel_keep_ = false;

    enum class Operator {
        kYank,
        kDelete,
        kIndent,
        kUnindent,
    };

    size_t count_ = 0;

    size_t op_pending_stored_count_ = 0;
    Character c_to_find_;
    bool find_forward_ = true;

    Operator pending_operator_;

    bool highlight_search_ = false;
    bool search_foward_ = true;

    std::unique_ptr<SingleTimer> autocmp_trigger_timer_;
    std::unique_ptr<SingleTimer> search_on_type_timer_;

    bool need_redraw_ = true;

    std::unique_ptr<GlobalOpts> global_opts_;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace charxed
