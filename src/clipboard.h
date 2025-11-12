#pragma once

#include <memory>
#include <string>

#include "utils.h"
namespace mango {

// An ClipBoard Interface
class ClipBoard {
   public:
    virtual std::string GetContent(bool& lines) const = 0;
    virtual void SetContent(std::string content, bool lines) = 0;
    virtual ~ClipBoard() = default;

    // Create a clipboard. If prefer_system is true, it will detect and use the
    // OS clipboard.
    static std::unique_ptr<ClipBoard> CreateClipBoard(bool prefer_system);
};

// Default clipboard is a simplest clipboard that only stores content in the
// process and will not communicates with outside world.
class DefaultClipBoard : public ClipBoard {
   public:
    MGO_DEFAULT_CONSTRUCT_DESTRUCT(DefaultClipBoard);
    MGO_DELETE_COPY(DefaultClipBoard);
    MGO_DELETE_MOVE(DefaultClipBoard);
    virtual std::string GetContent(bool& lines) const;
    virtual void SetContent(std::string content, bool lines);

   private:
    bool lines_;
    std::string content_;
};

// Use Xsel: https://github.com/kfish/xsel
class XClipBoard : public ClipBoard {
   public:
    XClipBoard();
    MGO_DELETE_COPY(XClipBoard);
    MGO_DELETE_MOVE(XClipBoard);
    virtual std::string GetContent(bool& lines) const;
    virtual void SetContent(std::string content, bool lines);

    static bool DetectUsable();

   private:
    static void WslFilterCharacter(std::string& content);

   private:
    bool lines_;
    bool in_wsl;
};

}  // namespace mango