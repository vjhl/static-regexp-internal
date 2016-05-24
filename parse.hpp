#ifndef REGEX_PARSE_HPP
#define REGEX_PARSE_HPP

#include <utility>
#include <tuple>
#include <static-regexp/static-regexp.hpp>

namespace sre {

	namespace ParsingInternal
	{
		namespace Utils
		{
			template <typename To, typename From>
			constexpr static To convert(const From & v)
			{
				return static_cast<To>(v);
			}

			template <typename ...Args>
			constexpr static auto forward_to_regex(const std::tuple<Args...> & sequence)
			{
				return RegExp<Args...>();
			}
		}

		template <unsigned int c>
		struct CharUInt { static const unsigned int value = c; };

		template <typename Element, typename Tail>
		struct _JoinSequence
		{
			using _tuple_c = std::tuple<Element>;
			using type = decltype(std::tuple_cat(std::declval<_tuple_c>(), std::declval<Tail>()));
		};

		template <typename Element, typename Tail>
		using JoinSequence = typename _JoinSequence<Element, Tail>::type;

		/*	Two main states of parsing.
		
			State<Prev, ...> brings information about the last group, to which
			we can then apply operations (star, choice, repeat etc.).

			StateEmpty just eats the next group (character or subregex). Operations
			in this empty state are just syntactic error in the regex.

			Both these types maintain already parsed part as a member type `tuple_type`.
		*/
		template <typename Prev, typename ... chars>
		struct State;
		template <typename ... chars>
		struct StateEmpty;

		// nothing to parse at the end
		template <>
		struct StateEmpty<>
		{
			using tuple_type = std::tuple<>;
			using unparsed_part = std::tuple<>;
		};

		// we don't have any preceding subregex, so just eat `c` from input and parse the rest with `c` as a state
		template <typename c, typename ... chars>
		struct StateEmpty<c, chars...>
			: State<Char<c::value>, chars...> {};

		// we are in State with Prev as previous subregex, but `c` is not any "operation" character - just push it in the final sequence
		template <typename Prev, typename ... chars>
		struct State<Prev, chars...>
		{
			using _next = StateEmpty<chars...>;
			using tuple_type = JoinSequence<Prev, typename _next::tuple_type>;
			using unparsed_part = typename _next::unparsed_part;
		};

		template <typename Prev, typename ... chars>
		struct State<Prev, CharUInt<'+'>, chars...>
		{
			using _next = StateEmpty<chars...>;
			using tuple_type = JoinSequence<Plus<Prev>, typename _next::tuple_type>;
			using unparsed_part = typename _next::unparsed_part;
		};

		template <typename Prev, typename ... chars>
		struct State<Prev, CharUInt<'*'>, chars...>
		{
			using _next = StateEmpty<chars...>;
			using tuple_type = JoinSequence<Star<Prev>, typename _next::tuple_type>;
			using unparsed_part = typename _next::unparsed_part;
		};

		template <typename ... chars>
		struct StateEmpty<CharUInt<'^'>, chars...>
		{
			using _next = StateEmpty<chars...>;
			using tuple_type = JoinSequence<Begin, typename _next::tuple_type>;
			using unparsed_part = typename _next::unparsed_part;
		};

		template <typename ... chars>
		struct StateEmpty<CharUInt<'$'>, chars...>
		{
			using _next = StateEmpty<chars...>;
			using tuple_type = JoinSequence<End, typename _next::tuple_type>;
			using unparsed_part = typename _next::unparsed_part;
		};


		// parentheses logic; if ')' is found, end the parse of this subregex
		template <typename ... chars>
		struct StateEmpty<CharUInt<')'>, chars...>
		{
			using tuple_type = std::tuple<>;
			using unparsed_part = std::tuple<chars...>;
		};

		template <typename ... chars>
		struct StateEmpty<CharUInt<'('>, chars...>
		{
			using _subregex_parse = StateEmpty<chars...>;

			using _subregex_tuple = typename _subregex_parse::tuple_type;
			using _subregex_rest = typename _subregex_parse::unparsed_part;

			using _subregex_regex = decltype(Utils::forward_to_regex(_subregex_tuple()));

			template <typename ... TupleElements>
			static auto explode(const std::tuple<TupleElements...> &) { return State<_subregex_regex, TupleElements...>(); }

			using _parsed_rest = decltype(explode(_subregex_rest()));

			using tuple_type = typename _parsed_rest::tuple_type;
			using unparsed_part = typename _parsed_rest::unparsed_part;
		};

		template <typename ... chars>
		struct Parser
		{
			constexpr static auto get()
			{
				// just transforms the tuple from StateEmpty::tuple_type<...> to RegExp<...>
				using Impl = StateEmpty<chars...>;
				static_assert(std::is_same<Impl::unparsed_part, std::tuple<>>::value, "closing parenthesis not found");
				return Utils::forward_to_regex(Impl::tuple_type());
			}
		};
	};

	template <typename C, std::size_t ... Indices>
	constexpr auto parse(const C &, std::index_sequence<Indices...>)
	{
		using namespace ParsingInternal;
		using Parser = Parser<CharUInt<Utils::convert<unsigned int>(C::get()[Indices])>... >;
		return Parser::get();
	}

	template <typename C>
	constexpr auto parse(const C & carrier)
	{
		return parse(carrier, std::make_index_sequence<sizeof C::get() - 1>());
	}
}

#define SRE_STR(s) \
	[] () { struct { static constexpr const auto & get() { return s; } } a; return a; }()

#endif

