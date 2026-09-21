/*
 * engine.js — JavaScript port of the C++ simulation core of
 * "Multithreaded File Transfer and Scheduling Simulator".
 *
 * The port mirrors the reference C++ implementation (src/scheduler.cpp,
 * src/simulation_engine.cpp, src/metrics.cpp, src/csv_utils.cpp) operation by
 * operation, including the binary-heap sift procedures and the tie-breaking
 * rules, so that identical inputs, thread counts, quantum and per-dispatch
 * overhead produce identical per-request metrics.
 *
 * Everything here is a deterministic, event-driven discrete simulation of a
 * transfer scheduler. No data is transferred and no network is contacted.
 */
(function (global) {
  'use strict';

  var EPS = 1e-9;
  var DEFAULT_CONFIG = {
    threads: 4,
    quantumSec: 1.0,
    overheadSec: 0.05
  };
  var SCHEDULERS = ['fifo', 'sjf', 'priority', 'rr', 'wfq'];
  var SCHEDULER_LABEL = {
    fifo: 'FIFO',
    sjf: 'Shortest Job First',
    priority: 'Priority',
    rr: 'Round Robin',
    wfq: 'WFQ-like'
  };

  /* ------------------------------------------------------------------ *
   * CSV helpers (mirror of src/csv_utils.cpp)
   * ------------------------------------------------------------------ */

  var WS = ' \t\n\v\f\r';

  function trim(s) {
    var begin = 0;
    var end = s.length;
    while (begin < end && WS.indexOf(s.charAt(begin)) >= 0) begin++;
    while (end > begin && WS.indexOf(s.charAt(end - 1)) >= 0) end--;
    return s.substring(begin, end);
  }

  function parseRequestsCsv(text) {
    var lines = String(text).split('\n');
    if (lines.length === 0) return { requests: [], error: 'empty input' };

    var header = trim(lines[0]);
    if (header === '') return { requests: [], error: 'empty input' };

    var headers = header.split(',');
    var idx = {};
    for (var i = 0; i < headers.length; i++) idx[trim(headers[i])] = i;

    var required = ['request_id', 'arrival_time', 'file_size_mb', 'estimated_bandwidth_mb_per_sec', 'priority'];
    for (var r = 0; r < required.length; r++) {
      if (!(required[r] in idx)) {
        return { requests: [], error: 'Missing required CSV column: ' + required[r] };
      }
    }
    var hasClient = Object.prototype.hasOwnProperty.call(idx, 'client_id');

    var requests = [];
    for (var ln = 1; ln < lines.length; ln++) {
      var line = trim(lines[ln]);
      if (line === '') continue;
      var cells = line.split(',');
      function cell(name) {
        var v = idx[name] < cells.length ? cells[idx[name]] : '';
        return trim(v);
      }
      var req = {
        requestId: parseInt(cell('request_id'), 10),
        clientId: hasClient ? parseInt(cell('client_id'), 10) : 0,
        arrivalTime: parseFloat(cell('arrival_time')),
        fileSizeMb: parseFloat(cell('file_size_mb')),
        bandwidth: parseFloat(cell('estimated_bandwidth_mb_per_sec')),
        priority: parseInt(cell('priority'), 10)
      };
      if (!hasClient) req.clientId = req.requestId;
      if (isNaN(req.requestId) || isNaN(req.arrivalTime) || isNaN(req.fileSizeMb) ||
          isNaN(req.bandwidth) || isNaN(req.priority)) {
        return { requests: [], error: 'Malformed numeric value on data row ' + ln };
      }
      req.remainingSizeMb = req.fileSizeMb;
      requests.push(req);
    }

    requests.sort(function (a, b) {
      if (a.arrivalTime === b.arrivalTime) return a.requestId - b.requestId;
      return a.arrivalTime < b.arrivalTime ? -1 : 1;
    });

    return { requests: requests, error: null };
  }

  function fixed(value, digits) {
    return Number(value).toFixed(digits);
  }

  function resultsToCsv(rows) {
    var out = 'request_id,client_id,arrival_time,start_time,finish_time,waiting_time,turnaround_time,response_time,assigned_thread,scheduler_name\n';
    for (var i = 0; i < rows.length; i++) {
      var row = rows[i];
      out += [row.requestId, row.clientId, fixed(row.arrivalTime, 4), fixed(row.startTime, 4),
        fixed(row.finishTime, 4), fixed(row.waitingTime, 4), fixed(row.turnaroundTime, 4),
        fixed(row.responseTime, 4), row.assignedThread, row.scheduler].join(',') + '\n';
    }
    return out;
  }

  function summaryToCsv(summaries) {
    var out = 'scheduler,avg_waiting_time,avg_turnaround_time,avg_response_time,throughput,fairness_index,total_completion_time\n';
    for (var i = 0; i < summaries.length; i++) {
      var s = summaries[i];
      out += [s.scheduler, fixed(s.avgWaitingTime, 6), fixed(s.avgTurnaroundTime, 6),
        fixed(s.avgResponseTime, 6), fixed(s.throughput, 6), fixed(s.fairnessIndex, 6),
        fixed(s.totalCompletionTime, 6)].join(',') + '\n';
    }
    return out;
  }

  /* ------------------------------------------------------------------ *
   * Binary heap reproducing the exact sift procedures of push_heap /
   * pop_heap as they are implemented in libc++ (the standard library used
   * to build the reference C++ binary). This matters because the
   * completion-event queue holds keys that tie frequently (equal finish
   * times), and the queue order of tied items decides the order in which
   * requests re-enter the Round-Robin queue.
   * ------------------------------------------------------------------ */

  function BinaryHeap(less) {
    this.less = less;
    this.data = [];
  }

  BinaryHeap.prototype.size = function () { return this.data.length; };
  BinaryHeap.prototype.empty = function () { return this.data.length === 0; };
  BinaryHeap.prototype.top = function () { return this.data[0]; };

  // std::__sift_up over [0, lastExclusive) with the element to sift at lastExclusive - 1.
  BinaryHeap.prototype._siftUp = function (lastExclusive, len) {
    var data = this.data;
    var less = this.less;
    if (len <= 1) return;
    var l = Math.trunc((len - 2) / 2);
    var ptr = l;
    var last = lastExclusive - 1;
    if (!less(data[ptr], data[last])) return;
    var moved = data[last];
    do {
      data[last] = data[ptr];
      last = ptr;
      if (l === 0) break;
      l = Math.trunc((l - 1) / 2);
      ptr = l;
    } while (less(data[ptr], moved));
    data[last] = moved;
  };

  BinaryHeap.prototype.push = function (value) {
    var data = this.data;
    data.push(value);
    this._siftUp(data.length, data.length);
  };

  // std::pop_heap + pop_back, mirroring std::__pop_heap and std::__floyd_sift_down.
  BinaryHeap.prototype.pop = function () {
    var data = this.data;
    var len = data.length;
    var top = data[0];
    if (len > 1) {
      // __floyd_sift_down: walk down to a leaf, keeping a hole.
      var hole = 0;
      var child = 0;
      while (true) {
        child = 2 * child + 1;
        if (child + 1 < len && this.less(data[child], data[child + 1])) child++;
        data[hole] = data[child];
        hole = child;
        if (child > Math.trunc((len - 2) / 2)) break;
      }
      var lastIndex = len - 1;
      if (hole === lastIndex) {
        data[hole] = top;
      } else {
        data[hole] = data[lastIndex];
        data[lastIndex] = top;
        this._siftUp(hole + 1, hole + 1);
      }
    }
    data.pop();
    return top;
  };

  /* ------------------------------------------------------------------ *
   * Schedulers (mirror of src/scheduler.cpp)
   * ------------------------------------------------------------------ */

  function safeBandwidth(request) {
    return Math.max(0.001, request.bandwidth);
  }

  function FifoScheduler() { this.queue = []; }
  FifoScheduler.prototype.push = function (index) { this.queue.push(index); };
  FifoScheduler.prototype.pop = function () {
    return this.queue.length === 0 ? null : this.queue.shift();
  };
  FifoScheduler.prototype.empty = function () { return this.queue.length === 0; };

  function SjfScheduler() {
    var seq = 0;
    this.heap = new BinaryHeap(function (a, b) {
      if (a.estimatedTime === b.estimatedTime) return a.seq > b.seq;
      return a.estimatedTime > b.estimatedTime;
    });
    (function (self) {
      self.push = function (index, requests) {
        var request = requests[index];
        var estimatedTime = request.remainingSizeMb / safeBandwidth(request);
        self.heap.push({ estimatedTime: estimatedTime, seq: seq++, requestIndex: index });
      };
    })(this);
  }
  SjfScheduler.prototype.pop = function () {
    if (this.heap.empty()) return null;
    return this.heap.pop().requestIndex;
  };
  SjfScheduler.prototype.empty = function () { return this.heap.empty(); };

  function PriorityScheduler() {
    var seq = 0;
    this.heap = new BinaryHeap(function (a, b) {
      if (a.priority === b.priority) return a.seq > b.seq;
      return a.priority > b.priority;
    });
    (function (self) {
      self.push = function (index, requests) {
        self.heap.push({ priority: requests[index].priority, seq: seq++, requestIndex: index });
      };
    })(this);
  }
  PriorityScheduler.prototype.pop = function () {
    if (this.heap.empty()) return null;
    return this.heap.pop().requestIndex;
  };
  PriorityScheduler.prototype.empty = function () { return this.heap.empty(); };

  function RoundRobinScheduler(quantumSec) {
    this.quantumSec = Math.max(0.001, quantumSec);
    this.queue = [];
  }
  RoundRobinScheduler.prototype.push = function (index) { this.queue.push(index); };
  RoundRobinScheduler.prototype.pop = function () {
    return this.queue.length === 0 ? null : this.queue.shift();
  };
  RoundRobinScheduler.prototype.empty = function () { return this.queue.length === 0; };

  function WfqLikeScheduler() {
    var seq = 0;
    this.virtualTime = 0.0;
    this.clientFinishTag = {};
    this.heap = new BinaryHeap(function (a, b) {
      if (a.finishTag === b.finishTag) return a.seq > b.seq;
      return a.finishTag > b.finishTag;
    });
    (function (self) {
      self.push = function (index, requests) {
        var request = requests[index];
        var weight = WfqLikeScheduler.weightForPriority(request.priority);
        var service = request.remainingSizeMb / safeBandwidth(request);
        var clientLast = self.clientFinishTag[request.clientId];
        if (clientLast === undefined) clientLast = 0.0;
        var startTag = Math.max(self.virtualTime, clientLast);
        var finishTag = startTag + (service / weight);
        self.clientFinishTag[request.clientId] = finishTag;
        self.heap.push({ finishTag: finishTag, seq: seq++, requestIndex: index });
      };
    })(this);
  }
  WfqLikeScheduler.weightForPriority = function (priority) {
    var clamped = Math.min(10, Math.max(1, priority));
    return 11 - clamped;
  };
  WfqLikeScheduler.prototype.pop = function () {
    if (this.heap.empty()) return null;
    var node = this.heap.pop();
    this.virtualTime = Math.max(this.virtualTime, node.finishTag);
    return node.requestIndex;
  };
  WfqLikeScheduler.prototype.empty = function () { return this.heap.empty(); };

  function createScheduler(type, quantumSec) {
    switch (type) {
      case 'fifo': return new FifoScheduler();
      case 'sjf': return new SjfScheduler();
      case 'priority': return new PriorityScheduler();
      case 'rr': return new RoundRobinScheduler(quantumSec);
      case 'wfq': return new WfqLikeScheduler();
      default: throw new Error('Unknown scheduler type: ' + type);
    }
  }

  /* ------------------------------------------------------------------ *
   * Simulation engine (mirror of src/simulation_engine.cpp)
   * ------------------------------------------------------------------ */

  function runOneScheduler(type, originalRequests, config) {
    var requests = originalRequests.map(function (req) {
      return {
        requestId: req.requestId,
        clientId: req.clientId,
        arrivalTime: req.arrivalTime,
        fileSizeMb: req.fileSizeMb,
        bandwidth: req.bandwidth,
        priority: req.priority,
        remainingSizeMb: req.fileSizeMb,
        firstStartTime: -1.0,
        finishTime: -1.0,
        totalProcessingTime: 0.0,
        assignedThread: -1,
        dispatchCount: 0
      };
    });

    var scheduler = createScheduler(type, config.quantumSec);
    var slots = [];
    for (var s = 0; s < config.threads; s++) slots.push({ busy: false, requestIndex: -1 });

    var events = new BinaryHeap(function (a, b) {
      return a.finishTime > b.finishTime;
    });

    var arrived = 0;
    var completed = 0;
    var total = requests.length;
    var simTime = 0.0;
    var dispatchLog = [];

    function enqueueArrivals(now) {
      while (arrived < total && requests[arrived].arrivalTime <= now + EPS) {
        scheduler.push(arrived, requests, now);
        arrived++;
      }
    }

    while (completed < total) {
      enqueueArrivals(simTime);

      for (var workerId = 0; workerId < slots.length; workerId++) {
        if (slots[workerId].busy) continue;
        if (scheduler.empty()) break;

        var requestIndex = scheduler.pop(requests, simTime);
        if (requestIndex === null) break;

        var req = requests[requestIndex];
        if (req.firstStartTime < 0.0) req.firstStartTime = simTime;

        req.assignedThread = workerId;
        req.dispatchCount += 1;

        var bandwidth = safeBandwidth(req);
        var fullTime = req.remainingSizeMb / bandwidth;
        var transferTime = fullTime;
        var processedMb = req.remainingSizeMb;

        if (type === 'rr') {
          transferTime = Math.min(fullTime, Math.max(0.001, config.quantumSec));
          processedMb = Math.min(req.remainingSizeMb, transferTime * bandwidth);
        }

        var sliceTime = transferTime + config.overheadSec;
        var finish = simTime + sliceTime;

        slots[workerId].busy = true;
        slots[workerId].requestIndex = requestIndex;

        dispatchLog.push({
          workerId: workerId,
          requestId: req.requestId,
          clientId: req.clientId,
          start: simTime,
          finish: finish,
          processedMb: processedMb,
          sliceTime: sliceTime
        });

        events.push({
          finishTime: finish,
          workerId: workerId,
          requestIndex: requestIndex,
          processedMb: processedMb,
          sliceTime: sliceTime
        });
      }

      var nextArrival = arrived < total ? requests[arrived].arrivalTime : Infinity;
      var nextFinish = events.empty() ? Infinity : events.top().finishTime;

      if (nextArrival === Infinity && nextFinish === Infinity) break;

      if (scheduler.empty() && nextFinish === Infinity && nextArrival > simTime) {
        simTime = nextArrival;
        continue;
      }

      simTime = Math.min(nextArrival, nextFinish);
      enqueueArrivals(simTime);

      while (!events.empty() && events.top().finishTime <= simTime + EPS) {
        var ev = events.pop();

        slots[ev.workerId].busy = false;
        slots[ev.workerId].requestIndex = -1;

        var done = requests[ev.requestIndex];
        done.remainingSizeMb = Math.max(0.0, done.remainingSizeMb - ev.processedMb);
        done.totalProcessingTime += ev.sliceTime;

        if (done.remainingSizeMb <= EPS) {
          done.remainingSizeMb = 0.0;
          done.finishTime = ev.finishTime;
          completed++;
        } else {
          scheduler.push(ev.requestIndex, requests, simTime);
        }
      }
    }

    var totalCompletionTime = 0.0;
    for (var k = 0; k < requests.length; k++) {
      totalCompletionTime = Math.max(totalCompletionTime, requests[k].finishTime);
    }

    var summary = buildSummary(requests, type, totalCompletionTime);
    var rows = buildRequestResults(requests, type);

    return {
      summary: summary,
      rows: rows,
      dispatchLog: dispatchLog,
      requests: requests
    };
  }

  /* ------------------------------------------------------------------ *
   * Metrics (mirror of src/metrics.cpp)
   * ------------------------------------------------------------------ */

  function buildRequestResults(requests, schedulerName) {
    var rows = [];
    for (var i = 0; i < requests.length; i++) {
      var req = requests[i];
      if (req.finishTime < 0.0 || req.firstStartTime < 0.0) continue;
      var turnaround = req.finishTime - req.arrivalTime;
      var response = req.firstStartTime - req.arrivalTime;
      var waiting = Math.max(0.0, turnaround - req.totalProcessingTime);
      rows.push({
        requestId: req.requestId,
        clientId: req.clientId,
        arrivalTime: req.arrivalTime,
        startTime: req.firstStartTime,
        finishTime: req.finishTime,
        waitingTime: waiting,
        turnaroundTime: turnaround,
        responseTime: response,
        assignedThread: req.assignedThread,
        scheduler: schedulerName
      });
    }
    rows.sort(function (a, b) { return a.requestId - b.requestId; });
    return rows;
  }

  function buildSummary(requests, schedulerName, totalCompletionTime) {
    var summary = {
      scheduler: schedulerName,
      avgWaitingTime: 0.0,
      avgTurnaroundTime: 0.0,
      avgResponseTime: 0.0,
      throughput: 0.0,
      fairnessIndex: 0.0,
      totalCompletionTime: totalCompletionTime,
      completedRequests: 0
    };

    var rows = buildRequestResults(requests, schedulerName);
    summary.completedRequests = rows.length;
    if (rows.length === 0 || totalCompletionTime <= 0.0) return summary;

    var n = rows.length;
    for (var i = 0; i < rows.length; i++) {
      summary.avgWaitingTime += rows[i].waitingTime;
      summary.avgTurnaroundTime += rows[i].turnaroundTime;
      summary.avgResponseTime += rows[i].responseTime;
    }
    summary.avgWaitingTime /= n;
    summary.avgTurnaroundTime /= n;
    summary.avgResponseTime /= n;
    summary.throughput = n / totalCompletionTime;

    var clients = {};
    for (var j = 0; j < requests.length; j++) {
      var req = requests[j];
      if (req.finishTime < 0.0) continue;
      var c = clients[req.clientId];
      if (!c) c = clients[req.clientId] = { totalSize: 0.0, firstArrival: -1.0, lastFinish: -1.0 };
      c.totalSize += req.fileSizeMb;
      if (c.firstArrival < 0.0 || req.arrivalTime < c.firstArrival) c.firstArrival = req.arrivalTime;
      c.lastFinish = Math.max(c.lastFinish, req.finishTime);
    }

    var throughputs = [];
    Object.keys(clients).forEach(function (key) {
      var c = clients[key];
      var span = Math.max(1e-6, c.lastFinish - c.firstArrival);
      throughputs.push(c.totalSize / span);
    });

    if (throughputs.length > 0) {
      var sum = 0.0;
      var sqSum = 0.0;
      for (var t = 0; t < throughputs.length; t++) {
        sum += throughputs[t];
        sqSum += throughputs[t] * throughputs[t];
      }
      if (sqSum > 0.0) {
        var nn = throughputs.length;
        summary.fairnessIndex = (sum * sum) / (nn * sqSum);
      }
    }

    return summary;
  }

  function runBenchmark(requests, config) {
    var out = [];
    for (var i = 0; i < SCHEDULERS.length; i++) {
      out.push(runOneScheduler(SCHEDULERS[i], requests, config));
    }
    return out;
  }

  function workerUtilization(run, config) {
    var busy = new Array(config.threads).fill(0);
    for (var i = 0; i < run.dispatchLog.length; i++) {
      busy[run.dispatchLog[i].workerId] += run.dispatchLog[i].finish - run.dispatchLog[i].start;
    }
    var span = Math.max(run.summary.totalCompletionTime, 1e-9);
    return busy.map(function (b) { return b / span; });
  }

  var api = {
    EPS: EPS,
    DEFAULT_CONFIG: DEFAULT_CONFIG,
    SCHEDULERS: SCHEDULERS,
    SCHEDULER_LABEL: SCHEDULER_LABEL,
    parseRequestsCsv: parseRequestsCsv,
    resultsToCsv: resultsToCsv,
    summaryToCsv: summaryToCsv,
    runOneScheduler: runOneScheduler,
    runBenchmark: runBenchmark,
    buildRequestResults: buildRequestResults,
    buildSummary: buildSummary,
    workerUtilization: workerUtilization,
    weightForPriority: WfqLikeScheduler.weightForPriority
  };

  global.MFTS = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof window !== 'undefined' ? window : this);
