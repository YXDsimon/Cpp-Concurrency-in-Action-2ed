#include "lock_based_stack.hpp"
#include <thread>
#include <future>
#include <atomic>
#include <vector>
#include <list>
#include <algorithm>
#include <memory>
#include <utility>

template<typename T>
struct sorter
{
    struct chunk_to_sort
    {
        std::list<T> data;
        std::promise<std::list<T>> promise;
    };

    thread_safe_stack<chunk_to_sort> chunks;
    std::vector<std::thread> threads;
    const unsigned max_thread_count;
    std::atomic<bool> end_of_data;

    sorter() : max_thread_count(std::thread::hardware_concurrency() - 1), end_of_data(false) {}

    ~sorter()
    {
        end_of_data = true;
        for (unsigned i = 0; i < threads.size(); ++i) threads[i].join();
    }

    void try_sort_chunk()
    {
        std::shared_ptr<chunk_to_sort> chunk = chunks.pop();
        if (chunk) sort_chunk(chunk);
    }

    std::list<T> do_sort(std::list<T>& v)
    {
        if (v.empty()) return v;
        std::list<T> res;
        res.splice(res.begin(), v, v.begin());
        const T& firstVal = *res.begin();
        auto it = std::partition(v.begin(), v.end(), [&](const T& val) { return val < firstVal; });
        chunk_to_sort low;
        low.data.splice(low.data.end(), v, v.begin(), it);
        std::future<std::list<T>> l = low.promise.get_future();
        chunks.push(std::move(low));
        if (threads.size() < max_thread_count)
        {
            threads.emplace_back(&sorter<T>::sort_thread, this);
        }
        auto r(do_sort(v));
        res.splice(res.end(), r);
        while (l.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            try_sort_chunk();
        }
        res.splice(res.begin(), l.get());
        return res;
    }

    void sort_chunk(const std::shared_ptr<chunk_to_sort>& chunk)
    {
        chunk->promise.set_value(do_sort(chunk->data));
    }

    void sort_thread()
    {
        while (!end_of_data)
        {
            try_sort_chunk();
            std::this_thread::yield();
        }
    }
};

template<typename T>
std::list<T> parallel_quick_sort(std::list<T> v)
{
    if (v.empty()) return v;
    sorter<T> s;
    return s.do_sort(v);
}