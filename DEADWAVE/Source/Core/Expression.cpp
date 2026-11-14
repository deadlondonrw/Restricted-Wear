#include "Expression.h"
#include <cmath>

namespace dw
{
namespace
{
enum Fn1Id : uint8_t
{
    F_sin, F_cos, F_tan, F_asin, F_acos, F_atan, F_sinh, F_cosh, F_tanh, F_exp, F_log, F_log2, F_log10, F_sqrt,
    F_abs, F_floor, F_ceil, F_round, F_frac, F_sign, F_saw, F_tri, F_sqr, F_noise, F_fold
};
enum Fn2Id : uint8_t { F2_min, F2_max, F2_pow, F2_atan2, F2_pulse, F2_step };
enum Fn3Id : uint8_t { F3_clamp, F3_lerp, F3_smooth };

struct FnDef { const char* name; int arity; uint8_t id; };

const FnDef fnTable[] = {
    { "sin", 1, F_sin }, { "cos", 1, F_cos }, { "tan", 1, F_tan }, { "asin", 1, F_asin }, { "acos", 1, F_acos },
    { "atan", 1, F_atan }, { "sinh", 1, F_sinh }, { "cosh", 1, F_cosh }, { "tanh", 1, F_tanh }, { "exp", 1, F_exp },
    { "log", 1, F_log }, { "log2", 1, F_log2 }, { "log10", 1, F_log10 }, { "sqrt", 1, F_sqrt }, { "abs", 1, F_abs },
    { "floor", 1, F_floor }, { "ceil", 1, F_ceil }, { "round", 1, F_round }, { "frac", 1, F_frac },
    { "sign", 1, F_sign }, { "saw", 1, F_saw }, { "tri", 1, F_tri }, { "sqr", 1, F_sqr }, { "noise", 1, F_noise },
    { "fold", 1, F_fold },
    { "min", 2, F2_min }, { "max", 2, F2_max }, { "pow", 2, F2_pow }, { "atan2", 2, F2_atan2 },
    { "pulse", 2, F2_pulse }, { "step", 2, F2_step },
    { "clamp", 3, F3_clamp }, { "lerp", 3, F3_lerp }, { "smooth", 3, F3_smooth },
};

inline double fracd (double v) { return v - std::floor (v); }

inline double hashNoise (double v)
{
    // Deterministic value noise in [-1, 1] keyed on floor(v * 2048).
    auto k = (uint64_t) (int64_t) std::floor (v * 2048.0);
    k ^= k >> 33; k *= 0xff51afd7ed558ccdULL; k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL; k ^= k >> 33;
    return (double) (k & 0xffffff) / (double) 0x7fffff - 1.0;
}
} // namespace

struct ExpressionParser
{
    juce::String::CharPointerType p;
    std::vector<Expression::Instr>& out;
    juce::String error;
    int depth = 0, maxDepth = 0;

    void push (int d) { depth += d; maxDepth = juce::jmax (maxDepth, depth); }

    void skip() { while (! p.isEmpty() && juce::CharacterFunctions::isWhitespace (*p)) ++p; }

    bool accept (juce::juce_wchar c)
    {
        skip();
        if (*p == c) { ++p; return true; }
        return false;
    }

    bool fail (const juce::String& msg)
    {
        if (error.isEmpty()) error = msg;
        return false;
    }

    bool parseExpr() { return parseComparison(); }

    bool parseComparison()
    {
        if (! parseAdditive()) return false;
        for (;;)
        {
            skip();
            Expression::Op op;
            if (*p == '<' && p[1] == '=') { p += 2; op = Expression::Op::Le; }
            else if (*p == '>' && p[1] == '=') { p += 2; op = Expression::Op::Ge; }
            else if (*p == '=' && p[1] == '=') { p += 2; op = Expression::Op::Eq; }
            else if (*p == '!' && p[1] == '=') { p += 2; op = Expression::Op::Ne; }
            else if (*p == '<') { ++p; op = Expression::Op::Lt; }
            else if (*p == '>') { ++p; op = Expression::Op::Gt; }
            else return true;
            if (! parseAdditive()) return false;
            out.push_back ({ op }); push (-1);
        }
    }

    bool parseAdditive()
    {
        if (! parseTerm()) return false;
        for (;;)
        {
            if (accept ('+')) { if (! parseTerm()) return false; out.push_back ({ Expression::Op::Add }); push (-1); }
            else if (accept ('-')) { if (! parseTerm()) return false; out.push_back ({ Expression::Op::Sub }); push (-1); }
            else return true;
        }
    }

    bool parseTerm()
    {
        if (! parseUnary()) return false;
        for (;;)
        {
            if (accept ('*')) { if (! parseUnary()) return false; out.push_back ({ Expression::Op::Mul }); push (-1); }
            else if (accept ('/')) { if (! parseUnary()) return false; out.push_back ({ Expression::Op::Div }); push (-1); }
            else if (accept ('%')) { if (! parseUnary()) return false; out.push_back ({ Expression::Op::Mod }); push (-1); }
            else return true;
        }
    }

