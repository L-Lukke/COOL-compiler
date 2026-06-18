#ifndef SEMANT_H_
#define SEMANT_H_

#include <assert.h>
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <algorithm>

#include "cool-tree.h"
#include "stringtab.h"
#include "symtab.h"
#include "list.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

class ClassTable;
typedef ClassTable *ClassTableP;

class ClassTable {
private:
  int semant_errors;
  ostream& error_stream;

  std::map<Symbol, std::vector<Symbol> > inheritance_graph;
  std::map<Symbol, Symbol> parent_type_of;

  void install_basic_classes();
  bool inheritance_dfs(Symbol symbol);

public:
  explicit ClassTable(Classes classes);

  int errors() { return semant_errors; }

  bool install_custom_classes(Classes classes);
  bool build_inheritance_graph();
  bool is_inheritance_graph_acyclic();
  bool is_class_table_valid();

  bool is_subtype_of(Symbol candidate, Symbol desired_type);
  bool is_type_defined(Symbol type);
  bool is_primitive(Symbol type);

  Symbol least_common_ancestor_type(Symbol lhs, Symbol rhs);
  Symbol get_parent_type_of(Symbol type);

  std::map<Symbol, Class_> class_lookup;

  ostream& semant_error();
  ostream& semant_error(Class_ c);
  ostream& semant_error(Symbol filename, tree_node *t);
  ostream& semant_error(tree_node *t);
};

#endif