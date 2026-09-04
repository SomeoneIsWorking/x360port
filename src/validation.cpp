#include "x360port/validation.hpp"

#include <array>

namespace x360port
{

std::string_view ToString(ValidationError error) noexcept
{
    constexpr std::array Names = {
        "none",
        "invalid-image-digest",
        "image-size-mismatch",
        "image-address-overflow",
        "image-digest-mismatch",
        "invalid-code-range",
        "entry-point-outside-code",
        "import-count-mismatch",
        "invalid-import",
        "import-ordinal-out-of-range",
        "import-address-conflict",
        "unsorted-import-manifest",
        "import-manifest-digest-mismatch",
        "import-binding-count-mismatch",
        "import-binding-mismatch",
        "import-binding-kind-mismatch",
        "import-binding-callback-mismatch",
    };
    const std::size_t index = static_cast<std::size_t>(error);
    return index < Names.size() ? Names[index] : "unknown";
}

} // namespace x360port
