#include "app/main.h"
#include "ui/replace_tool/visual_similarity_service.h"
#include "util/nvg_utils.h"
#include "ui/gui.h"
#include "item_definitions/core/item_definition_store.h"
#include "rendering/core/graphics.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <vector>
#include <memory>
#include <span>
#include <ranges>

VisualSimilarityService& VisualSimilarityService::Get() {
	static VisualSimilarityService instance;
	return instance;
}

namespace {
	// Suggestions below this score are visual noise: the aHash/Dice scoring
	// maps a perfect match to 1.0, so keep only convincing candidates.
	constexpr double kMinimumSimilarityScore = 0.55;
}

VisualSimilarityService::VisualSimilarityService() : m_timer(this), m_nextIdToIndex(1), isIndexed(false) {
	Bind(wxEVT_TIMER, &VisualSimilarityService::OnTimer, this, m_timer.GetId());
}

VisualSimilarityService::~VisualSimilarityService() {
	m_timer.Stop();
}

void VisualSimilarityService::StartIndexing() {
	if (isIndexed || m_timer.IsRunning()) {
		return;
	}
	m_timer.Start(50); // Faster background indexing
}

// ============================================================================
// CORE ALGORITHM HELPERS (Mappingtool 1:1)
// ============================================================================

static bool IsFullyOpaqueRGBA(std::span<const uint8_t> rgba, uint8_t threshold = 10) {
	for (size_t i = 3; i < rgba.size(); i += 4) {
		if (rgba[i] <= threshold) {
			return false;
		}
	}
	return true;
}

static uint64_t CalculateAHashRGBA(std::span<const uint8_t> rgba, int w, int h) {
	// mappingtool: 1. Resize to 8x8 using box sampling
	// grayscale = 0.299*R + 0.587*G + 0.114*B
	uint8_t gray_8x8[64];
	double total_brightness = 0;

	float block_w = (float)w / 8.0f;
	float block_h = (float)h / 8.0f;

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			double block_sum = 0;
			int pixel_count = 0;

			int start_x = (int)(x * block_w);
			int start_y = (int)(y * block_h);
			int end_x = (int)((x + 1) * block_w);
			int end_y = (int)((y + 1) * block_h);

			for (int py = start_y; py < end_y && py < h; ++py) {
				for (int px = start_x; px < end_x && px < w; ++px) {
					int idx = (py * w + px) * 4;
					double gray = 0.299 * rgba[idx] + 0.587 * rgba[idx + 1] + 0.114 * rgba[idx + 2];
					block_sum += gray;
					pixel_count++;
				}
			}

			uint8_t avg = (uint8_t)((pixel_count > 0) ? (block_sum / pixel_count) : 0);
			gray_8x8[y * 8 + x] = avg;
			total_brightness += avg;
		}
	}

	uint8_t global_avg = (uint8_t)(total_brightness / 64.0);
	uint64_t hash = 0;
	for (int i = 0; i < 64; ++i) {
		if (gray_8x8[i] >= global_avg) {
			hash |= (1ULL << i);
		}
	}
	return hash;
}

static std::vector<uint64_t> ExtractPackedMaskRGBA(std::span<const uint8_t> rgba, int& outTruePixels) {
	const size_t count = rgba.size() / 4;
	std::vector<uint64_t> mask((count + 63) / 64, 0);
	size_t truePixels = 0;
	for (size_t i = 0; i < count; ++i) {
		// Strictly > 10 per mappingtool docs
		if (rgba[i * 4 + 3] > 10) {
			mask[i >> 6] |= 1ULL << (i & 63);
			++truePixels;
		}
	}
	outTruePixels = static_cast<int>(truePixels);
	return mask;
}

static int HammingDistance(uint64_t h1, uint64_t h2) {
	return std::popcount(h1 ^ h2);
}

static bool MaskBit(const std::vector<uint64_t>& mask, int x, int y, int width) {
	const size_t index = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
	return (mask[index >> 6] >> (index & 63)) & 1ULL;
}

/** Population count of the intersection of two equally sized masks. */
static int IntersectionCount(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
	const size_t words = std::min(a.size(), b.size());
	int count = 0;
	for (size_t i = 0; i < words; ++i) {
		count += std::popcount(a[i] & b[i]);
	}
	return count;
}

