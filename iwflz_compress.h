// ============================================================================
// iwflz_compress.h
// ============================================================================
#pragma once
#include <vector>
#include <cstdint>
#include <QString>

// ✅ AJOUTER CETTE DÉCLARATION
bool iwflz_compress(const uint8_t* inData, size_t inSize,
                    uint8_t* outData, size_t* outSize);

class IwfLz_Compress {
public:
    // Compresse un buffer .iwf -> .iwf.lz
    static std::vector<uint8_t> compressBuffer(const std::vector<uint8_t>& plain);

    // Helpers Qt: compresser un fichier .iwf vers .iwf.lz
    // -> retourne true si OK, false si erreur (errmsg rempli)
    static bool compressFile(const QString& inPath, const QString& outPath, QString* errmsg = nullptr);

private:
    static void writeU32BE(std::vector<uint8_t>& out, uint32_t v);
    static size_t findLongestMatch(const std::vector<uint8_t>& outbuf,
                                   const uint8_t* in, size_t inRem,
                                   size_t& bestOff);
    static void emitLiterals(std::vector<uint8_t>& c, const uint8_t* lit, size_t n);
    static void emitBackref(std::vector<uint8_t>& c, size_t length, size_t off);
    static std::vector<uint8_t> compressBlock(const uint8_t* in, size_t len);
};
