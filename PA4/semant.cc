

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "semant.h"
#include "utilities.h"
#include <symtab.h>
#include <vector>
#include <map>
#include <queue>
#include <set>

extern int semant_debug;
extern char *curr_filename;

void raise_error();
Symbol type_check(method_class* method);
Symbol type_check(attr_class* attr);
Symbol type_check(Feature f);
std::map<Symbol, method_class*> collect_class_methods(Class_ class_definition);
std::map<Symbol, attr_class*> collect_class_attributes(Class_ class_definition);

SymbolTable<Symbol,Symbol> *object_env;
ClassTable *class_table;

//////////////////////////////////////////////////////////////////////
//                        TYPING ENVIRONMENT
//////////////////////////////////////////////////////////////////////

Symbol current_class_name;
Class_ current_class_definition;

std::map<Symbol, method_class*> current_methods;
std::map<Symbol, attr_class*> current_attrs;

std::map<Symbol, std::map<Symbol, method_class*>> methods_by_class;
std::map<Symbol, std::map<Symbol, attr_class*>> attrs_by_class;

//////////////////////////////////////////////////////////////////////
//                              SYMBOLS
//////////////////////////////////////////////////////////////////////

static Symbol 
    arg,
    arg2,
    Bool,
    concat,
    cool_abort,
    copy,
    Int,
    in_int,
    in_string,
    IO,
    length,
    Main,
    main_meth,
    No_class,
    No_type,
    Object,
    out_int,
    out_string,
    prim_slot,
    self,
    SELF_TYPE,
    Str,
    str_field,
    substr,
    type_name,
    val;

static void initialize_constants(void)
{
    arg         = idtable.add_string("arg");
    arg2        = idtable.add_string("arg2");
    Bool        = idtable.add_string("Bool");
    concat      = idtable.add_string("concat");
    cool_abort  = idtable.add_string("abort");
    copy        = idtable.add_string("copy");
    Int         = idtable.add_string("Int");
    in_int      = idtable.add_string("in_int");
    in_string   = idtable.add_string("in_string");
    IO          = idtable.add_string("IO");
    length      = idtable.add_string("length");
    Main        = idtable.add_string("Main");
    main_meth   = idtable.add_string("main");
    No_class    = idtable.add_string("_no_class");
    No_type     = idtable.add_string("_no_type");
    Object      = idtable.add_string("Object");
    out_int     = idtable.add_string("out_int");
    out_string  = idtable.add_string("out_string");
    prim_slot   = idtable.add_string("_prim_slot");
    self        = idtable.add_string("self");
    SELF_TYPE   = idtable.add_string("SELF_TYPE");
    Str         = idtable.add_string("String");
    str_field   = idtable.add_string("_str_field");
    substr      = idtable.add_string("substr");
    type_name   = idtable.add_string("type_name");
    val         = idtable.add_string("_val");
}

////////////////////////////////////////////////////////////////////
//                          CLASS TABLE
////////////////////////////////////////////////////////////////////

ClassTable::ClassTable(Classes classes) : semant_errors(0) , error_stream(cerr) {
   this->install_basic_classes();
}

void ClassTable::install_basic_classes() {

    Symbol filename = stringtable.add_string("<basic class>");

    Class_ Object_class =
	class_(Object, 
	       No_class,
	       append_Features(
			       append_Features(
					       single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
					       single_Features(method(type_name, nil_Formals(), Str, no_expr()))),
			       single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
	       filename);

    Class_ IO_class = 
	class_(IO, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       single_Features(method(out_string, single_Formals(formal(arg, Str)),
										      SELF_TYPE, no_expr())),
							       single_Features(method(out_int, single_Formals(formal(arg, Int)),
										      SELF_TYPE, no_expr()))),
					       single_Features(method(in_string, nil_Formals(), Str, no_expr()))),
			       single_Features(method(in_int, nil_Formals(), Int, no_expr()))),
	       filename);  

    Class_ Int_class =
	class_(Int, 
	       Object,
	       single_Features(attr(val, prim_slot, no_expr())),
	       filename);

    Class_ Bool_class =
	class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())),filename);
  
    Class_ Str_class =
	class_(Str, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       append_Features(
									       single_Features(attr(val, Int, no_expr())),
									       single_Features(attr(str_field, prim_slot, no_expr()))),
							       single_Features(method(length, nil_Formals(), Int, no_expr()))),
					       single_Features(method(concat, 
								      single_Formals(formal(arg, Str)),
								      Str, 
								      no_expr()))),
			       single_Features(method(substr, 
						      append_Formals(single_Formals(formal(arg, Int)), 
								     single_Formals(formal(arg2, Int))),
						      Str, 
						      no_expr()))),
	       filename);

    this->class_lookup[Object] = Object_class;
    this->class_lookup[IO] = IO_class;
    this->class_lookup[Int] = Int_class;
    this->class_lookup[Bool] = Bool_class;
    this->class_lookup[Str] = Str_class;
}

