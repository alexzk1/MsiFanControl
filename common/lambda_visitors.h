#pragma once

// https://en.cppreference.com/w/cpp/utility/variant/visit
namespace sfw {
template <class... Ts>
struct LambdaVisitor : Ts...
{
    using Ts::operator()...;
};

template <class... Ts>
LambdaVisitor(Ts...) -> LambdaVisitor<Ts...>;
} // namespace sfw
