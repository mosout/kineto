// Copyright (c) Meta Platforms, Inc. and affiliates.

// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <functional>
#include <list>
#include <memory>
#include <set>
#include <vector>
#include <map>
#include <unordered_map>
#include <deque>
#include <atomic>
#include <mutex>

#ifdef HAS_ROCTRACER
#include <roctracer.h>
#include <roctracer_hcc.h>
#include <roctracer_hip.h>
#include <roctracer_ext.h>
#include <roctracer_roctx.h>
#endif

#include "ActivityType.h"
#include "GenericTraceActivity.h"
#include "RoctracerActivityBuffer.h"


namespace KINETO_NAMESPACE {

using namespace libkineto;

class ApiIdList
{
public:
  ApiIdList();
  bool invertMode() { return invert_; }
  void setInvertMode(bool invert) { invert_ = invert; }
  void add(std::string apiName);
  void remove(std::string apiName);
  bool loadUserPrefs();
  bool contains(uint32_t apiId);
  const std::unordered_map<uint32_t, uint32_t> &filterList() { return filter_; }

private:
  std::unordered_map<uint32_t, uint32_t> filter_;
  bool invert_;
};

struct roctracerRow {
  roctracerRow(uint64_t id, uint32_t domain, uint32_t cid, uint32_t pid
             , uint32_t tid, uint64_t begin, uint64_t end)
    : id(id), domain(domain), cid(cid), pid(pid), tid(tid), begin(begin), end(end) {}
  uint64_t id;  // correlation_id
  uint32_t domain;
  uint32_t cid;
  uint32_t pid;
  uint32_t tid;
  uint64_t begin;
  uint64_t end;
};

struct kernelRow : public roctracerRow {
  kernelRow(uint64_t id, uint32_t domain, uint32_t cid, uint32_t pid
          , uint32_t tid, uint64_t begin, uint64_t end
          , const void *faddr, hipFunction_t function
          , unsigned int gx, unsigned int gy, unsigned int gz
          , unsigned int wx, unsigned int wy, unsigned int wz
          , size_t gss, hipStream_t stream)
    : roctracerRow(id, domain, cid, pid, tid, begin, end), functionAddr(faddr)
    , function(function), gridX(gx), gridY(gy), gridZ(gz)
    , workgroupX(wx), workgroupY(wy), workgroupZ(wz), groupSegmentSize(gss)
    , stream(stream) {}
  const void* functionAddr;
  hipFunction_t function;
  unsigned int gridX;
  unsigned int gridY;
  unsigned int gridZ;
  unsigned int workgroupX;
  unsigned int workgroupY;
  unsigned int workgroupZ;
  size_t groupSegmentSize;
  hipStream_t stream;
};

struct copyRow : public roctracerRow {
  copyRow(uint64_t id, uint32_t domain, uint32_t cid, uint32_t pid
             , uint32_t tid, uint64_t begin, uint64_t end
             , const void* src, const void *dst, size_t size, hipMemcpyKind kind
             , hipStream_t stream)
    : roctracerRow(id, domain, cid, pid, tid, begin, end)
    , src(src), dst(dst), size(size), kind(kind), stream(stream) {}
  const void *src;
  const void *dst;
  size_t size;
  hipMemcpyKind kind;
  hipStream_t stream;
};

struct mallocRow : public roctracerRow {
  mallocRow(uint64_t id, uint32_t domain, uint32_t cid, uint32_t pid
             , uint32_t tid, uint64_t begin, uint64_t end
             , const void* ptr, size_t size)
    : roctracerRow(id, domain, cid, pid, tid, begin, end)
    , ptr(ptr), size(size) {}
  const void *ptr;
  size_t size;
};


class RoctracerActivityApi {
 public:
  enum CorrelationFlowType {
    Default,
    User
  };

  RoctracerActivityApi();
  RoctracerActivityApi(const RoctracerActivityApi&) = delete;
  RoctracerActivityApi& operator=(const RoctracerActivityApi&) = delete;

  virtual ~RoctracerActivityApi();

  static RoctracerActivityApi& singleton();

  static void pushCorrelationID(int id, CorrelationFlowType type);
  static void popCorrelationID(CorrelationFlowType type);

  void enableActivities(
    const std::set<ActivityType>& selected_activities);
  void disableActivities(
    const std::set<ActivityType>& selected_activities);
  void clearActivities();

  int processActivities(ActivityLogger& logger);

  void setMaxBufferSize(int size);

  std::atomic_bool stopCollection{false};

 private:
  bool registered_{false};
  void endTracing();

#ifdef HAS_ROCTRACER
  roctracer_pool_t *hccPool_{NULL};
  static void api_callback(uint32_t domain, uint32_t cid, const void* callback_data, void* arg);
  static void activity_callback(const char* begin, const char* end, void* arg);

  //Name cache
  uint32_t nextStringId_{2};
  std::map<uint32_t, std::string> strings_;
  std::map<std::string, uint32_t> reverseStrings_;
  std::map<activity_correlation_id_t, uint32_t> kernelNames_;

  ApiIdList loggedIds_;

  // Api callback data
  std::deque<roctracerRow> rows_;
  std::deque<kernelRow> kernelRows_;
  std::deque<copyRow> copyRows_;
  std::deque<mallocRow> mallocRows_;
  std::map<activity_correlation_id_t, GenericTraceActivity> kernelLaunches_;
  std::mutex mutex_;
#endif

  int maxGpuBufferCount_{0};
  std::unique_ptr<std::list<RoctracerActivityBuffer>> gpuTraceBuffers_;
  bool externalCorrelationEnabled_{true};
};

} // namespace KINETO_NAMESPACE
