//
// Created by Matthew McCall on 8/10/23.
//

#include "Oasis/Subtract.hpp"
#include "Oasis/Exponent.hpp"
#include "Oasis/Multiply.hpp"
#include "Oasis/Variable.hpp"

namespace Oasis {

Subtract<Expression>::Subtract(const Expression& minuend, const Expression& subtrahend)
    : BinaryExpression(minuend, subtrahend)
{
}

auto Subtract<Expression>::Simplify() const -> std::unique_ptr<Expression>
{
    auto simplifiedMinuend = mostSigOp ? mostSigOp->Simplify() : nullptr;
    auto simplifiedSubtrahend = leastSigOp ? leastSigOp->Simplify() : nullptr;

    Subtract simplifiedSubtract { *simplifiedMinuend, *simplifiedSubtrahend };

    if (auto realCase = Subtract<Real>::Specialize(simplifiedSubtract); realCase != nullptr) {
        const Real& minuend = realCase->GetMostSigOp();
        const Real& subtrahend = realCase->GetLeastSigOp();

        return std::make_unique<Real>(minuend.GetValue() - subtrahend.GetValue());
    }

#pragma region Replace_with_expression
    // exponent - exponent
    if (auto exponentCase = Subtract<Exponent<Variable, Real>, Exponent<Variable, Real>>::Specialize(simplifiedSubtract); exponentCase != nullptr) {
        if (exponentCase->GetMostSigOp().GetMostSigOp().Equals(exponentCase->GetLeastSigOp().GetMostSigOp()) && exponentCase->GetMostSigOp().GetLeastSigOp().Equals(exponentCase->GetLeastSigOp().GetLeastSigOp())) {
            return std::make_unique<Real>(Real { 0.0 });
        }
    }

    // a*exponent - exponent
    if (auto exponentCase = Subtract<Multiply<Real, Exponent<Variable, Real>>, Exponent<Variable, Real>>::Specialize(simplifiedSubtract); exponentCase != nullptr) {
        if (exponentCase->GetMostSigOp().GetLeastSigOp().GetMostSigOp().Equals(exponentCase->GetLeastSigOp().GetMostSigOp()) && exponentCase->GetMostSigOp().GetLeastSigOp().GetLeastSigOp().Equals(exponentCase->GetLeastSigOp().GetLeastSigOp())) {
            if (exponentCase->GetMostSigOp().GetMostSigOp().GetValue() == 1.0)
                return std::make_unique<Real>(Real { 0.0 });
            return std::make_unique<Multiply<Real, Exponent<Variable, Real>>>(Real { exponentCase->GetMostSigOp().GetMostSigOp().GetValue() - 1.0 },
                exponentCase->GetLeastSigOp());
        }
    }

    // exponent - a*exponent
    if (auto exponentCase = Subtract<Exponent<Variable, Real>, Multiply<Real, Exponent<Variable, Real>>>::Specialize(simplifiedSubtract); exponentCase != nullptr) {
        if (exponentCase->GetLeastSigOp().GetLeastSigOp().GetMostSigOp().Equals(exponentCase->GetMostSigOp().GetMostSigOp()) && exponentCase->GetLeastSigOp().GetLeastSigOp().GetLeastSigOp().Equals(exponentCase->GetMostSigOp().GetLeastSigOp())) {
            if (exponentCase->GetLeastSigOp().GetMostSigOp().GetValue() == 1.0)
                return std::make_unique<Real>(Real { 0.0 });
            return std::make_unique<Multiply<Real, Exponent<Variable, Real>>>(Real { 1.0 - exponentCase->GetLeastSigOp().GetMostSigOp().GetValue() },
                exponentCase->GetMostSigOp());
        }
    }

    // a*exponent - b*exponent
    if (auto exponentCase = Subtract<Multiply<Real, Exponent<Variable, Real>>, Multiply<Real, Exponent<Variable, Real>>>::Specialize(simplifiedSubtract); exponentCase != nullptr) {
        if (exponentCase->GetLeastSigOp().GetLeastSigOp().GetMostSigOp().Equals(exponentCase->GetMostSigOp().GetLeastSigOp().GetMostSigOp()) && exponentCase->GetLeastSigOp().GetLeastSigOp().GetLeastSigOp().Equals(exponentCase->GetMostSigOp().GetLeastSigOp().GetLeastSigOp())) {
            if (exponentCase->GetLeastSigOp().GetMostSigOp().GetValue() == 1.0)
                return std::make_unique<Real>(Real { 0.0 });
            return std::make_unique<Multiply<Real, Exponent<Variable, Real>>>(
                Real { exponentCase->GetMostSigOp().GetMostSigOp().GetValue() - exponentCase->GetLeastSigOp().GetMostSigOp().GetValue() },
                exponentCase->GetMostSigOp().GetLeastSigOp());
        }
    }

#pragma endregion

    return simplifiedSubtract.Copy();
}

auto Subtract<Expression>::ToString() const -> std::string
{
    return fmt::format("({} - {})", mostSigOp->ToString(), leastSigOp->ToString());
}

auto Subtract<Expression>::Simplify(tf::Subflow& subflow) const -> std::unique_ptr<Expression>
{
    std::unique_ptr<Expression> simplifiedMinuend, simplifiedSubtrahend;

    tf::Task leftSimplifyTask = subflow.emplace([this, &simplifiedMinuend](tf::Subflow& sbf) {
        if (!mostSigOp) {
            return;
        }

        simplifiedMinuend = mostSigOp->Simplify(sbf);
    });

    tf::Task rightSimplifyTask = subflow.emplace([this, &simplifiedSubtrahend](tf::Subflow& sbf) {
        if (!leastSigOp) {
            return;
        }

        simplifiedSubtrahend = leastSigOp->Simplify(sbf);
    });

    Subtract simplifiedSubtract;

    // While this task isn't actually parallelized, it exists as a prerequisite for check possible cases in parallel
    tf::Task simplifyTask = subflow.emplace([&simplifiedSubtract, &simplifiedMinuend, &simplifiedSubtrahend](tf::Subflow& sbf) {
        if (simplifiedMinuend) {
            simplifiedSubtract.SetMostSigOp(*simplifiedMinuend);
        }

        if (simplifiedSubtrahend) {
            simplifiedSubtract.SetLeastSigOp(*simplifiedSubtrahend);
        }
    });

    simplifyTask.succeed(leftSimplifyTask, rightSimplifyTask);

    std::unique_ptr<Subtract<Real>> realCase;

    tf::Task realCaseTask = subflow.emplace([&simplifiedSubtract, &realCase](tf::Subflow& sbf) {
        realCase = Subtract<Real>::Specialize(simplifiedSubtract, sbf);
    });

    simplifyTask.precede(realCaseTask);

    subflow.join();

    if (realCase) {
        const Real& minuend = realCase->GetMostSigOp();
        const Real& subtrahend = realCase->GetLeastSigOp();

        return std::make_unique<Real>(minuend.GetValue() - subtrahend.GetValue());
    }

    return simplifiedSubtract.Copy();
}

auto Subtract<Expression>::Specialize(const Expression& other) -> std::unique_ptr<Subtract<Expression, Expression>>
{
    if (!other.Is<Subtract>()) {
        return nullptr;
    }

    auto otherGeneralized = other.Generalize();
    return std::make_unique<Subtract>(dynamic_cast<const Subtract&>(*otherGeneralized));
}

auto Subtract<Expression>::Specialize(const Expression& other, tf::Subflow& subflow) -> std::unique_ptr<Subtract>
{
    if (!other.Is<Subtract>()) {
        return nullptr;
    }

    auto otherGeneralized = other.Generalize(subflow);
    return std::make_unique<Subtract>(dynamic_cast<const Subtract&>(*otherGeneralized));
}

} // Oasis