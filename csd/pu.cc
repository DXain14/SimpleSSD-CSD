/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "csd/pu.hh"

#include <cmath>
#include <cstring>
#include <limits>

#include "sim/cpu.hh"
#include "sim/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace CSD {

namespace {

const uint8_t MAGIC[4] = {'C', 'S', 'D', '0'};
const uint16_t VERSION = 1;
const uint16_t OP_GEMV = 1;
const uint64_t DESCRIPTOR_SIZE = 56;

struct Descriptor {
  uint16_t version;
  uint16_t op;
  uint32_t flags;
  uint64_t matrixSlba;
  uint32_t rows;
  uint32_t cols;
  uint32_t lda;
  uint64_t vectorOffset;
  uint64_t outputOffset;
};

struct PUContext {
  PU *pu;
  HIL::NVMe::DMAInterface *dma;
  uint64_t controlBytes;
  uint64_t beginAt;
  uint64_t outputDmaBeginAt;
  std::vector<uint8_t> control;
  std::vector<uint8_t> matrix;
  std::vector<uint8_t> output;
  PU::MatrixReader reader;
  PU::Completion completion;
  PU::Status status;

  PUContext(PU *p, HIL::NVMe::DMAInterface *d, uint64_t bytes,
            PU::MatrixReader r, PU::Completion c)
      : pu(p),
        dma(d),
        controlBytes(bytes),
        beginAt(0),
        outputDmaBeginAt(0),
        reader(r),
        completion(c),
        status(PU::STATUS_SUCCESS) {}
};

uint64_t ceilLatencyPs(long double ps, const char *name) {
  long double ceiled;

  if (!std::isfinite(ps)) {
    panic("csd: %s latency is not finite", name);
  }

  ceiled = std::ceil(ps);

  if (ceiled > (long double)std::numeric_limits<uint64_t>::max()) {
    panic("csd: %s latency exceeds simulator tick range", name);
  }

  return ceiled < 1.L ? 1 : (uint64_t)ceiled;
}

}  // namespace

PU::PU(ConfigReader &c) : conf(c), computeAvailableAt(0) {
  memset(&stat, 0, sizeof(stat));
}

uint16_t PU::loadLE16(const uint8_t *ptr) {
  return (uint16_t)ptr[0] | ((uint16_t)ptr[1] << 8);
}

uint32_t PU::loadLE32(const uint8_t *ptr) {
  return (uint32_t)ptr[0] | ((uint32_t)ptr[1] << 8) |
         ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
}

uint64_t PU::loadLE64(const uint8_t *ptr) {
  return (uint64_t)loadLE32(ptr) | ((uint64_t)loadLE32(ptr + 4) << 32);
}

void PU::storeLE32(uint8_t *ptr, uint32_t value) {
  ptr[0] = value & 0xFF;
  ptr[1] = (value >> 8) & 0xFF;
  ptr[2] = (value >> 16) & 0xFF;
  ptr[3] = (value >> 24) & 0xFF;
}

uint64_t PU::psFromBandwidth(uint64_t bytes, uint64_t bytesPerSecond) {
  long double ps;

  if (bytes == 0) {
    return 0;
  }
  if (bytesPerSecond == 0) {
    panic("csd: InternalReadBandwidth should be positive");
  }

  ps = (long double)bytes * 1000000000000.L / (long double)bytesPerSecond;

  return ceilLatencyPs(ps, "internal read");
}

uint64_t PU::psFromGFLOPS(uint64_t ops, float gflops) {
  long double ps;

  if (ops == 0) {
    return 0;
  }
  if (!std::isfinite(gflops) || gflops <= 0.f) {
    panic("csd: PUComputeGFLOPS should be finite and positive");
  }

  ps = (long double)ops * 1000.L / (long double)gflops;

  return ceilLatencyPs(ps, "compute");
}

bool PU::checkedAdd(uint64_t a, uint64_t b, uint64_t &out) {
  if (a > std::numeric_limits<uint64_t>::max() - b) {
    return false;
  }

  out = a + b;

  return true;
}

bool PU::checkedMul(uint64_t a, uint64_t b, uint64_t &out) {
  if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) {
    return false;
  }

  out = a * b;

  return true;
}

float PU::decodeFP16(uint16_t value) {
  uint32_t sign = (value >> 15) & 0x1;
  uint32_t exponent = (value >> 10) & 0x1F;
  uint32_t fraction = value & 0x03FF;
  float ret;

  if (exponent == 0) {
    if (fraction == 0) {
      ret = 0.f;
    }
    else {
      ret = ldexp((float)fraction, -24);
    }
  }
  else if (exponent == 0x1F) {
    if (fraction == 0) {
      ret = std::numeric_limits<float>::infinity();
    }
    else {
      ret = std::numeric_limits<float>::quiet_NaN();
    }
  }
  else {
    ret = ldexp(1.f + (float)fraction / 1024.f, (int)exponent - 15);
  }

  return sign ? -ret : ret;
}

