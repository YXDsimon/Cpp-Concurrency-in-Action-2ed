#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <utility>

// 简单的消息队列
namespace messaging
{
    struct message_base
    {
        virtual ~message_base() = default;
    };

    template<typename Msg>
    struct wrapped_message : message_base
    {
        Msg contents;
        explicit wrapped_message(const Msg& contents_) : contents(contents_) {}
    };

    class queue
    {
        std::mutex m;
        std::condition_variable c;
        std::queue<std::shared_ptr<message_base>> q; // 实际存储指向message_base的指针
    public:
        template<typename T>
        void push(const T& msg)
        {
            std::lock_guard<std::mutex> lk(m);
            q.push(std::make_shared<wrapped_message<T>>(msg));
            c.notify_all();
        }
        std::shared_ptr<message_base> wait_and_pop()
        {
            std::unique_lock<std::mutex> lk(m);
            c.wait(lk, [&] { return !q.empty(); }); // 队列为空时阻塞
            auto res = q.front();
            q.pop();
            return res;
        }
    };
}

//  TemplateDispatcher
namespace messaging
{
    template<typename PreviousDispatcher, typename Msg, typename Func>
    class TemplateDispatcher
    {
        queue* q;
        PreviousDispatcher* prev;
        Func f;
        bool chained;
        template<typename Dispatcher, typename OtherMsg, typename OtherFunc>
        friend class TemplateDispatcher; // TemplateDispatcher实例互为友元
        void wait_and_dispatch()
        {
            for (;;)
            {
                auto msg = q->wait_and_pop();
                if (dispatch(msg)) break; // 消息被处理后则退出循环
            }
        }

        bool dispatch(const std::shared_ptr<message_base>& msg)
        {
            if (wrapped_message<Msg>* wrapper = dynamic_cast<wrapped_message<Msg>*>(msg.get()))
            { // message_base没有任何成员，要先转为派生类wrapped_message才能访问contents
                f(wrapper->contents);
                return true;
            }
            else
            { // 如果消息类型不匹配，则链接到前一个dispatcher
                return prev->dispatch(msg);
            }
        }
    public:
        TemplateDispatcher(const TemplateDispatcher&) = delete;
        TemplateDispatcher& operator=(const TemplateDispatcher&) = delete;

        TemplateDispatcher(TemplateDispatcher&& other) noexcept
        : q(other.q), prev(other.prev), f(std::move(other.f)), chained(other.chained)
        {
            other.chained = true;
        }

        TemplateDispatcher(queue* q_, PreviousDispatcher* prev_, Func&& f_)
        : q(q_), prev(prev_), f(std::forward<Func>(f_)), chained(false)
        {
            prev_->chained = true;
        }

        template<typename OtherMsg, typename OtherFunc>
        TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc> handle(OtherFunc&& of)
        {
            return TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>(q, this, std::forward<OtherFunc>(of));
        }

        ~TemplateDispatcher() noexcept(false) // 所有调度器都可能抛出异常
        {
            if (!chained) wait_and_dispatch(); // 析构函数中完成任务调度
        }
    };
}

// dispatcher
namespace messaging
{
    class close_queue {}; // 用于关闭队列的消息
    class dispatcher
    {
        queue* q;
        bool chained;
        template<typename Dispatcher, typename Msg, typename Func>
        friend class TemplateDispatcher; // 允许TemplateDispatcher访问内部成员
        void wait_and_dispatch()
        {
            for (;;)
            {
                auto msg = q->wait_and_pop();
                dispatch(msg);
            }
        }
        bool dispatch(const std::shared_ptr<message_base>& msg)
        { // 检查消息是否为close_queue，如果是则抛出异常，否则返回false表示消息未被处理
            if (dynamic_cast<wrapped_message<close_queue>*>(msg.get()))
            {
                throw close_queue();
            }
            return false;
        }
    public:
        dispatcher(const dispatcher&) = delete;
        dispatcher& operator=(const dispatcher&) = delete;
        dispatcher(dispatcher&& other) noexcept : q(other.q), chained(other.chained)
        {
            other.chained = true;
        }
        explicit dispatcher(queue* q_) : q(q_), chained(false) {}
        template<typename Message, typename Func>
        TemplateDispatcher<dispatcher, Message, Func> handle(Func&& f)
        { // 用TemplateDispatcher处理特定类型的消息
            return TemplateDispatcher<dispatcher, Message, Func>(q, this, std::forward<Func>(f));
        }
        ~dispatcher() noexcept(false) // bool dispatch()可能抛出close_queue异常
        { // 从receiver::wait返回的dispatcher实例会马上被析构
            if (!chained) wait_and_dispatch(); // 析构函数中完成任务调度
        }
    };
}