bool ClassTable::install_custom_classes(Classes classes) {
    for (int i = classes->first(); classes->more(i); i = classes->next(i))
    {
        Class_ user_class = classes->nth(i);
        Symbol class_name = user_class->get_name();
        if (
            class_name == Int    ||
            class_name == Bool   ||
            class_name == Str    ||
            class_name == Object ||
            class_name == IO     ||
            class_name == SELF_TYPE
        ) 
        {
            semant_error(user_class) << "Cannot redefine basic class " << class_name << ".\n";
            return false;
        }
        else if (this->class_lookup.find(class_name) != this->class_lookup.end())
        {
            semant_error(user_class) << "Class " << class_name << " is already defined.\n";
            return false;
        }
        else
            this->class_lookup[class_name] = user_class;
    }
    return true;
}

bool ClassTable::build_inheritance_graph() {

    for (const auto& class_entry : this->class_lookup)
    {
        Symbol class_name = class_entry.first;

        if (class_name == Object)
            continue;

        Class_ class_definition = class_entry.second;
        Symbol parent_class_name = class_definition->get_parent_name();

        parent_type_of[class_name] = parent_class_name;

        if( 
            parent_class_name == Str ||  
            parent_class_name == Int || 
            parent_class_name == Bool ||
            parent_class_name == SELF_TYPE
        )
        {
            this->semant_error(class_definition)
                << "Class "
                << class_definition->get_name()
                << " cannot inherit from "
                << parent_class_name
                << ".\n";
            return false;
        }
        
        if (this->class_lookup.find(parent_class_name) == this->class_lookup.end())
        {
            semant_error(class_definition) << "Class "
                << class_name 
                << " inherits from undefined class "
                << parent_class_name
                << ".\n";
            return false;
        }

        if (this->inheritance_graph.find(parent_class_name) == this->inheritance_graph.end()) 
            this->inheritance_graph[parent_class_name] = std::vector<Symbol>();
    
        this->inheritance_graph[parent_class_name].push_back(class_name);
    } 
    return true;
}

enum SymbolColor { gray, black, white };
std::map<Symbol, SymbolColor> inheritance_color;

bool ClassTable::inheritance_dfs(Symbol class_name) {
    inheritance_color[class_name] = gray;
    for (const auto& child_class_name : inheritance_graph[class_name])
    {
        if(inheritance_color[child_class_name] == gray)
        {
            semant_error() << "Cyclic inheritance involving ";
            class_name->print(semant_error());
            semant_error() << " and ";
            child_class_name->print(semant_error());
            return false;
        }
        else if (inheritance_color[child_class_name] == white)
        {
            if (!inheritance_dfs(child_class_name))
                return false;
        }
    }
    inheritance_color[class_name] = black;
    return true;
}

bool ClassTable::is_inheritance_graph_acyclic() {

    inheritance_color.clear();
    for (const auto& class_entry : this->class_lookup)
        inheritance_color[class_entry.first] = white;

    for (const auto& class_entry : this->class_lookup)
        if (inheritance_color[class_entry.first] == white) 
            if (!this->inheritance_dfs(class_entry.first))
                return false;

    return true;
}


int count_formals(Formals formals) {
    int count = 0;
    for (int i = formals->first(); formals->more(i); i = formals->next(i))
        ++count;
    return count;
}

bool ClassTable::is_class_table_valid() {
    if (!this->is_inheritance_graph_acyclic())
        return false;

    if (!this->is_type_defined(Main)) {
        semant_error() << "Missing class Main.\n";
        return false;
    }

    Class_ main_class = this->class_lookup[Main];
    std::map<Symbol, method_class*> main_methods = collect_class_methods(main_class);

    if (main_methods.find(main_meth) == main_methods.end()) {
        semant_error(main_class) << "Missing method Main.main().\n";
        return false;
    }

    if (count_formals(main_methods[main_meth]->get_formals()) != 0) {
        semant_error(main_class) << "Main.main() must have no parameters.\n";
        return false;
    }

    return true;
}

bool ClassTable::is_subtype_of(Symbol candidate, Symbol desired_type) {
    if (candidate == No_type)
        return true;

    if (desired_type == SELF_TYPE)
        return candidate == SELF_TYPE;
        
    if (candidate == SELF_TYPE)
        candidate = current_class_name;

    if (!this->is_type_defined(candidate) || !this->is_type_defined(desired_type))
        return false;

    Symbol current_type = candidate;

    while (current_type != Object && current_type != desired_type) {
        auto parent_it = parent_type_of.find(current_type);
        if (parent_it == parent_type_of.end())
            return false;
        current_type = parent_it->second;
    }

    return current_type == desired_type;
}

Symbol ClassTable::least_common_ancestor_type(Symbol left_type, Symbol right_type) {
    if (left_type == SELF_TYPE)
        left_type = current_class_name; 
    if (right_type == SELF_TYPE)
        right_type = current_class_name;

    Symbol left_ancestor = left_type;
    Symbol right_ancestor = right_type;
    std::set<Symbol> right_ancestors;
    
    while (right_ancestor != Object) 
    {
        right_ancestors.insert(right_ancestor);
        right_ancestor = parent_type_of[right_ancestor];
    }
    while (left_ancestor != Object) 
    {
        if (right_ancestors.find(left_ancestor) != right_ancestors.end())
            return left_ancestor;
        left_ancestor = parent_type_of[left_ancestor];
    }
    return Object;
}

