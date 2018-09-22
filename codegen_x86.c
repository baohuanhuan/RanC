#include "codegen_x86.h"

char** local_var;
int local_var_num;
int in_function;

// Params node for current function.
// Used for param look up when generating code for function body.
int function_params; 
// Label for function epilog. Needed for return statement.
int return_label;

// Label for breaking current loop.
int break_label;
// Label for continuing current loop.
int continue_label;

int tmp_label_count = 0;

int new_temp_label() {
  return tmp_label_count++;
}

char* get_symbol(int cur) {
  check(node_type[cur] == symbol_node, "get_symbol");
  return node_payload[cur];
}

int get_int(int cur) {
  check(node_type[cur] == int_node, "get_int");
  return atoi(node_payload[cur]);
}

char* get_string(int cur) {
  check(node_type[cur] == string_node, "get_string");
  return node_payload[cur];
}

void register_local_var(char* s) {
  local_var[local_var_num++] = s;
  check(local_var_num <= MAX_LOCAL_VARS, "too many local vars");
}

int lookup_local_var(char* s) {
  if (!in_function) {
    return -1;
  }
  for (int i = 0; i < local_var_num; i++) {
    if (!strcmp(s, local_var[i])) {
      return i;
    }
  }
  return -1;
}

int lookup_param(char* s) {
  if (!in_function) {
    return -1;
  }
  for (int i = 0; i < node_child_num[function_params]; i++) {
    if (!strcmp(s, get_symbol(node_child[function_params][i]))) {
      return i;
    }
  }
  return -1;
}

// search for all var_decl_node and var_init_node in subtree
void register_local_vars(int root) {
  for (int i = 0; i < node_child_num[root]; i++) {
    int cur = node_child[root][i];
    int t = node_type[cur];
    if (t == var_decl_node || t == var_init_node) {
      register_local_var(get_symbol(node_child[cur][0]));
    } else {
      register_local_vars(cur);
    }
  }
}

char* get_set_cmp_inst(int type) {
  if (type == eq_node) {
    return "sete";
  } else if (type == ne_node) {
    return "setne";
  } else if (type == lt_node) {
    return "setl";
  } else if (type == le_node) {
    return "setle";
  } else if (type == gt_node) {
    return "setg";
  } else if (type == ge_node) {
    return "setge";
  }
  return 0;
}

void generate_expr(int expr);

