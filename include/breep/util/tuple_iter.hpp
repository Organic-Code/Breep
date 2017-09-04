#ifndef BREEP_UTIL_TUPLE_ITER_HPP
#define BREEP_UTIL_TUPLE_ITER_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @file tuple_iter.hpp
 * @author Lucas Lazare
 * @since 1.0.0
 */

#include <tuple>
#include <utility>

namespace breep {

    namespace detail {
        template<typename FuncT, typename T>
        void do_for_each(FuncT&& funct, T&& t) {
            funct(std::forward<T>(t));
        }

        template<typename FuncT, typename T, typename... R>
        void do_for_each(FuncT&& funct, T&& t, R&& ... r) {
            funct(std::forward<T>(t));
            do_for_each(std::forward<FuncT>(funct), std::forward<R>(r)...);
        }

        template<typename FuncT, typename ...T, size_t... I>
        void for_each_helper(FuncT&& funct, std::tuple<T...>& tuple, std::index_sequence<I...> /* unused */) {
            do_for_each(std::forward<FuncT>(funct), std::get<I>(tuple)...);
        }

        template<typename FuncT, typename ...T, size_t... I>
        void for_each_helper(FuncT&& funct, const std::tuple<T...>& tuple, std::index_sequence<I...> /* unused */) {
            do_for_each(std::forward<FuncT>(funct), std::get<I>(tuple)...);
        }
    }

    /**
     * @brief Iterate through all the elements of a tuple
     *
     * @param tuple Tuple to iterate through
     * @param funct Functor taking tuple elements
     */
    template <typename FuncT, typename... T>
    void for_each(std::tuple<T...>& tuple, FuncT&& funct) {
        detail::for_each_helper(std::forward<FuncT>(funct), tuple, std::make_index_sequence<sizeof...(T)>());
    }

    /**
     * @copydoc for_each(std::tuple<T...>&, FuncT)
     */
    template <typename FuncT, typename... T>
    void for_each(const std::tuple<T...>& tuple, FuncT&& funct) {
        detail::for_each_helper(std::forward<FuncT>(funct), tuple, std::make_index_sequence<sizeof...(T)>());
    }
} // namespace breep

#endif //BREEP_UTIL_TUPLE_ITER_HPP