Symbol ClassTable::get_parent_type_of(Symbol class_name) {
    if (this->parent_type_of.find(class_name) == this->parent_type_of.end())
        return No_type;

    return parent_type_of[class_name];
}

bool ClassTable::is_type_defined(Symbol type_name) {
    return class_lookup.find(type_name) != class_lookup.end();
}

bool ClassTable::is_primitive(Symbol type_name) {
    return (
        type_name == Object ||
        type_name == IO     ||
        type_name == Int    ||
        type_name == Bool   ||
        type_name == Str
    );
}
////////////////////////////////////////////////////////////////////
//                          CLASS UTILITIES
////////////////////////////////////////////////////////////////////

std::map<Symbol, method_class*> collect_class_methods(Class_ class_definition) {
    std::map<Symbol, method_class*> methods;
    Symbol class_name = class_definition->get_name();
    Features class_features = class_definition->get_features();

    for (int i = class_features->first(); class_features->more(i); i = class_features->next(i)) 
    {
        Feature feature = class_features->nth(i);

        if (!feature->is_method())
            continue;

        method_class* method = static_cast<method_class*>(feature);
        Symbol method_name = method->get_name();
        
        if (methods.find(method_name) != methods.end())
        {
            ostream& error_stream = class_table->semant_error(class_definition);
            error_stream << "Method ";
            method_name->print(error_stream);
            error_stream << " is already defined in class ";
            class_name->print(error_stream);
            error_stream << ".\n";
        }
        else
        {
            methods[method_name] = method;
        }
    }
    return methods;
}

method_class* get_class_method(Symbol class_name, Symbol method_name) {
    auto class_it = methods_by_class.find(class_name);
    if (class_it == methods_by_class.end())
        return nullptr;

    auto method_it = class_it->second.find(method_name);
    if (method_it == class_it->second.end())
        return nullptr;

    return method_it->second;
}


attr_class* get_class_attr(Symbol class_name, Symbol attr_name) {
    auto class_it = attrs_by_class.find(class_name);
    if (class_it == attrs_by_class.end())
        return nullptr;

    auto attr_it = class_it->second.find(attr_name);
    if (attr_it == class_it->second.end())
        return nullptr;

    return attr_it->second;
}

void ensure_class_attributes_are_unique(Class_ class_definition) {
    std::set<Symbol> seen_attr_names;
    Symbol class_name = class_definition->get_name();
    Features class_features = class_definition->get_features();

    for (int i = class_features->first(); class_features->more(i); i = class_features->next(i)) 
    {
        Feature feature = class_features->nth(i);

        if (!feature->is_attr())
            continue;

        attr_class* attr = static_cast<attr_class*>(feature);
        Symbol attr_name = attr->get_name();
        
        if (seen_attr_names.find(attr_name) != seen_attr_names.end())
        {
            class_table->semant_error(class_definition)
                << "Attribute "
                << attr_name
                << " is already defined in class "
                << class_name
                << ".\n";
        }
        seen_attr_names.insert(attr_name);
    }
}

std::map<Symbol, attr_class*> collect_class_attributes(Class_ class_definition) {
    std::map<Symbol, attr_class*> attributes;
    Symbol class_name = class_definition->get_name();
    Features class_features = class_definition->get_features();

    for (int i = class_features->first(); class_features->more(i); i = class_features->next(i)) 
    {
        Feature feature = class_features->nth(i);

        if (!feature->is_attr())
            continue;

        attr_class* attr = static_cast<attr_class*>(feature);
        Symbol attr_name = attr->get_name();
        attributes[attr_name] = attr;
    }

    return attributes;
}

////////////////////////////////////////////////////////////////////
//                          TYPECHECKING
////////////////////////////////////////////////////////////////////

void build_attribute_scopes(Class_ class_definition) {
    object_env->enterscope();
    std::map<Symbol, attr_class*> attributes = collect_class_attributes(class_definition);
    for (const auto& attr_entry : attributes) {
        attr_class* attribute_definition = attr_entry.second;
        object_env->addid(
            attribute_definition->get_name(), 
            new Symbol(attribute_definition->get_type())
        );
    }

    if(class_definition->get_name() == Object)
        return;

    Symbol parent_class_name = class_table->get_parent_type_of(class_definition->get_name());
    Class_ parent_class = class_table->class_lookup[parent_class_name];
    build_attribute_scopes(parent_class);
}

void check_inherited_attribute_redefinition(Class_ ancestor_class, attr_class* attribute) {
    if (get_class_attr(ancestor_class->get_name(), attribute->get_name()) != nullptr)
    {
        class_table->semant_error(current_class_definition) 
            << "Attribute "
            << attribute->get_name()
            << " illegally redefines inherited attribute.\n";
    }

    Symbol parent_class_name = class_table->get_parent_type_of(ancestor_class->get_name());
    if (parent_class_name == No_type)
        return;

    Class_ parent_class = class_table->class_lookup[parent_class_name];
    check_inherited_attribute_redefinition(parent_class, attribute);
}

