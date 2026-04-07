# CPP-Guide

## Format

This project uses clang-format to format code.

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