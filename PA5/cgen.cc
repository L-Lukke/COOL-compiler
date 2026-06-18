#include "cgen.h"
#include "cgen_gc.h"
#include <vector>
#include <map>
#include <algorithm>
#include <set>
#include <queue>
#include <cassert>
#include <string>
#include <cstdlib>


extern void emit_string_constant(ostream& str, char *s);
extern int cgen_debug;

Symbol
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

static const char *gc_init_names[] =
  { "_NoGC_Init", "_GenGC_Init", "_ScnGC_Init" };
static const char *gc_collect_names[] =
  { "_NoGC_Collect", "_GenGC_Collect", "_ScnGC_Collect" };

BoolConst falsebool(FALSE);
BoolConst truebool(TRUE);

void program_class::cgen(ostream &os)
{

  initialize_constants();
  CgenClassTable codegen_classtable(classes, os);
  (void)codegen_classtable;
}


/*
 * Low-level MIPS emission helpers.
 */

static void emit_load(char *dest_reg, int offset, char *source_reg, ostream& s)
{
  s << LW << dest_reg << " " << offset * WORD_SIZE << "(" << source_reg << ")"
    << endl;
}

static void emit_store(char *source_reg, int offset, char *dest_reg, ostream& s)
{
  s << SW << source_reg << " " << offset * WORD_SIZE << "(" << dest_reg << ")"
      << endl;
}

static void emit_load_imm(char *dest_reg, int val, ostream& s)
{ s << LI << dest_reg << " " << val << endl; }

static void emit_load_address(char *dest_reg, const char *address, ostream& s)
{ s << LA << dest_reg << " " << address << endl; }

static void emit_load_address(char *dest_reg, const std::string& address, ostream& s)
{ emit_load_address(dest_reg, address.c_str(), s); }

static void emit_partial_load_address(char *dest_reg, ostream& s)
{ s << LA << dest_reg << " "; }

static void emit_load_bool(char *dest, const BoolConst& b, ostream& s)
{
  emit_partial_load_address(dest,s);
  b.code_ref(s);
  s << endl;
}

static void emit_load_string(char *dest, StringEntry *str, ostream& s)
{
  emit_partial_load_address(dest,s);
  str->code_ref(s);
  s << endl;
}

static void emit_load_int(char *dest, IntEntry *i, ostream& s)
{
  emit_partial_load_address(dest,s);
  i->code_ref(s);
  s << endl;
}

static void emit_move(char *dest_reg, char *source_reg, ostream& s)
{ s << MOVE << dest_reg << " " << source_reg << endl; }

static void emit_neg(char *dest, char *src1, ostream& s)
{ s << NEG << dest << " " << src1 << endl; }

static void emit_add(char *dest, char *src1, char *src2, ostream& s)
{ s << ADD << dest << " " << src1 << " " << src2 << endl; }

static void emit_addu(char *dest, char *src1, char *src2, ostream& s)
{ s << ADDU << dest << " " << src1 << " " << src2 << endl; }

static void emit_addiu(char *dest, char *src1, int imm, ostream& s)
{ s << ADDIU << dest << " " << src1 << " " << imm << endl; }

static void emit_div(char *dest, char *src1, char *src2, ostream& s)
{ s << DIV << dest << " " << src1 << " " << src2 << endl; }

static void emit_mul(char *dest, char *src1, char *src2, ostream& s)
{ s << MUL << dest << " " << src1 << " " << src2 << endl; }

static void emit_sub(char *dest, char *src1, char *src2, ostream& s)
{ s << SUB << dest << " " << src1 << " " << src2 << endl; }

static void emit_sll(char *dest, char *src1, int num, ostream& s)
{ s << SLL << dest << " " << src1 << " " << num << endl; }

static void emit_jalr(char *dest, ostream& s)
{ s << JALR << "\t" << dest << endl; }

static void emit_jal(const char *address, ostream& s)
{ s << JAL << address << endl; }

static void emit_jal_without_address(ostream &s)
{ s << JAL; }

static void emit_return(ostream& s)
{ s << RET << endl; }

static void emit_gc_assign(ostream& s)
{ s << JAL << "_GenGC_Assign" << endl; }

/*
 * Activation-record layout used by emitted methods/initializers after
 * emit_spill_activation_record_registers():
 *   FP + 0: saved return address
 *   FP + 4: saved SELF
 *   FP + 8: saved old FP
 *   FP + 12 and above: actual arguments, from rightmost to leftmost
 *
 * The stack pointer always points to the next free word below the most recently
 * pushed value. Local let/case bindings are addressed relative to SP through
 * cgen_context.
 */

static const int SAVED_RA_OFFSET = 1;
static const int SAVED_SELF_OFFSET = 2;
static const int SAVED_FP_OFFSET = 3;
static const int FORMAL_ARGUMENT_BASE_OFFSET = 3;

static void emit_gc_assign_for_object_field(char *object_reg, int field_offset, ostream& s)
{
  if (cgen_Memmgr == GC_GENGC) {
    emit_addiu(A1, object_reg, field_offset * WORD_SIZE, s);
    emit_gc_assign(s);
  }
}


static void emit_init_ref(Symbol sym, ostream& s)
{ s << sym << CLASSINIT_SUFFIX; }

static void emit_label_ref(int l, ostream &s)
{ s << "label" << l; }

static void emit_protobj_ref(Symbol sym, ostream& s)
{ s << sym << PROTOBJ_SUFFIX; }

static void emit_method_ref(Symbol classname, Symbol methodname, ostream& s)
{ s << classname << METHOD_SEP << methodname; }


static void emit_label_def(int l, ostream &s)
{
  emit_label_ref(l,s);
  s << ":" << endl;
}


static void emit_beq(char *src1, char *src2, int label, ostream &s)
{
  s << BEQ << src1 << " " << src2 << " ";
  emit_label_ref(label,s);
  s << endl;
}

static void emit_bne(char *src1, char *src2, int label, ostream &s)
{
  s << BNE << src1 << " " << src2 << " ";
  emit_label_ref(label,s);
  s << endl;
}

static void emit_bleq(char *src1, char *src2, int label, ostream &s)
{
  s << BLEQ << src1 << " " << src2 << " ";
  emit_label_ref(label,s);
  s << endl;
}

