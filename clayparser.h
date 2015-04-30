#ifndef CLAYPARSER_H
#define CLAYPARSER_H

#include "transformation.h"
#include <string>

class ClayParser {
public:
  ClayParser();
  TransformationSequence parse(const std::string &);
};

#endif // CLAYPARSER_H