void generate_expr_internal(int expr, int lvalue) {
  int t = node_type[expr];
  int end_label;
  if (t == assignment_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("mov dword ptr [ebx], eax\n");
  } else if (t == add_eq_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("add [ebx], eax\n");
    printf("mov eax, [ebx]\n");
  } else if (t == sub_eq_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("sub [ebx], eax\n");
    printf("mov eax, [ebx]\n");
  } else if (t == mul_eq_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("imul eax, dword ptr [ebx]\n");
    printf("mov [ebx], eax\n");
  } else if (t == div_eq_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("mov ebx, eax\n");
    printf("mov eax, dword ptr [esp]\n");
    printf("mov eax, [eax]\n");
    printf("cdq\n");
    printf("idiv ebx\n");
    printf("pop ebx\n");
    printf("mov [ebx], eax\n");
  } else if (t == or_node) {
    end_label = new_temp_label();
    for (int i = 0; i < node_child_num[expr]; i++) {
      generate_expr(node_child[expr][i]);
      printf("cmp eax, 0\n");
      printf("jnz _%d\n", end_label);
    }
    printf("_%d:\n", end_label);
  } else if (t == and_node) {
    end_label = new_temp_label();
    for (int i = 0; i < node_child_num[expr]; i++) {
      generate_expr(node_child[expr][i]);
      printf("cmp eax, 0\n");
      printf("jz _%d\n", end_label);
    }
    printf("_%d:\n", end_label);
  } else if (t == not_node) {
    generate_expr(node_child[expr][0]);
    printf("cmp eax, 0\n");
    printf("mov eax, 0\n");
    printf("sete al\n");
  } else if (get_set_cmp_inst(t)) {
    generate_expr(node_child[expr][0]);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("cmp ebx, eax\n");
    printf("mov eax, 0\n");
    printf("%s al\n", get_set_cmp_inst(t));
  } else if (t == add_node) {
    generate_expr(node_child[expr][0]);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("add eax, ebx\n");
  } else if (t == sub_node) {
    generate_expr(node_child[expr][0]);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("sub ebx, eax\n");
    printf("mov eax, ebx\n");
  } else if (t == mul_node) {
    generate_expr(node_child[expr][0]);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("pop ebx\n");
    printf("imul eax, ebx\n");
  } else if (t == div_node || t == mod_node) {
    generate_expr(node_child[expr][0]);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("mov ebx, eax\n");
    printf("pop eax\n");
    printf("cdq\n");
    printf("idiv ebx\n");
    if (t == mod_node) {
      printf("mov eax, edx\n");
    }
  } else if (t == int_node || t == char_node) {
    printf("mov eax, %s\n", node_payload[expr]);
  } else if (t == string_node) {
    int string_label = new_temp_label();
    printf(".section .rodata\n");
    printf("_%d:\n", string_label);
    printf(".ascii %s\n", get_string(expr));
    printf(".byte 0\n");
    printf(".section .text\n");
    printf("mov eax, offset _%d\n", string_label);
  } else if(t == symbol_node) {
    char* name = get_symbol(expr);
    int local_index = lookup_local_var(name);
    int param_index = lookup_param(name);
    char* op;
    if (lvalue) {
      op = "lea";
    } else {
      op = "mov";
    }
    if (local_index >= 0) {
      printf("%s eax, [ebp-%d]\n", op, (1 + local_index) * WORD_SIZE);
    } else if (param_index >= 0) {
      printf("%s eax, [ebp+%d]\n", op, (2 + param_index) * WORD_SIZE);
    } else {
      printf("%s eax, [%s]\n", op, name);
    }
  } else if(t == access_node) {
    generate_expr(node_child[expr][0]);
    printf("push eax\n");
    generate_expr(node_child[expr][1]);
    printf("mov ebx, eax\n");
    printf("pop ebx\n");
    char* op;
    if (lvalue) {
      op = "lea";
    } else {
      op = "mov";
    }
    printf("%s eax, dword ptr [eax * %d + ebx]\n", op, WORD_SIZE);
  } else if(t == call_node) {
    int fun = node_child[expr][0];
    check(node_type[fun] == symbol_node, "function has to be a symbol");
    char* fname = get_symbol(fun);
    int args = node_child[expr][1];
    for (int i = node_child_num[args] - 1; i >= 0; i--) {
      generate_expr(node_child[args][i]);
      printf("push eax\n");
    }
    printf("call %s\n", fname);
    printf("add esp, %d\n", node_child_num[args] * WORD_SIZE);
  } else if (t == negative_node) {
    generate_expr(node_child[expr][0]);
    printf("not eax\n");
    printf("add eax, 1\n");
  } else if (t == inc_prefix_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("add dword ptr [eax], 1\n");
    printf("mov eax, dword ptr [eax]\n");
  } else if (t == dec_prefix_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("sub dword ptr [eax], 1\n");
    printf("mov eax, dword ptr [eax]\n");
  } else if (t == inc_suffix_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("add dword ptr [eax], 1\n");
    printf("mov eax, dword ptr [eax]\n");
    printf("sub eax, 1\n");
  } else if (t == dec_suffix_node) {
    generate_expr_internal(node_child[expr][0], 1);
    printf("sub dword ptr [eax], 1\n");
    printf("mov eax, dword ptr [eax]\n");
    printf("add eax, 1\n");
  } else if (t == var_decl_node || t == noop_node) {
    // do nothing
  } else {
    check(0, "unknown expr node type");
  }
}

void generate_expr(int expr) {
  generate_expr_internal(expr, 0);
}

void generate_stmts(int stmts);

