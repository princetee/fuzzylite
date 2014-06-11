// #BEGIN_LICENSE
// fuzzylite: a fuzzy logic control library in C++
// Copyright (C) 2014  Juan Rada-Vilela
// 
// This file is part of fuzzylite.
//
// fuzzylite is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// fuzzylite is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fuzzylite.  If not, see <http://www.gnu.org/licenses/>.
// #END_LICENSE

/*
 * Engine.cpp
 *
 *  Created on: 2/12/2012
 *      Author: jcrada
 */

#include "fl/Engine.h"

#include "fl/factory/DefuzzifierFactory.h"
#include "fl/factory/FactoryManager.h"
#include "fl/factory/SNormFactory.h"
#include "fl/factory/TNormFactory.h"
#include "fl/hedge/Hedge.h"
#include "fl/variable/InputVariable.h"
#include "fl/variable/OutputVariable.h"
#include "fl/rule/RuleBlock.h"
#include "fl/rule/Rule.h"
#include "fl/term/Accumulated.h"

#include "fl/imex/FllExporter.h"

#include "fl/defuzzifier/WeightedAverage.h"
#include "fl/defuzzifier/WeightedSum.h"

#include "fl/term/Constant.h"
#include "fl/term/Linear.h"
#include "fl/term/Function.h"

#include "fl/norm/t/AlgebraicProduct.h"
#include "fl/term/Ramp.h"
#include "fl/term/Sigmoid.h"
#include "fl/term/SShape.h"
#include "fl/term/ZShape.h"

namespace fl {

    Engine::Engine(const std::string& name) : _name(name) {
    }

    Engine::Engine(const Engine& source) : _name("") {
        copyFrom(source);
    }

