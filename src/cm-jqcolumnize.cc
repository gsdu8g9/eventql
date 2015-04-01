/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "fnord-base/io/fileutil.h"
#include "fnord-base/application.h"
#include "fnord-base/logging.h"
#include "fnord-base/cli/flagparser.h"
#include "fnord-base/util/SimpleRateLimit.h"
#include "fnord-base/InternMap.h"
#include "fnord-json/json.h"
#include "fnord-mdb/MDB.h"
#include "fnord-mdb/MDBUtil.h"
#include "fnord-sstable/sstablereader.h"
#include "fnord-sstable/sstablewriter.h"
#include "fnord-sstable/SSTableColumnSchema.h"
#include "fnord-sstable/SSTableColumnReader.h"
#include "fnord-sstable/SSTableColumnWriter.h"
#include "fnord-cstable/UInt16ColumnReader.h"
#include "fnord-cstable/UInt16ColumnWriter.h"
#include "fnord-cstable/CSTableWriter.h"
#include "fnord-cstable/CSTableReader.h"
#include "common.h"
#include "CustomerNamespace.h"
#include "FeatureSchema.h"
#include "JoinedQuery.h"
#include "CTRCounter.h"

using namespace fnord;


int main(int argc, const char** argv) {
  fnord::Application::init();
  fnord::Application::logToStderr();

  fnord::cli::FlagParser flags;

  flags.defineFlag(
      "loglevel",
      fnord::cli::FlagParser::T_STRING,
      false,
      NULL,
      "INFO",
      "loglevel",
      "<level>");

  flags.parseArgv(argc, argv);

  Logger::get()->setMinimumLogLevel(
      strToLogLevel(flags.getString("loglevel")));


  cstable::UInt16ColumnWriter position_col;

  uint64_t r = 0;
  uint64_t n = 0;

  auto add_session = [&] (const cm::JoinedSession& sess) {
    ++n;
    for (const auto& q : sess.queries) {
      for (const auto& i : q.items) {
        position_col.addDatum(r, 0, i.position);
        r = 2;
      }

      r = 1;
    }

    r = 0;
  };


  /* read input tables */
  auto sstables = flags.getArgv();
  int row_idx = 0;
  for (int tbl_idx = 0; tbl_idx < sstables.size(); ++tbl_idx) {
    const auto& sstable = sstables[tbl_idx];
    fnord::logInfo("cm.jqcolumnize", "Importing sstable: $0", sstable);

    /* read sstable header */
    sstable::SSTableReader reader(File::openFile(sstable, File::O_READ));

    if (reader.bodySize() == 0) {
      fnord::logCritical("cm.jqcolumnize", "unfinished sstable: $0", sstable);
      exit(1);
    }

    /* get sstable cursor */
    auto cursor = reader.getCursor();
    auto body_size = reader.bodySize();

    /* status line */
    util::SimpleRateLimitedFn status_line(kMicrosPerSecond, [&] () {
      fnord::logInfo(
          "cm.jqcolumnize",
          "[$1/$2] [$0%] Reading sstable... rows=$3",
          (size_t) ((cursor->position() / (double) body_size) * 100),
          tbl_idx + 1, sstables.size(), row_idx);
    });

    /* read sstable rows */
    for (; cursor->valid(); ++row_idx) {
      status_line.runMaybe();

      auto key = cursor->getKeyString();
      auto val = cursor->getDataBuffer();
      Option<cm::JoinedQuery> q;

      try {
        q = Some(json::fromJSON<cm::JoinedQuery>(val));
      } catch (const Exception& e) {
        fnord::logWarning("cm.jqcolumnize", e, "invalid json: $0", val.toString());
      }

      if (!q.isEmpty()) {
        cm::JoinedSession s;
        s.queries.emplace_back(q.get());
        add_session(s);
      }

      if (!cursor->next()) {
        break;
      }
    }

    status_line.runForce();
  }

  if (sstables.size() > 0) {
    cstable::CSTableWriter writer("fnord.cstable", n);
    writer.addColumn("queries.items.position", &position_col);
    writer.commit();
  }

  auto t0 = WallClock::unixMicros();
  cstable::CSTableReader reader("fnord.cstable");
  void* coldata;
  size_t colsize;
  reader.getColumn("queries.items.position", &coldata, &colsize);
  cstable::UInt16ColumnReader poscol_reader(coldata, colsize);

  auto t1 = WallClock::unixMicros();
  HashMap<uint16_t, Pair<uint64_t, uint64_t>> posi_info;

  uint64_t d;
  uint16_t val;
  while (poscol_reader.next(&r, &d, &val)) {
    ++posi_info[val].first;
    //fnord::iputs("val: $0", val);
  }

  auto t2 = WallClock::unixMicros();
  fnord::iputs("reading took $0ms ($1ms)", (t2 - t0) / 1000.0f, (t2 - t1) / 1000.0f);

  return 0;
}

