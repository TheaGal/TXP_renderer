#pragma once

#include <mutex>


namespace BT
{

/// Template class to wrap a mutex around a type.
template<typename T>
class Mutex_wrapper
{
public:
    /// Lock guard helper class.
    class Lock_guard
    {
    public:
        Lock_guard(std::mutex& m, T& d)
            : m_lock(m)
            , m_data(d)
        {
        }

        T* operator->()
        {
            return &m_data;
        }

        T& operator*()
        {
            return m_data;
        }

        T* get()
        {
            return &m_data;
        }

    private:
        std::lock_guard<std::mutex> m_lock;
        T& m_data;
    };

    /// Creates lock guard that contains the data.
    Lock_guard scoped_lock()
    {
        return Lock_guard(m_mutex, m_data);
    }

private:
    mutable std::mutex m_mutex;
    T m_data;
};

}  // namespace BT
