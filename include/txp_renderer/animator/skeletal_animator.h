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
    Skeletal_animator(void* internal_animator);  // @TODO: add reference counting for this!!! (pay attention to move ctors and dtors, or delete move/copy ctors).

    friend class Renderer;
};

}  // namespace TXP
