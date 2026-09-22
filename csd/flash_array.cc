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

#include "csd/flash_array.hh"

#include <cstring>

namespace SimpleSSD {

namespace CSD {

FlashArrayStore::FlashArrayStore() : unitSize(0) {
  memset(&stat, 0, sizeof(stat));
}

FlashArrayStore::PhysicalPayloadKey FlashArrayStore::makeKey(
    uint32_t blockIndex, uint32_t pageIndex, uint32_t ioUnitIndex) const {
  PhysicalPayloadKey key = {blockIndex, pageIndex, ioUnitIndex};

  return key;
}

void FlashArrayStore::setUnitSize(uint32_t size) {
  unitSize = size;
}

uint32_t FlashArrayStore::getUnitSize() const {
  return unitSize;
}

bool FlashArrayStore::write(uint32_t blockIndex, uint32_t pageIndex,
                            uint32_t ioUnitIndex, uint64_t offset,
                            const uint8_t *buffer, uint64_t length) {
  if (unitSize == 0 || !buffer || offset + length > unitSize) {
    return false;
  }

  auto &entry = payload[makeKey(blockIndex, pageIndex, ioUnitIndex)];

  if (entry.size() != unitSize) {
    entry.assign(unitSize, 0);
  }

  memcpy(entry.data() + offset, buffer, length);

  stat.writeBytes += length;

  return true;
}

bool FlashArrayStore::read(uint32_t blockIndex, uint32_t pageIndex,
                           uint32_t ioUnitIndex, uint64_t offset,
                           uint8_t *buffer, uint64_t length) {
  if (unitSize == 0 || !buffer || offset + length > unitSize) {
    return false;
  }

  auto iter = payload.find(makeKey(blockIndex, pageIndex, ioUnitIndex));

  if (iter == payload.end() || iter->second.size() != unitSize) {
    return false;
  }

  memcpy(buffer, iter->second.data() + offset, length);

  stat.readBytes += length;

  return true;
}

bool FlashArrayStore::copy(uint32_t srcBlock, uint32_t srcPage,
                           uint32_t srcUnit, uint32_t dstBlock,
                           uint32_t dstPage, uint32_t dstUnit) {
  auto src = payload.find(makeKey(srcBlock, srcPage, srcUnit));

  if (src == payload.end()) {
    return false;
  }

  payload[makeKey(dstBlock, dstPage, dstUnit)] = src->second;
  stat.copyCount++;
  stat.writeBytes += src->second.size();

  return true;
}

void FlashArrayStore::erase(uint32_t blockIndex, uint32_t pageIndex,
                            uint32_t ioUnitIndex) {
  payload.erase(makeKey(blockIndex, pageIndex, ioUnitIndex));
}

void FlashArrayStore::eraseBlock(uint32_t blockIndex) {
  for (auto iter = payload.begin(); iter != payload.end();) {
    if (iter->first.blockIndex == blockIndex) {
      iter = payload.erase(iter);
    }
    else {
      iter++;
    }
  }

  stat.eraseCount++;
}

void FlashArrayStore::clear() {
  payload.clear();
}

void FlashArrayStore::getStatList(std::vector<Stats> &list,
                                  std::string prefix) {
  Stats temp;

  temp.name = prefix + "array.read.bytes";
  temp.desc = "CSD physical flash array payload bytes read";
  list.push_back(temp);

  temp.name = prefix + "array.write.bytes";
  temp.desc = "CSD physical flash array payload bytes written";
  list.push_back(temp);

  temp.name = prefix + "array.erase.count";
  temp.desc = "CSD physical flash array payload block erases";
  list.push_back(temp);

  temp.name = prefix + "array.copy.count";
  temp.desc = "CSD physical flash array payload copies";
  list.push_back(temp);
}

void FlashArrayStore::getStatValues(std::vector<double> &values) {
  values.push_back(stat.readBytes);
  values.push_back(stat.writeBytes);
  values.push_back(stat.eraseCount);
  values.push_back(stat.copyCount);
}

void FlashArrayStore::resetStatValues() {
  memset(&stat, 0, sizeof(stat));
}

}  // namespace CSD

}  // namespace SimpleSSD
