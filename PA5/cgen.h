#ifndef CGEN_H_
#define CGEN_H_

#include "cool-tree.h"
#include "symtab.h"
#include "emit.h"

#include <iosfwd>
#include <map>
#include <set>
#include <vector>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

enum Basicness {
  Basic,
  NotBasic
};

struct cgen_class_definition {
  Symbol name;
  int tag;
  bool is_primitive_type;

  std::vector<Symbol> dispatch_table;
  std::vector<Symbol> attrs;

  std::map<Symbol, int> attr_offset;
  std::map<Symbol, int> method_offset;

  std::map<Symbol, Symbol> method_definition_containing_class;
  std::map<Symbol, attr_class*> attr_definitions;
  std::map<Symbol, method_class*> method_definitions;
  std::set<Symbol> directly_owned_attrs;

  cgen_class_definition()
    : name(0), tag(0), is_primitive_type(false)
  { }

  int object_size() const
  {
    return DEFAULT_OBJFIELDS + static_cast<int>(attrs.size());
  }

  int size() const
  {
    return object_size();
  }

  bool owns_attribute(Symbol symbol) const
  {
    return directly_owned_attrs.find(symbol) != directly_owned_attrs.end();
  }

  bool is_directly_owned(Symbol symbol) const
  {
    return owns_attribute(symbol);
  }

  bool defines_method(Symbol symbol) const
  {
    std::map<Symbol, Symbol>::const_iterator found =
      method_definition_containing_class.find(symbol);
    return found != method_definition_containing_class.end() && found->second == name;
  }

  bool is_defining_method(Symbol symbol) const
  {
    return defines_method(symbol);
  }
};

class CgenClassTable;
typedef CgenClassTable *CgenClassTableP;

class CgenNode;
typedef CgenNode *CgenNodeP;

class CgenClassTable : public SymbolTable<Symbol, CgenNode> {
private:
  List<CgenNode> *nds;
  std::ostream& str;

  int stringclasstag;
  int intclasstag;
  int boolclasstag;
  int objectclasstag;
  int ioclasstag;
  int objectparenttag;
  int current_classtag;

  std::map<Symbol, std::vector<Symbol> > class_attributes;
  std::map<Symbol, std::set<Symbol> > class_directly_owned_attributes;
  std::map<Symbol, std::map<Symbol, attr_class*> > class_attribute_defs;

  std::map<Symbol, std::vector<Symbol> > class_methods;
  std::map<Symbol, std::map<Symbol, method_class*> > class_method_defs;
  std::map<Symbol, std::map<Symbol, Symbol> > class_method_defined_in;

  std::map<Symbol, Class_> class_definitions;
  std::map<Symbol, std::map<Symbol, int> > dispatch_offsets_of_class_methods;
  std::map<Symbol, cgen_class_definition> cgen_class_definition_of;
  std::vector<Symbol> cgen_class_names;

  std::map<Symbol, int> classtag_of;
  std::map<Symbol, Symbol> inheritance_parent;

  void code_global_data();
  void code_global_text();
  void code_bools(int boolclasstag);
  void code_select_gc();
  void code_constants();

  void install_basic_classes();
  void install_class(CgenNodeP node);
  void install_classes(Classes classes);
  void build_inheritance_tree();
  void set_relations(CgenNodeP node);

  void traverse_inheritance_tree();
  void attach_inherited_definitions_to(Class_ class_definition, Class_ parent_definition);
  void register_properties_and_definitions_of(Class_ class_definition);

  int next_classtag();
  int get_classtag_for(Symbol type);

  void construct_protObjs();
  cgen_class_definition construct_cgen_class_definition(Class_ class_definition);

  void emit_nameTab();
  void emit_objTab();
  void emit_parentTab();

  void emit_default_value_for_attr_type(Symbol type);
  void emit_protObj_from(const cgen_class_definition& class_definition);
  void emit_protObjs();

  void emit_dispatch_table(const cgen_class_definition& class_definition);
  void emit_dispatch_tables();

  void emit_initialiser(const cgen_class_definition& class_definition);
  void emit_initialisers();

  void emit_method(const cgen_class_definition& class_definition, method_class *method_definition);
  void emit_methods(const cgen_class_definition& class_definition);
  void emit_class_methods();

public:
  CgenClassTable(Classes classes, std::ostream& output_stream);
  void code();
  CgenNodeP root();
};

class CgenNode : public class__class {
private:
  CgenNodeP parentnd;
  List<CgenNode> *children;
  Basicness basic_status;
  Class_ c;

public:
  CgenNode(Class_ class_definition,
           Basicness basic_status,
           CgenClassTableP class_table);

  void add_child(CgenNodeP child);
  List<CgenNode> *get_children() const { return children; }

  void set_parentnd(CgenNodeP parent);
  CgenNodeP get_parentnd() const { return parentnd; }

  int basic() const { return basic_status == Basic; }
  Class_ get_class_definition() const { return c; }
};

class BoolConst {
private:
  int val;

public:
  BoolConst(int value);
  void code_def(std::ostream& output_stream, int boolclasstag);
  void code_ref(std::ostream& output_stream) const;
};

#endif