void check_method_override(Class_ ancestor_class, method_class* overriding_method, method_class* inherited_method) {
    if (inherited_method == nullptr)
        return;

    Formals overriding_formals = overriding_method->get_formals();
    Formals inherited_formals = inherited_method->get_formals();
    
    int overriding_formal_ix = 0;
    int inherited_formal_ix = 0;
    
    if(overriding_method->get_return_type() != inherited_method->get_return_type()) {
        class_table->semant_error(current_class_definition) 
            << "Override "
            << overriding_method->get_name()
            << " returns "
            << overriding_method->get_return_type()
            << "; expected "
            << inherited_method->get_return_type()
            << ".\n";
    }

    int overriding_formal_count = 0;
    int inherited_formal_count = 0;

    while (overriding_formals->more(overriding_formal_count))
        overriding_formal_count = overriding_formals->next(overriding_formal_count);

    while (inherited_formals->more(inherited_formal_count))
        inherited_formal_count = inherited_formals->next(inherited_formal_count);

    if (overriding_formal_count != inherited_formal_count) {
        class_table->semant_error(current_class_definition) 
            << "Override "
            << overriding_method->get_name()
            << " has "
            << overriding_formal_count
            << " parameters; expected "
            << inherited_formal_count
            << ".\n";
    }

    while (
        overriding_formals->more(overriding_formal_ix) &&
        inherited_formals->more(inherited_formal_ix)
    )
    {
        Formal overriding_formal = overriding_formals->nth(overriding_formal_ix);
        Formal inherited_formal = inherited_formals->nth(inherited_formal_ix);

        if (overriding_formal->get_type() != inherited_formal->get_type()) {
            class_table->semant_error(current_class_definition) 
                << "Override "
                << overriding_method->get_name()
                << " parameter "
                << overriding_formal->get_name()
                << " has type "
                << overriding_formal->get_type()
                << "; expected "
                << inherited_formal->get_type()
                << ".\n";
        }

        overriding_formal_ix = overriding_formals->next(overriding_formal_ix);
        inherited_formal_ix = inherited_formals->next(inherited_formal_ix);
    }

    Symbol parent_class_name = class_table->get_parent_type_of(ancestor_class->get_name());

    if (parent_class_name == No_type)
        return;

    Class_ parent_class = class_table->class_lookup[parent_class_name];

    check_method_override(
        parent_class, 
        overriding_method,
        get_class_method(
            parent_class_name, 
            overriding_method->get_name()
        )
    );
}

void register_class_members(Class_ class_definition) {
    methods_by_class[class_definition->get_name()] = collect_class_methods(class_definition);
    attrs_by_class[class_definition->get_name()] = collect_class_attributes(class_definition);
}

void type_check(Class_ class_definition) {
    current_class_name = class_definition->get_name();
    current_class_definition = class_definition;
    current_methods = methods_by_class[class_definition->get_name()];
    ensure_class_attributes_are_unique(class_definition);
    current_attrs = attrs_by_class[class_definition->get_name()];

    object_env = new SymbolTable<Symbol, Symbol>();
    object_env->enterscope();
    object_env->addid(self, new Symbol(current_class_definition->get_name()));

    build_attribute_scopes(class_definition);
    
    for (const auto& method_entry : current_methods) {
        check_method_override(class_definition, method_entry.second, method_entry.second);
    }

    for (const auto& attr_entry : current_attrs) {
        Symbol parent_class_name = class_table->get_parent_type_of(current_class_name);
        if (parent_class_name != No_type) {
            Class_ parent_class = class_table->class_lookup[parent_class_name];
            check_inherited_attribute_redefinition(parent_class, attr_entry.second);
        }
    }

    for (const auto& attr_entry : current_attrs) {
        attr_entry.second->type_check();
    }

    for (const auto& method_entry : current_methods) {
        method_entry.second->type_check();
    }

    object_env->exitscope();
}

Symbol object_class::type_check() {
    if (name == self) {
        this->set_type(SELF_TYPE);
        return SELF_TYPE;
    }

    Symbol* object_type = object_env->lookup(name);
    if (object_type != nullptr){
        this->set_type(*object_type);
        return *object_type;
    }

    this->set_type(Object);
    class_table->semant_error(this)
        << "Undefined identifier "
        << name
        << ".\n";
    return Object;
}

Symbol no_expr_class::type_check() {
    this->set_type(No_type);
    return No_type;
}

Symbol isvoid_class::type_check() {
    e1->type_check();
    this->set_type(Bool);
    return Bool;
}

Symbol new__class::type_check() {
    if(type_name != SELF_TYPE && !class_table->is_type_defined(type_name))
    {
        this->set_type(Object);
        class_table->semant_error(this)
            << "Cannot instantiate undefined class "
            << type_name
            << ".\n";
        return Object;
    }
    this->set_type(type_name);
    return type_name;
}

