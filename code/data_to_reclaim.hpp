#include <atomic>
#include <functional>

template<typename T>
void f(void* p) // 删除器
{
    delete static_cast<T*>(p);
}

struct DataToReclaim {
    void* data;
    std::function<void(void*)> deleter;
    DataToReclaim* next;

    template<typename T>
    DataToReclaim(T* p) : data(p), deleter(&f<T>), next(nullptr) {}

    ~DataToReclaim()
    {
        deleter(data);
    }
};

std::atomic<DataToReclaim*> toDel; // 待删除节点的列表的头节点

void addToDel(DataToReclaim* n)
{
    n->next = toDel.load();
    while(!toDel.compare_exchange_weak(n->next, n));
}

template<typename T> // 这里用模板来实现
void reclaim_later(T* data) // 因为DataToReclaim的构造函数是模板
{
    addToDel(new DataToReclaim(data));
}

void delete_nodes_with_no_hazards() // 释放待删除节点列表中可删除的节点
{
    DataToReclaim* cur = toDel.exchange(nullptr);
    while(cur)
    {
        DataToReclaim* const tmp = cur->next;
        if(!outstanding_hazard_pointers_for(cur->data)) delete cur;
        else addToDel(cur);
        cur = tmp;
    }
}
