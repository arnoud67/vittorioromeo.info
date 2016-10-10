// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.info | vittorio.romeo@outlook.com

// #define VR_USE_BOOST_VARIANT

#include <iostream>
#include <vector>
#include <type_traits>
#include <boost/hana.hpp>
// #include <vrm/core/static_if.hpp>
#include "variant_aliases.hpp"

// TODO: mention this works poorly with state

namespace impl
{
    struct vnum_wrapper;

    using varr = std::vector<vnum_wrapper>;
    using vnum = vr::variant<int, float, double, varr>;

    struct vnum_wrapper : public vnum
    {
        using vnum::vnum;
    };
}

using vnum = impl::vnum_wrapper;
using impl::varr;

template <typename TF>
struct recurse_wrapper : public TF
{
    template <typename TFFwd>
    recurse_wrapper(TFFwd&& f)
        : TF(FWD(f))
    {
    }
};

template <typename, template <typename...> class>
struct is_specialization_of : boost::hana::false_
{
};

template <template <typename...> class TTemplate, typename... Ts>
struct is_specialization_of<TTemplate<Ts...>, TTemplate> : boost::hana::true_
{
};

namespace impl
{
    struct is_recurse_wrapper_t
    {
        template <typename T>
        constexpr auto operator()(const T&) const
        {
            return is_specialization_of<T, recurse_wrapper>{};
        }
    };
}


constexpr impl::is_recurse_wrapper_t is_recurse_wrapper;

template <typename TTpl>
auto overload_tuple(TTpl&& x)
{
    return boost::hana::unpack(FWD(x), boost::hana::overload);
}

template <typename TReturn, typename... TFs>
auto make_recursive_visitor(TFs&&... fs)
{
    namespace bh = boost::hana;

    auto fns_tuple = bh::make_basic_tuple(FWD(fs)...);

    auto non_rec_overload =
        overload_tuple(bh::remove_if(fns_tuple, is_recurse_wrapper));

    auto rec_overload =
        overload_tuple(bh::filter(fns_tuple, is_recurse_wrapper));

    auto final_overload = bh::overload_linearly(rec_overload,
        [nrc = std::move(non_rec_overload)](auto, auto&& x)
        {
            return nrc(FWD(x));
        });

    return boost::hana::fix([fo = std::move(final_overload)](
                                auto self, auto&& x)
                                ->TReturn
                            {
                                return fo(
                                    [&self](auto&& v)
                                    {
                                        return vr::visit(self, v);
                                    },
                                    FWD(x));
                            });
}

template <typename TF>
auto recurse(TF&& f)
{
    return recurse_wrapper<TF>{FWD(f)};
}

int main()
{
    // clang-format off
    auto vnp = make_recursive_visitor<void>
    (
        [](int x)    { std::cout << x << "i\n"; },
        [](float x)  { std::cout << x << "f\n"; },
        [](double x) { std::cout << x << "d\n"; },

        recurse([](auto visit_self, const varr& arr)
        {
            for(const auto& x : arr)
            {
                visit_self(x);
            }
        })
    );
    // clang-format on

    vnum v0{0};
    vr::visit(vnp, v0);

    v0 = 5.f;
    vr::visit(vnp, v0);

    v0 = 33.51;
    vr::visit(vnp, v0);

    v0 = varr{vnum{1}, vnum{2.0}, vnum{3.f}};
    vr::visit(vnp, v0);

    v0 = varr{vnum{5}, varr{vnum{7}, vnum{8.0}, vnum{9.}}, vnum{4.f}};
    vr::visit(vnp, v0);
}