Symbol comp_class::type_check() {
    Symbol expr_type = e1->type_check();
    if (expr_type == Bool) {
        this->set_type(expr_type);
        return expr_type;
    }
    this->set_type(Object);
    class_table->semant_error(this)
        << "'not' requires Bool, got "
        << expr_type
        << ".\n";
    return Object;
}

Symbol leq_class::type_check() {
    Symbol left_type = e1->type_check();
    Symbol right_type = e2->type_check();

    if(left_type == Int && right_type == Int) {
        this->set_type(Bool);
        return Bool;
    }
    else
    {
        this->set_type(Object);
        class_table->semant_error(this) 
            << "Operator <= requires Int operands, got "
            << left_type
            << " and "
            << right_type
            << ".\n";
    }
    return this->get_type();
}

Symbol eq_class::type_check() {
    Symbol left_type = e1->type_check();
    Symbol right_type = e2->type_check();
    
    bool is_left_type_primitive = left_type == Int || left_type == Bool || left_type == Str;
    bool is_right_type_primitive = right_type == Int || right_type == Bool || right_type == Str;

    if ((is_left_type_primitive && is_right_type_primitive) && left_type != right_type)
    {
        class_table->semant_error(this)
            << "Cannot compare "
            << left_type
            << " with "
            << right_type
            << " using =.\n";
    }

    this->set_type(Bool);
    return Bool;
}

Symbol lt_class::type_check() {
    Symbol left_type = e1->type_check();
    Symbol right_type = e2->type_check();

    if(left_type == Int && right_type == Int) {
        this->set_type(Bool);
        return Bool;
    }
    else
    {
        this->set_type(Object);
        class_table->semant_error(this) 
            << "Operator < requires Int operands, got "
            << left_type
            << " and "
            << right_type
            << ".\n";
    }
    return this->get_type();
}

Symbol neg_class::type_check() {
    Symbol inner_expr_type = e1->type_check();
    if (inner_expr_type != Int)
    {
        this->set_type(Object);
        class_table -> semant_error(this) 
            << "Operator ~ requires Int, got "
            << inner_expr_type
            << ".\n";
        return Object;
    }
    this->set_type(Int);
    return Int;
}

Symbol divide_class::type_check() {
    Symbol left_type = e1->type_check();
    Symbol right_type = e2->type_check();
    if(left_type == Int && right_type == Int)
        this->set_type(Int);
    else
    {
        class_table->semant_error(this) 
            << "Operator / requires Int operands, got "
            << left_type
            << " and "
            << right_type
            << ".\n";
        this->set_type(Object);
    }
    return this->get_type();
}

Symbol mul_class::type_check() {
    Symbol left_type = e1->type_check();
    Symbol right_type = e2->type_check();
    if(left_type == Int && right_type == Int)
        this->set_type(Int);
    else
    {
        class_table->semant_error(this) 
            << "Operator * requires Int operands, got "
            << left_type
            << " and "
            << right_type
            << ".\n";
        this->set_type(Object);
    }
    return this->get_type();
}

Symbol sub_class::type_check() {
    Symbol left_type = e1->type_check();
    Symbol right_type = e2->type_check();
    if(left_type == Int && right_type == Int)
        this->set_type(Int);
    else
    {
        class_table->semant_error(this) 
            << "Operator - requires Int operands, got "
            << left_type
            << " and "
            << right_type
            << ".\n";
        this->set_type(Object);
    }
    return this->get_type();
}

Symbol plus_class::type_check() {
    Symbol left_type = e1->type_check();
    Symbol right_type = e2->type_check();
    if(left_type == Int && right_type == Int)
        this->set_type(Int);
    else
    {
        class_table->semant_error(this) 
            << "Operator + requires Int operands, got "
            << left_type
            << " and "
            << right_type
            << ".\n";
        this->set_type(Object);
    }
    return this->get_type();
}

Symbol let_class::type_check() {
    object_env->enterscope();

    bool identifier_is_valid = true;
    if (identifier == self) {
        identifier_is_valid = false;
        class_table->semant_error(this) << "Cannot bind self in let.\n";
    }

    bool declared_type_is_valid = type_decl == SELF_TYPE || class_table->is_type_defined(type_decl);
    if (!declared_type_is_valid)
        class_table->semant_error(this) 
            << "Let variable "
            << identifier
            << " has undefined type "
            << type_decl
            << ".\n";

    Symbol initializer_type = init->type_check();

    if (declared_type_is_valid && initializer_type != No_type && !class_table->is_subtype_of(initializer_type, type_decl))
        class_table->semant_error(this)
            << "Let variable "
            << identifier
            << " has type "
            << type_decl
            << ", initialized with "
            << initializer_type
            << ".\n";

    if (identifier_is_valid) {
        Symbol binding_type = declared_type_is_valid ? type_decl : Object;
        object_env->addid(identifier, new Symbol(binding_type));
    }

    this->set_type(body->type_check());
    object_env->exitscope();
    return type;
}

Symbol block_class::type_check() {
    Symbol last_body_expr_type = Object;
    for (int i = body->first(); body->more(i); i = body->next(i))
        last_body_expr_type = body->nth(i)->type_check();
    this->set_type(last_body_expr_type);
    return last_body_expr_type;
}

