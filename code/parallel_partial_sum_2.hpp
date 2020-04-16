#include "threads_guard.hpp"
#include "barrier.hpp"
#include <iterator>
#include <functional>
#include <vector>
#include <thread>

template<typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last)
{
    using value_type = typename Iterator::value_type;
    struct process_element {
        void operator()(Iterator first, Iterator last, std::vector<value_type>& v, unsigned i, barrier& b)
        {
            value_type& ith_element = *(first + i);
            bool update_source = false;
            for (unsigned step = 0, stride = 1; stride <= i; ++step, stride *= 2)
            {
                const value_type& source = step % 2 ? v[i] : ith_element;
                value_type& dest = step % 2 ? ith_element : v[i];
                const value_type& addend = step % 2 ? v[i - stride] : *(first + i - stride);
                dest = source + addend;
                update_source = !(step % 2);
                b.wait();
            }
            if (update_source) ith_element = v[i];
            b.done_waiting();
        }
    };
    const unsigned long len = std::distance(first, last);
    if (len <= 1) return;
    std::vector<value_type> v(len);
    barrier b(len);
    std::vector<std::thread> threads(len - 1);
    threads_guard g(threads);
    Iterator block_start = first;
    for (unsigned long i = 0; i < len - 1; ++i)
    {
        threads[i] = std::thread(process_element{}, first, last, std::ref(v), i, std::ref(b));
    }
    process_element{}(first, last, v, len - 1, b);
}