#pragma once


namespace TXP
{

class Skeletal_animator
{
public:
    // @TODO: put public funcs here.


private:
    void* m_animator;

    /// Private ctor.
    Skeletal_animator(void* internal_animator);

    friend class Renderer;
};

}  // namespace TXP
