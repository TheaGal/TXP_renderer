#pragma once

#include <cstdint>
#include <memory>
#include <string>


namespace TXP
{

/// Key to access editing 
using pool_key_t = std::uint32_t;

/// Engine that handles render processes and presenting.
class Renderer
{
public:
    Renderer(std::string const& title);
    ~Renderer();  // For pimpl.

    // vv UNSURE vv


private:
    struct Impl;
    std::unique_ptr<Impl> m_pimpl;
};

}  // namespace TXP
