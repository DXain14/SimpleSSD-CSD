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

#include "csd/config.hh"

#include <cmath>
#include <cstdlib>
#include <limits>

#include "util/algorithm.hh"
#include "util/simplessd.hh"

namespace SimpleSSD {

namespace CSD {

const char NAME_ENABLE[] = "Enable";
const char NAME_READ_COMPUTE_OPCODE[] = "ReadComputeOpcode";
const char NAME_PU_COMPUTE_GFLOPS[] = "PUComputeGFLOPS";
const char NAME_INTERNAL_READ_BANDWIDTH[] = "InternalReadBandwidth";
const char NAME_MAX_MATRIX_BYTES[] = "MaxMatrixBytes";
const char NAME_MAX_CONTROL_BYTES[] = "MaxControlBytes";
const char NAME_REQUIRE_ICL_CACHE_OFF[] = "RequireICLCacheOff";

Config::Config()
    : enable(false),
      readComputeOpcode(0xC0),
      puComputeGFLOPS(13.3f),
      internalReadBandwidth(11200000000ULL),
      maxMatrixBytes(268435456ULL),
      maxControlBytes(67108864ULL),
      requireICLCacheOff(true) {}

bool Config::setConfig(const char *name, const char *value) {
  bool ret = true;

  if (MATCH_NAME(NAME_ENABLE)) {
    enable = convertBool(value);
  }
  else if (MATCH_NAME(NAME_READ_COMPUTE_OPCODE)) {
    readComputeOpcode = strtoull(value, nullptr, 0);
  }
  else if (MATCH_NAME(NAME_PU_COMPUTE_GFLOPS)) {
    puComputeGFLOPS = strtof(value, nullptr);
  }
  else if (MATCH_NAME(NAME_INTERNAL_READ_BANDWIDTH)) {
    internalReadBandwidth = strtoull(value, nullptr, 0);
  }
  else if (MATCH_NAME(NAME_MAX_MATRIX_BYTES)) {
    maxMatrixBytes = strtoull(value, nullptr, 0);
  }
  else if (MATCH_NAME(NAME_MAX_CONTROL_BYTES)) {
    maxControlBytes = strtoull(value, nullptr, 0);
  }
  else if (MATCH_NAME(NAME_REQUIRE_ICL_CACHE_OFF)) {
    requireICLCacheOff = convertBool(value);
  }
  else {
    ret = false;
  }

  return ret;
}

void Config::update() {
  const long double maxLatency =
      (long double)std::numeric_limits<uint64_t>::max();
  const long double maxMatrix = (long double)maxMatrixBytes;
  long double worstComputeLatency;
  long double worstReadLatency;

  if (readComputeOpcode < 0xC0 || readComputeOpcode > 0xFF) {
    panic("csd: ReadComputeOpcode should be vendor-specific NVM opcode "
          "0xC0-0xFF");
  }
  if (internalReadBandwidth == 0) {
    panic("csd: InternalReadBandwidth should be positive");
  }
  if (maxMatrixBytes == 0) {
    panic("csd: MaxMatrixBytes should be positive");
  }
  if (maxControlBytes < 56 ||
      maxControlBytes > std::numeric_limits<uint32_t>::max()) {
    panic("csd: MaxControlBytes should be in range 56-0xFFFFFFFF");
  }
  if (!std::isfinite(puComputeGFLOPS) || puComputeGFLOPS <= 0.f) {
    panic("csd: PUComputeGFLOPS should be finite and positive");
  }

  worstComputeLatency =
      maxMatrix * 1000.L / (long double)puComputeGFLOPS;
  if (!std::isfinite(worstComputeLatency) ||
      worstComputeLatency >= maxLatency) {
    panic("csd: PUComputeGFLOPS is too small for MaxMatrixBytes");
  }

  worstReadLatency =
      maxMatrix * 1000000000000.L / (long double)internalReadBandwidth;
  if (!std::isfinite(worstReadLatency) ||
      worstReadLatency >= maxLatency) {
    panic("csd: InternalReadBandwidth is too small for MaxMatrixBytes");
  }
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case CSD_READ_COMPUTE_OPCODE:
      ret = readComputeOpcode;
      break;
    case CSD_INTERNAL_READ_BANDWIDTH:
      ret = internalReadBandwidth;
      break;
    case CSD_MAX_MATRIX_BYTES:
      ret = maxMatrixBytes;
      break;
    case CSD_MAX_CONTROL_BYTES:
      ret = maxControlBytes;
      break;
  }

  return ret;
}

float Config::readFloat(uint32_t idx) {
  float ret = 0.f;

  switch (idx) {
    case CSD_PU_COMPUTE_GFLOPS:
      ret = puComputeGFLOPS;
      break;
  }

  return ret;
}

bool Config::readBoolean(uint32_t idx) {
  bool ret = false;

  switch (idx) {
    case CSD_ENABLE:
      ret = enable;
      break;
    case CSD_REQUIRE_ICL_CACHE_OFF:
      ret = requireICLCacheOff;
      break;
  }

  return ret;
}

}  // namespace CSD

}  // namespace SimpleSSD