static int PopCount(const std::vector<uint64_t>& mask) {
	int count = 0;
	for (const uint64_t word : mask) {
		count += std::popcount(word);
	}
	return count;
}

/** Nearest neighbour scaling on packed masks; empty when the input is unusable. */
static std::vector<uint64_t> ResizePackedMask(const std::vector<uint64_t>& src, int srcW, int srcH, int dstW, int dstH) {
	if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) {
		return {};
	}
	if (src.size() * 64 < static_cast<size_t>(srcW) * static_cast<size_t>(srcH)) {
		return {};
	}

	std::vector<uint64_t> dst((static_cast<size_t>(dstW) * static_cast<size_t>(dstH) + 63) / 64, 0);
	for (int y = 0; y < dstH; ++y) {
		const int sy = std::clamp((y * srcH) / dstH, 0, srcH - 1);
		for (int x = 0; x < dstW; ++x) {
			const int sx = std::clamp((x * srcW) / dstW, 0, srcW - 1);
			if (MaskBit(src, sx, sy, srcW)) {
				const size_t index = static_cast<size_t>(y) * static_cast<size_t>(dstW) + static_cast<size_t>(x);
				dst[index >> 6] |= 1ULL << (index & 63);
			}
		}
	}
	return dst;
}

// ============================================================================
// SERVICE IMPLEMENTATION
// ============================================================================

VisualSimilarityService::VisualItemData VisualSimilarityService::CalculateData(uint16_t itemId) {
	VisualItemData data;
	data.id = itemId;
	data.isOpaque = false;
	data.aHash = 0;
	data.width = 0;
	data.height = 0;
	data.truePixels = 0;

	if (!g_version.getLoadedVersion()) {
		return data;
	}

	const auto it = g_item_definitions.get(itemId);
	if (!it || it.clientId() == 0) {
		return data;
	}

	Sprite* sprite = g_gui.gfx.getSprite(it.clientId());
	if (!sprite) {
		return data;
	}

	GameSprite* gs = dynamic_cast<GameSprite*>(sprite);
	if (!gs) {
		return data;
	}

	int w, h;
	auto composite = NvgUtils::CreateCompositeRGBA(*gs, w, h);
	if (!composite) {
		return data;
	}

	data.width = w;
	data.height = h;
	data.isOpaque = IsFullyOpaqueRGBA({composite.get(), static_cast<size_t>(w * h * 4)});

	// Store both for robustness
	data.aHash = CalculateAHashRGBA({composite.get(), static_cast<size_t>(w * h * 4)}, w, h);
	data.packedMask = ExtractPackedMaskRGBA({composite.get(), static_cast<size_t>(w * h * 4)}, data.truePixels);

	return data;
}

void VisualSimilarityService::OnTimer(wxTimerEvent&) {
	int processed = 0;
	int maxId = g_item_definitions.getMaxID();

	// Process fewer items per tick to prevent UI lag and handle spikes
	while (processed < 100 && m_nextIdToIndex <= maxId) {
		uint16_t id = m_nextIdToIndex++;
		const auto it = g_item_definitions.get(id);
		if (it && it.clientId() != 0) {
			VisualItemData data = CalculateData(id);
			if (data.IsComplete()) {
				std::lock_guard<std::mutex> lock(dataMutex);
				itemDataCache[id] = std::move(data);
			}
		}
		processed++;
	}

	if (m_nextIdToIndex > maxId) {
		m_timer.Stop();
		std::lock_guard<std::mutex> lock(dataMutex);
		isIndexed = true;
		resultCache.clear();
	}
}

