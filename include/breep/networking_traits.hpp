#ifndef BREEP_NETWORKING_TRAITS_HPP
#define BREEP_NETWORKING_TRAITS_HPP

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
 * @file networking_traits.hpp
 * @author Lucas Lazare
 * @since 0.1.0
 */

#include <type_traits>
#include <cstdint>
#include <string>
#include <utility>
#include <tuple>

/**
 * @brief Used to declare an untemplatized class, so that it can be sent through the network. Must be called from the global namespace
 * @details Specialises networking_traits<T>, giving access to ::universal_name (unmangled) and ::hash_code
 *
 *  primitive types are already defined.
 *
 *  When calling this macro, You should not explicitly overload a templatized class (ie: std::vector<int>), except for classes templatized by value (ie: std::tuple_element<0, std::tuple<int>>).
 *  Please use BREEP_DECLARE_TEMPLATE instead.
 *
 *  @since 0.1.0
 *  @sa BREEP_DECLARE_TEMPLATE
 *  @sa breep::networking_traits
 */
#define BREEP_DECLARE_TYPE(T) \
	namespace breep { namespace detail { \
		template <> \
		struct networking_traits_impl<T> { /* If you have an error here, you should probably declare T with BREEP_DECLARE_TEMPLATE instead of BREEP_DECLARE_TYPE*/\
			const std::string universal_name = std::string(#T); \
		}; \
	}}

/**
 *  @brief Used to declare a templatized class, so that it can be sent through the network. Must be called from the global namespace
 *  @details Specialises networking_traits<T>, giving access to ::universal_name (unmangled) and ::hash_code
 *
 *  @attention When using BREEP_DECLARE_TEMPLATE, no template parameter of the passed class must be a value.
 *  @sa BREEP_DECLARE_TYPE
 *  @sa breep::networking_traits
 */
#define BREEP_DECLARE_TEMPLATE(TType) \
	namespace breep { namespace detail { \
	    template <typename... T> \
	    struct networking_traits_impl<TType<T...>> { \
			networking_traits_impl(); \
	        const std::string universal_name = std::string(#TType"<") + networking_traits_impl<typename std::tuple_element<0, std::tuple<T...>>::type>().universal_name + detail::identifier_from_tuple<detail::remove_type<0, T...>>().value + ">"; /* if you have an error here, you probably forgot to declare the type T (breep::networking_traits<breep::T>) with BREEP_DECLARE_TYPE(T) or BREEP_DECLARE_TEMPLATE(T). */\
	    }; \
		template <typename... T> /* it's ok if this constructor is not inline */ \
		networking_traits_impl<TType<T...>>::networking_traits_impl() {}\
	}}

namespace breep {

    // define BREEP_MODIFIER_AWARE_IDENTIFIER before including this file if you want modifiers to affect the generated
    // universal_name and hash_code.
#ifdef BREEP_MODIFIER_AWARE_IDENTIFIER
#define BREEP_REF_STRING " &"
#define BREEP_PTR_STRING " *"
#define BREEP_RVL_STRING " &&"
#define BREEP_CST_STRING " const"
#define BREEP_VLT_STRING " volatile"
#else
#define BREEP_REF_STRING ""
#define BREEP_PTR_STRING ""
#define BREEP_RVL_STRING ""
#define BREEP_CST_STRING ""
#define BREEP_VLT_STRING ""
#endif

	namespace detail {
		uint64_t hash(const std::string& str);

		template <typename>
		struct networking_traits_impl {};

		// Partial specialization for references.
		template<typename T>
		struct networking_traits_impl<T&> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_REF_STRING;
		};

		// Partial specialization for pointers.
		template<typename T>
		struct networking_traits_impl<T*> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_PTR_STRING;
		};

		// Partial specialization for const types.
		template<typename T>
		struct networking_traits_impl<const T> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_CST_STRING;
		};

		// Partial specialization for rvalue references.
		template<typename T>
		struct networking_traits_impl<T&&> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_RVL_STRING;
		};

		// Partial specialization for volatile.
		template<typename T>
		struct networking_traits_impl<volatile T> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_VLT_STRING;
		};
	}

