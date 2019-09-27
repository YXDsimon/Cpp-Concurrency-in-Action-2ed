#include <list>
#include <algorithm>
#include <future>
#include <utility>

template<typename T>
std::list<T> parallel_quick_sort(std::list<T> v)
{
    if(v.empty()) return v;
    std::list<T> res;
    res.splice(res.begin(), v, v.begin());
    const T& firstVal = *res.begin();
    auto it = std::partition(v.begin(), v.end(), [&](const T& x) { return x < firstVal; });
    std::list<T> low;
    low.splice(low.end(), v, v.begin(), it);
    std::future<std::list<T>> l(std::async(&parallel_quick_sort<T>, std::move(low)));
    auto r(parallel_quick_sort(std::move(v)));
    res.splice(res.end(), r);
    res.splice(res.begin(), l.get());
    return res;
}