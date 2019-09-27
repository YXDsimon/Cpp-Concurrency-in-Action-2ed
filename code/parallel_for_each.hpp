#include "threads_guard.hpp"
#include <iterator>
#include <algorithm>
#include <vector>
#include <thread>
#include <future>
#include <utility>

template<typename Iterator, typename Func>
void parallel_for_each(Iterator first, Iterator last, Func f)
{
    const unsigned long len = std::distance(first, last);
    if(!len) return;
    const unsigned long min_per_thread = 25;
    const unsigned long max_threads = (len + min_per_thread - 1) / min_per_thread;
    const unsigned long hardware_threads = std::thread::hardware_concurrency();
    const unsigned long num_threads =
        std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    const unsigned long block_size = len / num_threads;
    std::vector<std::future<void>> fts(num_threads - 1);
    std::vector<std::thread> threads(num_threads - 1);
    threads_guard g(threads);
    Iterator block_start = first;
    for(unsigned long i = 0; i < num_threads - 1; ++i)
    {
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        std::packaged_task<void(void)> pt([=] { std::for_each(block_start, block_end, f); });
        fts[i] = pt.get_future();
        threads[i] = std::thread(std::move(pt));
        block_start = block_end;
    }
    std::for_each(block_start, last, f);
    for(unsigned long i = 0; i < num_threads - 1; ++i) fts[i].get(); // 只是为了传递异常
}