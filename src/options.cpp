#include "options.h"

namespace mango {
Options::Options() {
    // NOTE: Change carefully
    // See Terminall::Init
    // TODO: better color control
    attr_table.resize(__kCharacterTypeCount);

    attr_table[kNormal] = {Terminal::kDefault, Terminal::kDefault};
    attr_table[kReverse] = {Terminal::kDefault | Terminal::kReverse,
                            Terminal::kDefault | Terminal::kReverse};
    attr_table[kSelection] = {Terminal::kDefault | Terminal::kReverse,
                              Terminal::kDefault | Terminal::kReverse};
    attr_table[kMenu] = {Terminal::kDefault, Terminal::kMagenta};

    attr_table[kKeyword] = {Terminal::kBlue, Terminal::kDefault};
    attr_table[kTypeBuiltin] = {Terminal::kBlue, Terminal::kDefault};
    attr_table[kOperator] = {Terminal::kBlue, Terminal::kDefault};
    attr_table[kString] = {Terminal::kGreen, Terminal::kDefault};
    attr_table[kComment] = {Terminal::kCyan | Terminal::kItalic,
                            Terminal::kDefault};
    attr_table[kNumber] = {Terminal::kYellow,
                           Terminal::kDefault};
    attr_table[kConstant] = {Terminal::kYellow, Terminal::kDefault};
    attr_table[kFunction] = {Terminal::kDefault, Terminal::kDefault};
    attr_table[kType] = {Terminal::kMagenta, Terminal::kDefault};
    attr_table[kVariable] = {Terminal::kDefault, Terminal::kDefault};
    attr_table[kDelimiter] = {Terminal::kDefault, Terminal::kDefault};
    attr_table[kProperty] = {Terminal::kDefault, Terminal::kDefault};
    attr_table[kLabel] = {Terminal::kDefault, Terminal::kDefault};
}
}  // namespace mango