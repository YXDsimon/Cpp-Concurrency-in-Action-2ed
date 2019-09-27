#include <memory>
#include <utility>

class function_wrapper {
    struct impl_base {
        virtual void call() = 0;
        virtual ~impl_base() {}
    };
    std::unique_ptr<impl_base> impl;

    template<typename F>
    struct impl_type : impl_base
    {
        F f;
        impl_type(F&& f_) noexcept : f(std::move(f_)) {}
        void call() override { f(); }
    };
public:
    function_wrapper() = default;
    function_wrapper(const function_wrapper&) = delete;
    function_wrapper& operator=(const function_wrapper&) = delete;
    function_wrapper(function_wrapper&& rhs) noexcept : impl(std::move(rhs.impl)) {}
    function_wrapper& operator=(function_wrapper&& rhs) noexcept
    {
        impl = std::move(rhs.impl);
        return *this;
    }
    template<typename F>
    function_wrapper(F&& f) : impl(new impl_type<F>(std::move(f))) {}

    void operator()() const { impl->call(); }
};