Symbol branch_class::type_check() {
    Symbol branch_type = type_decl;
    Symbol branch_name = name;

    bool branch_name_is_valid = true;
    if (branch_name == self) {
        branch_name_is_valid = false;
        class_table->semant_error(this) << "Cannot bind self in case branch.\n";
    }

    bool branch_type_is_valid = true;
    if (branch_type == SELF_TYPE) {
        branch_type_is_valid = false;
        class_table->semant_error(this) << "Case branch cannot declare SELF_TYPE.\n";
    }
    else if (!class_table->is_type_defined(branch_type)) {
        branch_type_is_valid = false;
        class_table->semant_error(this)
            << "Case branch has undefined type "
            << branch_type
            << ".\n";
    }

    object_env->enterscope();
    if (branch_name_is_valid) {
        Symbol binding_type = branch_type_is_valid ? branch_type : Object;
        object_env->addid(branch_name, new Symbol(binding_type));
    }

    Symbol branch_body_type = expr->type_check();
    this->set_type(branch_body_type);
    object_env->exitscope();
    return branch_body_type;
}

Symbol typcase_class::type_check() {
    expr->type_check();

    std::set<Symbol> seen_branch_types;
    Symbol result_type = Object;

    for (int case_ix = cases->first(); cases->more(case_ix); case_ix = cases->next(case_ix)) {
        branch_class* branch = static_cast<branch_class*>(cases->nth(case_ix));
        Symbol branch_type = branch->get_declaration_type();
        if (seen_branch_types.find(branch_type) != seen_branch_types.end())
            class_table->semant_error(branch) 
                << "Duplicate case branch type "
                << branch_type
                << ".\n";
        else
            seen_branch_types.insert(branch_type);

        if (case_ix == cases->first())
            result_type = branch->type_check();
        else
            result_type = class_table->least_common_ancestor_type(
                result_type,
                branch->type_check()
            );
    }

    this->set_type(result_type);
    return result_type;
}


Symbol loop_class::type_check() {
    Symbol predicate_type = pred->type_check();
    body->type_check();

    if (predicate_type != Bool)
    {
        class_table->semant_error(this) 
            << "While predicate must be Bool, got "
            << predicate_type
            << ".\n";
    }

    this->set_type(Object);
    return Object; 
}

Symbol cond_class::type_check() {
    Symbol predicate_type = pred->type_check();
    Symbol then_type = then_exp->type_check();
    Symbol else_type = else_exp->type_check();

    if (predicate_type != Bool)
    {
        class_table->semant_error(this) 
            << "If predicate must be Bool, got "
            << predicate_type
            << ".\n";
    }

    Symbol result_type = class_table->least_common_ancestor_type(then_type, else_type);
    this->set_type(result_type);
    return result_type;
}

method_class* lookup_method_in_chain(Symbol class_name, Symbol method_name) {  
    if (class_name == No_type)
        return nullptr;

    method_class* method = get_class_method(class_name, method_name);
    if (method)
        return method;

    Symbol parent_class_name = class_table->get_parent_type_of(class_name);
    return lookup_method_in_chain(parent_class_name, method_name);
}

method_class* lookup_method(Symbol class_name, Symbol method_name) {  
    method_class* inherited_method = lookup_method_in_chain(class_name, method_name);

    if (inherited_method)
        return inherited_method;

    if (class_table->is_primitive(class_name)) 
    {
        return get_class_method(class_name, method_name);
    }
    return nullptr;
}

Symbol dispatch_class::type_check() {

    Symbol expr_type = expr->type_check();

    if (expr_type != SELF_TYPE && !class_table->is_type_defined(expr_type)) {

        class_table->semant_error(this) 
            << "Dispatch receiver has undefined type "
            << expr_type
            << ".\n";

        this->set_type(Object);
        return Object;
    }
    
    Symbol receiver_type_name = expr_type == SELF_TYPE ? current_class_name : expr_type;
    method_class* target_method = lookup_method(receiver_type_name, name);
  
    if (!target_method) 
    {
        class_table->semant_error(this) 
            << "Undefined method "
            << name
            << " in type "
            << receiver_type_name
            << ".\n";
        
        this->set_type(Object);
        return Object;
    }

    Symbol method_return_type = target_method->get_return_type();
    Formals formal_params = target_method->get_formals();
    Expressions actual_args = this->actual;

    int formal_count = 0;
    int actual_count = 0;

    while (formal_params->more(formal_count))
        formal_count = formal_params->next(formal_count);
    while (actual_args->more(actual_count))
        actual_count = actual_args->next(actual_count);

    bool dispatch_is_valid = true;
    if (formal_count != actual_count) {
        dispatch_is_valid = false;
        class_table->semant_error(this) 
            << "Method "
            << target_method->get_name()
            << " expects "
            << formal_count
            << " arguments, got "
            << actual_count
            << ".\n";
    }

    int formal_ix = formal_params->first();
    int actual_ix = actual_args->first();

    Formal formal_param;
    Expression actual_arg;

    while (
        actual_args->more(actual_ix) && 
        formal_params->more(formal_ix)
    )
    {
        actual_arg = actual_args->nth(actual_ix);
        formal_param = formal_params->nth(formal_ix);

        Symbol actual_arg_type = actual_arg->type_check();
        Symbol formal_type = formal_param->get_type();

        if (!class_table->is_subtype_of(actual_arg_type, formal_type)) {
            dispatch_is_valid = false;

            class_table->semant_error(this) 
                << "Argument "
                << formal_param->get_name()
                << " of "
                << target_method->get_name()
                << " has type "
                << actual_arg_type
                << "; expected "
                << formal_type
                << ".\n";
        }

        actual_ix = actual_args->next(actual_ix);
        formal_ix = formal_params->next(formal_ix);
    }

    while (actual_args->more(actual_ix)) {
        actual_args->nth(actual_ix)->type_check();
        actual_ix = actual_args->next(actual_ix);
    }
    
    if (!dispatch_is_valid)
    {
        this->set_type(Object);
        return Object;
    }

    Symbol dispatch_type = method_return_type == SELF_TYPE ? expr_type : method_return_type;
    this->set_type(dispatch_type);
    return dispatch_type;
}

