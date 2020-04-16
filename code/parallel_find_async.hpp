#include <iterator>
#include <functional>
#include <atomic>
#include <future>

template<typename Iterator, typename T>
Iterator parallel_find_impl(Iterator first, Iterator last, T match, std::atomic<bool>& done_flag)
{
    try
    {
        const unsigned long len = std::distance(first, last);
        const unsigned long min_per_thread = 25;
        if (len < (2 * min_per_thread))
        {
            for (; first != last && !done_flag.load(); ++first)
            {
                if (*first == match)
                {
                    done_flag = true;
                    return first;
                }
            }
            return last; 
        }
        else
        {
            const Iterator mid_point = first + len / 2;
            std::future<Iterator> async_res =
                std::async(&parallel_find_impl<Iterator, T>, mid_point, last, match, std::ref(done_flag));
            const Iterator direct_res = parallel_find_impl(first, mid_point, match, done_flag);
            return direct_res == mid_point ? async_res.get() : direct_res;
        }
    }
    catch(...)
    {
        done_flag = true;
        throw;
    }
}

template<typename Iterator, typename T>
Iterator parallel_find(Iterator first, Iterator last, T match)
{
    std::atomic<bool> done_flag(false);
    return parallel_find_impl(first, last, match, done_flag);
}