static void emit_blt(char *src1, char *src2, int label, ostream &s)
{
  s << BLT << src1 << " " << src2 << " ";
  emit_label_ref(label,s);
  s << endl;
}




static void emit_push(char *reg, ostream& str)
{
  emit_store(reg,0,SP,str);
  emit_addiu(SP,SP,-4,str);
}

static void emit_pop(char *reg, ostream& str)
{
  emit_addiu(SP,SP,4,str);
  emit_load(reg,0,SP,str);
}

static void emit_pop_without_load(ostream& str)
{
  emit_addiu(SP,SP,4,str);
}


static void emit_fetch_int(char *dest, char *source, ostream& s)
{ emit_load(dest, DEFAULT_OBJFIELDS, source, s); }


static void emit_store_int(char *source, char *dest, ostream& s)
{ emit_store(source, DEFAULT_OBJFIELDS, dest, s); }





static void emit_spill_activation_record_registers(ostream& str) {
  emit_addiu(SP, SP, -12, str);
  emit_store(FP, SAVED_FP_OFFSET, SP, str);
  emit_store(SELF, SAVED_SELF_OFFSET, SP, str);
  emit_store(RA, SAVED_RA_OFFSET, SP, str);
}

static void emit_restore_activation_record_registers(ostream& str) {
  emit_load(RA, SAVED_RA_OFFSET, SP, str);
  emit_load(SELF, SAVED_SELF_OFFSET, SP, str);
  emit_load(FP, SAVED_FP_OFFSET, SP, str);
  emit_addiu(SP, SP, 12, str);
}

static void emit_setup_frame_pointer(ostream&str) {
  emit_addiu(FP, SP, 4, str);
}

static void emit_setup_self_pointer(ostream&str) {
  emit_move(SELF, ACC, str);
}

static void emit_jump_to_label(int label, ostream &s) {
  s << "\tj\t";
  emit_label_ref(label, s);
  s << endl;
}



/*
 * Constant object emission.
 */

void StringEntry::code_ref(ostream& s)
{
  s << STRCONST_PREFIX << index;
}


void StringEntry::code_def(ostream& s, int stringclasstag)
{
  IntEntryP lensym = inttable.add_int(len);


  s << WORD << "-1" << endl;

  code_ref(s);  s  << LABEL
      << WORD << stringclasstag << endl
      << WORD << (DEFAULT_OBJFIELDS + STRING_SLOTS + (len+4)/4) << endl
      << WORD;

  s << STRINGNAME << DISPTAB_SUFFIX;

      s << endl;
      s << WORD;  lensym->code_ref(s);  s << endl;
  emit_string_constant(s,str);
  s << ALIGN;
}


void StrTable::code_string_table(ostream& s, int stringclasstag)
{
  for (List<StringEntry> *l = tbl; l; l = l->tl())
    l->hd()->code_def(s,stringclasstag);
}


void IntEntry::code_ref(ostream &s)
{
  s << INTCONST_PREFIX << index;
}


void IntEntry::code_def(ostream &s, int intclasstag)
{

  s << WORD << "-1" << endl;

  code_ref(s);  s << LABEL
      << WORD << intclasstag << endl
      << WORD << (DEFAULT_OBJFIELDS + INT_SLOTS) << endl
      << WORD;

  s << INTNAME << DISPTAB_SUFFIX;

      s << endl;
      s << WORD << str << endl;
}


void IntTable::code_string_table(ostream &s, int intclasstag)
{
  for (List<IntEntry> *l = tbl; l; l = l->tl())
    l->hd()->code_def(s,intclasstag);
}


BoolConst::BoolConst(int i) : val(i) { assert(i == 0 || i == 1); }

void BoolConst::code_ref(ostream& s) const
{
  s << BOOLCONST_PREFIX << val;
}


void BoolConst::code_def(ostream& s, int boolclasstag)
{

  s << WORD << "-1" << endl;

  code_ref(s);  s << LABEL
      << WORD << boolclasstag << endl
      << WORD << (DEFAULT_OBJFIELDS + BOOL_SLOTS) << endl
      << WORD;

  s << BOOLNAME << DISPTAB_SUFFIX;

      s << endl;
      s << WORD << val << endl;
}


/*
 * Global tables, class graph construction, and class layout.
 */

void CgenClassTable::code_global_data()
{
  Symbol main    = idtable.lookup_string(MAINNAME);
  Symbol string  = idtable.lookup_string(STRINGNAME);
  Symbol integer = idtable.lookup_string(INTNAME);

  str << "\t.data\n" << ALIGN;


  str << GLOBAL << CLASSNAMETAB << endl;
  str << GLOBAL; emit_protobj_ref(main,str);    str << endl;
  str << GLOBAL; emit_protobj_ref(integer,str); str << endl;
  str << GLOBAL; emit_protobj_ref(string,str);  str << endl;
  str << GLOBAL; falsebool.code_ref(str);  str << endl;
  str << GLOBAL; truebool.code_ref(str);   str << endl;
  str << GLOBAL << INTTAG << endl;
  str << GLOBAL << BOOLTAG << endl;
  str << GLOBAL << STRINGTAG << endl;
  str << INTTAG << LABEL
      << WORD << intclasstag << endl;
  str << BOOLTAG << LABEL
      << WORD << boolclasstag << endl;
  str << STRINGTAG << LABEL
      << WORD << stringclasstag << endl;
}


void CgenClassTable::code_global_text()
{
  str << GLOBAL << HEAP_START << endl
      << HEAP_START << LABEL
      << WORD << 0 << endl
      << "\t.text" << endl
      << GLOBAL;
  emit_init_ref(idtable.add_string("Main"), str);
  str << endl << GLOBAL;
  emit_init_ref(idtable.add_string("Int"),str);
  str << endl << GLOBAL;
  emit_init_ref(idtable.add_string("String"),str);
  str << endl << GLOBAL;
  emit_init_ref(idtable.add_string("Bool"),str);
  str << endl << GLOBAL;
  emit_method_ref(idtable.add_string("Main"), idtable.add_string("main"), str);
  str << endl;
}

void CgenClassTable::code_bools(int boolclasstag)
{
  falsebool.code_def(str,boolclasstag);
  truebool.code_def(str,boolclasstag);
}

