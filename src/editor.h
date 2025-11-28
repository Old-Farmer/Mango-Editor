#pragma once

#include <memory>

#include "buffer.h"
#include "buffer_manager.h"
#include "cmp_menu.h"
#include "command_manager.h"
#include "cursor.h"
#include "event_loop.h"
#include "keyseq_manager.h"
#include "mango_peel.h"
#include "mouse.h"
#include "state.h"
#include "status_line.h"
#include "syntax.h"
#include "timer_manager.h"
#include "utils.h"
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
    // throws TermException
    void Loop(std::unique_ptr<GlobalOpts> global_opts,
              std::unique_ptr<InitOpts> init_opts);

    static Editor& GetInstance();

    void Help();
    void Quit();

    void GotoPeel();
    void ExitFromMode();
    void ExitFromModeVi();
    void TriggerCompletion(bool autocmp);
    void CancellCompletion();
    void StartAutoCompletionTimer();
    bool CompletionTriggered();
    void SearchNext();
    void SearchPrev();
    void PickBuffers();
    void EditFile();
    void PeelHitEnter();

    void CursorUp();
    void CursorDown();

    void RemoveCurrentBuffer();
    void SaveCurrentBuffer();

   private:
    void InitKeymaps();
    void InitKeymapsVi();
    void InitCommands();
    void InitCommandsVi();
    void PrintKey(const Terminal::KeyInfo& key_info);
    void HandleBracketedPaste(std::string& bracketed_paste_buffer);
    void HandleKey();
    void HandleLeftClick(int s_row, int s_col);
    void HandleRelease(int s_row, int s_col);
    void HandleMouse();
    void HandleResize();

    void Draw();
    void PreProcess();
    void Resize(int width, int height);

    // helper methods
    Window* LocateWindow(int s_col, int s_row);

   private:
    std::unique_ptr<EventLoop> loop_;

    MouseState moust_state_ = MouseState::kReleased;
    Mode mode_;

    BufferManager buffer_manager_;
    KeyseqManager keymap_manager_{mode_};
    CommandManager command_manager_;
    TimerManager timer_manager_;
    std::unique_ptr<SyntaxParser> syntax_parser_;

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

    bool quit_ = false;
    bool ask_quit_ = false;
    static constexpr const char* kAskQuitStr =
        "Some buffers have not saved, force quit(y/[n])? ";

    std::unique_ptr<GlobalOpts> global_opts_;

    // Cmp context
    Mode mode_trigger_cmp_;
    Completer* tmp_completer_ = nullptr;
    bool show_cmp_menu_ = false; // if false, hide cmp menu.

    std::shared_ptr<SingleTimer> autocmp_trigger_timer_;

    size_t count_ = 1;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
