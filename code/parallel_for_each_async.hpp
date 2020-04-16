#include <iterator>
#include <algorithm>
#include <future>

template<typename Iterator, typename Func>
void parallel_for_each(Iterator first, Iterator last, Func f)
{
    const unsigned long len = std::distance(first, last);
    if (!len) return;
    const unsigned long min_per_thread = 25;
    if (len < 2 * min_per_thread)
    {
        std::for_each(first, last, f);
    }
    else
    {
        const Iterator mid_point = first + len / 2;
        std::future<void> first_half = std::async(&parallel_for_each<Iterator, Func>, first, mid_point, f);
        parallel_for_each(mid_point, last, f);
        first_half.get();
    }
}