void CgenClassTable::code_select_gc()
{


  str << GLOBAL << "_MemMgr_INITIALIZER" << endl;
  str << "_MemMgr_INITIALIZER:" << endl;
  str << WORD << gc_init_names[cgen_Memmgr] << endl;
  str << GLOBAL << "_MemMgr_COLLECTOR" << endl;
  str << "_MemMgr_COLLECTOR:" << endl;
  str << WORD << gc_collect_names[cgen_Memmgr] << endl;
  str << GLOBAL << "_MemMgr_TEST" << endl;
  str << "_MemMgr_TEST:" << endl;
  str << WORD << (cgen_Memmgr_Test == GC_TEST) << endl;
}


void CgenClassTable::code_constants()
{


  stringtable.add_string("");
  inttable.add_string("0");

  stringtable.code_string_table(str,stringclasstag);
  inttable.code_string_table(str,intclasstag);
  code_bools(boolclasstag);
}


CgenClassTable::CgenClassTable(Classes classes, ostream& s) : nds(NULL) , str(s)
{
    current_classtag = 5;
    stringclasstag = 4;
    intclasstag = 3;
    boolclasstag = 2;
    ioclasstag     = 1;
    objectclasstag = 0;
    objectparenttag = INVALIDPARENTTAG;

    enterscope();
    if (cgen_debug) cout << "Building CgenClassTable" << endl;
    install_basic_classes();
    install_classes(classes);
    build_inheritance_tree();
    code();
    exitscope();
}

void CgenClassTable::install_basic_classes()
{
  Symbol filename = stringtable.add_string("<basic class>");


  addid(No_class,
	new CgenNode(class_(No_class,No_class,nil_Features(),filename),
			    Basic,this));
  addid(SELF_TYPE,
	new CgenNode(class_(SELF_TYPE,No_class,nil_Features(),filename),
			    Basic,this));
  addid(prim_slot,
	new CgenNode(class_(prim_slot,No_class,nil_Features(),filename),
			    Basic,this));


  install_class(
   new CgenNode(
    class_(Object,
	   No_class,
	   append_Features(
           append_Features(
           single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
           single_Features(method(type_name, nil_Formals(), Str, no_expr()))),
           single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
	   filename),
    Basic,this));


   install_class(
    new CgenNode(
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
	   filename),
    Basic,this));


   install_class(
    new CgenNode(
     class_(Int,
	    Object,
            single_Features(attr(val, prim_slot, no_expr())),
	    filename),
     Basic,this));


    install_class(
     new CgenNode(
      class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())),filename),
      Basic,this));


   install_class(
    new CgenNode(
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
	     filename),
        Basic,this));

}


void CgenClassTable::install_class(CgenNodeP nd)
{
  Symbol name = nd->get_name();

  if (probe(name))
    {
      return;
    }


  nds = new List<CgenNode>(nd,nds);
  addid(name,nd);
}

void CgenClassTable::install_classes(Classes cs)
{
  for(int i = cs->first(); cs->more(i); i = cs->next(i))
    install_class(new CgenNode(cs->nth(i),NotBasic,this));
}


void CgenClassTable::build_inheritance_tree()
{
  for(List<CgenNode> *l = nds; l; l = l->tl())
      set_relations(l->hd());
}


void CgenClassTable::set_relations(CgenNodeP nd)
{
  CgenNode *parent_node = probe(nd->get_parent());

  if (parent_node == NULL) {
    cerr << "fatal: missing parent in inheritance tree for class "
         << nd->get_name() << endl;
    abort();
  }

  nd->set_parentnd(parent_node);
  parent_node->add_child(nd);
}

void CgenNode::add_child(CgenNodeP n)
{
  children = new List<CgenNode>(n,children);
}

void CgenNode::set_parentnd(CgenNodeP p)
{
  assert(parentnd == NULL);
  assert(p != NULL);
  parentnd = p;
}

void CgenClassTable::code()
{
  if (cgen_debug) cout << "coding global data" << endl;
  code_global_data();

  if (cgen_debug) cout << "choosing gc" << endl;
  code_select_gc();

  if (cgen_debug) cout << "coding constants" << endl;
  code_constants();

  this->traverse_inheritance_tree();
  this->construct_protObjs();
  this->emit_nameTab();
  this->emit_objTab();
  this->emit_parentTab();
  this->emit_dispatch_tables();
  this->emit_protObjs();

  if (cgen_debug) cout << "coding global text" << endl;
  code_global_text();

  this->emit_initialisers();
  this->emit_class_methods();
}

CgenNodeP CgenClassTable::root()
{
  return probe(Object);
}


CgenNode::CgenNode(Class_ nd, Basicness bstatus, CgenClassTableP ct) :
   class__class((const class__class &) *nd),
   parentnd(NULL),
   children(NULL),
   basic_status(bstatus)
{
   (void)ct;
   this->c = nd;
   stringtable.add_string(name->get_string());
}


/*
 * Feature collection and layout ordering helpers.
 */

static std::vector<Symbol> collect_method_names(Class_ class_definition)
{
  std::vector<Symbol> names;
  Features features = class_definition->get_features();

  for (int i = features->first(); features->more(i); i = features->next(i)) {
    Feature feature = features->nth(i);
    if (feature->is_method()) {
      method_class *method = static_cast<method_class *>(feature);
      names.push_back(method->get_name());
    }
  }

  return names;
}

static std::map<Symbol, method_class *> collect_method_definitions(Class_ class_definition)
{
  std::map<Symbol, method_class *> definitions;
  Features features = class_definition->get_features();

  for (int i = features->first(); features->more(i); i = features->next(i)) {
    Feature feature = features->nth(i);
    if (feature->is_method()) {
      method_class *method = static_cast<method_class *>(feature);
      definitions[method->get_name()] = method;
    }
  }

  return definitions;
}

static std::vector<Symbol> collect_attribute_names(Class_ class_definition)
{
  std::vector<Symbol> names;
  Features features = class_definition->get_features();

  for (int i = features->first(); features->more(i); i = features->next(i)) {
    Feature feature = features->nth(i);
    if (feature->is_attr()) {
      attr_class *attribute = static_cast<attr_class *>(feature);
      names.push_back(attribute->get_name());
    }
  }

  return names;
}

