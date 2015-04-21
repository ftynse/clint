#ifndef CLAYPARSER_H
#define CLAYPARSER_H

class ClayParser
{
public:
  ClayParser();
};

#include "transformation.h"
#include <string>

TransformationSequence parse(const std::string &);
void parse();

#endif // CLAYPARSER_H
