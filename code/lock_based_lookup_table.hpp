#include <memory>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <list>
#include <utility> // for std::pair
#include <algorithm> // for std::find_if
#include <functional> // for std::hash

template<typename K, typename V, typename Hash = std::hash<K>>
class thread_safe_lookup_table {
    class Bucket {
    public:
        std::list<std::pair<K, V>> data;
        mutable std::shared_mutex m; // 每个桶都用这个锁保护

        V value_for(const K& k, const V& v) const // 如果未找到则返回v
        { // 没有修改任何值，异常安全
            std::shared_lock<std::shared_mutex> l(m); // 只读锁，可共享
            auto it = std::find_if(data.begin(), data.end(), [&](auto& x) { return x.first == k; });
            return it == data.end() ? v : it->second;
        }
        
        void add_or_update_mapping(const K& k, const V& v) // 找到则修改，未找到则添加
        {
            std::unique_lock<std::shared_mutex> l(m); // 写，单独占用
            auto it = std::find_if(data.begin(), data.end(), [&](auto& x) { return x.first == k; });
            if (it == data.end())
            {
                data.emplace_back(k, v); // emplace_back异常安全
            }
            else
            {
                it->second = v; // 赋值可能抛异常，但值是用户提供的，可放心让用户处理
            }
        }
        
        void remove_mapping(const K& k)
        { // std::list::erase不会抛异常，因此异常安全
            std::unique_lock<std::shared_mutex> l(m); // 写，单独占用
            auto it = std::find_if(data.begin(), data.end(), [&](auto& x) { return x.first == k; });
            if (it != data.end()) data.erase(it);
        }
    };
    
    std::vector<std::unique_ptr<Bucket>> buckets;
    Hash hasher;
    Bucket& get_bucket(const K& k) const
    { // 桶数固定因此可以无锁调用
        return *buckets[hasher(k) % buckets.size()];
    }
public:
    // 桶的默认数量为19（一般用x%桶数决定放置x的桶的索引，桶数为质数可以使得桶分布均匀）
    thread_safe_lookup_table(unsigned n = 19, const Hash& h = Hash{}) : buckets(n), hasher(h)
    {
        for (auto& x : buckets) x.reset(new Bucket);
    }
    thread_safe_lookup_table(const thread_safe_lookup_table&) = delete;
    thread_safe_lookup_table& operator=(const thread_safe_lookup_table&) = delete;
    V value_for(const K& k, const V& v = V{}) const
    {
        return get_bucket(k).value_for(k, v);
    }
    
    void add_or_update_mapping(const K& k, const V& v)
    {
        get_bucket(k).add_or_update_mapping(k, v);
    }
    
    void remove_mapping(const K& k)
    {
        get_bucket(k).remove_mapping(k);
    }
    // 为了方便使用，提供一个到std::map的映射
    std::map<K, V> get_map() const
    {
        std::vector<std::unique_lock<std::shared_mutex>> l;
        for (auto& x : buckets)
        {
            l.push_back(std::unique_lock<std::shared_mutex>(x->m));
        }
        std::map<K, V> res;
        for (auto& x : buckets)
        {
            for (auto& y : x->data) res.insert(y);
        }
        return res;
    }
};