Symbol static_dispatch_class::type_check() {
    Symbol expr_type = expr->type_check();

    if (this->type_name == SELF_TYPE) {
        class_table->semant_error(this)
            << "Static dispatch type cannot be SELF_TYPE.\n";

        this->set_type(Object);
        return Object;
    }

    if (!class_table->is_type_defined(type_name)) {
        class_table->semant_error(this) 
            << "Static dispatch uses undefined type "
            << type_name
            << ".\n";

        this->set_type(Object);
        return Object;
    }
    if (expr_type != SELF_TYPE && !class_table->is_type_defined(expr_type)) {
        this->set_type(Object);
        return Object;
    }

    bool dispatch_is_valid = true;

    if (!class_table->is_subtype_of(expr_type, this->type_name)) {
        dispatch_is_valid = false;
        class_table -> semant_error(this) 
            << "Static dispatch receiver has type "
            << expr_type
            << "; expected subtype of "
            << this->type_name
            << ".\n";
    }
    
    method_class* target_method = lookup_method(type_name, name);
    if (!target_method) 
    {
        class_table->semant_error(this) 
            << "Undefined method "
            << name
            << " in type "
            << type_name
            << ".\n";
        
        this->set_type(Object);
        return Object;
    }

    Symbol method_return_type = target_method->get_return_type();
    Formals formal_params = target_method->get_formals();
    Expressions actual_args = this->actual;

    int formal_count = 0;
    int actual_count = 0;

    while (formal_params->more(formal_count))
        formal_count = formal_params->next(formal_count);
    while (actual_args->more(actual_count))
        actual_count = actual_args->next(actual_count);

    if (formal_count != actual_count) {
        dispatch_is_valid = false;
        class_table->semant_error(this) 
            << "Method "
            << target_method->get_name()
            << " expects "
            << formal_count
            << " arguments, got "
            << actual_count
            << ".\n";
    }

    int formal_ix = formal_params->first();
    int actual_ix = actual_args->first();
    Formal formal_param;
    Expression actual_arg;

    while (
        actual_args->more(actual_ix) && 
        formal_params->more(formal_ix)
    )
    {
        actual_arg = actual_args->nth(actual_ix);
        formal_param = formal_params->nth(formal_ix);

        Symbol actual_arg_type = actual_arg->type_check();
        Symbol formal_type = formal_param->get_type();

        if (!class_table->is_subtype_of(actual_arg_type, formal_type)) {
            dispatch_is_valid = false;

            class_table->semant_error(this) 
                << "Argument "
                << formal_param->get_name()
                << " of "
                << target_method->get_name()
                << " has type "
                << actual_arg_type
                << "; expected "
                << formal_type
                << ".\n";
        }

        actual_ix = actual_args->next(actual_ix);
        formal_ix = formal_params->next(formal_ix);
    }

    while (actual_args->more(actual_ix)) {
        actual_args->nth(actual_ix)->type_check();
        actual_ix = actual_args->next(actual_ix);
    }
    
    if (!dispatch_is_valid)
    {
        this->set_type(Object);
        return Object;
    }

    Symbol dispatch_type = method_return_type == SELF_TYPE ? expr_type : method_return_type;
    this->set_type(dispatch_type);
    return dispatch_type;
}

Symbol assign_class::type_check() {
    Symbol target_name = name;
    Expression assigned_expr = expr;

    if (target_name == self) {
        class_table->semant_error(this) << "Cannot assign to self.\n";
        return Object;
    }

    Symbol* target_type = object_env->lookup(target_name);

    if (!target_type) {
        class_table->semant_error(this) 
            << "Assignment to undefined identifier "
            << target_name
            << ".\n";

        this->set_type(Object);
        return this->get_type();
    }

    Symbol assigned_type = assigned_expr->type_check();

    bool assignment_conforms = class_table->is_subtype_of(
        assigned_type, 
        *target_type
    );

    if (!assignment_conforms) {
        class_table->semant_error(this) 
            << "Assignment to "
            << target_name
            << " has type "
            << assigned_type
            << "; expected "
            << *target_type
            << ".\n";

        this->set_type(Object);
        return Object;
    }

    this->set_type(assigned_type);
    return assigned_type;
}


