#ifndef __ALGO_UTILS_HPP
#define __ALGO_UTILS_HPP

#include <functional>
#include <utility>

// Given two ranges (iterators), compare each value in the ranges and replace
// the value in the second range if pred returns true.
// CIt1 - Const iterator type for left-hand comparison range.
// OutIt - Iterator type for right-hand comparison range (the one being possibly overwritten)
// BinaryPred - Binary predicate that returns true if a value should be replaced in the out range.
// TODO: Auto-vectorization.
template<typename CIt1, typename OutIt, typename BinaryPred>
inline constexpr void ReplaceIfComp(CIt1 first, CIt1 last, OutIt out, BinaryPred&& pred) {
    for(; first != last; ++first, ++out) {
        if(pred(*first, *out)) {
            *out = *first;
        }
    }
}

// TODO: Add range ReplaceIfComp
// template<typename R, typename OutIt, typename BinaryPred>
// inline constexpr void ReplaceIfComp(R&& r, OutIt out, BinaryPred&& pred) {
//     for(; first != last; ++first, ++out) {
//         if(pred(*first, *out)) {
//             *out = *first;
//         }
//     }
// }

// Given two ranges (iterators), compare each value in the ranges and replace
// the value in the second range if pred is false.
// BinaryPredicate(first, out.begin()) ... BinaryPredicate(last, out.begin() + distance(first, last))
// CIt1 - Const iterator type for left-hand comparison range.
// OutIt - Iterator type for right-hand comparison range (the one being possibly overwritten)
// BinaryPred - Binary predicate that returns false if a value should be replaced in the out range.
// TODO: Auto-vectorization.
template<typename CIt1, typename OutIt, typename BinaryPred>
inline constexpr void ReplaceIfNotComp(CIt1 first, CIt1 last, OutIt out, BinaryPred&& prednot) {
    for(; first != last; ++first, ++out) {
        if(!prednot(*first, *out)) {
            *out = *first;
        }
    }
}

// TODO: Add range ReplaceIfNotComp
// Given two ranges (iterators), compare each value in the ranges and replace
// the value in the second range if pred is not true (false).
// CIt1 - Const iterator type for left-hand comparison range.
// OutIt - Iterator type for right-hand comparison range (the one being possibly overwritten)
// BinaryPred - Binary predicate that returns false if a value should be replaced in the out range.
// template<typename CIt1, typename OutIt, typename BinaryPred>
// inline constexpr void ReplaceIfNotComp(CIt1 first, CIt1 last, OutIt out, BinaryPred&& prednot) {
//     for(; first != last; ++first, ++out) {
//         if(!prednot(*first, *out)) {
//             *out = *first;
//         }
//     }
// }

// Given 2 ranges, if the BinaryPred pred returns true, invoke the function f.
// Returns a pair of iterators where the BinaryPred returned true; otherwise return
// a pair, with the first being last1 and the second being the corresponding iterator from first2.
template<typename CIt1, typename CIt2, typename BinaryPred, typename Func>
inline constexpr std::pair<CIt1, CIt2> InvokeIfComp(CIt1 first1, CIt1 last1, CIt2 first2, BinaryPred&& pred, Func&& f) {
    for(; first1 != last1; ++first1, ++first2) {
        if(pred(*first1, *first2)) {
            std::invoke(f);
            return {first1, first2};
        }
    }

    return {last1, first2 + std::distance(first1, last1)};
}

#endif // __ALGO_UTILS_HPP