#ifndef EVICTINGQUEUE_H
#define EVICTINGQUEUE_H

#include <string>
#include <deque>

using std::deque;
using std::wstring;

class EvictingQueue
{
    public:
        static const size_t NotFound = static_cast<size_t>(-1);

        EvictingQueue(size_t capacity) : m_Capacity(capacity) {}

        void PushAndEvictExcess(wstring s)
        {
            m_Content.push_front(s);

            if (m_Content.size() > m_Capacity)
                m_Content.pop_back();
        }

        size_t IndexOf(wstring text)
        {
            for (deque<wstring>::const_iterator it = m_Content.begin(); it != m_Content.end(); ++it)
            {
                if (*it == text)
                    return it - m_Content.begin();
            }
            return NotFound;
        }

        bool Contains(wstring text)
        {
            return IndexOf(text) != NotFound;
        }

        std::wstring &operator[](size_t i)
        {
            if (i >= m_Content.size())
                throw std::out_of_range("Index out of range");
            return m_Content[i];
        }

        const std::wstring &operator[](size_t i) const
        {
            if (i >= m_Content.size())
                throw std::out_of_range("Index out of range");
            return m_Content[i];
        }

        void setCapacity(size_t capacity)
        {
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

        std::deque<std::wstring>::iterator begin() { return m_Content.begin(); }
        std::deque<std::wstring>::iterator end() { return m_Content.end(); }

        std::deque<std::wstring>::const_iterator begin() const { return m_Content.begin(); }
        std::deque<std::wstring>::const_iterator end() const { return m_Content.end(); }

    private:
        std::deque<std::wstring> m_Content;
        size_t m_Capacity;
};

#endif // EVICTINGQUEUE_H

