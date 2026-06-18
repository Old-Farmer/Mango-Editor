# CPP-Guide

## Format

This project uses clang-format to format code.

## Lint

This project use clang-tidy.

## Best Practice

This project heavily ref [Cpp Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#rf-conventional) and uses the [GSL](https://github.com/microsoft/GSL) library.

### Parameter passing: in & retain copy

Other ref:

- <https://ananyapam7.github.io/resources/C++/Scott_Meyers_Effective_Modern_C++.pdf#page=299.55>
- <https://github.com/isocpp/CppCoreGuidelines/issues/2177>

1. Use const& if performance is just fine and not in hot-path.
2. Otherwise, use const& and && overload if performance is really critical and both l-value and r-value is possible to pass; Use only && if you just want r-value.
3. Use perfect fowarding if arguments is too much.
4. I nearly don't use pass-by-value + std::move idiom. It's have one more std::move cost and will have some potential performace issues.

### Prefer return std::tuple/struct over out reference/pointer

Use return value whenever possible to represent function/method output. There are some reasons:

1. Return value is more intuitive at the first glance.
2. Return value will init variables right away, eliminating many bugs caused by uninitilized variables.
3. funtions/methods using out reference/ptr may have some requirements of out parameters, which lead to a lot bugs.

Cpp structure binding and std::tie is your friend.
