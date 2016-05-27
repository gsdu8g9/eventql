/**
 * Copyright (c) 2016 zScale Technology GmbH <legal@zscale.io>
 * Authors:
 *   - Paul Asmuth <paul@zscale.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#pragma once
#include "eventql/eventql.h"
#include "eventql/util/thread/threadpool.h"
#include "eventql/db/TSDBService.h"
#include "eventql/server/auth/internal_auth.h"
#include "eventql/config/namespace_config.h"
#include "eventql/config/config_directory.h"
#include "eventql/JavaScriptContext.h"
#include "eventql/mapreduce/MapReduceTask.h"
#include "eventql/server/session.h"

namespace eventql {

class MapReduceService {
public:

  MapReduceService(
      ConfigDirectory* cdir,
      InternalAuth* auth,
      eventql::TSDBService* tsdb,
      eventql::PartitionMap* pmap,
      eventql::ReplicationScheme* repl,
      JSRuntime* js_runtime,
      const String& cachedir);

  void executeScript(
      Session* session,
      RefPtr<MapReduceJobSpec> job,
      const String& program_source);

  Option<SHA1Hash> mapPartition(
      Session* session,
      RefPtr<MapReduceJobSpec> job,
      const String& table_name,
      const SHA1Hash& partition_key,
      const String& map_fn,
      const String& globals,
      const String& params,
      const Set<String>& required_columns);

  Option<SHA1Hash> reduceTables(
      Session* session,
      RefPtr<MapReduceJobSpec> job,
      const Vector<String>& input_tables,
      const String& reduce_fn,
      const String& globals,
      const String& params);

  Option<String> getResultFilename(
      const SHA1Hash& result_id);

  bool saveLocalResultToTable(
      Session* session,
      const String& table_name,
      const SHA1Hash& partition,
      const SHA1Hash& result_id);

  bool saveRemoteResultsToTable(
      Session* session,
      const String& table_name,
      const SHA1Hash& partition,
      const Vector<String>& input_tables);

  static void downloadResult(
      const http::HTTPRequest& req,
      Function<void (const void*, size_t, const void*, size_t)> fn);

protected:
  ConfigDirectory* cdir_;
  InternalAuth* auth_;
  eventql::TSDBService* tsdb_;
  eventql::PartitionMap* pmap_;
  eventql::ReplicationScheme* repl_;
  JSRuntime* js_runtime_;
  String cachedir_;
  thread::ThreadPool tpool_;
};

} // namespace eventql
