#pragma once

#include <memory>

#include "buffer.h"
#include "buffer_manager.h"
#include "cmp_menu.h"
#include "command_manager.h"
#include "cursor.h"
#include "keyseq_manager.h"
#include "mango_peel.h"
#include "state.h"
#include "status_line.h"
#include "syntax.h"
#include "utils.h"
#include "window.h"

namespace mango {

class InitOptions;
class Options;

class Editor {
   private:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(Editor);

   public:
    MGO_DELETE_COPY(Editor);
    MGO_DELETE_MOVE(Editor);

    // Make sure that options is static lifetime
    // throws TermException
    void Loop(std::unique_ptr<Options> options,
              std::unique_ptr<InitOptions> init_options);

    static Editor& GetInstance();

    void Help();
    void Quit();

    void GotoPeel();
    void ExitFromMode();
    void TriggerCmp(std::function<void(size_t)> accept_call_back,
                    std::function<void(void)> cancel_call_back,
                    std::vector<std::string> entries);
    void SearchNext();
    void SearchPrev();

   private:
    void InitKeymaps();
    void InitCommands();
    void HandleKey(const Terminal::Event& e,
                   std::string* bracketed_paste_buffer);
    void HandleLeftClick(int s_row, int s_col);
    void HandleRelease(int s_row, int s_col);
    void HandleMouse(const Terminal::Event& e);
    void HandleResize(const Terminal::Event& e);

    void Draw();
    void PreProcess();
    void Resize(int width, int height);

    // helper methods
    Window* LocateWindow(int s_col, int s_row);

   private:
    MouseState moust_state_ = MouseState::kReleased;
    Mode mode_ = Mode::kEdit;

    BufferManager buffer_manager_;
    KeyseqManager keymap_manager_{mode_};
    CommandManager command_manager_;
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

    Cursor cursor_;
    // Now only support one window in the screen
    // TODO: mutiple window logic
    std::unique_ptr<Window> window_;
    std::unique_ptr<StatusLine> status_line_;
    std::unique_ptr<MangoPeel> peel_;
    std::unique_ptr<CmpMenu> cmp_menu_;

    bool quit_ = false;
    bool have_event_ = true;

    std::unique_ptr<Options> options_;

    // Cmp context
    std::function<void(size_t)> cmp_accept_callback_ = nullptr;
    std::function<void()> cmp_cancel_callback_ = nullptr;
    Mode mode_trigger_cmp_;

    Terminal& term_ = Terminal::GetInstance();
};

}  // namespace mango
