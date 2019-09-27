#include "hazard_pointer.hpp"
#include "data_to_reclaim.hpp"
#include <atomic>
#include <memory>

template<typename T>
class lock_free_stack {
    struct node {
        std::shared_ptr<T> val;
        node* next;
        node(const T& x) : val(std::make_shared<T>(x)) {}
    };
    std::atomic<node*> head;
    std::atomic<unsigned> cnt; // 调用pop的线程数
    std::atomic<node*> toDel; // 待删除节点的列表的头节点
public:
    void push(const T& x)
    {
        const auto newNode = new node(x);
        newNode->next = head.load();
        while(!head.compare_exchange_weak(newNode->next, newNode));
    }
    std::shared_ptr<T> pop()
    {
        ++cnt; // 调用pop的线程数加一，表示oldHead正被持有，保证可以被解引用
        node* oldHead = head.load();
        while(oldHead && !head.compare_exchange_weak(oldHead, oldHead->next));
        std::shared_ptr<T> res;
        if(oldHead) res.swap(oldHead->val); // oldHead一定可以解引用，oldHead->val设为nullptr
        try_reclaim(oldHead); // 如果计数器为1则释放oldHead，否则添加到待删除列表中
        return res; // res保存了oldHead->val
    }
private:
    static void deleteNodes(node* n) // 释放n及之后的所有节点
    {
        while(n)
        {
            node* tmp = n->next;
            delete n;
            n = tmp;
        }
    }
    void try_reclaim(node* oldHead)
    {
        if(cnt == 1) // 调用pop的线程数为1则可以进行释放
        {
            // exchange返回toDel值，即待删除列表的头节点，再将toDel设为nullptr
            node* n = toDel.exchange(nullptr); // 获取待删除列表的头节点
            if(--cnt == 0) deleteNodes(n); // 没有其他线程，则释放待删除列表中所有节点
            else if(n) addToDel(n); // 如果多于一个线程则继续保存到待删除列表
            delete oldHead; // 删除传入的节点
        }
        else // 调用pop的线程数超过1，添加当前节点到待删除列表
        {
            addToDel(oldHead, oldHead);
            --cnt;
        }
    }
    void addToDel(node* n) // 把n及之后的节点置于待删除列表之前
    {
        node* last = n;
        while(const auto tmp = last->next) last = tmp; // last指向尾部
        addToDel(n, last); // 添加从n至last的所有节点到待删除列表
    }
    void addToDel(node* first, node* last)
    {
        last->next = toDel; // 链接到已有的待删除列表之前
        // 确保最后last->next为toDel，再将toDel设为first，first即新的头节点
        while(!toDel.compare_exchange_weak(last->next, first));
    }
};