void PU::submit(HIL::NVMe::DMAInterface *dma, uint64_t controlBytes,
                MatrixReader reader, Completion completion) {
  auto context = new PUContext(this, dma, controlBytes, reader, completion);

  context->beginAt = getTick();
  stat.commandCount++;

  if (!conf.readBoolean(CONFIG_CSD, CSD_ENABLE)) {
    stat.failedCount++;
    completion(getTick(), STATUS_DISABLED);
    delete context;

    return;
  }

  if (controlBytes < DESCRIPTOR_SIZE) {
    stat.failedCount++;
    completion(getTick(), STATUS_INVALID_DESCRIPTOR);
    delete context;

    return;
  }
  if (controlBytes > conf.readUint(CONFIG_CSD, CSD_MAX_CONTROL_BYTES)) {
    stat.failedCount++;
    completion(getTick(), STATUS_INVALID_DESCRIPTOR);
    delete context;

    return;
  }

  context->control.assign(controlBytes, 0);

  DMAFunction descriptorDone = [](uint64_t tick, void *opaque) {
    auto context = (PUContext *)opaque;
    auto pThis = context->pu;
    Descriptor desc;
    uint64_t matrixElements;
    uint64_t matrixBytes;
    uint64_t vectorBytes;
    uint64_t outputBytes;
    uint64_t vectorEnd;
    uint64_t outputEnd;
    uint64_t ops;
    uint64_t readLatency;
    uint64_t computeLatency;
    uint64_t computeStart;
    uint64_t targetTick;
    PU::Status readStatus;

    pThis->stat.dmaInBytes += context->controlBytes;

    if (memcmp(context->control.data(), MAGIC, sizeof(MAGIC)) != 0) {
      context->status = PU::STATUS_INVALID_DESCRIPTOR;
      goto FAIL;
    }

    desc.version = PU::loadLE16(context->control.data() + 4);
    desc.op = PU::loadLE16(context->control.data() + 6);
    desc.flags = PU::loadLE32(context->control.data() + 8);
    desc.matrixSlba = PU::loadLE64(context->control.data() + 16);
    desc.rows = PU::loadLE32(context->control.data() + 24);
    desc.cols = PU::loadLE32(context->control.data() + 28);
    desc.lda = PU::loadLE32(context->control.data() + 32);
    desc.vectorOffset = PU::loadLE64(context->control.data() + 40);
    desc.outputOffset = PU::loadLE64(context->control.data() + 48);

    if (desc.version != VERSION || desc.op != OP_GEMV || desc.flags != 0 ||
        desc.rows == 0 || desc.cols == 0 || desc.lda != desc.cols) {
      context->status = PU::STATUS_INVALID_DESCRIPTOR;
      goto FAIL;
    }

    if (!PU::checkedMul(desc.rows, desc.cols, matrixElements) ||
        !PU::checkedMul(matrixElements, 2, matrixBytes) ||
        !PU::checkedMul(desc.cols, 2, vectorBytes) ||
        !PU::checkedMul(desc.rows, 4, outputBytes) ||
        !PU::checkedMul(matrixElements, 2, ops)) {
      context->status = PU::STATUS_INVALID_DESCRIPTOR;
      goto FAIL;
    }

    if (matrixBytes > pThis->conf.readUint(CONFIG_CSD, CSD_MAX_MATRIX_BYTES)) {
      context->status = PU::STATUS_INVALID_DESCRIPTOR;
      goto FAIL;
    }

    if (!PU::checkedAdd(desc.vectorOffset, vectorBytes, vectorEnd) ||
        !PU::checkedAdd(desc.outputOffset, outputBytes, outputEnd) ||
        desc.vectorOffset < DESCRIPTOR_SIZE || desc.outputOffset < vectorEnd ||
        vectorEnd > context->controlBytes || outputEnd > context->controlBytes) {
      context->status = PU::STATUS_INVALID_DESCRIPTOR;
      goto FAIL;
    }

    context->matrix.assign(matrixBytes, 0);
    readStatus = context->reader(desc.matrixSlba, matrixBytes,
                                 context->matrix.data(), tick, true);
    if (readStatus != PU::STATUS_SUCCESS) {
      context->status = readStatus;
      goto FAIL;
    }

    readLatency = PU::psFromBandwidth(
        matrixBytes,
        pThis->conf.readUint(CONFIG_CSD, CSD_INTERNAL_READ_BANDWIDTH));
    if (!PU::checkedAdd(tick, readLatency, tick)) {
      panic("csd: internal read latency overflows simulator tick");
    }
    pThis->stat.internalReadLatency += readLatency;
    pThis->stat.arrayReadBytes += matrixBytes;

    context->output.assign(outputBytes, 0);

    for (uint32_t row = 0; row < desc.rows; row++) {
      float sum = 0.f;
      uint64_t rowOffset = (uint64_t)row * desc.cols * 2;

      for (uint32_t col = 0; col < desc.cols; col++) {
        float a = PU::decodeFP16(PU::loadLE16(
            context->matrix.data() + rowOffset + col * 2));
        float x = PU::decodeFP16(PU::loadLE16(
            context->control.data() + desc.vectorOffset + col * 2));

        sum += a * x;
      }

      uint32_t raw;

      memcpy(&raw, &sum, sizeof(raw));
      PU::storeLE32(context->output.data() + (uint64_t)row * 4, raw);
    }

    computeLatency = PU::psFromGFLOPS(
        ops, pThis->conf.readFloat(CONFIG_CSD, CSD_PU_COMPUTE_GFLOPS));
    computeStart = MAX(tick, pThis->computeAvailableAt);
    pThis->stat.computeQueueLatency += computeStart - tick;
    if (!PU::checkedAdd(computeStart, computeLatency, tick)) {
      panic("csd: compute latency overflows simulator tick");
    }
    pThis->computeAvailableAt = tick;
    pThis->stat.computeLatency += computeLatency;
    pThis->stat.computeOps += ops;

    targetTick = tick;

    {
      DMAFunction doOutput = [desc](uint64_t, void *opaque) {
        auto context = (PUContext *)opaque;
        DMAFunction outputDone = [](uint64_t now, void *opaque) {
          auto context = (PUContext *)opaque;

          context->pu->stat.dmaOutBytes += context->output.size();
          context->pu->stat.writebackLatency += now - context->outputDmaBeginAt;
          context->pu->stat.totalLatency += now - context->beginAt;
          context->completion(now, PU::STATUS_SUCCESS);

          delete context;
        };

        context->outputDmaBeginAt = getTick();
        context->dma->write(desc.outputOffset, context->output.size(),
                            context->output.data(), outputDone, context);
      };

      execute(CPU::NVME__SUBSYSTEM, CPU::SUBMIT_COMMAND, doOutput, context,
              targetTick > getTick() ? targetTick - getTick() : 0);
    }

    return;

  FAIL:
    pThis->stat.failedCount++;
    pThis->stat.totalLatency += tick - context->beginAt;
    context->completion(tick, context->status);
    delete context;
  };

  dma->read(0, controlBytes, context->control.data(), descriptorDone, context);
}

