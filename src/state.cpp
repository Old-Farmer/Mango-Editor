#include "state.h"

namespace mango {

const std::vector<Mode> kDefaultsModes = {Mode::kEdit};
const std::vector<Mode> kAllModes = {Mode::kEdit, Mode::kPeelCommand,
                                     Mode::kPeelShow};

}  // namespace mango