static std::map<Symbol, attr_class *> collect_attribute_definitions(Class_ class_definition)
{
  std::map<Symbol, attr_class *> definitions;
  Features features = class_definition->get_features();

  for (int i = features->first(); features->more(i); i = features->next(i)) {
    Feature feature = features->nth(i);
    if (feature->is_attr()) {
      attr_class *attribute = static_cast<attr_class *>(feature);
      definitions[attribute->get_name()] = attribute;
    }
  }

  return definitions;
}

static bool is_runtime_class(Symbol name)
{
  return name == Object || name == IO || name == Int || name == Bool || name == Str;
}

static bool has_initializer(Expression expression)
{
  return dynamic_cast<no_expr_class *>(expression) == 0;
}

struct ClassTagLess
{
  const std::map<Symbol, cgen_class_definition> *definitions;

  ClassTagLess(const std::map<Symbol, cgen_class_definition>& definitions_)
    : definitions(&definitions_)
  { }

  bool operator()(Symbol lhs, Symbol rhs) const
  {
    std::map<Symbol, cgen_class_definition>::const_iterator lhs_definition = definitions->find(lhs);
    std::map<Symbol, cgen_class_definition>::const_iterator rhs_definition = definitions->find(rhs);

    assert(lhs_definition != definitions->end());
    assert(rhs_definition != definitions->end());

    return lhs_definition->second.tag < rhs_definition->second.tag;
  }
};

struct BranchTagGreater
{
  const std::map<Symbol, int> *tags;

  BranchTagGreater(const std::map<Symbol, int>& tags_)
    : tags(&tags_)
  { }

  bool operator()(Symbol lhs, Symbol rhs) const
  {
    std::map<Symbol, int>::const_iterator lhs_tag = tags->find(lhs);
    std::map<Symbol, int>::const_iterator rhs_tag = tags->find(rhs);

    assert(lhs_tag != tags->end());
    assert(rhs_tag != tags->end());

    return lhs_tag->second > rhs_tag->second;
  }
};

void CgenClassTable::attach_inherited_definitions_to(Class_ class_definition, Class_ parent_definition)
{
  Symbol class_name = class_definition->get_name();
  Symbol parent_name = parent_definition->get_name();

  class_definitions[class_name] = class_definition;
  inheritance_parent[class_name] = parent_name;

  class_methods[class_name] = class_methods[parent_name];
  class_method_defs[class_name] = class_method_defs[parent_name];
  class_method_defined_in[class_name] = class_method_defined_in[parent_name];

  class_attributes[class_name] = class_attributes[parent_name];
  class_attribute_defs[class_name] = class_attribute_defs[parent_name];
}

void CgenClassTable::register_properties_and_definitions_of(Class_ class_definition)
{
  Symbol class_name = class_definition->get_name();
  std::vector<Symbol> method_names = collect_method_names(class_definition);
  std::vector<Symbol> attribute_names = collect_attribute_names(class_definition);
  std::map<Symbol, method_class *> method_definitions = collect_method_definitions(class_definition);
  std::map<Symbol, attr_class *> attribute_definitions = collect_attribute_definitions(class_definition);
  std::vector<Symbol>& dispatch_table = class_methods[class_name];

  for (std::vector<Symbol>::iterator it = method_names.begin(); it != method_names.end(); ++it) {
    Symbol method_name = *it;

    if (std::find(dispatch_table.begin(), dispatch_table.end(), method_name) == dispatch_table.end()) {
      dispatch_table.push_back(method_name);
    }

    class_method_defined_in[class_name][method_name] = class_name;
    class_method_defs[class_name][method_name] = method_definitions[method_name];
  }

  for (std::vector<Symbol>::iterator it = attribute_names.begin(); it != attribute_names.end(); ++it) {
    Symbol attribute_name = *it;

    assert(class_attribute_defs[class_name].find(attribute_name) == class_attribute_defs[class_name].end());
    class_attributes[class_name].push_back(attribute_name);
    class_directly_owned_attributes[class_name].insert(attribute_name);
    class_attribute_defs[class_name][attribute_name] = attribute_definitions[attribute_name];
  }
}

void CgenClassTable::traverse_inheritance_tree()
{
  std::queue<CgenNodeP> pending;

  pending.push(root());
  classtag_of[Object] = get_classtag_for(Object);

  while (!pending.empty()) {
    CgenNodeP node = pending.front();
    pending.pop();

    Class_ class_definition = node->get_class_definition();
    class_definitions[class_definition->get_name()] = class_definition;

    if (node->get_parentnd()) {
      Class_ parent_definition = node->get_parentnd()->get_class_definition();
      attach_inherited_definitions_to(class_definition, parent_definition);
    }

    register_properties_and_definitions_of(class_definition);

    List<CgenNode> *children = node->get_children();
    while (children) {
      CgenNodeP child = children->hd();
      Symbol child_name = child->get_class_definition()->get_name();

      classtag_of[child_name] = get_classtag_for(child_name);
      pending.push(child);
      children = children->tl();
    }
  }
}

void CgenClassTable::construct_protObjs()
{
  for (List<CgenNode> *node = nds; node; node = node->tl()) {
    Class_ class_definition = node->hd()->get_class_definition();
    Symbol class_name = class_definition->get_name();

    cgen_class_definition_of[class_name] = construct_cgen_class_definition(class_definition);

    std::map<Symbol, int>& method_offsets = cgen_class_definition_of[class_name].method_offset;
    for (std::map<Symbol, int>::iterator it = method_offsets.begin(); it != method_offsets.end(); ++it) {
      dispatch_offsets_of_class_methods[class_name][it->first] = it->second;
    }

    cgen_class_names.push_back(class_name);
  }

  std::sort(cgen_class_names.begin(), cgen_class_names.end(), ClassTagLess(cgen_class_definition_of));
}

int CgenClassTable::next_classtag() { return current_classtag++; }
int CgenClassTable::get_classtag_for(Symbol type) {
  if (type == Object)
    return objectclasstag;
  else if (type == Bool)
    return boolclasstag;
  else if (type == Int)
    return intclasstag;
  else if (type == Str)
    return stringclasstag;
  else if (type == IO)
    return ioclasstag;
  else
    return next_classtag();
}