    bool parseUnary()
    {
        if (accept ('-')) { if (! parseUnary()) return false; out.push_back ({ Expression::Op::Neg }); return true; }
        if (accept ('+')) return parseUnary();
        return parsePower();
    }

    bool parsePower()
    {
        if (! parsePrimary()) return false;
        if (accept ('^'))
        {
            if (! parseUnary()) return false; // right associative
            out.push_back ({ Expression::Op::Pow }); push (-1);
        }
        return true;
    }

    bool parsePrimary()
    {
        skip();
        if (p.isEmpty()) return fail ("Unexpected end of expression");

        if (accept ('('))
        {
            if (! parseExpr()) return false;
            if (! accept (')')) return fail ("Missing ')'");
            return true;
        }

        if (juce::CharacterFunctions::isDigit (*p) || *p == '.')
        {
            auto start = p;
            while (juce::CharacterFunctions::isDigit (*p) || *p == '.') ++p;
            if (*p == 'e' || *p == 'E')
            {
                auto save = p; ++p;
                if (*p == '-' || *p == '+') ++p;
                if (juce::CharacterFunctions::isDigit (*p)) { while (juce::CharacterFunctions::isDigit (*p)) ++p; }
                else p = save;
            }
            const auto num = juce::String (start, p).getDoubleValue();
            out.push_back ({ Expression::Op::Const, 0, num }); push (1);
            return true;
        }

        if (juce::CharacterFunctions::isLetter (*p) || *p == '_')
        {
            auto start = p;
            while (juce::CharacterFunctions::isLetterOrDigit (*p) || *p == '_') ++p;
            const auto name = juce::String (start, p);

            if (name == "x") { out.push_back ({ Expression::Op::VarX }); push (1); return true; }
            if (name == "f") { out.push_back ({ Expression::Op::VarF }); push (1); return true; }
            if (name == "n") { out.push_back ({ Expression::Op::VarN }); push (1); return true; }
            if (name == "N") { out.push_back ({ Expression::Op::VarCount }); push (1); return true; }
            if (name == "pi") { out.push_back ({ Expression::Op::Const, 0, juce::MathConstants<double>::pi }); push (1); return true; }
            if (name == "tau") { out.push_back ({ Expression::Op::Const, 0, juce::MathConstants<double>::twoPi }); push (1); return true; }
            if (name == "e") { out.push_back ({ Expression::Op::Const, 0, juce::MathConstants<double>::euler }); push (1); return true; }

            for (const auto& fn : fnTable)
            {
                if (name != fn.name) continue;
                if (! accept ('(')) return fail ("Expected '(' after " + name);
                for (int a = 0; a < fn.arity; ++a)
                {
                    if (a > 0 && ! accept (',')) return fail (name + " expects " + juce::String (fn.arity) + " arguments");
                    if (! parseExpr()) return false;
                }
                if (! accept (')')) return fail ("Missing ')' after arguments of " + name);
                const auto op = fn.arity == 1 ? Expression::Op::Fn1 : fn.arity == 2 ? Expression::Op::Fn2 : Expression::Op::Fn3;
                out.push_back ({ op, fn.id });
                push (1 - fn.arity);
                return true;
            }
            return fail ("Unknown name '" + name + "'");
        }

        return fail ("Unexpected character '" + juce::String::charToString (*p) + "'");
    }
};

juce::String Expression::compile (const juce::String& text)
{
    program.clear();
    std::vector<Instr> prog;
    ExpressionParser parser { text.getCharPointer(), prog, {} };

    if (! parser.parseExpr())
        return parser.error.isNotEmpty() ? parser.error : juce::String ("Invalid expression");

    parser.skip();
    if (! parser.p.isEmpty())
        return "Unexpected text after expression: '" + juce::String (parser.p) + "'";

    if (prog.size() > 4096)
        return "Expression is too long";
    if (parser.maxDepth > 256)
        return "Expression is too deeply nested";

    program = std::move (prog);
    maxStack = juce::jmax (1, parser.maxDepth);
    return {};
}

double Expression::evaluate (const Vars& v) const
{
    double stack[256];
    if (program.empty() || maxStack > 256) return 0.0;
    int sp = 0;

    for (const auto& in : program)
    {
        switch (in.op)
        {
            case Op::Const: stack[sp++] = in.value; break;
            case Op::VarX: stack[sp++] = v.x; break;
            case Op::VarF: stack[sp++] = v.f; break;
            case Op::VarN: stack[sp++] = v.n; break;
            case Op::VarCount: stack[sp++] = v.N; break;
            case Op::Neg: stack[sp - 1] = -stack[sp - 1]; break;
            case Op::Add: --sp; stack[sp - 1] += stack[sp]; break;
            case Op::Sub: --sp; stack[sp - 1] -= stack[sp]; break;
            case Op::Mul: --sp; stack[sp - 1] *= stack[sp]; break;
            case Op::Div: --sp; stack[sp - 1] = std::abs (stack[sp]) < 1e-300 ? 0.0 : stack[sp - 1] / stack[sp]; break;
            case Op::Mod: --sp; stack[sp - 1] = std::abs (stack[sp]) < 1e-300 ? 0.0 : std::fmod (stack[sp - 1], stack[sp]); break;
            case Op::Pow: --sp; stack[sp - 1] = std::pow (stack[sp - 1], stack[sp]); break;
            case Op::Lt: --sp; stack[sp - 1] = stack[sp - 1] < stack[sp] ? 1.0 : 0.0; break;
            case Op::Gt: --sp; stack[sp - 1] = stack[sp - 1] > stack[sp] ? 1.0 : 0.0; break;
            case Op::Le: --sp; stack[sp - 1] = stack[sp - 1] <= stack[sp] ? 1.0 : 0.0; break;
            case Op::Ge: --sp; stack[sp - 1] = stack[sp - 1] >= stack[sp] ? 1.0 : 0.0; break;
            case Op::Eq: --sp; stack[sp - 1] = std::abs (stack[sp - 1] - stack[sp]) < 1e-12 ? 1.0 : 0.0; break;
            case Op::Ne: --sp; stack[sp - 1] = std::abs (stack[sp - 1] - stack[sp]) >= 1e-12 ? 1.0 : 0.0; break;
            case Op::Fn1:
            {
                double& a = stack[sp - 1];
                switch (in.fn)
                {
                    case F_sin: a = std::sin (a); break;
                    case F_cos: a = std::cos (a); break;
                    case F_tan: a = std::tan (a); break;
                    case F_asin: a = std::asin (juce::jlimit (-1.0, 1.0, a)); break;
                    case F_acos: a = std::acos (juce::jlimit (-1.0, 1.0, a)); break;
                    case F_atan: a = std::atan (a); break;
                    case F_sinh: a = std::sinh (a); break;
                    case F_cosh: a = std::cosh (a); break;
                    case F_tanh: a = std::tanh (a); break;
                    case F_exp: a = std::exp (juce::jmin (a, 700.0)); break;
                    case F_log: a = a > 0 ? std::log (a) : -1e9; break;
                    case F_log2: a = a > 0 ? std::log2 (a) : -1e9; break;
                    case F_log10: a = a > 0 ? std::log10 (a) : -1e9; break;
                    case F_sqrt: a = std::sqrt (std::abs (a)); break;
                    case F_abs: a = std::abs (a); break;
                    case F_floor: a = std::floor (a); break;
                    case F_ceil: a = std::ceil (a); break;
                    case F_round: a = std::round (a); break;
                    case F_frac: a = fracd (a); break;
                    case F_sign: a = a > 0 ? 1.0 : (a < 0 ? -1.0 : 0.0); break;
                    case F_saw: a = 2.0 * fracd (a + 0.5) - 1.0; break;
                    case F_tri: { const double t = fracd (a + 0.25); a = t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t; break; }
                    case F_sqr: a = fracd (a) < 0.5 ? 1.0 : -1.0; break;
                    case F_noise: a = hashNoise (a); break;
                    case F_fold: { double t = fracd ((a + 1.0) * 0.25); a = t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t; break; }
                    default: break;
                }
                break;
            }
            case Op::Fn2:
            {
                --sp;
                double& a = stack[sp - 1];
                const double b = stack[sp];
                switch (in.fn)
                {
                    case F2_min: a = juce::jmin (a, b); break;
                    case F2_max: a = juce::jmax (a, b); break;
                    case F2_pow: a = std::pow (a, b); break;
                    case F2_atan2: a = std::atan2 (a, b); break;
                    case F2_pulse: a = fracd (a) < b ? 1.0 : -1.0; break;
                    case F2_step: a = a >= b ? 1.0 : 0.0; break;
                    default: break;
                }
                break;
            }
            case Op::Fn3:
            {
                sp -= 2;
                double& a = stack[sp - 1];
                const double b = stack[sp], c = stack[sp + 1];
                switch (in.fn)
                {
                    case F3_clamp: a = juce::jlimit (juce::jmin (b, c), juce::jmax (b, c), a); break;
                    case F3_lerp: a = a + (b - a) * c; break;
                    case F3_smooth:
                    {
                        // smooth(edge0, edge1, v) — smoothstep
                        const double t = juce::jlimit (0.0, 1.0, (c - a) / (std::abs (b - a) < 1e-12 ? 1e-12 : (b - a)));
                        a = t * t * (3.0 - 2.0 * t);
                        break;
                    }
                    default: break;
                }
                break;
            }
        }
    }

    const double r = sp > 0 ? stack[sp - 1] : 0.0;
    return std::isfinite (r) ? r : 0.0;
}
} // namespace dw
