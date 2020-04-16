#include "threads_guard.hpp"
#include <iterator>
#include <algorithm>
#include <vector>
#include <atomic>
#include <thread>
#include <future>
#include <exception>

template<typename Iterator, typename T>
Iterator parallel_find(Iterator first, Iterator last, T match)
{
    struct find_element {
        void operator()(Iterator begin, Iterator end, T match,
            std::promise<Iterator>* res, std::atomic<bool>* done_flag)
        {
            try
            {
                for (; begin != end && !done_flag->load(); ++begin)
                {
                    if (*begin == match)
                    {
                        res->set_value(begin);
                        done_flag->store(true);
                        return;
                    }
                }
            }
            catch(...)
            {
                try
                {
                    res->set_exception(std::current_exception());
                    done_flag->store(true);
                }
                catch(...){}
            }
        }
    };
    const unsigned long len = std::distance(first, last);
    if (!len) return last;
    const unsigned long min_per_thread = 25;
    const unsigned long max_threads = (len + min_per_thread - 1) / min_per_thread;
    const unsigned long hardware_threads = std::thread::hardware_concurrency();
    const unsigned long num_threads =
        std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    const unsigned long block_size = len / num_threads;
    std::promise<Iterator> res;
    std::atomic<bool> done_flag(false);
    std::vector<std::thread> threads(num_threads - 1);
    {
        threads_guard g(threads);
        Iterator block_start = first;
        for (unsigned long i = 0; i < num_threads - 1; ++i)
        {
            Iterator block_end = block_start;
            std::advance(block_end, block_size);
            threads[i] = std::thread(find_element{}, block_start, block_end, match, &res, &done_flag);
            block_start = block_end;
        }
        find_element()(block_start, last, match, &res, &done_flag);
    }
    if (!done_flag.load()) return last;
    return res.get_future().get();
}