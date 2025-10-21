#include "state.h"

namespace mango {

const std::vector<Mode> kDefaultsModes = {Mode::kEdit, Mode::kFind};
const std::vector<Mode> kAllModes = {Mode::kEdit, Mode::kFind,
                                     Mode::kPeelCommand, Mode::kPeelShow};

}  // namespace mango