cgen_class_definition CgenClassTable::construct_cgen_class_definition(Class_ class_definition)
{
  Symbol class_name = class_definition->get_name();
  cgen_class_definition result;

  result.name = class_name;
  result.is_primitive_type = is_runtime_class(class_name);
  result.tag = classtag_of[class_name];
  result.dispatch_table = class_methods[class_name];
  result.attrs = class_attributes[class_name];
  result.method_definitions = class_method_defs[class_name];
  result.attr_definitions = class_attribute_defs[class_name];

  int attribute_offset = DEFAULT_OBJFIELDS;
  for (std::vector<Symbol>::iterator it = result.attrs.begin(); it != result.attrs.end(); ++it) {
    result.attr_offset[*it] = attribute_offset++;
  }

  int method_offset = 0;
  for (std::vector<Symbol>::iterator it = result.dispatch_table.begin(); it != result.dispatch_table.end(); ++it) {
    result.method_offset[*it] = method_offset++;
  }

  result.method_definition_containing_class = class_method_defined_in[class_name];
  result.directly_owned_attrs = class_directly_owned_attributes[class_name];
  return result;
}

void CgenClassTable::emit_nameTab()
{
  str << CLASSNAMETAB << LABEL;

  for (std::vector<Symbol>::iterator it = cgen_class_names.begin(); it != cgen_class_names.end(); ++it) {
    str << WORD;
    stringtable.lookup_string((*it)->get_string())->code_ref(str);
    str << endl;
  }
}

void CgenClassTable::emit_objTab()
{
  str << CLASSOBJTAB << LABEL;

  for (std::vector<Symbol>::iterator it = cgen_class_names.begin(); it != cgen_class_names.end(); ++it) {
    str << WORD;
    emit_protobj_ref(*it, str);
    str << endl << WORD;
    emit_init_ref(*it, str);
    str << endl;
  }
}

void CgenClassTable::emit_default_value_for_attr_type(Symbol type)
{
  if (type == Int) {
    inttable.lookup_string("0")->code_ref(str);
  } else if (type == Bool) {
    falsebool.code_ref(str);
  } else if (type == Str) {
    stringtable.lookup_string("")->code_ref(str);
  } else {
    str << 0;
  }
}

void CgenClassTable::emit_protObj_from(const cgen_class_definition& class_definition)
{
  str << WORD << -1 << endl;
  str << class_definition.name << PROTOBJ_SUFFIX << LABEL;
  str << WORD << class_definition.tag << endl;
  str << WORD << class_definition.size() << endl;
  str << WORD << class_definition.name << DISPTAB_SUFFIX << endl;

  for (std::vector<Symbol>::const_iterator it = class_definition.attrs.begin();
       it != class_definition.attrs.end();
       ++it) {
    Symbol attribute_name = *it;
    std::map<Symbol, attr_class *>::const_iterator attribute_definition =
      class_definition.attr_definitions.find(attribute_name);

    assert(attribute_definition != class_definition.attr_definitions.end());

    str << WORD;
    emit_default_value_for_attr_type(attribute_definition->second->get_type());
    str << endl;
  }
}

void CgenClassTable::emit_protObjs()
{
  for (std::vector<Symbol>::iterator it = cgen_class_names.begin(); it != cgen_class_names.end(); ++it) {
    emit_protObj_from(cgen_class_definition_of[*it]);
  }
}

void CgenClassTable::emit_dispatch_table(const cgen_class_definition& class_definition)
{
  str << class_definition.name << DISPTAB_SUFFIX << LABEL;

  for (std::vector<Symbol>::const_iterator it = class_definition.dispatch_table.begin();
       it != class_definition.dispatch_table.end();
       ++it) {
    Symbol method_name = *it;
    std::map<Symbol, Symbol>::const_iterator defining_class =
      class_definition.method_definition_containing_class.find(method_name);

    assert(defining_class != class_definition.method_definition_containing_class.end());

    str << WORD;
    emit_method_ref(defining_class->second, method_name, str);
    str << endl;
  }
}

void CgenClassTable::emit_dispatch_tables()
{
  for (std::vector<Symbol>::iterator it = cgen_class_names.begin(); it != cgen_class_names.end(); ++it) {
    emit_dispatch_table(cgen_class_definition_of[*it]);
  }
}

void CgenClassTable::emit_initialiser(const cgen_class_definition& class_definition)
{
  str << class_definition.name << CLASSINIT_SUFFIX << LABEL;

  emit_spill_activation_record_registers(str);
  emit_setup_frame_pointer(str);
  emit_setup_self_pointer(str);

  if (class_definition.name != Object) {
    emit_jal_without_address(str);
    str << inheritance_parent[class_definition.name] << CLASSINIT_SUFFIX << endl;
  }

  cgen_context context;
  context.self_name = class_definition.name;
  context.class_attr_offset = class_definition.attr_offset;
  context.self_class_definition = class_definitions[class_definition.name];
  context.dispatch_offsets_of_class_methods = dispatch_offsets_of_class_methods;
  context.classtag_of = classtag_of;

  for (std::vector<Symbol>::const_iterator it = class_definition.attrs.begin();
       it != class_definition.attrs.end();
       ++it) {
    Symbol attribute_name = *it;
    std::map<Symbol, attr_class *>::const_iterator attribute_definition =
      class_definition.attr_definitions.find(attribute_name);
    std::map<Symbol, int>::const_iterator attribute_offset =
      class_definition.attr_offset.find(attribute_name);

    assert(attribute_definition != class_definition.attr_definitions.end());
    assert(attribute_offset != class_definition.attr_offset.end());

    attr_class *attribute = attribute_definition->second;
    Expression initializer = attribute->get_init_expr();

    if (class_definition.is_directly_owned(attribute_name) && has_initializer(initializer)) {
      initializer->code(str, context);
      emit_store(ACC, attribute_offset->second, SELF, str);
      emit_gc_assign_for_object_field(SELF, attribute_offset->second, str);
    }
  }

  emit_move(ACC, SELF, str);
  emit_restore_activation_record_registers(str);
  emit_return(str);
}

void CgenClassTable::emit_initialisers()
{
  for (std::vector<Symbol>::iterator it = cgen_class_names.begin(); it != cgen_class_names.end(); ++it) {
    emit_initialiser(cgen_class_definition_of[*it]);
  }
}

