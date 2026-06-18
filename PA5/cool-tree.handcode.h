#ifndef COOL_TREE_HANDCODE_H
#define COOL_TREE_HANDCODE_H

#include <iostream>
#include <map>
#include <vector>
#include <cassert>

#include "tree.h"
#include "cool.h"
#include "stringtab.h"

#define yylineno curr_lineno
extern int yylineno;

inline Boolean copy_Boolean(Boolean value)
{
  return value;
}

inline void assert_Boolean(Boolean value)
{
  (void)value;
}

inline void dump_Boolean(ostream& stream, int padding, Boolean value)
{
  stream << pad(padding) << static_cast<int>(value) << "\n";
}

void dump_Symbol(ostream& stream, int padding, Symbol symbol);
void assert_Symbol(Symbol symbol);
Symbol copy_Symbol(Symbol symbol);

class Program_class;
typedef Program_class *Program;

class Class__class;
typedef Class__class *Class_;

class Feature_class;
typedef Feature_class *Feature;

class Formal_class;
typedef Formal_class *Formal;

class Expression_class;
typedef Expression_class *Expression;

class Case_class;
typedef Case_class *Case;

struct cgen_context {
  Symbol self_name;
  Symbol method_name;
  Class_ self_class_definition;

  std::vector<Symbol> scope_symbols;

  std::map<Symbol, int> class_attr_offset;
  std::map<Symbol, int> method_attr_offset;
  std::map<Symbol, int> classtag_of;
  std::map<Symbol, std::map<Symbol, int> > dispatch_offsets_of_class_methods;

  cgen_context()
    : self_name(0),
      method_name(0),
      self_class_definition(0)
  { }

  void push_scope_identifier(Symbol identifier)
  {
    scope_symbols.push_back(identifier);
  }

  void pop_scope_identifier()
  {
    assert(!scope_symbols.empty());
    scope_symbols.pop_back();
  }

  int get_scope_identifier_offset(Symbol identifier) const
  {
    int index;
    int distance;

    for (index = static_cast<int>(scope_symbols.size()) - 1, distance = 0;
         index >= 0;
         --index, ++distance) {
      if (scope_symbols[index] == identifier) {
        return distance;
      }
    }

    return -1;
  }

  int get_method_attr_offset(Symbol identifier) const
  {
    std::map<Symbol, int>::const_iterator found = method_attr_offset.find(identifier);
    return found == method_attr_offset.end() ? -1 : found->second;
  }

  int get_class_attribute_identifier_offset(Symbol identifier) const
  {
    std::map<Symbol, int>::const_iterator found = class_attr_offset.find(identifier);
    return found == class_attr_offset.end() ? -1 : found->second;
  }

  int get_class_method_dispatch_offset(Symbol class_name, Symbol method_name) const
  {
    std::map<Symbol, std::map<Symbol, int> >::const_iterator class_entry;
    std::map<Symbol, int>::const_iterator method_entry;

    class_entry = dispatch_offsets_of_class_methods.find(class_name);
    assert(class_entry != dispatch_offsets_of_class_methods.end());

    method_entry = class_entry->second.find(method_name);
    assert(method_entry != class_entry->second.end());

    return method_entry->second;
  }
};

typedef list_node<Class_> Classes_class;
typedef Classes_class *Classes;

typedef list_node<Feature> Features_class;
typedef Features_class *Features;

typedef list_node<Formal> Formals_class;
typedef Formals_class *Formals;

typedef list_node<Expression> Expressions_class;
typedef Expressions_class *Expressions;

typedef list_node<Case> Cases_class;
typedef Cases_class *Cases;

#define Program_EXTRAS                 \
  virtual void cgen(ostream&) = 0;     \
  virtual void dump_with_types(ostream&, int) = 0;

#define program_EXTRAS                 \
  void cgen(ostream&);                 \
  void dump_with_types(ostream&, int);

#define Class__EXTRAS                  \
  virtual Symbol get_name() = 0;       \
  virtual Symbol get_parent() = 0;     \
  virtual Symbol get_filename() = 0;   \
  virtual Features get_features() = 0; \
  virtual void dump_with_types(ostream&, int) = 0;

#define class__EXTRAS                  \
  Symbol get_name() { return name; }   \
  Symbol get_parent() { return parent; } \
  Symbol get_filename() { return filename; } \
  Features get_features() { return features; } \
  void dump_with_types(ostream&, int);

#define Feature_EXTRAS                 \
  virtual bool is_method() = 0;        \
  virtual bool is_attr() = 0;          \
  virtual void dump_with_types(ostream&, int) = 0;

#define Feature_SHARED_EXTRAS          \
  void dump_with_types(ostream&, int);

#define method_EXTRAS                  \
  bool is_method() { return true; }    \
  bool is_attr() { return false; }     \
  Symbol get_name() { return name; }   \
  Formals get_formals() { return formals; } \
  Symbol get_return_type() { return return_type; } \
  Expression get_body_expr() { return expr; }

#define attr_EXTRAS                    \
  bool is_method() { return false; }   \
  bool is_attr() { return true; }      \
  Symbol get_name() { return name; }   \
  Symbol get_type() { return type_decl; } \
  Expression get_init_expr() { return init; }

#define Formal_EXTRAS                  \
  virtual void dump_with_types(ostream&, int) = 0; \
  virtual Symbol get_name() = 0;

#define formal_EXTRAS                  \
  void dump_with_types(ostream&, int); \
  Symbol get_name() { return name; }

#define Case_EXTRAS                    \
  Symbol type;                         \
  Symbol get_type() { return type; }   \
  virtual void dump_with_types(ostream&, int) = 0;

#define branch_EXTRAS                  \
  Symbol type;                         \
  Symbol get_name() { return name; }   \
  Symbol get_type() { return type; }   \
  void set_type(Symbol symbol) { type = symbol; } \
  void dump_with_types(ostream&, int); \
  Symbol get_declaration_type() { return type_decl; }

#define Expression_EXTRAS              \
  Symbol type;                         \
  Symbol get_type() { return type; }   \
  Expression set_type(Symbol symbol)   \
  {                                    \
    type = symbol;                     \
    return this;                       \
  }                                    \
  virtual void code(ostream&, cgen_context) = 0; \
  virtual void dump_with_types(ostream&, int) = 0; \
  void dump_type(ostream&, int);       \
  Expression_class() { type = static_cast<Symbol>(0); }

#define Expression_SHARED_EXTRAS       \
  void code(ostream&, cgen_context);   \
  void dump_with_types(ostream&, int);

#endif