void generate_stmt(int stmt) {
  int t = node_type[stmt];
  if (t == if_node) {
    int else_label = new_temp_label();
    int endif_label = new_temp_label();

    generate_expr(node_child[stmt][0]);

    printf("cmp eax, 0\n");
    printf("je _%d\n", else_label);

    generate_stmts(node_child[stmt][1]);

    printf("jmp _%d\n", endif_label);
    printf("_%d:\n", else_label);

    if (node_child_num[stmt] == 3) {
      int else_node = node_child[stmt][2];
      if (node_type[else_node] == stmts_node) {
        generate_stmts(else_node);
      } else {
        // else-if
        generate_stmt(else_node);
      }
    }

    printf("_%d:\n", endif_label);
  } else if (t == while_do_node) {
    int while_label = new_temp_label();
    int endwhile_label = new_temp_label();
    int old_break_label = break_label;
    break_label = endwhile_label;
    int old_continue_label = continue_label;
    continue_label = while_label;
    printf("_%d:\n", while_label);

    generate_expr(node_child[stmt][0]);

    printf("cmp eax, 0\n");
    printf("je _%d\n", endwhile_label);

    generate_stmts(node_child[stmt][1]);

    printf("jmp _%d\n", while_label);
    printf("_%d:\n", endwhile_label);

    break_label = old_break_label;
    continue_label = old_continue_label;
  } else if (t == do_while_node) {
    int while_label = new_temp_label();
    int old_break_label = break_label;
    break_label = new_temp_label();
    int old_continue_label = continue_label;
    continue_label = new_temp_label();

    printf("_%d:\n", while_label);
    generate_stmts(node_child[stmt][0]);
    printf("_%d:\n", continue_label);
    generate_expr(node_child[stmt][1]);
    printf("cmp eax, 0\n");
    printf("jne _%d\n", while_label);
    printf("_%d:\n", break_label);

    break_label = old_break_label;
    continue_label = old_continue_label;
  } else if (t == for_node) {
    int forloop_label = new_temp_label();
    int endfor_label = new_temp_label();
    int old_continue_label = continue_label;
    continue_label = new_temp_label();
    int old_break_label = break_label;
    break_label = endfor_label;
    generate_stmt(node_child[stmt][0]);
    printf("_%d:\n", forloop_label);
    if (node_type[node_child[stmt][1]] != noop_node) {
      generate_expr(node_child[stmt][1]);
      printf("cmp eax, 0\n");
      printf("je _%d\n", endfor_label);
    }
    generate_stmts(node_child[stmt][3]);
    printf("_%d:\n", continue_label);
    generate_expr(node_child[stmt][2]);
    printf("jmp _%d\n", forloop_label);
    printf("_%d:\n", endfor_label);

    break_label = old_break_label;
    continue_label = old_continue_label;
  } else if (t == return_node) {
    if (node_child_num[stmt] == 1) {
      generate_expr(node_child[stmt][0]);
    }
    printf("jmp _%d\n", return_label);
  } else if (t == break_node) {
    printf("jmp _%d\n", break_label);
  } else if (t == continue_node) {
    printf("jmp _%d\n", continue_label);
  } else if (t == var_init_node) {
    int index = lookup_local_var(get_symbol(node_child[stmt][0]));
    check(index >= 0, "local var not found");
    generate_expr(node_child[stmt][1]);
    printf("mov dword ptr [ebp-%d], eax\n", (1 + index) * WORD_SIZE);
  } else if (t == noop_node) {
    // do nothing
  } else {
    generate_expr(stmt);
  }
}

void generate_stmts(int stmts) {
  check(node_type[stmts] == stmts_node, "generate_stmts");
  for (int i = 0; i < node_child_num[stmts]; i++) {
    generate_stmt(node_child[stmts][i]);
  }
}

void generate_code(int root) {
  check(node_type[root] == prog_node, "prog_node expected");
  printf(".intel_syntax noprefix\n");
  printf(".section .data\n");
  // declare all global variables.
  for (int i = 0; i < node_child_num[root]; i++) {
    int cur = node_child[root][i];
    char* name = get_symbol(node_child[cur][0]);
    // expose function names and global var names
    printf(".globl %s\n", name);
    if (node_type[cur] == var_init_node) {
      // declare initialized global var
      check(node_child_num[cur] == 2 && node_type[node_child[cur][1]] == int_node,
          "only integer variable initialization is allowed\n");
      int value = get_int(node_child[cur][1]);
      printf("%s: .long %d\n", name, value);
    } else if (node_type[cur] == var_decl_node) {
      // declare uninitialized global var
      printf("%s: .long 0\n", name);
    }
  }
  printf(".section .text\n");
  // generate code for functions
  for (int i = 0; i < node_child_num[root]; i++) {
    int cur = node_child[root][i];
    if (node_type[cur] == function_impl_node) {
      char* name = get_symbol(node_child[cur][0]);
      int stmts = node_child[cur][2];
      function_params = node_child[cur][1];
      return_label = new_temp_label();

      local_var_num = 0;
      // register local vars for later look up 
      register_local_vars(stmts);

      in_function = 1;

      // function entry point
      printf("%s:\n", name);
      printf("push ebp\n");
      printf("mov ebp, esp\n");
      printf("sub esp, %d\n", local_var_num * WORD_SIZE);

      generate_stmts(stmts);
      // function epilog
      printf("_%d:\n", return_label);
      printf("mov esp, ebp\n");
      printf("pop ebp\n");
      printf("ret\n");

      in_function = 0;
    }
  }
}

void init_codegen() {
  local_var = malloc(MAX_LOCAL_VARS * WORD_SIZE);
  local_var_num = 0;
}