std::vector<uint16_t> VisualSimilarityService::FindSimilar(uint16_t itemId, size_t count) {
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		if (const auto cached = resultCache.find(itemId); cached != resultCache.end()) {
			const std::vector<uint16_t>& ranked = cached->second;
			return count < ranked.size() ? std::vector<uint16_t>(ranked.begin(), ranked.begin() + count) : ranked;
		}
	}

	VisualItemData sourceData;
	{
		std::lock_guard<std::mutex> lock(dataMutex);
		auto it = itemDataCache.find(itemId);
		if (it != itemDataCache.end()) {
			sourceData = it->second;
		}
	}

	if (!isIndexed) {
		return {};
	}

	// A miss leaves sourceData empty, and an item with no sprite is never
	// indexed, so the completeness check has to cover the size and the mask.
	if (!sourceData.IsComplete()) {
		sourceData = CalculateData(itemId);
		if (!sourceData.IsComplete()) {
			return {};
		}
	}

	struct ScoredItem {
		uint16_t id;
		double score;
		bool operator<(const ScoredItem& other) const {
			// mappingtool: sort by similarity_score DESC, then ID ASC
			if (std::abs(score - other.score) > 0.00001) {
				return score > other.score;
			}
			return id < other.id;
		}
	};
	std::vector<ScoredItem> candidates;

	{
		std::lock_guard<std::mutex> lock(dataMutex);
		for (const auto& pair : itemDataCache) {
			const auto& target = pair.second;
			if (target.id == itemId) {
				continue;
			}
			if (!target.IsComplete()) {
				continue;
			}

			double score = 0.0;

			if (sourceData.isOpaque) {
				// Strategy A: aHash for opaque sprites
				// Mappingtool: We only compare against other opaque sprites to avoid matching walls with items
				if (target.isOpaque) {
					int dist = HammingDistance(sourceData.aHash, target.aHash);
					// Score: 1.0 is perfect match (dist 0), 0.0 is max distance (64)
					score = 1.0 - (static_cast<double>(dist) / 64.0);
				} else {
					continue; // Strict 1:1 behavior
				}
			} else {
				// Strategy B: Standard Binary Mask for transparent sprites
				if (target.isOpaque) {
					continue; // Mappingtool: Transparent target does not match opaque candidate
				}

				// Dice can never beat 2 * min(|A|, |B|) / (|A| + |B|), so most
				// candidates are rejected without touching a mask at all.
				const int sourceOnes = sourceData.truePixels;
				const int targetOnes = target.truePixels;
				const int totalOnes = sourceOnes + targetOnes;
				if (totalOnes <= 0) {
					continue;
				}
				if ((2.0 * std::min(sourceOnes, targetOnes)) / totalOnes < kMinimumSimilarityScore) {
					continue;
				}

				int tp = 0;
				if (sourceData.width == target.width && sourceData.height == target.height) {
					tp = IntersectionCount(sourceData.packedMask, target.packedMask);
					if (tp > 0) {
						// Dice: (2.0 * TP) / (|A| + |B|)
						score = (2.0 * tp) / static_cast<double>(totalOnes);
					}
				} else {
					// 1:1 Resize Logic (Nearest Neighbor)
					const int w = std::max(sourceData.width, target.width);
					const int h = std::max(sourceData.height, target.height);
					const auto m1 = ResizePackedMask(sourceData.packedMask, sourceData.width, sourceData.height, w, h);
					const auto m2 = ResizePackedMask(target.packedMask, target.width, target.height, w, h);
					if (m1.empty() || m2.empty() || m1.size() != m2.size()) {
						continue;
					}
					tp = IntersectionCount(m1, m2);
					if (tp > 0) {
						const int resizedOnes = PopCount(m1) + PopCount(m2);
						score = resizedOnes > 0 ? (2.0 * tp) / static_cast<double>(resizedOnes) : 0.0;
					}
				}
			}

			if (score >= kMinimumSimilarityScore) {
				candidates.push_back({ target.id, score });
			}
		}
	}

	if (count > candidates.size()) {
		count = candidates.size();
	}
	std::partial_sort(candidates.begin(), candidates.begin() + count, candidates.end());

	std::vector<uint16_t> results;
	results.reserve(count);
	for (const auto& candidate : candidates | std::views::take(count)) {
		results.push_back(candidate.id);
	}

	// The index is immutable once complete, so a ranking can be reused.
	std::lock_guard<std::mutex> lock(dataMutex);
	if (isIndexed && results.size() <= kMaxCachedResults) {
		resultCache[itemId] = results;
	}
	return results;
}