// sender：messaging::queue*的包裹类，保存指针而非队列
namespace messaging
{
    class sender
    {
        queue* q;
    public:
        sender() : q(nullptr) {}
        explicit sender(queue* q_) : q(q_) {}
        template<typename Message>
        void send(const Message& msg)
        {
            if (q)  q->push(msg);
        }
    };
}

// receiver：拥有一个messaging::queue
namespace messaging
{
    class receiver
    {
        queue q;
    public:
        operator sender() // 允许将receiver的q隐式转换为sender
        {
            return sender(&q);
        }
        dispatcher wait() // 等待对队列的调度
        {
            return dispatcher(&q);
        }
    };
}

// ATM消息
struct withdraw
{
    std::string account;
    unsigned amount;
    mutable messaging::sender atm_queue;
    withdraw(const std::string& account_, unsigned amount_, messaging::sender atm_queue_)
    : account(account_), amount(amount_), atm_queue(atm_queue_) {}
};

struct withdraw_ok {};

struct withdraw_denied {};

struct cancel_withdrawal
{
    std::string account;
    unsigned amount;
    cancel_withdrawal(const std::string& account_, unsigned amount_) : account(account_), amount(amount_) {}
};

struct withdrawal_processed
{
    std::string account;
    unsigned amount;
    withdrawal_processed(const std::string& account_, unsigned amount_) : account(account_), amount(amount_) {}
};

struct card_inserted
{
    std::string account;
    explicit card_inserted(const std::string& account_) : account(account_) {}
};

struct digit_pressed
{
    char digit;
    explicit digit_pressed(char digit_) : digit(digit_) {}
};

struct clear_last_pressed {};

struct eject_card {};

struct withdraw_pressed
{
    unsigned amount;
    explicit withdraw_pressed(unsigned amount_) : amount(amount_) {}
};

struct cancel_pressed {};

struct issue_money
{
    unsigned amount;
    issue_money(unsigned amount_) : amount(amount_) {}
};

struct verify_pin
{
    std::string account;
    std::string pin;
    mutable messaging::sender atm_queue;
    verify_pin(const std::string& account_, const std::string& pin_, messaging::sender atm_queue_)
    : account(account_), pin(pin_), atm_queue(atm_queue_) {}
};

struct pin_verified {};

struct pin_incorrect {};

struct display_enter_pin {};

struct display_enter_card {};

struct display_insufficient_funds {};

struct display_withdrawal_cancelled {};

struct display_pin_incorrect_message {};

struct display_withdrawal_options {};

struct get_balance
{
    std::string account;
    mutable messaging::sender atm_queue;
    get_balance(const std::string& account_, messaging::sender atm_queue_)
    : account(account_), atm_queue(atm_queue_) {}
};

struct balance
{
    unsigned amount;
    explicit balance(unsigned amount_) : amount(amount_) {}
};

struct display_balance
{
    unsigned amount;
    explicit display_balance(unsigned amount_) : amount(amount_) {}
};

struct balance_pressed {};

