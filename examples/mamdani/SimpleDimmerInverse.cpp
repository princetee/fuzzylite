#include <fl/Headers.h>

int main(int argc, char** argv){
using namespace fl;

Engine* engine = new Engine;
engine->setName("simple-dimmer");

InputVariable* inputVariable = new InputVariable;
inputVariable->setEnabled(true);
inputVariable->setName("Ambient");
inputVariable->setRange(0.000, 1.000);
inputVariable->addTerm(new Triangle("DARK", 0.000, 0.250, 0.500));
inputVariable->addTerm(new Triangle("MEDIUM", 0.250, 0.500, 0.750));
inputVariable->addTerm(new Triangle("BRIGHT", 0.500, 0.750, 1.000));
engine->addInputVariable(inputVariable);

OutputVariable* outputVariable1 = new OutputVariable;
outputVariable1->setEnabled(true);
outputVariable1->setName("Power");
outputVariable1->setRange(0.000, 1.000);
outputVariable1->fuzzyOutput()->setAccumulation(new Maximum);
outputVariable1->setDefuzzifier(new Centroid(200));
outputVariable1->setDefaultValue(fl::nan);
outputVariable1->setLockPreviousOutputValue(false);
outputVariable1->setLockOutputValueInRange(false);
outputVariable1->addTerm(new Triangle("LOW", 0.000, 0.250, 0.500));
outputVariable1->addTerm(new Triangle("MEDIUM", 0.250, 0.500, 0.750));
outputVariable1->addTerm(new Triangle("HIGH", 0.500, 0.750, 1.000));
engine->addOutputVariable(outputVariable1);

OutputVariable* outputVariable2 = new OutputVariable;
outputVariable2->setEnabled(true);
outputVariable2->setName("InversePower");
outputVariable2->setRange(0.000, 1.000);
outputVariable2->fuzzyOutput()->setAccumulation(new Maximum);
outputVariable2->setDefuzzifier(new Centroid(500));
outputVariable2->setDefaultValue(fl::nan);
outputVariable2->setLockPreviousOutputValue(false);
outputVariable2->setLockOutputValueInRange(false);
outputVariable2->addTerm(new Cosine("LOW", 0.200, 0.500));
outputVariable2->addTerm(new Cosine("MEDIUM", 0.500, 0.500));
outputVariable2->addTerm(new Cosine("HIGH", 0.800, 0.500));
engine->addOutputVariable(outputVariable2);

RuleBlock* ruleBlock = new RuleBlock;
ruleBlock->setEnabled(true);
ruleBlock->setName("");
ruleBlock->setConjunction(NULL);
ruleBlock->setDisjunction(NULL);
ruleBlock->setActivation(new Minimum);
ruleBlock->addRule(fl::Rule::parse("if Ambient is DARK then Power is HIGH", engine));
ruleBlock->addRule(fl::Rule::parse("if Ambient is MEDIUM then Power is MEDIUM", engine));
ruleBlock->addRule(fl::Rule::parse("if Ambient is BRIGHT then Power is LOW", engine));
ruleBlock->addRule(fl::Rule::parse("if Power is LOW then InversePower is HIGH", engine));
ruleBlock->addRule(fl::Rule::parse("if Power is MEDIUM then InversePower is MEDIUM", engine));
ruleBlock->addRule(fl::Rule::parse("if Power is HIGH then InversePower is LOW", engine));
engine->addRuleBlock(ruleBlock);


}
