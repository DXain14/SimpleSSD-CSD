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

#ifndef __CSD_FLASH_ARRAY__
#define __CSD_FLASH_ARRAY__

#include <cinttypes>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sim/statistics.hh"

namespace SimpleSSD {

namespace CSD {

class FlashArrayStore : public StatObject {
 private:
  struct PhysicalPayloadKey {
    uint32_t blockIndex;
    uint32_t pageIndex;
    uint32_t ioUnitIndex;

    bool operator==(const PhysicalPayloadKey &rhs) const {
      return blockIndex == rhs.blockIndex && pageIndex == rhs.pageIndex &&
             ioUnitIndex == rhs.ioUnitIndex;
    }
  };

  struct PhysicalPayloadKeyHash {
    std::size_t operator()(const PhysicalPayloadKey &key) const {
      std::size_t h = std::hash<uint32_t>()(key.blockIndex);

      h ^= std::hash<uint32_t>()(key.pageIndex) + 0x9E3779B9 + (h << 6) +
           (h >> 2);
      h ^= std::hash<uint32_t>()(key.ioUnitIndex) + 0x9E3779B9 + (h << 6) +
           (h >> 2);

      return h;
    }
  };

  uint32_t unitSize;
  std::unordered_map<PhysicalPayloadKey, std::vector<uint8_t>,
                     PhysicalPayloadKeyHash>
      payload;

  struct {
    uint64_t readBytes;
    uint64_t writeBytes;
    uint64_t eraseCount;
    uint64_t copyCount;
  } stat;

  PhysicalPayloadKey makeKey(uint32_t, uint32_t, uint32_t) const;

 public:
  FlashArrayStore();

  void setUnitSize(uint32_t);
  uint32_t getUnitSize() const;

  bool write(uint32_t, uint32_t, uint32_t, uint64_t, const uint8_t *,
             uint64_t);
  bool read(uint32_t, uint32_t, uint32_t, uint64_t, uint8_t *, uint64_t);
  bool copy(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
  void erase(uint32_t, uint32_t, uint32_t);
  void eraseBlock(uint32_t);
  void clear();

  void getStatList(std::vector<Stats> &, std::string) override;
  void getStatValues(std::vector<double> &) override;
  void resetStatValues() override;
};

}  // namespace CSD

}  // namespace SimpleSSD

#endif
