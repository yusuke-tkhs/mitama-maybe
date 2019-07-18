#ifndef MITAMA_MAYBE_HPP
#define MITAMA_MAYBE_HPP

#include <type_traits>
#include <optional>
#include <utility>
#include <memory>
#include <functional>
#ifdef WITH_BOOST_OPTIONAL
#include <boost/optional.hpp>
#endif

namespace mitama::mitamagic {
template <class, class=void> struct is_pointer_like: std::false_type {};

template <class PointerLike>
struct is_pointer_like<PointerLike,
    std::void_t<
        decltype(std::declval<PointerLike&>().operator->()),
        decltype(*std::declval<PointerLike&>()),
        decltype(bool(std::declval<PointerLike&>()))>>
    : std::true_type
{};

template <class, class=void>
struct element_type;

template <class T>
struct element_type<T, std::enable_if_t<std::disjunction_v<is_pointer_like<T>, std::is_pointer<T>>>> {
    using type = std::remove_reference_t<decltype(*std::declval<T>())>;
};
}

namespace mitama::mitamagic {

template <class T>
class polymorphic_storage {
  public:
    using pointer_type = T*;
    using pointer_const_type = const T*;
    using reference_type = T&;

    virtual ~polymorphic_storage() = default;
    virtual T& deref() = 0;
    virtual bool is_ok() const = 0;
    virtual pointer_type get_pointer() = 0;
    virtual pointer_const_type get_pointer() const  = 0;
};

template <class, class=void>
class maybe_view;

template <class T>
class maybe_view<T,
    std::enable_if_t<
        std::disjunction_v<
            mitamagic::is_pointer_like<std::remove_reference_t<T>>,
            std::is_pointer<std::remove_reference_t<T>>>>>
    : public polymorphic_storage<typename mitamagic::element_type<std::decay_t<T>>::type>
{
    T storage_;
  public:
    using reference_type = typename polymorphic_storage<typename mitamagic::element_type<std::decay_t<T>>::type>::reference_type;
    using pointer_type = typename polymorphic_storage<typename mitamagic::element_type<std::decay_t<T>>::type>::pointer_type;
    using pointer_const_type = typename polymorphic_storage<typename mitamagic::element_type<std::decay_t<T>>::type>::pointer_const_type;

    virtual ~maybe_view() = default;
    maybe_view() = delete;

    template <typename U, std::enable_if_t<std::is_convertible_v<U,T>, bool> = false>
    maybe_view(U&& u) : storage_{std::forward<U>(u)} {}

    bool is_ok() const override final {
        return bool(storage_);
    }
    reference_type deref() override final {
        return *storage_;
    }
    pointer_type get_pointer() override final {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
            return storage_;
        }
        else {
            return storage_.operator->();
        }
    }
    pointer_const_type get_pointer() const override final {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
            return storage_;
        }
        else {
            return storage_.operator->();
        }
    }
};

template <class T> maybe_view(T&&) -> maybe_view<T>;
}

namespace mitama {

template <class T>
class maybe {
    std::unique_ptr<mitamagic::polymorphic_storage<T>> storage_;
  public:
    
    template <typename U>
    maybe(U&& u) : storage_(new mitamagic::maybe_view(std::forward<U>(u))) {}

    template <class F,
        std::enable_if_t<
            std::is_invocable_v<F&&, T&>,
        bool> = false>
    auto operator>>(F&& f) const& {
        static_assert(std::is_invocable_v<F&&, T&>,
            "specified functor is not invocable.");
#ifdef WITH_BOOST_OPTIONAL
        using option = boost::optional<std::invoke_result_t<F&&, T&>>;
        auto Some = [](auto&& v){ return option(std::forward<decltype(v)>(v)); };
        auto None = boost::none;
#else
        using option = std::optional<std::remove_reference_t<std::invoke_result_t<F&&, T&>>>;
        auto Some = [](auto&& v){ return option(std::forward<decltype(v)>(v)); };
        auto None = std::nullopt;
#endif
        using result_type = std::remove_reference_t<std::invoke_result_t<F&&, T&>>;

        if constexpr (std::is_constructible_v<std::invoke_result_t<F&&, T&>, decltype(nullptr)>) {
            if ( storage_->is_ok() ) {
                return maybe<typename mitamagic::element_type<std::decay_t<result_type>>::type>{std::invoke(std::forward<F>(f), storage_->deref())};
            }
            else {
                return maybe<typename mitamagic::element_type<std::decay_t<result_type>>::type>{result_type{nullptr}};
            }
        }
        else if constexpr (std::is_constructible_v<std::invoke_result_t<F&&, T&>, decltype(None)>) {
            if ( storage_->is_ok() ) {
                return maybe<typename mitamagic::element_type<std::decay_t<result_type>>::type>{std::invoke(std::forward<F>(f), storage_->deref())};
            }
            else {
                return maybe<typename mitamagic::element_type<std::decay_t<result_type>>::type>{None};
            }
        }
        else {
            if ( storage_->is_ok() ) {
                return maybe<result_type>{Some(std::invoke(std::forward<F>(f), storage_->deref()))};
            }
            else {
                return maybe<result_type>{option{None}};
            }
        }
    }
    
    explicit operator bool() const {
        return storage_->is_ok();
    }
    
    decltype(auto) unwrap() & {
        return storage_->deref();
    }

    decltype(auto) unwrap() const& {
        return storage_->deref();
    }

    decltype(auto) operator->() & {
        return storage_->get_pointer();
    }

    decltype(auto) operator->() const& {
        return storage_->get_pointer();
    }
};

template <class T> maybe(T&&) -> maybe<typename mitamagic::element_type<std::decay_t<T>>::type>;
}
#endif
