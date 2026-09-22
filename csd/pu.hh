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

#pragma once

#ifndef __CSD_PU__
#define __CSD_PU__

#include <cinttypes>
#include <functional>
#include <string>
#include <vector>

#include "csd/config.hh"
#include "hil/nvme/dma.hh"
#include "sim/statistics.hh"

namespace SimpleSSD {

namespace CSD {

class PU : public StatObject {
 public:
  enum Status {
    STATUS_SUCCESS,
    STATUS_DISABLED,
    STATUS_INVALID_DESCRIPTOR,
    STATUS_LBA_RANGE,
    STATUS_UNMAPPED_PAYLOAD,
    STATUS_INTERNAL_ERROR,
  };

  typedef std::function<Status(uint64_t, uint64_t, uint8_t *, uint64_t &, bool)>
      MatrixReader;
  typedef std::function<void(uint64_t, Status)> Completion;

 private:
  ConfigReader &conf;
  uint64_t computeAvailableAt;

  struct {
    uint64_t commandCount;
    uint64_t failedCount;
    uint64_t arrayReadBytes;
    uint64_t computeOps;
    uint64_t dmaInBytes;
    uint64_t dmaOutBytes;
    uint64_t internalReadLatency;
    uint64_t computeQueueLatency;
    uint64_t computeLatency;
    uint64_t writebackLatency;
    uint64_t totalLatency;
  } stat;

  static uint16_t loadLE16(const uint8_t *);
  static uint32_t loadLE32(const uint8_t *);
  static uint64_t loadLE64(const uint8_t *);
  static void storeLE32(uint8_t *, uint32_t);
  static uint64_t psFromBandwidth(uint64_t, uint64_t);
  static uint64_t psFromGFLOPS(uint64_t, float);
  static bool checkedAdd(uint64_t, uint64_t, uint64_t &);
  static bool checkedMul(uint64_t, uint64_t, uint64_t &);

 public:
  explicit PU(ConfigReader &);

  static float decodeFP16(uint16_t);

  void submit(HIL::NVMe::DMAInterface *, uint64_t, MatrixReader, Completion);

  void getStatList(std::vector<Stats> &, std::string) override;
  void getStatValues(std::vector<double> &) override;
  void resetStatValues() override;
};

}  // namespace CSD

}  // namespace SimpleSSD

#endif
