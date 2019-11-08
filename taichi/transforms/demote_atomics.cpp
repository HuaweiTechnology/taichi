#include "../ir.h"
#include <deque>
#include <set>

TLANG_NAMESPACE_BEGIN

class DemoteAtomics : public BasicStmtVisitor {
public:
  OffloadedStmt *current_offloaded;

  DemoteAtomics() : BasicStmtVisitor() { current_offloaded = nullptr; }

  void visit(AtomicOpStmt *stmt) override {
    if (current_offloaded && current_offloaded->num_cpu_threads == 1) {
      // replace atomics with load, add, store
      auto ptr = stmt->dest;
      auto val = stmt->val;

      auto new_stmts = VecStatement();
      auto load = new_stmts.push_back<GlobalLoadStmt>(ptr);
      auto add =
          new_stmts.push_back<BinaryOpStmt>(BinaryOpType::add, load, val);
      auto store = new_stmts.push_back<GlobalStoreStmt>(ptr, add);

      stmt->parent->replace_with(stmt, new_stmts);
    }
  }

  void visit(OffloadedStmt *stmt) {
    TC_ASSERT(current_offloaded == nullptr);
    current_offloaded = stmt;
    if (stmt->body) {
      stmt->body->accept(this);
    }
    current_offloaded = nullptr;
  }

  static void run(IRNode *node) {
    DemoteAtomics demoter;
    node->accept(&demoter);
  }
};

namespace irpass {

void demote_atomics(IRNode *root) {
  DemoteAtomics::run(root);
  typecheck(root);
}

} // namespace irpass

TLANG_NAMESPACE_END
