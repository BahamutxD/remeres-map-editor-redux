#ifndef RME_IO_COLLECTION_XML_SERVICE_H_
#define RME_IO_COLLECTION_XML_SERVICE_H_

#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <filesystem>
#include <cstdint>

class Brush;

namespace rme::collection {

class CollectionXmlService {
public:
	/**
	 * Sanitizes a tileset name for use as a filesystem filename.
	 * Replaces < > : " / \ | ? * and control chars with '_'.
	 * Trims leading and trailing spaces and dots.
	 */
	static std::string SanitizeTilesetFilename(std::string_view name);

	/**
	 * Returns the collections directory under the active client version's data folder:
	 * <data_dir>/<version>/tilesets/collections/
	 */
	static std::filesystem::path GetCollectionsDirectory();

	/**
	 * Returns full path to palettes.xml for the active client version.
	 */
	static std::filesystem::path GetPalettesXmlPath();

	/**
	 * Returns full path to a collection tileset xml file:
	 * <data_dir>/<version>/tilesets/collections/<sanitizedName>.xml
	 */
	static std::filesystem::path GetTilesetFilePath(std::string_view tilesetName);

	/**
	 * Returns names of all existing collection tilesets found in the collections folder
	 * and/or declared in palettes.xml under <palette name="Collection">.
	 */
	static std::vector<std::string> GetExistingCollectionTilesetNames();

	/**
	 * Saves a collection tileset to disk:
	 * <tileset name="<tilesetName>">
	 *     <brush name="..."/>
	 *     <item id="..."/>
	 * </tileset>
	 * If appendToExisting is true, merges with any existing entries in the file.
	 */
	static bool SaveTilesetXml(
		const std::string& tilesetName,
		std::span<const uint16_t> itemIds,
		std::span<Brush* const> brushes,
		bool appendToExisting,
		std::string& outErrorMessage);

	/**
	 * Ensures <palette name="Collection"> in palettes.xml includes the tileset file:
	 * <include file="tilesets/collections/<sanitizedName>.xml" />
	 */
	static bool EnsurePaletteInclude(
		const std::string& tilesetName,
		std::string& outErrorMessage);
};

} // namespace rme::collection

#endif // RME_IO_COLLECTION_XML_SERVICE_H_
