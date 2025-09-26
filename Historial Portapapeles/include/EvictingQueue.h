#ifndef EVICTINGQUEUE_H
#define EVICTINGQUEUE_H

#include <deque>
using std::deque;
#include <stdexcept>

template <typename T>
class EvictingQueue
{
    public:
        static const size_t NotFound = static_cast<size_t>(-1);

        EvictingQueue(size_t capacity) : m_Capacity(capacity) {}

        /**
         * Inserts s and evicts last element if full.
         * @param s Element to insert
         */
        void PushAndEvictExcess(T s)
        {
            m_Content.push_front(s);

            if (m_Content.size() > m_Capacity)
                m_Content.pop_back();
        }

        void MoveIndexAsFirst(size_t i)
        {
            if (i > m_Content.size())
            {
                throw std::out_of_range("Index is greater than current elements");
            }
            if (i == 0)
                return;

            T value = m_Content[i];
            m_Content.erase(m_Content.begin() + i);
            m_Content.push_front(value);
        }

        size_t IndexOf(T item)
        {
            for (typename deque<T>::const_iterator it = m_Content.begin(); it != m_Content.end(); ++it)
            {
                if (*it == item)
                    return it - m_Content.begin();
            }
            return NotFound;
        }

        bool Contains(T item)
        {
            return IndexOf(item) != NotFound;
        }

        T &operator[](size_t i)
        {
            if (i >= m_Content.size())
                throw std::out_of_range("Index out of range");
            return m_Content[i];
        }

        const T &operator[](size_t i) const
        {
            if (i >= m_Content.size())
                throw std::out_of_range("Index out of range");
            return m_Content[i];
        }

        void SetCapacity(size_t capacity)
        {
            if (capacity < m_Content.size())
            {
                throw std::out_of_range("More elements than new capacity");
            }
            m_Capacity = capacity;
        }

        size_t Capacity() const
        {
            return m_Capacity;
        }

        size_t Size() const
        {
            return m_Content.size();
        }

        typename deque<T>::iterator begin() { return m_Content.begin(); }
        typename deque<T>::iterator end()   { return m_Content.end(); }

        typename deque<T>::const_iterator begin() const { return m_Content.begin(); }
        typename deque<T>::const_iterator end()   const { return m_Content.end(); }

    private:
        deque<T> m_Content;
        size_t m_Capacity;
};

#endif // EVICTINGQUEUE_H