void CgenClassTable::emit_parentTab()
{
  str << CLASSPARENTTAB << LABEL;

  for (std::vector<Symbol>::iterator it = cgen_class_names.begin(); it != cgen_class_names.end(); ++it) {
    Symbol class_name = *it;

    if (class_name == Object) {
      str << WORD << objectparenttag << endl;
    } else {
      str << WORD << classtag_of[inheritance_parent[class_name]] << endl;
    }
  }
}

static std::map<Symbol, int> build_method_args_lookup(method_class* method_definition) {
  std::map<Symbol, int> arg_offset_map;
  Formals formals = method_definition->get_formals();
  int formal_count = 0;

  for (int i = formals->first(); formals->more(i); i = formals->next(i)) {
    formal_count++;
  }

  int argument_offset = formal_count - 1;
  for (int i = formals->first(); formals->more(i); i = formals->next(i)) {
    Formal formal = formals->nth(i);
    arg_offset_map[formal->get_name()] = argument_offset;
    argument_offset--;
  }

  return arg_offset_map;
}

void CgenClassTable::emit_method(const cgen_class_definition& cgen_definition, method_class* method_definition)
{
  cgen_context ctx;
  ctx.self_name = cgen_definition.name;
  ctx.class_attr_offset = cgen_definition.attr_offset;
  ctx.method_attr_offset = build_method_args_lookup(method_definition);
  ctx.self_class_definition = class_definitions[cgen_definition.name];
  ctx.dispatch_offsets_of_class_methods = dispatch_offsets_of_class_methods;
  ctx.method_name = method_definition->get_name();
  ctx.classtag_of = classtag_of;

  emit_method_ref(ctx.self_name, ctx.method_name, str);
  str << LABEL;
  emit_spill_activation_record_registers(str);
  emit_setup_frame_pointer(str);
  emit_setup_self_pointer(str);

  method_definition->get_body_expr()->code(str, ctx);

  emit_restore_activation_record_registers(str);

  for (size_t i = 0; i < ctx.method_attr_offset.size(); i++) {
    emit_pop_without_load(str);
  }
  emit_return(str);
}

void CgenClassTable::emit_methods(const cgen_class_definition& class_definition)
{
  for (std::map<Symbol, method_class *>::const_iterator it = class_definition.method_definitions.begin();
       it != class_definition.method_definitions.end();
       ++it) {
    if (class_definition.is_defining_method(it->first)) {
      emit_method(class_definition, it->second);
    }
  }
}

void CgenClassTable::emit_class_methods()
{
  for (std::vector<Symbol>::iterator it = cgen_class_names.begin(); it != cgen_class_names.end(); ++it) {
    const cgen_class_definition& class_definition = cgen_class_definition_of[*it];

    if (!class_definition.is_primitive_type) {
      emit_methods(class_definition);
    }
  }
}


/*
 * Expression code generation.
 */

int current_label_ix = 0;
int next_label()
{
  return current_label_ix++;
}

static int emit_actual_arguments(Expressions actuals, ostream& s, cgen_context& ctx)
{
  int count = 0;

  for (int i = actuals->first(); actuals->more(i); i = actuals->next(i)) {
    Expression actual = actuals->nth(i);
    actual->code(s, ctx);
    emit_push(ACC, s);
    ctx.push_scope_identifier(No_type);
    count++;
  }

  return count;
}

static void restore_context_after_actual_arguments(int count, cgen_context& ctx)
{
  for (int i = 0; i < count; i++) {
    ctx.pop_scope_identifier();
  }
}

static void emit_abort_with_filename(Class_ current_class, int line_number, const char *abort_routine, ostream& s)
{
  emit_partial_load_address(ACC, s);
  stringtable.lookup_string(current_class->get_filename()->get_string())->code_ref(s);
  s << endl;
  emit_load_imm(T1, line_number, s);
  emit_jal(abort_routine, s);
}

static void emit_default_value(Symbol type, ostream& s)
{
  if (type == Str) {
    emit_load_string(ACC, stringtable.lookup_string(""), s);
  } else if (type == Int) {
    emit_load_int(ACC, inttable.lookup_string("0"), s);
  } else if (type == Bool) {
    emit_load_bool(ACC, falsebool, s);
  } else {
    emit_move(ACC, ZERO, s);
  }
}

typedef void (*BinaryIntEmitter)(char *, char *, char *, ostream&);

static void emit_binary_integer_expression(Expression left, Expression right, BinaryIntEmitter emit_operation, ostream& s, cgen_context ctx)
{
  left->code(s, ctx);
  emit_push(ACC, s);
  ctx.push_scope_identifier(No_type);

  right->code(s, ctx);
  emit_jal("Object.copy", s);

  emit_pop(T1, s);
  ctx.pop_scope_identifier();

  emit_fetch_int(T1, T1, s);
  emit_fetch_int(T2, ACC, s);
  emit_operation(T3, T1, T2, s);
  emit_store_int(T3, ACC, s);
}

static void emit_integer_comparison(Expression left, Expression right, ostream& s, cgen_context ctx)
{
  left->code(s, ctx);
  emit_push(ACC, s);
  ctx.push_scope_identifier(No_type);

  right->code(s, ctx);
  emit_pop(T1, s);
  ctx.pop_scope_identifier();

  emit_move(T2, ACC, s);
  emit_fetch_int(T1, T1, s);
  emit_fetch_int(T2, T2, s);
}

void assign_class::code(ostream &s, cgen_context ctx) {
  expr->code(s, ctx);

  int scope_stack_offset = ctx.get_scope_identifier_offset(name);
  int method_arg_offset = ctx.get_method_attr_offset(name);
  int class_attr_offset = ctx.get_class_attribute_identifier_offset(name);

  if (scope_stack_offset != -1) {
    emit_store(ACC, scope_stack_offset + 1, SP, s);
    return;
  }

  if (method_arg_offset != -1) {
    emit_store(ACC, FORMAL_ARGUMENT_BASE_OFFSET + method_arg_offset, FP, s);
    return;
  }

  if (class_attr_offset != -1) {
    emit_store(ACC, class_attr_offset, SELF, s);
    emit_gc_assign_for_object_field(SELF, class_attr_offset, s);
    return;
  }

  assert(false);
}

