#include "utils.h"

namespace VoidEngine
{

template<typename T, uint32_t N>
class FixedQueue
{
private:
    T m_data[N];
    uint32_t m_head;
    uint32_t m_tail;
public:
    FixedQueue() : m_head(0), m_tail(0)
    {
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible!");
        static_assert(std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>,
                "T must be move or copy constructible!");
        static_assert(std::is_destructible_v<T>, "T must be destructible!");
        ENGINE_ASSERT(is_power_of_2(N), "Capacity must be power of 2");
    }

    template<typename U>
    bool try_push(U&& data)
    {
        if(get_count() == N - 1)
        {
            return false;
        }

        m_data[m_tail] = std::forward<U>(data);
        m_tail = (m_tail + 1) & (N - 1);

        return true;
    }

    bool try_pop(T& out)
    {
        if(is_empty())
        {
            return false;
        }

        out = std::move(m_data[m_head]);
        m_head = (m_head + 1) & (N - 1);

        return true;
    }

    void pop()
    {
        if(is_empty())
        {
            return;
        }

        m_head = (m_head + 1) & (N - 1);
    }

    uint32_t get_count() const
    {
        return m_tail >= m_head ? (m_tail - m_head) : (N - m_head + m_tail);
    }

    bool is_empty() const
    {
        return m_tail == m_head;
    }

    static constexpr uint32_t get_capacity()
    {
        return N;
    }

    T& front()
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");

        return m_data[m_head];
    }

    T& back()
    {
        ENGINE_ASSERT(!is_empty(), "Queue is empty!");

        return m_data[(m_tail - 1) & (N -1)];
    }
};

}
