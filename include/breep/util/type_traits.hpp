#ifndef BREEP_UTIL_TYPE_TRAITS_HPP
#define BREEP_UTIL_TYPE_TRAITS_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017-2018 Lucas Lazare.                                                             //
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

#include <cstdint>
#include <string>
#include <utility>
#include <tuple>
#include <cstddef>

/**
 * @brief Used to declare an untemplatized class, so that it can be sent through the network. Must be called from the global namespace
 * @details Specialises networking_traits<T>, giving access to ::universal_name (unmangled) and ::hash_code
 *
 *  primitive types are already defined.
 *
 *  When calling this macro, You should not explicitly overload a templatized class (ie: std::vector<int>), except for classes templatized by value (ie: std::tuple_element<0, std::tuple<int>>).
 *  Please use BREEP_DECLARE_TEMPLATE instead.
 *
 *  @sa BREEP_DECLARE_TEMPLATE
 *  @sa breep::networking_traits
 *
 *  @since 0.1.0
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
 *
 *  @since 0.1.0
 */
#define BREEP_DECLARE_TEMPLATE(TType) \
	namespace breep { namespace detail { \
		/* If you have an error here, should probably declare your type with\
            BREEP_DECLARE_TYPE instead of BREEP_DECLARE_TEMPLATE \
            Note that types that have literals as template parameters cannot be declared \
            through this macro; use BREEP_DECLARE_TYPE instead. (ie: BREEP_DECLARE_TYPE(std::array<int,8>)*/ \
	    template <typename... T> \
	    struct networking_traits_impl<TType<T...>> { \
			networking_traits_impl(); \
	        const std::string universal_name = std::string(#TType"<") + ::breep::detail::template_param<T...>().name + ">"; \
	    }; \
		template <typename... T> /* it's ok if this constructor is not inline */ \
		networking_traits_impl<TType<T...>>::networking_traits_impl() = default;\
	}}

namespace breep {

    // define BREEP_MODIFIER_AWARE_IDENTIFIER before including this file if you want modifiers to affect the generated
    // universal_name and hash_code.
	// since 0.1.0
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

		template <typename, typename S>
		using second_type = S;

		template <typename T>
		auto enum_class_test(int) -> second_type<decltype(+T{}), std::false_type>;

		template <typename T>
		auto enum_class_test(...) -> std::true_type;

		template <unsigned int N>
		constexpr uint64_t hash(const char str[N]);

		uint64_t hash(const std::string& str);

		template <typename>
		struct networking_traits_impl {};

		// Partial specialization for references. (since 0.1.0)
		template<typename T>
		struct networking_traits_impl<T&> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_REF_STRING;
		};

		// Partial specialization for pointers. (since 0.1.0)
		template<typename T>
		struct networking_traits_impl<T*> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_PTR_STRING;
		};

		// Partial specialization for const types. (since 0.1.0)
		template<typename T>
		struct networking_traits_impl<const T> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_CST_STRING;
		};

		// Partial specialization for rvalue references. (since 0.1.0)
		template<typename T>
		struct networking_traits_impl<T&&> {
			const std::string universal_name = networking_traits_impl<T>().universal_name + BREEP_RVL_STRING;
		};

		// Partial specialization for volatile. (since 0.1.0)
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
	 * @note When computing hash_code, '>' and ',' are ignored.
	 *
	 * @sa BREEP_DECLARE_TYPE
	 * @sa BREEP_DECLARE_TEMPLATE
	 *
	 * @since 0.1.0
	 */
	template <typename T>
	struct type_traits {
		static constexpr bool is_any_ptr =  std::is_pointer<std::remove_cv_t<std::remove_reference_t<T>>>::value;
		static constexpr bool is_enum = std::is_enum<T>::value;
		static constexpr bool is_enum_class = is_enum && decltype(detail::enum_class_test<T>(0))::value;
		static constexpr bool is_enum_plain = is_enum && !is_enum_class;

		/**
		 * holds the name of the template class (unmangled), including namespace and template parameters.
		 *
		 * @since 0.1.0
		 */
		static const std::string& universal_name(){
			// If you have an error here, you probably forgot to declare the type T with either
			// BREEP_DECLARE_TYPE or BREEP_DECLARE_TEMPLATE.
			static const std::string name = detail::networking_traits_impl<T>().universal_name;
			return name;
		}

		/**
		 * holds an hash of the class universal_name (ignoring '>' and ',' ).
		 * It is guaranteed that networking_traits<T>::hash_code == networking_traits<U>::hash_code if T == U
		 * It is however not guaranteed that they differ if T != U, even though it is very unlikely.
		 *       The hashing function was choosen because of its good performance regarding collision avoidance.
		 *
		 * @since 0.1.0
		 */
		static uint64_t hash_code() {
			static const uint64_t hash = detail::hash(universal_name());
			return hash;
		}
	};

	namespace detail {

		// sdbm's hash algorithm, gawk's implementation.
		// when modified, basic_io_manager::IO_PROTOCOL_ID_1 should be updated aswell
		template <size_t N>
		constexpr uint64_t hash(const char str[N]) {
			uint64_t hash_code = 0;

			for (std::string::size_type i = N ; i-- ;) {
				if (str[i] != '>' && str[i] != ' ' && (str[i] != ':' || str[i+1] != ':')) {
					hash_code = str[i] + (hash_code << 6u) + (hash_code << 16u) - hash_code;
				}
			}
			return hash_code;
		}

		inline uint64_t hash(const std::string& str) {
			uint64_t hash_code = 0;

			for (std::string::size_type i = str.size() ; i-- ;) {
				if (str[i] != '>' && str[i] != ' ' && (str[i] != ':' || str[i+1] != ':')) {
					hash_code = str[i] + (hash_code << 6u) + (hash_code << 16u) - hash_code;
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
		inline const std::string identifier_from_tuple<std::tuple<>>::value{};

		template<typename... T>
		struct identifier_from_tuple<std::tuple<T...>> {
			static const std::string value;
		};
		template<typename... T>
		const std::string identifier_from_tuple<std::tuple<T...>>::value =
				"," + networking_traits_impl<typename std::tuple_element<0, std::tuple<T...>>::type>().universal_name +
				identifier_from_tuple<remove_type<0, T...>>::value; // if you have an error here, you probably forgot to declare the type T<template...> (breep::type_traits<T<template...>>) with BREEP_DECLARE_TEMPLATE(T).


        // This struct is not technically required, but is
        // here to help gcc5 to understand what's happening
        template <typename... T>
        struct template_param {
            const std::string name{networking_traits_impl<typename std::tuple_element<0, std::tuple<T...>>::type>().universal_name + identifier_from_tuple<detail::remove_type<0, T...>>().value};
        };

	}

}  // namespace breep

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

#endif //BREEP_UTIL_TYPE_TRAITS_HPP
