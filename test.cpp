#include "cavl.h"
#include <iostream>

namespace
{
struct Node final : Cavl
{
    explicit Node(const std::string& x) : Cavl{}, value(x) {}
    std::string   value;
    std::uint64_t counter = 0;
};

}  // namespace

int main()
{
    return 0;
}