    Engine& Engine::operator =(const Engine& rhs) {
        if (this == &rhs) return *this;
        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) delete _ruleblocks.at(i);
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) delete _outputVariables.at(i);
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) delete _inputVariables.at(i);
        _ruleblocks.clear();
        _outputVariables.clear();
        _inputVariables.clear();

        copyFrom(rhs);
        return *this;
    }

    void Engine::copyFrom(const Engine& source) {
        _name = source._name;
        for (std::size_t i = 0; i < source._inputVariables.size(); ++i)
            _inputVariables.push_back(new InputVariable(*source._inputVariables.at(i)));
        for (std::size_t i = 0; i < source._outputVariables.size(); ++i)
            _outputVariables.push_back(new OutputVariable(*source._outputVariables.at(i)));

        updateReferences();

        for (std::size_t i = 0; i < source._ruleblocks.size(); ++i) {
            RuleBlock* ruleBlock = new RuleBlock(*source._ruleblocks.at(i));
            try {
                ruleBlock->loadRules(this);
            } catch (...) {
            }
            _ruleblocks.push_back(ruleBlock);
        }
    }

    void Engine::updateReferences() const {
        std::vector<Variable*> myVariables = variables();
        for (std::size_t i = 0; i < myVariables.size(); ++i) {
            Variable* variable = myVariables.at(i);
            for (int t = 0; t < variable->numberOfTerms(); ++t) {
                Term::updateReference(variable->getTerm(t), this);
            }
        }
    }

    Engine::~Engine() {
        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) delete _ruleblocks.at(i);
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) delete _outputVariables.at(i);
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) delete _inputVariables.at(i);
    }

    void Engine::configure(const std::string& activationT, const std::string& accumulationS,
            const std::string& defuzzifier, int resolution) {
        configure("", "", activationT, accumulationS, defuzzifier, resolution);
    }

    void Engine::configure(const std::string& conjunctionT, const std::string& disjunctionS,
            const std::string& activationT, const std::string& accumulationS,
            const std::string& defuzzifierName, int resolution) {
        TNormFactory* tnormFactory = FactoryManager::instance()->tnorm();
        SNormFactory* snormFactory = FactoryManager::instance()->snorm();
        DefuzzifierFactory* defuzzFactory = FactoryManager::instance()->defuzzifier();
        TNorm* conjunction = tnormFactory->constructObject(conjunctionT);
        SNorm* disjunction = snormFactory->constructObject(disjunctionS);
        TNorm* activation = tnormFactory->constructObject(activationT);
        SNorm* accumulation = snormFactory->constructObject(accumulationS);
        Defuzzifier* defuzzifier = defuzzFactory->constructObject(defuzzifierName);
        IntegralDefuzzifier* integralDefuzzifier = dynamic_cast<IntegralDefuzzifier*> (defuzzifier);
        if (integralDefuzzifier) integralDefuzzifier->setResolution(resolution);

        configure(conjunction, disjunction, activation, accumulation, defuzzifier);

        if (defuzzifier) delete defuzzifier;
        if (accumulation) delete accumulation;
        if (activation) delete activation;
        if (disjunction) delete disjunction;
        if (conjunction) delete conjunction;
    }

    void Engine::configure(const TNorm* activation, const SNorm* accumulation,
            const Defuzzifier* defuzzifier) {
        configure(NULL, NULL, activation, accumulation, defuzzifier);
    }

    void Engine::configure(const TNorm* conjunction, const SNorm* disjunction,
            const TNorm* activation, const SNorm* accumulation, const Defuzzifier* defuzzifier) {
        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) {
            _ruleblocks.at(i)->setConjunction(conjunction ? conjunction->clone() : NULL);
            _ruleblocks.at(i)->setDisjunction(disjunction ? disjunction->clone() : NULL);
            _ruleblocks.at(i)->setActivation(activation ? activation->clone() : NULL);
        }

        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            _outputVariables.at(i)->setDefuzzifier(defuzzifier ? defuzzifier->clone() : NULL);
            _outputVariables.at(i)->fuzzyOutput()->setAccumulation(
                    accumulation ? accumulation->clone() : NULL);
        }
    }

    bool Engine::isReady(std::string* status) const {
        std::ostringstream ss;
        if (_inputVariables.empty()) {
            ss << "- Engine <" << _name << "> has no input variables\n";
        }
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) {
            InputVariable* inputVariable = _inputVariables.at(i);
            if (not inputVariable) {
                ss << "- Engine <" << _name << "> has a NULL input variable at index <" << i << ">\n";
            } else if (inputVariable->terms().empty()) {
                //ignore because sometimes inputs can be empty: takagi-sugeno/matlab/slcpp1.fis
                //                ss << "- Input variable <" << _inputVariables.at(i)->getName() << ">"
                //                        << " has no terms\n";
            }
        }

        if (_outputVariables.empty()) {
            ss << "- Engine <" << _name << "> has no output variables\n";
        }
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            if (not outputVariable) {
                ss << "- Engine <" << _name << "> has a NULL output variable at index <" << i << ">\n";
            } else {
                if (outputVariable->terms().empty()) {
                    ss << "- Output variable <" << outputVariable->getName() << ">"
                            << " has no terms\n";
                }
                SNorm* accumulation = outputVariable->fuzzyOutput()->getAccumulation();
                if (not accumulation) {
                    ss << "- Output variable <" << outputVariable->getName() << ">"
                            << " has no Accumulation\n";
                }
                Defuzzifier* defuzzifier = outputVariable->getDefuzzifier();
                if (not defuzzifier) {
                    ss << "- Output variable <" << outputVariable->getName() << ">"
                            << " has no Defuzzifier\n";
                }
            }
        }

        if (_ruleblocks.empty()) {
            ss << "- Engine <" << _name << "> has no rule blocks\n";
        }
        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) {
            RuleBlock* ruleblock = _ruleblocks.at(i);
            if (not ruleblock) {
                ss << "- Engine <" << _name << "> has a NULL rule block at index <" << i << ">\n";
            } else {
                if (ruleblock->rules().empty()) {
                    ss << "- Rule block " << (i + 1) << " <" << ruleblock->getName() << "> has no rules\n";
                }
                int requiresConjunction = 0;
                int requiresDisjunction = 0;
                for (int r = 0; r < ruleblock->numberOfRules(); ++r) {
                    Rule* rule = ruleblock->getRule(r);
                    if (not rule) {
                        ss << "- Rule block " << (i + 1) << " <" << ruleblock->getName()
                                << "> has a NULL rule at index <" << r << ">\n";
                    } else {
                        std::size_t thenIndex = rule->getText().find(" " + Rule::thenKeyword() + " ");
                        std::size_t andIndex = rule->getText().find(" " + Rule::andKeyword() + " ");
                        std::size_t orIndex = rule->getText().find(" " + Rule::orKeyword() + " ");
                        if (andIndex != std::string::npos and andIndex < thenIndex) {
                            ++requiresConjunction;
                        }
                        if (orIndex != std::string::npos and orIndex < thenIndex) {
                            ++requiresDisjunction;
                        }
                    }
                }
                const TNorm* conjunction = ruleblock->getConjunction();
                if (requiresConjunction > 0 and not conjunction) {
                    ss << "- Rule block " << (i + 1) << " <" << ruleblock->getName() << "> has no Conjunction\n";
                    ss << "- Rule block " << (i + 1) << " <" << ruleblock->getName() << "> has "
                            << requiresConjunction << " rules that require Conjunction\n";
                }
                const SNorm* disjunction = ruleblock->getDisjunction();
                if (requiresDisjunction > 0 and not disjunction) {
                    ss << "- Rule block " << (i + 1) << " <" << ruleblock->getName() << "> has no Disjunction\n";
                    ss << "- Rule block " << (i + 1) << " <" << ruleblock->getName() << "> has "
                            << requiresDisjunction << " rules that require Disjunction\n";
                }
                const TNorm* activation = ruleblock->getActivation();
                if (not activation) {
                    ss << "- Rule block " << (i + 1) << " <" << ruleblock->getName() << "> has no Activation\n";
                }
            }
        }
        if (status) *status = ss.str();
        return ss.str().empty();
    }

    void Engine::restart() {
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) {
            _inputVariables.at(i)->setInputValue(fl::nan);
        }
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            _outputVariables.at(i)->clear();
        }
    }

    void Engine::process() {
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            _outputVariables.at(i)->fuzzyOutput()->clear();
        }

        FL_DEBUG_BLOCK(
                FL_DBG("===============");
                FL_DBG("CURRENT INPUTS:");
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) {
            InputVariable* inputVariable = _inputVariables.at(i);
                    scalar inputValue = inputVariable->getInputValue();
            if (inputVariable->isEnabled()) {
                FL_DBG(inputVariable->getName() << ".input = " << Op::str(inputValue));
                        FL_DBG(inputVariable->getName() << ".fuzzy = " << inputVariable->fuzzify(inputValue));
            } else {
                FL_DBG(inputVariable->getName() << ".enabled = false");
            }
        });


        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) {
            RuleBlock* ruleBlock = _ruleblocks.at(i);
            if (ruleBlock->isEnabled()) {
                ruleBlock->activate();
            }
        }

        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            outputVariable->defuzzify();
        }


        FL_DEBUG_BLOCK(
                FL_DBG("===============");
                FL_DBG("CURRENT OUTPUTS:");
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            if (outputVariable->isEnabled()) {
                FL_DBG(outputVariable->getName() << ".default = "
                        << outputVariable->getDefaultValue());

                        FL_DBG(outputVariable->getName() << ".lockRange = "
                        << outputVariable->isLockedOutputValueInRange());

                        FL_DBG(outputVariable->getName() << ".lockValid = "
                        << outputVariable->isLockedPreviousOutputValue());

                        scalar output = outputVariable->getOutputValue();
                        FL_DBG(outputVariable->getName() << ".output = " << output);
                        FL_DBG(outputVariable->getName() << ".fuzzy = " <<
                        outputVariable->fuzzify(output));
                        FL_DBG(outputVariable->fuzzyOutput()->toString());
            } else {
                FL_DBG(outputVariable->getName() << ".enabled = false");
            }
        }
        FL_DBG("==============");
                );
    }

    void Engine::setName(const std::string& name) {
        this->_name = name;
    }

    std::string Engine::getName() const {
        return this->_name;
    }

    void Engine::setInputValue(const std::string& name, scalar value) {
        InputVariable* inputVariable = getInputVariable(name);
        inputVariable->setInputValue(value);
    }

    scalar Engine::getOutputValue(const std::string& name) {
        OutputVariable* outputVariable = getOutputVariable(name);
        return outputVariable->getOutputValue();
    }

    std::string Engine::toString() const {
        return FllExporter().toString(this);
    }

    Engine::Type Engine::type(std::string* name, std::string* reason) const {
        if (_outputVariables.empty()) {
            if (name) *name = "Unknown";
            if (reason) *reason = "There are no output variables";
            return Engine::Unknown;
        }

        //Mamdani
        bool mamdani = true;
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            //Defuzzifier must be integral
            mamdani &= NULL != dynamic_cast<IntegralDefuzzifier*> (outputVariable->getDefuzzifier());
        }
        //Larsen
        bool larsen = mamdani and not _ruleblocks.empty();
        //Larsen is Mamdani with AlgebraicProduct as Activation
        if (mamdani) {
            for (std::size_t i = 0; i < _ruleblocks.size(); ++i) {
                RuleBlock* ruleBlock = _ruleblocks.at(i);
                larsen &= dynamic_cast<const AlgebraicProduct*>
                        (ruleBlock->getActivation()) != NULL;
            }
        }
        if (larsen) {
            if (name) *name = "Larsen";
            if (reason) *reason = "-Output variables have integral defuzzifiers\n"
                    "-Rule blocks activate using the algebraic product t-norm";
            return Engine::Larsen;
        }
        if (mamdani) {
            if (name) *name = "Mamdani";
            if (reason) *reason = "-Output variables have integral defuzzifiers";
            return Engine::Mamdani;
        }
        //Else, keep checking

        //TakagiSugeno
        bool takagiSugeno = true;
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            //Takagi-Sugeno has only Constant, Linear or Function terms
            for (int t = 0; t < outputVariable->numberOfTerms(); ++t) {
                Term* term = outputVariable->getTerm(t);
                takagiSugeno &= (dynamic_cast<Constant*> (term)) or
                        (dynamic_cast<Linear*> (term)) or
                        (dynamic_cast<Function*> (term));
            }
            //and the defuzzifier cannot be integral
            Defuzzifier* defuzzifier = outputVariable->getDefuzzifier();
            takagiSugeno &= defuzzifier and not (dynamic_cast<IntegralDefuzzifier*> (defuzzifier));
        }
        if (takagiSugeno) {
            if (name) *name = "Takagi-Sugeno";
            if (reason) *reason = "-Output variables do not have integral defuzzifiers\n"
                    "-Output variables have constant, linear or function terms";
            return Engine::TakagiSugeno;
        }

        //Tsukamoto
        bool tsukamoto = true;
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            //Tsukamoto has only monotonic terms: Ramp, Sigmoid, SShape, or ZShape
            for (int t = 0; t < outputVariable->numberOfTerms(); ++t) {
                Term* term = outputVariable->getTerm(t);
                tsukamoto &= (dynamic_cast<Ramp*> (term)) or
                        (dynamic_cast<Sigmoid*> (term)) or
                        (dynamic_cast<SShape*> (term)) or
                        (dynamic_cast<ZShape*> (term));
            }
            //and the defuzzifier cannot be integral
            Defuzzifier* defuzzifier = outputVariable->getDefuzzifier();
            tsukamoto &= defuzzifier and not (dynamic_cast<IntegralDefuzzifier*> (defuzzifier));
        }
        if (tsukamoto) {
            if (name) *name = "Tsukamoto";
            if (reason) *reason = "-Output variables only have monotonic terms\n"
                    "Output variables do not have integral defuzzifiers";
            return Engine::Tsukamoto;
        }

        //Inverse Tsukamoto
        bool inverseTsukamoto = true;
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            //Defuzzifier cannot be integral
            Defuzzifier* defuzzifier = outputVariable->getDefuzzifier();
            inverseTsukamoto &= defuzzifier and not (dynamic_cast<IntegralDefuzzifier*> (defuzzifier));
        }
        if (inverseTsukamoto) {
            if (name) *name = "Inverse Tsukamoto";
            if (reason) *reason = "-Type is not Takagi-Sugeno or Tsukamoto\n"
                    "-Output variables do not have integral defuzzifiers\n"
                    "-Output variables do not only have constant, linear or Function terms\n"
                    "-Output variables do not only have monotonic terms\n";
            return Engine::InverseTsukamoto;
        }

        bool hybrid = true;
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            OutputVariable* outputVariable = _outputVariables.at(i);
            //Defuzzifier cannot be integral
            Defuzzifier* defuzzifier = outputVariable->getDefuzzifier();
            hybrid &= defuzzifier != NULL;
        }
        if (hybrid) {
            if (name) *name = "Hybrid";
            if (reason) *reason = "-Output variables have different defuzzifiers";
            return Engine::Hybrid;
        }

        if (name) *name = "Unknown";
        if (reason) *reason = "-There are output variables without a defuzzifier";
        return Engine::Unknown;
    }

    std::vector<Variable*> Engine::variables() const {
        std::vector<Variable*> result;
        result.reserve(_inputVariables.size() + _outputVariables.size());
        result.insert(result.end(), _inputVariables.begin(), _inputVariables.end());
        result.insert(result.end(), _outputVariables.begin(), _outputVariables.end());
        return result;
    }

    /**
     * Operations for iterable datatype _inputVariables
     */
    void Engine::addInputVariable(InputVariable* inputVariable) {
        this->_inputVariables.push_back(inputVariable);
    }

    InputVariable* Engine::setInputVariable(InputVariable* inputVariable, int index) {
        InputVariable* result = this->_inputVariables.at(index);
        this->_inputVariables.at(index) = inputVariable;
        return result;
    }

    void Engine::insertInputVariable(InputVariable* inputVariable, int index) {
        this->_inputVariables.insert(this->_inputVariables.begin() + index,
                inputVariable);
    }

    InputVariable* Engine::getInputVariable(int index) const {
        return this->_inputVariables.at(index);
    }

    InputVariable* Engine::getInputVariable(const std::string& name) const {
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) {
            if (_inputVariables.at(i)->getName() == name)
                return _inputVariables.at(i);
        }
        throw fl::Exception("[engine error] input variable <" + name + "> not found", FL_AT);
    }

    bool Engine::hasInputVariable(const std::string& name) const {
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) {
            if (_inputVariables.at(i)->getName() == name)
                return true;
        }
        return false;
    }

    InputVariable* Engine::removeInputVariable(int index) {
        InputVariable* result = this->_inputVariables.at(index);
        this->_inputVariables.erase(this->_inputVariables.begin() + index);
        return result;
    }

    InputVariable* Engine::removeInputVariable(const std::string& name) {
        for (std::size_t i = 0; i < _inputVariables.size(); ++i) {
            if (_inputVariables.at(i)->getName() == name) {
                InputVariable* result = this->_inputVariables.at(i);
                this->_inputVariables.erase(this->_inputVariables.begin() + i);
                return result;
            }
        }
        throw fl::Exception("[engine error] input variable <" + name + "> not found", FL_AT);
    }

    int Engine::numberOfInputVariables() const {
        return this->_inputVariables.size();
    }

    const std::vector<InputVariable*>& Engine::inputVariables() const {
        return this->_inputVariables;
    }

    void Engine::setInputVariables(const std::vector<InputVariable*>& inputVariables) {
        this->_inputVariables = inputVariables;
    }

    std::vector<InputVariable*>& Engine::inputVariables() {
        return this->_inputVariables;
    }

    /**
     * Operations for iterable datatype _outputVariables
     */
    void Engine::addOutputVariable(OutputVariable* outputVariable) {
        this->_outputVariables.push_back(outputVariable);
    }

    OutputVariable* Engine::setOutputVariable(OutputVariable* outputVariable, int index) {
        OutputVariable* result = this->_outputVariables.at(index);
        this->_outputVariables.at(index) = outputVariable;
        return result;
    }

    void Engine::insertOutputVariable(OutputVariable* outputVariable, int index) {
        this->_outputVariables.insert(this->_outputVariables.begin() + index,
                outputVariable);
    }

    OutputVariable* Engine::getOutputVariable(int index) const {
        return this->_outputVariables.at(index);
    }

    OutputVariable* Engine::getOutputVariable(const std::string& name) const {
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            if (_outputVariables.at(i)->getName() == name)
                return _outputVariables.at(i);
        }
        throw fl::Exception("[engine error] output variable <" + name + "> not found", FL_AT);
    }

    bool Engine::hasOutputVariable(const std::string& name) const {
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            if (_outputVariables.at(i)->getName() == name)
                return true;
        }
        return false;
    }

    OutputVariable* Engine::removeOutputVariable(int index) {
        OutputVariable* result = this->_outputVariables.at(index);
        this->_outputVariables.erase(this->_outputVariables.begin() + index);
        return result;
    }

    OutputVariable* Engine::removeOutputVariable(const std::string& name) {
        for (std::size_t i = 0; i < _outputVariables.size(); ++i) {
            if (_outputVariables.at(i)->getName() == name) {
                OutputVariable* result = this->_outputVariables.at(i);
                this->_outputVariables.erase(this->_outputVariables.begin() + i);
                return result;
            }
        }
        throw fl::Exception("[engine error] output variable <" + name + "> not found", FL_AT);
    }

    int Engine::numberOfOutputVariables() const {
        return this->_outputVariables.size();
    }

    const std::vector<OutputVariable*>& Engine::outputVariables() const {
        return this->_outputVariables;
    }

    void Engine::setOutputVariables(const std::vector<OutputVariable*>& outputVariables) {
        this->_outputVariables = outputVariables;
    }

    std::vector<OutputVariable*>& Engine::outputVariables() {
        return this->_outputVariables;
    }

    /**
     * Operations for iterable datatype _ruleblocks
     */
    void Engine::addRuleBlock(RuleBlock* ruleblock) {
        this->_ruleblocks.push_back(ruleblock);
    }

    RuleBlock* Engine::setRuleBlock(RuleBlock* ruleBlock, int index) {
        RuleBlock* result = this->_ruleblocks.at(index);
        this->_ruleblocks.at(index) = ruleBlock;
        return result;
    }

    void Engine::insertRuleBlock(RuleBlock* ruleblock, int index) {
        this->_ruleblocks.insert(this->_ruleblocks.begin() + index, ruleblock);
    }

    RuleBlock* Engine::getRuleBlock(int index) const {
        return this->_ruleblocks.at(index);
    }

    RuleBlock* Engine::getRuleBlock(const std::string& name) const {
        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) {
            if (_ruleblocks.at(i)->getName() == name)
                return _ruleblocks.at(i);
        }
        throw fl::Exception("[engine error] rule block <" + name + "> not found", FL_AT);
    }

    bool Engine::hasRuleBlock(const std::string& name) const {
        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) {
            if (_ruleblocks.at(i)->getName() == name)
                return true;
        }
        return false;
    }

    RuleBlock* Engine::removeRuleBlock(int index) {
        RuleBlock* result = this->_ruleblocks.at(index);
        this->_ruleblocks.erase(this->_ruleblocks.begin() + index);
        return result;
    }

    RuleBlock* Engine::removeRuleBlock(const std::string& name) {
        for (std::size_t i = 0; i < _ruleblocks.size(); ++i) {
            if (_ruleblocks.at(i)->getName() == name) {
                RuleBlock* result = this->_ruleblocks.at(i);
                this->_ruleblocks.erase(this->_ruleblocks.begin() + i);
                return result;
            }
        }
        throw fl::Exception("[engine error] rule block <" + name + "> not found", FL_AT);
    }

    int Engine::numberOfRuleBlocks() const {
        return this->_ruleblocks.size();
    }

    const std::vector<RuleBlock*>& Engine::ruleBlocks() const {
        return this->_ruleblocks;
    }

    void Engine::setRuleBlocks(const std::vector<RuleBlock*>& ruleBlocks) {
        this->_ruleblocks = ruleBlocks;
    }

    std::vector<RuleBlock*>& Engine::ruleBlocks() {
        return this->_ruleblocks;
    }


}
