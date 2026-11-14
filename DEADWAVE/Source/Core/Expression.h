#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace dw
{
/**
    Small, safe math-expression compiler used by the wave editor's mathematical waveform generator.

    Variables:  x  (phase 0..1 within the cycle)
                f  (frame position 0..1 across the wavetable)
                n  (frame index), N (frame count)
                pi, tau, e
    Operators:  + - * / % ^, unary -, comparisons < > <= >= == != (yield 1 or 0)
    Functions:  sin cos tan asin acos atan atan2 sinh cosh tanh exp log log2 log10 sqrt abs floor ceil round
                frac sign min max pow clamp lerp saw tri sqr pulse noise step smooth fold

    Expressions compile to a flat RPN program, so evaluation is allocation-free.
*/
class Expression
{
public:
    struct Vars
    {
        double x = 0.0, f = 0.0, n = 0.0, N = 1.0;
    };

    /** Compiles the expression. Returns an empty string on success, otherwise an error message. */
    juce::String compile (const juce::String& text);
    bool isValid() const noexcept { return ! program.empty(); }

    double evaluate (const Vars& v) const;

private:
    enum class Op : uint8_t
    {
        Const, VarX, VarF, VarN, VarCount,
        Add, Sub, Mul, Div, Mod, Pow, Neg,
        Lt, Gt, Le, Ge, Eq, Ne,
        Fn1, Fn2, Fn3
    };

    struct Instr
    {
        Op op;
        uint8_t fn = 0;
        double value = 0.0;
    };

    std::vector<Instr> program;
    int maxStack = 0;

    friend struct ExpressionParser;
};
} // namespace dw
