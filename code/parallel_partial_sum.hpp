#include "threads_guard.hpp"
#include <iterator>
#include <numeric>
#include <algorithm>
#include <vector>
#include <thread>
#include <future>

template<typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last)
{
    using value_type = typename Iterator::value_type;
    struct process_chunk {
        void operator()(Iterator begin, Iterator last,
            std::future<value_type>* previous_end_value,
            std::promise<value_type>* end_value)
        {
            try
            {
                Iterator end = last;
                ++end;
                std::partial_sum(begin, end, begin);
                if(previous_end_value) // 不是第一个块
                {
                    value_type addend = previous_end_value->get();
                    *last += addend;
                    if(end_value) end_value->set_value(*last);
                    std::for_each(begin, last, [addend](value_type& item) { item += addend; });
                }
                else if(end_value) end_value->set_value(*last); // 是第一个块则可以为下个块更新尾元素
            }
            catch(...)
            {
                // 如果抛出异常则存储到std::promise，异常会传播给下一个块（获取这个块的尾元素时）
                if(end_value) end_value->set_exception(std::current_exception());
                else throw; // 异常最终传给最后一个块，此时再抛出异常
            }
        }
    };
    const unsigned long len = std::distance(first, last);
    if(!len) return;
    const unsigned long min_per_thread = 25;
    const unsigned long max_threads = (len + min_per_thread - 1) / min_per_thread;
    const unsigned long hardware_threads = std::thread::hardware_concurrency();
    const unsigned long num_threads =
        std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);
    const unsigned long block_size = len / num_threads;
    std::vector<std::thread> threads(num_threads - 1);
    std::vector<std::promise<value_type>> end_values(num_threads - 1); // 存储块内尾元素值
    std::vector<std::future<value_type>> previous_end_values; // 检索前一个块的尾元素
    previous_end_values.reserve(num_threads - 1);
    threads_guard g(threads);
    Iterator block_start = first;
    for(unsigned long i = 0; i < num_threads - 1; ++i)
    {
        Iterator block_last = block_start;
        std::advance(block_last, block_size - 1); // 指向尾元素
        threads[i] = std::thread(process_chunk{},
            block_start, block_last,
            i != 0 ? &previous_end_values[i - 1] : nullptr,
            &end_values[i]);
        block_start = block_last;
        ++block_start;
        previous_end_values.emplace_back(end_values[i].get_future());
    }
    Iterator final_element = block_start;
    std::advance(final_element, std::distance(block_start, last) - 1);
    process_chunk{}(block_start, final_element,
        num_threads > 1 ? &previous_end_values.back() : nullptr,
        nullptr);
}