void PU::getStatList(std::vector<Stats> &list, std::string prefix) {
  Stats temp;

  temp.name = prefix + "command.count";
  temp.desc = "CSD read_compute command count";
  list.push_back(temp);

  temp.name = prefix + "command.failed";
  temp.desc = "CSD read_compute failed command count";
  list.push_back(temp);

  temp.name = prefix + "array.read.bytes";
  temp.desc = "CSD PU matrix bytes read from physical flash array";
  list.push_back(temp);

  temp.name = prefix + "compute.ops";
  temp.desc = "CSD PU GEMV floating point operations";
  list.push_back(temp);

  temp.name = prefix + "dma.in.bytes";
  temp.desc = "CSD PU host descriptor/vector DMA bytes read";
  list.push_back(temp);

  temp.name = prefix + "dma.out.bytes";
  temp.desc = "CSD PU output DMA bytes written";
  list.push_back(temp);

  temp.name = prefix + "latency.internal_read";
  temp.desc = "CSD PU internal array-read bandwidth latency";
  list.push_back(temp);

  temp.name = prefix + "latency.compute";
  temp.desc = "CSD PU GEMV compute latency";
  list.push_back(temp);

  temp.name = prefix + "latency.compute_queue";
  temp.desc = "CSD PU GEMV queue latency";
  list.push_back(temp);

  temp.name = prefix + "latency.writeback";
  temp.desc = "CSD PU output DMA writeback latency";
  list.push_back(temp);

  temp.name = prefix + "latency.total";
  temp.desc = "CSD PU total read_compute latency";
  list.push_back(temp);
}

void PU::getStatValues(std::vector<double> &values) {
  values.push_back(stat.commandCount);
  values.push_back(stat.failedCount);
  values.push_back(stat.arrayReadBytes);
  values.push_back(stat.computeOps);
  values.push_back(stat.dmaInBytes);
  values.push_back(stat.dmaOutBytes);
  values.push_back(stat.internalReadLatency);
  values.push_back(stat.computeLatency);
  values.push_back(stat.computeQueueLatency);
  values.push_back(stat.writebackLatency);
  values.push_back(stat.totalLatency);
}

void PU::resetStatValues() {
  memset(&stat, 0, sizeof(stat));
  computeAvailableAt = 0;
}

}  // namespace CSD

}  // namespace SimpleSSD
