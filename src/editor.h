#pragma once

#include <memory>

#include "buffer.h"
#include "buffer_manager.h"
#include "cmp_menu.h"
#include "command_manager.h"
#include "cursor.h"
#include "editor_event_manager.h"
#include "event_loop.h"
#include "keyseq_manager.h"
#include "layout_manager.h"
#include "mango_peel.h"
#include "mouse.h"
#include "state.h"
#include "status_line.h"
#include "syntax.h"
#include "timer_manager.h"
#include "utils.h"
#include "vim.h"
#include "window.h"

namespace mango {

struct InitOpts;
struct Options;

class Editor {
   private:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(Editor);

   public:
    MGO_DELETE_COPY(Editor);
    MGO_DELETE_MOVE(Editor);

    // Make sure that options is static lifetime
    void Init(std::unique_ptr<GlobalOpts> global_opts,
              std::unique_ptr<InitOpts> init_opts);

    // throws TermException
    void Loop();

    static Editor& GetInstance();

    void Help(const std::string& doc_name);
    void Quit(bool force);

    void GotoPeel(Mode mode = Mode::kPeelCommand);
    void ExitFromMode();
    void ExitFromModeVim();
    std::function<void()>
        exit_from_mode_;  // init to call ExitFromMode or ExitFromModeVim.
    void GotoModeVim(Mode mode);
    void TriggerCompletion(bool autocmp);
    void CancellCompletion();
    bool CompletionTriggered();
    void SearchCurrentBuffer(const std::string& pattern);
    void PickBuffers();
    void EditFile();
    void CommandHitEnter();
    void SearchHitEnter();

    void CursorUp(size_t count);
    void CursorDown(size_t count);
    void CursorGoSearch(bool next, size_t count, bool keep_current_if_one);
    void CursorGoSearchVim(bool next, size_t count, bool keep_current_if_one);

    void RemoveCurrentBuffer();
    void SaveCurrentBuffer();
    void SaveCurrentBufferAs(const Path& path);

    void NotifyUser(std::string_view str);

   private:
    // Editor Lifetime
    void InitKeymaps();
    void InitKeymapsVim();
    void InitCommands();
    void InitCommandsVim();
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

    void Draw();
    void PreProcess();

    // Count is at least 1.
    size_t Count() { return count_ == 0 ? 1 : count_; }
    // OpPendingCount is at least 1.
    // Used in vim operator pending mode
    size_t OpPendingCountVim() {
        return Count() * (state_vim_->op_pending_stored_count == 0
                              ? 1
                              : state_vim_->op_pending_stored_count);
    }

    // helper methods
    void PrintKey(const Terminal::KeyInfo& key_info);
    Window* LocateWindow(int s_col, int s_row);

   private:
    std::unique_ptr<EventLoop> loop_;

    Mode mode_;

    std::unique_ptr<BufferManager> buffer_manager_;
    KeyseqManager keymap_manager_{mode_};
    CommandManager command_manager_;
    std::unique_ptr<SyntaxParser> syntax_parser_;
    EditorEventManager editor_event_manager_;
    std::unique_ptr<LayoutManager> layout_manager_;

    enum class ContextID : int {};
    class ContextManager {
        std::unordered_map<ContextID, void*> contexts_;

       public:
        MGO_DEFAULT_CONSTRUCT_DESTRUCT(ContextManager);
        MGO_DELETE_COPY(ContextManager);
        MGO_DELETE_MOVE(ContextManager);

        void*& GetContext(ContextID id);
        void FreeContext(ContextID id);
    };
    ContextManager contexts_manager_;

    std::unique_ptr<ClipBoard> clipboard_;

    Mouse mouse_;
    Cursor cursor_;
    // Now only support one window in the screen
    // TODO: mutiple window logic
    std::unique_ptr<Window> window_;
    std::unique_ptr<StatusLine> status_line_;
    std::unique_ptr<MangoPeel> peel_;
    std::unique_ptr<CmpMenu> cmp_menu_;

    // Cmp context
    Completer* completer_ = nullptr;
    bool show_cmp_menu_ = false;  // if false, hide cmp menu.

    bool multirow_peel_keep_ = false;

    // buffer view stored for peel -> edit
    BufferView b_view_stored_for_edit;

    bool highlight_search_ = false;
    size_t count_;

    std::unique_ptr<EditorStateVim> state_vim_;

    std::unique_ptr<SingleTimer> autocmp_trigger_timer_;
    std::unique_ptr<SingleTimer> search_on_type_timer_;

    std::unique_ptr<GlobalOpts> global_opts_;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
