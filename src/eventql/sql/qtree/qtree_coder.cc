/**
 * This file is part of the "libcsql" project
 *   Copyright (c) 2015 Paul Asmuth, zScale Technology GmbH
 *
 * libcsql is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <eventql/sql/qtree/qtree_coder.h>
#include <eventql/sql/qtree/LimitNode.h>

using namespace stx;

namespace csql {

QueryTreeCoder::QueryTreeCoder(Transaction* txn) : txn_(txn) {
  registerType<LimitNode>(1);
}

void QueryTreeCoder::encode(RefPtr<QueryTreeNode> tree, stx::OutputStream* os) {
  auto coder = coders_by_type_id_.find(&typeid(*tree));
  if (coder == coders_by_type_id_.end()) {
    RAISEF(kIOError, "don't know how to encode this QueryTreeNode: $0", tree->toString());
  }

  os->appendVarUInt(coder->second.wire_type_id);
  coder->second.encode_fn(this, tree, os);
}

RefPtr<QueryTreeNode> QueryTreeCoder::decode(stx::InputStream* is) {
  auto wire_type = is->readVarUInt();

  auto coder = coders_by_wire_type_id_.find(wire_type);
  if (coder == coders_by_wire_type_id_.end()) {
    RAISEF(kIOError, "don't know how to decode this QueryTreeNode: $0", wire_type);
  }

  return coder->second.decode_fn(this, is);
}

void QueryTreeCoder::registerType(QueryTreeCoderType t) {
  coders_by_type_id_[t.type_id] = t;
  coders_by_wire_type_id_[t.wire_type_id] = t;
}

} // namespace csql