#undef BREEP_REF_STRING
#undef BREEP_PTR_STRING
#undef BREEP_RVL_STRING
#undef BREEP_CST_STRING
#undef BREEP_VLT_STRING


	template <typename>
	struct universal_name {};

	/**
	 * @brief structure holding information about the type
	 * @details If a call to BREEP_DECLARE_TYPE or BREEP_DECLARE_TEMPLATE has been previously made for the template type,
	 *          the static const variables ::universal_name (std::string) and ::hash_code (uint64_t) are available.
	 *
	 * @note When computing hash_code ':' '>' ',' and '<' are all ignored.
	 *
	 * @sa BREEP_DECLARE_TYPE
	 * @sa BREEP_DECLARE_TEMPLATE
	 */
	template <typename T>
	struct networking_traits {
		/**
		 * holds the name of the template class (unmangled), including namespace and template parameters.
		 */
		static const std::string& universal_name(){
			static const std::string name = detail::networking_traits_impl<T>().universal_name;
			return name;
		}

		/**
		 * holds an hash of the class universal_name (ignoring '>' '<' ',' ':').
		 * It is guaranteed that networking_traits<T>::hash_code == networking_traits<U>::hash_code if T == U
		 * It is however not guaranteed that they differ if T != U, even though it is very unlikely.
		 *       The hashing function was choosen because of its good performance regarding collision avoidance.
		 */
		static uint64_t hash_code() {
			static const uint64_t hash = detail::hash(universal_name());
			return hash;
		}
	};

	namespace detail {

		// sdbm's hash algorithm, gawk's implementation.
		uint64_t hash(const std::string& str) {
			uint64_t hash_code = 0;

			for (auto c : str) {
				if (c != '>' && c != '<' && c != ',' && c != ':') {
					hash_code = c + (hash_code << 6) + (hash_code << 16) - hash_code;
				}
			}
			return hash_code;
		}

		template<std::size_t N, typename Tuple, std::size_t... Idx>
		auto remove (std::index_sequence<Idx...>) ->
		decltype(std::tuple_cat(std::declval<std::conditional_t<(N == Idx),
				std::tuple<>,
				std::tuple<typename std::tuple_element<Idx, Tuple>::type>>>()...
		));

		template <std::size_t N, typename... Args>
		using remove_type = decltype(detail::remove<N,std::tuple<Args...>>(std::index_sequence_for<Args...>{}));

		template<typename... T>
		struct identifier_from_tuple {
		};

		template<>
		struct identifier_from_tuple<std::tuple<>> {
			static const std::string value;
		};
		const std::string identifier_from_tuple<std::tuple<>>::value = "";

		template<typename... T>
		struct identifier_from_tuple<std::tuple<T...>> {
			static const std::string value;
		};
		template<typename... T>
		const std::string identifier_from_tuple<std::tuple<T...>>::value =
				"," + networking_traits_impl<typename std::tuple_element<0, std::tuple<T...>>::type>().universal_name +
				identifier_from_tuple<remove_type<0, T...>>::value; // if you have an error here, you probably forgot to declare the type T<template...> (breep::networking_traits<T<template...>>) with BREEP_DECLARE_TEMPLATE(T).
	}
}

// fundamental types
BREEP_DECLARE_TYPE(void)
BREEP_DECLARE_TYPE(std::nullptr_t)
BREEP_DECLARE_TYPE(bool)
BREEP_DECLARE_TYPE(signed char)
BREEP_DECLARE_TYPE(unsigned char)
BREEP_DECLARE_TYPE(char)
BREEP_DECLARE_TYPE(wchar_t)
BREEP_DECLARE_TYPE(char16_t)
BREEP_DECLARE_TYPE(char32_t)
BREEP_DECLARE_TYPE(short)
BREEP_DECLARE_TYPE(unsigned short)
BREEP_DECLARE_TYPE(int)
BREEP_DECLARE_TYPE(unsigned int)
BREEP_DECLARE_TYPE(long)
BREEP_DECLARE_TYPE(unsigned long)
BREEP_DECLARE_TYPE(long long)
BREEP_DECLARE_TYPE(unsigned long long)
BREEP_DECLARE_TYPE(float)
BREEP_DECLARE_TYPE(double)
BREEP_DECLARE_TYPE(long double)

#endif //BREEP_NETWORKING_TRAITS