void static_dispatch_class::code(ostream &s, cgen_context ctx)
{
  Symbol dispatch_type = type_name;
  int receiver_is_not_void = next_label();
  int dispatch_offset = ctx.get_class_method_dispatch_offset(dispatch_type, name);
  int actual_count = emit_actual_arguments(actual, s, ctx);

  expr->code(s, ctx);

  emit_bne(ACC, ZERO, receiver_is_not_void, s);
  emit_abort_with_filename(ctx.self_class_definition, get_line_number(), "_dispatch_abort", s);

  emit_label_def(receiver_is_not_void, s);
  emit_load_address(T1, std::string(dispatch_type->get_string()) + DISPTAB_SUFFIX, s);
  emit_load(T1, dispatch_offset, T1, s);
  emit_jalr(T1, s);

  restore_context_after_actual_arguments(actual_count, ctx);
}

void dispatch_class::code(ostream &s, cgen_context ctx)
{
  Symbol dispatch_type = expr->get_type() == SELF_TYPE ? ctx.self_name : expr->get_type();
  int receiver_is_not_void = next_label();
  int dispatch_offset = ctx.get_class_method_dispatch_offset(dispatch_type, name);
  int actual_count = emit_actual_arguments(actual, s, ctx);

  expr->code(s, ctx);

  emit_bne(ACC, ZERO, receiver_is_not_void, s);
  emit_abort_with_filename(ctx.self_class_definition, get_line_number(), "_dispatch_abort", s);

  emit_label_def(receiver_is_not_void, s);
  emit_load(T1, 2, ACC, s);
  emit_load(T1, dispatch_offset, T1, s);
  emit_jalr(T1, s);

  restore_context_after_actual_arguments(actual_count, ctx);
}

void cond_class::code(ostream &s, cgen_context ctx) {
  int predicate_fails = next_label();
  int done_label = next_label();

  pred->code(s, ctx);
  emit_fetch_int(T1, ACC, s);
  emit_beq(T1, ZERO, predicate_fails, s);

  then_exp->code(s, ctx);
  emit_jump_to_label(done_label, s);

  emit_label_def(predicate_fails, s);
  else_exp->code(s, ctx);

  emit_label_def(done_label, s);
}

void loop_class::code(ostream &s, cgen_context ctx) {
  int loop_begin_label = next_label();
  int loop_exit_label = next_label();

  emit_label_def(loop_begin_label, s);

  pred->code(s, ctx);
  emit_fetch_int(T1, ACC, s);
  emit_beq(T1, ZERO, loop_exit_label, s);

  body->code(s, ctx);
  emit_jump_to_label(loop_begin_label, s);

  emit_label_def(loop_exit_label, s);

  emit_move(ACC, ZERO, s);
}

void typcase_class::code(ostream &s, cgen_context ctx)
{
  int case_expression_is_not_void = next_label();
  int no_branch_matches = next_label();
  int case_done = next_label();
  int ancestor_scan = next_label();

  std::map<Symbol, branch_class *> branch_by_type;
  std::map<Symbol, int> tag_by_branch_type;
  std::map<Symbol, int> label_by_branch_type;
  std::vector<Symbol> branch_types;

  for (int i = cases->first(); cases->more(i); i = cases->next(i)) {
    branch_class *branch = static_cast<branch_class *>(cases->nth(i));
    Symbol branch_type = branch->get_declaration_type();

    branch_by_type[branch_type] = branch;
    tag_by_branch_type[branch_type] = ctx.classtag_of[branch_type];
    label_by_branch_type[branch_type] = next_label();
    branch_types.push_back(branch_type);
  }

  std::sort(branch_types.begin(), branch_types.end(), BranchTagGreater(tag_by_branch_type));

  expr->code(s, ctx);

  emit_bne(ACC, ZERO, case_expression_is_not_void, s);
  emit_abort_with_filename(ctx.self_class_definition, get_line_number(), "_case_abort2", s);

  emit_label_def(case_expression_is_not_void, s);
  emit_load(T1, TAG_OFFSET, ACC, s);

  emit_label_def(ancestor_scan, s);
  emit_load_imm(T2, INVALIDPARENTTAG, s);
  emit_beq(T1, T2, no_branch_matches, s);

  for (std::vector<Symbol>::iterator it = branch_types.begin(); it != branch_types.end(); ++it) {
    Symbol branch_type = *it;

    emit_load_imm(T2, tag_by_branch_type[branch_type], s);
    emit_beq(T1, T2, label_by_branch_type[branch_type], s);
  }

  emit_load_address(T2, CLASSPARENTTAB, s);
  emit_sll(T1, T1, 2, s);
  emit_addu(T1, T1, T2, s);
  emit_load(T1, 0, T1, s);
  emit_jump_to_label(ancestor_scan, s);

  for (std::vector<Symbol>::iterator it = branch_types.begin(); it != branch_types.end(); ++it) {
    Symbol branch_type = *it;
    branch_class *branch = branch_by_type[branch_type];

    emit_label_def(label_by_branch_type[branch_type], s);
    emit_push(ACC, s);
    ctx.push_scope_identifier(branch->get_name());
    branch->expr->code(s, ctx);
    emit_pop_without_load(s);
    ctx.pop_scope_identifier();
    emit_jump_to_label(case_done, s);
  }

  emit_label_def(no_branch_matches, s);
  emit_load(T1, TAG_OFFSET, ACC, s);
  emit_load_address(T2, CLASSNAMETAB, s);
  emit_sll(T1, T1, 2, s);
  emit_addu(T2, T2, T1, s);
  emit_load(ACC, 0, T2, s);
  emit_jal("_case_abort", s);

  emit_label_def(case_done, s);
}

void block_class::code(ostream &s, cgen_context ctx)
{
  for (int i = body->first(); body->more(i); i = body->next(i)) {
    body->nth(i)->code(s, ctx);
  }
}

void let_class::code(ostream &s, cgen_context ctx)
{
  if (has_initializer(init)) {
    init->code(s, ctx);
  } else {
    emit_default_value(type_decl, s);
  }

  ctx.push_scope_identifier(identifier);
  emit_push(ACC, s);

  body->code(s, ctx);

  emit_pop_without_load(s);
  ctx.pop_scope_identifier();
}

