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

#ifndef __CSD_CONFIG__
#define __CSD_CONFIG__

#include "sim/base_config.hh"

namespace SimpleSSD {

namespace CSD {

typedef enum {
  CSD_ENABLE,
  CSD_READ_COMPUTE_OPCODE,
  CSD_PU_COMPUTE_GFLOPS,
  CSD_INTERNAL_READ_BANDWIDTH,
  CSD_MAX_MATRIX_BYTES,
  CSD_MAX_CONTROL_BYTES,
  CSD_REQUIRE_ICL_CACHE_OFF,
} CSD_CONFIG;

class Config : public BaseConfig {
 private:
  bool enable;
  uint64_t readComputeOpcode;
  float puComputeGFLOPS;
  uint64_t internalReadBandwidth;
  uint64_t maxMatrixBytes;
  uint64_t maxControlBytes;
  bool requireICLCacheOff;

 public:
  Config();

  bool setConfig(const char *, const char *) override;
  void update() override;

  uint64_t readUint(uint32_t) override;
  float readFloat(uint32_t) override;
  bool readBoolean(uint32_t) override;
};

}  // namespace CSD

}  // namespace SimpleSSD

#endif