Symbol method_class::type_check() {
    object_env->enterscope();
    std::set<Symbol> seen_formal_names;

    for (int formal_ix = formals->first(); formals->more(formal_ix); formal_ix = formals->next(formal_ix))
    {
        Formal formal_param = formals->nth(formal_ix);
        Symbol formal_name = formal_param->get_name();
        Symbol formal_type = formal_param->get_type();

        bool formal_name_is_valid = true;
        bool formal_type_is_valid = true;

        if(formal_name == self) {
            formal_name_is_valid = false;
            class_table->semant_error(formal_param) << "Formal parameter cannot be named self.\n";
        }
        else if(seen_formal_names.find(formal_name) != seen_formal_names.end()) {
            formal_name_is_valid = false;
            class_table->semant_error(formal_param) 
                << "Duplicate formal parameter "
                << formal_name
                << " in method "
                << get_name()
                << ".\n";
        }
        else
        {
           seen_formal_names.insert(formal_name);
        }

        if (formal_type == SELF_TYPE) {
            formal_type_is_valid = false;
            class_table->semant_error(formal_param)
                << "Formal parameter "
                << formal_name
                << " cannot have type SELF_TYPE.\n";
        }
        else if (!class_table->is_type_defined(formal_type)) {
            formal_type_is_valid = false;
            class_table->semant_error(formal_param) 
                << "Formal parameter "
                << formal_name
                << " has undefined type "
                << formal_type
                << ".\n";
        }

        if (formal_name_is_valid && formal_type_is_valid)
            object_env->addid(formal_name, new Symbol(formal_type));
    }

    Symbol declared_return_type = get_return_type();
    bool return_type_is_valid = declared_return_type == SELF_TYPE || class_table->is_type_defined(declared_return_type);
    if (!return_type_is_valid)
        class_table->semant_error(this)
            << "Method "
            << this->get_name()
            << " has undefined return type "
            << declared_return_type
            << ".\n";

    Symbol body_type = this->expr->type_check();

    if (return_type_is_valid && !class_table->is_subtype_of(body_type, declared_return_type))
    {
        class_table->semant_error(this)
            << "Method "
            << this->get_name()
            << " returns "
            << body_type
            << "; expected "
            << declared_return_type
            << ".\n";
    }
    
    object_env->exitscope();
    return this->get_return_type();
}

Symbol attr_class::type_check() {
    Expression initializer_expr = this->get_init_expr();
    Symbol declared_type = this->get_type();

    if (this->get_name() == self)
        class_table->semant_error(this) << "Attribute cannot be named self.\n";

    bool declared_type_is_valid = declared_type == SELF_TYPE || class_table->is_type_defined(declared_type);
    if (!declared_type_is_valid) {
        class_table->semant_error(this)
            << "Attribute "
            << this->get_name()
            << " has undefined type "
            << declared_type
            << ".\n";
    }

    if (dynamic_cast<const no_expr_class*>(initializer_expr) != nullptr)
        return declared_type;

    Symbol initializer_type = initializer_expr->type_check();

    if (declared_type_is_valid && !class_table->is_subtype_of(initializer_type, declared_type)) {
        class_table->semant_error(initializer_expr)
            << "Attribute "
            << this->get_name()
            << " has type "
            << declared_type
            << ", initialized with "
            << initializer_type
            << ".\n";
    }
    return declared_type;
}

Symbol int_const_class::type_check() {
    this->set_type(Int);
    return Int;
}

Symbol bool_const_class::type_check() {
    this->set_type(Bool);
    return Bool;
}

Symbol string_const_class::type_check() {
    this->set_type(Str);
    return Str;
}

ostream& ClassTable::semant_error(Class_ c)
{                                                             
    return semant_error(c->get_filename(),c);
}    

ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
{
    error_stream << filename << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream& ClassTable::semant_error(tree_node *t) {
    error_stream << current_class_definition->get_filename() << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream& ClassTable::semant_error()                  
{                                                 
    semant_errors++;                            
    return error_stream;
} 

void raise_error() {
  cerr << "Compilation halted: semantic errors found." << endl;
  exit(1);
}

void program_class::semant()
{
    initialize_constants();

    class_table = new ClassTable(classes);
    object_env = new SymbolTable<Symbol, Symbol>();

    if(!class_table->install_custom_classes(classes))
        raise_error();
    if(!class_table->build_inheritance_graph())
        raise_error();
    if(!class_table->is_class_table_valid())
        raise_error();
    if (class_table->errors())
        raise_error();
    for (const auto& class_entry : class_table->class_lookup)
        register_class_members(class_entry.second);
    for (int i = classes->first(); classes->more(i); i = classes->next(i))
        type_check(classes->nth(i));
    if (class_table->errors())
        raise_error();
}