// ATM状态机
class atm
{
    messaging::receiver incoming;
    messaging::sender bank;
    messaging::sender interface_hardware;
    void (atm::*state)();
    std::string account;
    unsigned withdrawal_amount;
    std::string pin;
    void process_withdrawal()
    {
        incoming.wait()
        .handle<withdraw_ok>([&](const withdraw_ok& msg)
        {
            interface_hardware.send(issue_money(withdrawal_amount));
            bank.send(withdrawal_processed(account, withdrawal_amount));
            state = &atm::done_processing;
        })
        .handle<withdraw_denied>([&](const withdraw_denied& msg)
        {
            interface_hardware.send(display_insufficient_funds());
            state = &atm::done_processing;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            bank.send(cancel_withdrawal(account, withdrawal_amount));
            interface_hardware.send(display_withdrawal_cancelled());
            state = &atm::done_processing;
        });
    }

    void process_balance()
    {
        incoming.wait()
        .handle<balance>([&](const balance& msg)
        {
            interface_hardware.send(display_balance(msg.amount));
            state = &atm::wait_for_action;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }

    void wait_for_action()
    {
        interface_hardware.send(display_withdrawal_options());
        incoming.wait()
        .handle<withdraw_pressed>([&](const withdraw_pressed& msg)
        {
            withdrawal_amount = msg.amount;
            bank.send(withdraw(account, msg.amount, incoming));
            state = &atm::process_withdrawal;
        })
        .handle<balance_pressed>([&](const balance_pressed& msg)
        {
            bank.send(get_balance(account, incoming));
            state = &atm::process_balance;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }
    void verifying_pin()
    {
        incoming.wait()
        .handle<pin_verified>([&](const pin_verified& msg)
        {
            state = &atm::wait_for_action;
        })
        .handle<pin_incorrect>([&](const pin_incorrect& msg)
        {
            interface_hardware.send(
                display_pin_incorrect_message());
            state = &atm::done_processing;
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }

    void getting_pin()
    {
        incoming.wait()
        .handle<digit_pressed>([&](const digit_pressed& msg)
        {
            unsigned const pin_length = 4;
            pin += msg.digit;
            if (pin.length() == pin_length)
            {
                bank.send(verify_pin(account, pin, incoming));
                state = &atm::verifying_pin;
            }
        })
        .handle<clear_last_pressed>([&](const clear_last_pressed& msg)
        {
            if (!pin.empty()) pin.pop_back();
        })
        .handle<cancel_pressed>([&](const cancel_pressed& msg)
        {
            state = &atm::done_processing;
        });
    }

    void waiting_for_card()
    {
        interface_hardware.send(display_enter_card());
        incoming.wait()
        .handle<card_inserted>([&](const card_inserted& msg)
        {
            account = msg.account;
            pin = "";
            interface_hardware.send(display_enter_pin());
            state = &atm::getting_pin;
        });
    }

    void done_processing()
    {
        interface_hardware.send(eject_card());
        state = &atm::waiting_for_card;
    }
public:
    atm(const atm&) = delete;
    atm& operator=(const atm&) = delete;
    atm(messaging::sender bank_, messaging::sender interface_hardware_)
    : bank(bank_), interface_hardware(interface_hardware_) {}
    void done()
    {
        get_sender().send(messaging::close_queue());
    }
    void run()
    {
        state = &atm::waiting_for_card;
        try
        {
            for (;;) (this->*state)();
        }
        catch (const messaging::close_queue&) {}
    }
    messaging::sender get_sender()
    {
        return incoming;
    }
};

// 银行状态机
class bank_machine
{
    messaging::receiver incoming;
    unsigned balance;
public:
    bank_machine() : balance(199) {} // 余额
    void done()
    {
        get_sender().send(messaging::close_queue());
    }
    void run()
    {
        try
        {
            for (;;)
            {
                incoming.wait()
                .handle<verify_pin>([&](const verify_pin& msg)
                {
                    if (msg.pin == "6666")
                    { // 输入密码为6666则通过验证
                        msg.atm_queue.send(pin_verified());
                    }
                    else
                    { // 否则发送密码错误的消息
                        msg.atm_queue.send(pin_incorrect());
                    }
                })
                .handle<withdraw>([&](const withdraw& msg) // 取钱
                {
                    if (balance >= msg.amount)
                    {
                        msg.atm_queue.send(withdraw_ok());
                        balance -= msg.amount;
                    }
                    else
                    {
                        msg.atm_queue.send(withdraw_denied());
                    }
                })
                .handle<get_balance>([&](const get_balance& msg)
                {
                    msg.atm_queue.send(::balance(balance));
                })
                .handle<withdrawal_processed>([&](const withdrawal_processed& msg) {})
                .handle<cancel_withdrawal>([&](const cancel_withdrawal& msg) {});
            }
        }
        catch (const messaging::close_queue&) {}
    }
    messaging::sender get_sender()
    {
        return incoming;
    }
};

// 用户接口状态机
class interface_machine
{
    messaging::receiver incoming;
    std::mutex m;
public:
    void done()
    {
        get_sender().send(messaging::close_queue());
    }
    void run()
    {
        try
        {
            for (;;)
            {
                incoming.wait()
                .handle<issue_money>([&](const issue_money& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "Issuing " << msg.amount << std::endl;
                    }
                })
                .handle<display_insufficient_funds>([&](const display_insufficient_funds& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "Insufficient funds" << std::endl;
                    }
                })
                .handle<display_enter_pin>([&](const display_enter_pin& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "Please enter your PIN (0-9)" << std::endl;
                    }
                })
                .handle<display_enter_card>([&](const display_enter_card& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "Please enter your card (I)" << std::endl;
                    }
                })
                .handle<display_balance>([&](const display_balance& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "The balance of your account is " << msg.amount << std::endl;
                    }
                })
                .handle<display_withdrawal_options>([&](const display_withdrawal_options& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "Withdraw 50? (w)" << std::endl;
                        std::cout << "Display Balance? (b)" << std::endl;
                        std::cout << "Cancel? (c)" << std::endl;
                    }
                })
                .handle<display_withdrawal_cancelled>([&](const display_withdrawal_cancelled& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "Withdrawal cancelled" << std::endl;
                    }
                })
                .handle<display_pin_incorrect_message>([&](const display_pin_incorrect_message& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "PIN incorrect" << std::endl;
                    }
                })
                .handle<eject_card>([&](const eject_card& msg)
                {
                    {
                        std::lock_guard<std::mutex> lk(m);
                        std::cout << "Ejecting card" << std::endl;
                    }
                });
            }
        }
        catch (messaging::close_queue&) {}
    }
    messaging::sender get_sender()
    {
        return incoming;
    }
};

int main()
{
    bank_machine bank;
    interface_machine interface_hardware;
    atm machine(bank.get_sender(), interface_hardware.get_sender());
    std::thread bank_thread(&bank_machine::run, &bank);
    std::thread if_thread(&interface_machine::run, &interface_hardware);
    std::thread atm_thread(&atm::run, &machine);
    messaging::sender atmqueue(machine.get_sender());
    bool quit_pressed = false;
    while (!quit_pressed)
    {
        char c = getchar();
        switch (c)
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                atmqueue.send(digit_pressed(c));
                break;
            case 'b': // 显示余额
                atmqueue.send(balance_pressed());
                break;
            case 'w': // 取钱
                atmqueue.send(withdraw_pressed(50)); // 取50
                break;
            case 'c': // 取消输入（退卡）
                atmqueue.send(cancel_pressed());
                break;
            case 'q': // 结束程序
                quit_pressed = true;
                break;
            case 'i': // 插卡
                atmqueue.send(card_inserted("downdemo")); // 用户名
                break;
        }
    }
    bank.done();
    machine.done();
    interface_hardware.done();
    atm_thread.join();
    bank_thread.join();
    if_thread.join();
}
