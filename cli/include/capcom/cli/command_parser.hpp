#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace capcom::cli
{

    enum class CommandType
    {
        help,
        version,
        init,
        create,
        link,
        unlink,
        tree,
        scan,
        status,
        validate,
        test_import,
        verify,
        unknown
    };

    struct Command
    {
        CommandType type{CommandType::unknown};
        std::vector<std::string> arguments;
    };

    class CommandParser final
    {
    public:
        [[nodiscard]] Command parse(
            int argc,
            const char *const argv[]) const;

        [[nodiscard]] static std::string_view help_text() noexcept;
    };

} // namespace capcom::cli
