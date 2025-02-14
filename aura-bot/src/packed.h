/*

   Copyright [2008] [Trevor Hogan]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#ifndef PACKED_H
#define PACKED_H

#include "includes.h"
#include <filesystem>

//
// CPacked
//

class CPacked
{
protected:
  CAura* m_Aura;
	bool m_Valid;
	uint32_t m_HeaderSize;
	uint32_t m_CompressedSize;
	uint32_t m_HeaderVersion;
	uint32_t m_DecompressedSize;
	uint32_t m_NumBlocks;
	uint32_t m_War3Identifier;
	uint32_t m_War3Version;
	uint16_t m_BuildNumber;
	uint16_t m_Flags;
	uint32_t m_ReplayLength;
	std::string m_Compressed;
	std::string m_Decompressed;

public:
  CPacked(CAura* nAura);
	~CPacked();

	[[nodiscard]] bool GetValid()				{ return m_Valid; }
	[[nodiscard]] uint32_t GetHeaderSize()		{ return m_HeaderSize; }
	[[nodiscard]] uint32_t GetCompressedSize()	{ return m_CompressedSize; }
	[[nodiscard]] uint32_t GetHeaderVersion()	{ return m_HeaderVersion; }
	[[nodiscard]] uint32_t GetDecompressedSize()	{ return m_DecompressedSize; }
  [[nodiscard]] const std::string& GetDecompressed() { return m_Decompressed; }
	[[nodiscard]] uint32_t GetNumBlocks()		{ return m_NumBlocks; }
	[[nodiscard]] uint32_t GetWar3Identifier()	{ return m_War3Identifier; }
	[[nodiscard]] uint32_t GetWar3Version()		{ return m_War3Version; }
	[[nodiscard]] uint16_t GetBuildNumber()		{ return m_BuildNumber; }
	[[nodiscard]] uint16_t GetFlags()			{ return m_Flags; }
	[[nodiscard]] uint32_t GetReplayLength()		{ return m_ReplayLength; }

	void SetWar3Version(const uint32_t nWar3Version)			{ m_War3Version = nWar3Version; }
	void SetBuildNumber(const uint16_t nBuildNumber)			{ m_BuildNumber = nBuildNumber; }
	void SetFlags(const uint16_t nFlags)						{ m_Flags = nFlags; }
	void SetReplayLength(const uint32_t nReplayLength)			{ m_ReplayLength = nReplayLength; }

	bool Load(const std::filesystem::path& filePath, const bool allBlocks);
	bool Save(const bool TFT, const std::filesystem::path& filePath);
	bool Extract(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath);
	bool Pack(const bool TFT, const std::filesystem::path& inputPath, const std::filesystem::path& outputPath);
	void Decompress(const bool allBlocks);
	void Compress(const bool TFT);

  void Reset() {
    m_Valid = true;
    m_Compressed.clear();
    m_Decompressed.clear();
  };
};

#endif
