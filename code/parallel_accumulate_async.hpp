#include <iterator>
#include <numeric>
#include <future>

template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
    const unsigned long len = std::distance(first, last);
    const unsigned long max_chunk_size = 25;
    if (len <= max_chunk_size)
    {
        return std::accumulate(first, last, init);
    }
    else
    {
        Iterator mid_point = first;
        std::advance(mid_point, len / 2);
        std::future<T> first_half_res = std::async(parallel_accumulate<Iterator, T>, first, mid_point, init);
        // 递归调用如果抛出异常，由std::async创建的std::future将在异常传播时被析构
        T second_half_res = parallel_accumulate(mid_point, last, T{});
        // 如果异步任务抛出异常，get就会捕获异常并重新抛出
        return first_half_res.get() + second_half_res;
    }
}