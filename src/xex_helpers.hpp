#ifndef X360PORT_XEX_HELPERS_HPP
#define X360PORT_XEX_HELPERS_HPP

#include "x360port/xex_inspect.hpp"

namespace x360port
{

void ScanXexHelpers(const PeImageLayout& image, std::array<std::vector<GuestAddress>, 8>& helpers);

} // namespace x360port

#endif