void plus_class::code(ostream &s, cgen_context ctx)
{
  emit_binary_integer_expression(e1, e2, emit_add, s, ctx);
}

void sub_class::code(ostream &s, cgen_context ctx)
{
  emit_binary_integer_expression(e1, e2, emit_sub, s, ctx);
}

void mul_class::code(ostream &s, cgen_context ctx)
{
  emit_binary_integer_expression(e1, e2, emit_mul, s, ctx);
}

void divide_class::code(ostream &s, cgen_context ctx)
{
  emit_binary_integer_expression(e1, e2, emit_div, s, ctx);
}

void neg_class::code(ostream &s, cgen_context ctx)
{
  e1->code(s, ctx);
  emit_jal("Object.copy", s);
  emit_fetch_int(T1, ACC, s);
  emit_neg(T1, T1, s);
  emit_store_int(T1, ACC, s);
}

void lt_class::code(ostream &s, cgen_context ctx)
{
  int is_true = next_label();
  int done = next_label();

  emit_integer_comparison(e1, e2, s, ctx);
  emit_blt(T1, T2, is_true, s);
  emit_load_bool(ACC, falsebool, s);
  emit_jump_to_label(done, s);
  emit_label_def(is_true, s);
  emit_load_bool(ACC, truebool, s);
  emit_label_def(done, s);
}

void eq_class::code(ostream &s, cgen_context ctx)
{
  int references_are_equal = next_label();
  int done = next_label();

  e1->code(s, ctx);
  emit_push(ACC, s);
  ctx.push_scope_identifier(No_type);

  e2->code(s, ctx);
  emit_pop(T1, s);
  ctx.pop_scope_identifier();

  emit_move(T2, ACC, s);

  Symbol lhs_type = e1->get_type();
  Symbol rhs_type = e2->get_type();
  bool primitive_equality = lhs_type == Int || lhs_type == Str || lhs_type == Bool ||
                            rhs_type == Int || rhs_type == Str || rhs_type == Bool;

  if (primitive_equality) {
    emit_load_bool(ACC, truebool, s);
    emit_load_bool(A1, falsebool, s);
    emit_jal("equality_test", s);
    return;
  }

  emit_beq(T1, T2, references_are_equal, s);
  emit_load_bool(ACC, falsebool, s);
  emit_jump_to_label(done, s);
  emit_label_def(references_are_equal, s);
  emit_load_bool(ACC, truebool, s);
  emit_label_def(done, s);
}

void leq_class::code(ostream &s, cgen_context ctx)
{
  int is_true = next_label();
  int done = next_label();

  emit_integer_comparison(e1, e2, s, ctx);
  emit_bleq(T1, T2, is_true, s);
  emit_load_bool(ACC, falsebool, s);
  emit_jump_to_label(done, s);
  emit_label_def(is_true, s);
  emit_load_bool(ACC, truebool, s);
  emit_label_def(done, s);
}

void comp_class::code(ostream &s, cgen_context ctx)
{
  int is_false = next_label();
  int done = next_label();

  e1->code(s, ctx);
  emit_fetch_int(T1, ACC, s);
  emit_beq(T1, ZERO, is_false, s);
  emit_load_bool(ACC, falsebool, s);
  emit_jump_to_label(done, s);
  emit_label_def(is_false, s);
  emit_load_bool(ACC, truebool, s);
  emit_label_def(done, s);
}

void int_const_class::code(ostream& s, cgen_context ctx)
{
  emit_load_int(ACC,inttable.lookup_string(token->get_string()),s);
}

void string_const_class::code(ostream& s, cgen_context ctx)
{
  emit_load_string(ACC,stringtable.lookup_string(token->get_string()),s);
}

void bool_const_class::code(ostream& s, cgen_context ctx)
{
  emit_load_bool(ACC, BoolConst(val), s);
}

void new__class::code(ostream &s, cgen_context ctx)
{
  if (type_name != SELF_TYPE) {
    std::string prototype_label = std::string(type_name->get_string()) + PROTOBJ_SUFFIX;
    std::string initializer_label = std::string(type_name->get_string()) + CLASSINIT_SUFFIX;

    emit_load_address(ACC, prototype_label, s);
    emit_jal("Object.copy", s);
    emit_jal(initializer_label.c_str(), s);
    return;
  }

  emit_load_address(T1, CLASSOBJTAB, s);
  emit_load(T2, TAG_OFFSET, SELF, s);
  emit_load_imm(T3, 8, s);
  emit_mul(T2, T2, T3, s);
  emit_addu(T1, T1, T2, s);

  emit_push(T1, s);
  emit_load(ACC, 0, T1, s);
  emit_jal("Object.copy", s);
  emit_pop(T1, s);

  emit_load(T1, 1, T1, s);
  emit_jalr(T1, s);
}

void isvoid_class::code(ostream &s, cgen_context ctx) {
  int void_label = next_label();
  int done_label = next_label();

  e1->code(s, ctx);
  emit_move(T1, ACC, s);
  emit_beq(T1, ZERO, void_label, s);
  emit_load_bool(ACC, BoolConst(0), s);
  emit_jump_to_label(done_label, s);
  emit_label_def(void_label, s);
  emit_load_bool(ACC, BoolConst(1), s);
  emit_label_def(done_label, s);
}

void no_expr_class::code(ostream &s, cgen_context ctx) {
  emit_move(ACC, ZERO, s);
}

void object_class::code(ostream &s, cgen_context ctx) {
  int scope_stack_offset = ctx.get_scope_identifier_offset(name);
  int method_arg_offset = ctx.get_method_attr_offset(name);
  int class_attr_offset = ctx.get_class_attribute_identifier_offset(name);

  if (scope_stack_offset != -1) {
    emit_load(ACC, scope_stack_offset + 1, SP, s);
    return;
  }

  if (method_arg_offset != -1) {
    emit_load(ACC, FORMAL_ARGUMENT_BASE_OFFSET + method_arg_offset, FP, s);
    return;
  }

  if (class_attr_offset != -1) {
    emit_load(ACC, class_attr_offset, SELF, s);
    return;
  }

  assert(name == self);
  emit_move(ACC, SELF, s);
}
