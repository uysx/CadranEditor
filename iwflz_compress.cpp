// ============================================================================
// iwflz_compress_LAZY.cpp - VERSION AVEC LAZY MATCHING + MULTI-BLOCS CORRIGÉ
// ============================================================================

#include "iwflz_compress.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include <QDebug>

#define MIN_MATCH 3
#define MAX_MATCH 1024
#define MAX_OFFSET 8191
#define MAX_BLOCK_SIZE_SOURCE 4096  // Découper tous les 4KB de données source

// Structure pour stocker les matches
struct Match {
    int length;
    size_t offset;

    // Comparateur pour choisir le meilleur match
    bool isBetterThan(const Match& other) const {
        // 1. Priorité au plus long
        if (length > other.length) return true;
        if (length < other.length) return false;

        // 2. À longueur égale, préférer offset plus petit
        return offset < other.offset;
    }
};

// Recherche de match standard
static Match findBestMatch(const std::vector<uint8_t>& outbuf,
                           const uint8_t* inData,
                           size_t in_pos,
                           size_t inSize) {
    Match best = {0, 0};

    size_t outPos = outbuf.size();
    size_t available = inSize - in_pos;

    if (available < MIN_MATCH) return best;

    // Fenêtre de recherche
    size_t winStart = (outPos > MAX_OFFSET + 1) ? (outPos - MAX_OFFSET - 1) : 0;

    // Recherche linéaire dans toute la fenêtre
    for (size_t pos = winStart; pos < outPos; pos++) {
        size_t offset = (outPos - 1) - pos;
        if (offset > MAX_OFFSET) continue;

        // Calculer la longueur du match
        int match_len = 0;
        int max_len = std::min<int>(available, MAX_MATCH);

        while (match_len < max_len &&
               pos + match_len < outbuf.size() &&
               outbuf[pos + match_len] == inData[in_pos + match_len]) {
            match_len++;
        }

        // Garder le meilleur match
        if (match_len >= MIN_MATCH) {
            Match candidate = {match_len, offset};
            if (candidate.isBetterThan(best)) {
                best = candidate;
            }
        }
    }

    return best;
}

// Détection RLE améliorée
static Match findRLE(const std::vector<uint8_t>& outbuf,
                     const uint8_t* inData,
                     size_t in_pos,
                     size_t inSize) {
    Match rle = {0, 0};

    if (outbuf.empty()) return rle;

    uint8_t last = outbuf.back();
    int rle_len = 0;
    size_t available = inSize - in_pos;
    int max_rle = std::min<int>(available, MAX_MATCH);

    while (rle_len < max_rle && inData[in_pos + rle_len] == last) {
        rle_len++;
    }

    if (rle_len >= MIN_MATCH) {
        rle.length = rle_len;
        rle.offset = 0;  // Offset 0 = RLE
    }

    return rle;
}

// Trouver le meilleur match avec RLE
static Match findBestMatchWithRLE(const std::vector<uint8_t>& outbuf,
                                  const uint8_t* inData,
                                  size_t in_pos,
                                  size_t inSize) {
    Match rle = findRLE(outbuf, inData, in_pos, inSize);
    Match standard = findBestMatch(outbuf, inData, in_pos, inSize);

    if (rle.isBetterThan(standard)) {
        return rle;
    }
    return standard;
}

// ============================================================================
// NOUVELLE FONCTION: Compresser UN SEUL BLOC
// ============================================================================
static bool compressBlock(const uint8_t* inData,
                          size_t blockStartPos,
                          size_t blockEndPos,
                          std::vector<uint8_t>& outbuf,
                          uint8_t* outData,
                          size_t* outPos,
                          size_t maxOut,
                          bool isFirstBlockOfFile) {

    size_t in_pos = blockStartPos;
    std::vector<uint8_t> blockData;  // Données compressées du bloc

    // ✅ CRITIQUE: Premier token du BLOC (réinitialisé pour chaque bloc)
    bool first_token = true;

    // === RÈGLE SPÉCIALE: Le premier BLOC du fichier doit commencer par 8 littéraux "iwf..."
    if (isFirstBlockOfFile) {
        size_t first_lit_count = std::min<size_t>(8, blockEndPos - in_pos);

        // Token du premier run
        uint8_t token = (first_lit_count == 8) ? 0x27 : (uint8_t)((first_lit_count - 1) & 0x1F);
        blockData.push_back(token);

        // Copier les littéraux
        for (size_t i = 0; i < first_lit_count; ++i) {
            blockData.push_back(inData[in_pos]);
            outbuf.push_back(inData[in_pos]);
            in_pos++;
        }

        first_token = false;  // On a émis le premier token
    }

    // === Boucle principale avec LAZY MATCHING
    while (in_pos < blockEndPos) {
        // Trouver le meilleur match à la position actuelle
        Match current = findBestMatchWithRLE(outbuf, inData, in_pos, blockEndPos);

        // Lazy matching
        Match next = {0, 0};
        bool use_lazy = false;

        if (current.length >= MIN_MATCH && in_pos + 1 < blockEndPos) {
            std::vector<uint8_t> temp_outbuf = outbuf;
            temp_outbuf.push_back(inData[in_pos]);
            next = findBestMatchWithRLE(temp_outbuf, inData, in_pos + 1, blockEndPos);

            if (next.length >= MIN_MATCH && next.length > current.length + 1) {
                if (!(current.offset == 0 && current.length >= 20)) {
                    use_lazy = true;
                }
            }
        }

        if (use_lazy) {
            // Émettre 1 littéral
            // ✅ Si c'est le premier token, encoder correctement
            if (first_token) {
                blockData.push_back(0x00);  // Token: 1 littéral
                first_token = false;
            } else {
                blockData.push_back(0x00);  // Token: 1 littéral
            }
            blockData.push_back(inData[in_pos]);
            outbuf.push_back(inData[in_pos]);
            in_pos++;
            current = next;
        }

        // Décider : backref ou littéraux ?
        if (current.length >= MIN_MATCH && current.offset <= MAX_OFFSET) {
            // ✅ BACKREF trouvée

            // ✅ CRITIQUE: Si c'est le premier token, on ne peut PAS émettre un backref directement
            if (first_token) {
                // Forcer l'émission d'AU MOINS 1 littéral avant le backref
                blockData.push_back(0x00);  // Token: 1 littéral
                blockData.push_back(inData[in_pos]);
                outbuf.push_back(inData[in_pos]);
                in_pos++;
                first_token = false;
                continue;  // Re-chercher un match à la position suivante
            }

            size_t length = current.length;
            size_t offset = current.offset;

            // Encodage du token
            size_t base = length - 2;
            if (base <= 6) {
                // Format court (longueurs 3-8)
                uint8_t hi3 = (uint8_t)base;
                uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
                uint8_t token = (uint8_t)((hi3 << 5) | low5);
                blockData.push_back(token);
                blockData.push_back((uint8_t)(offset & 0xFF));
            } else if (base == 7) {
                // Format long (longueur = 9)
                uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
                uint8_t token = (uint8_t)((7u << 5) | low5);
                blockData.push_back(token);
                blockData.push_back(0x00);
                blockData.push_back((uint8_t)(offset & 0xFF));
            } else {
                // Format long avec extension
                uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
                uint8_t token = (uint8_t)((7u << 5) | low5);
                blockData.push_back(token);

                size_t ext = length - 9;
                while (ext >= 255) {
                    blockData.push_back(0xFF);
                    ext -= 255;
                }
                blockData.push_back((uint8_t)ext);
                blockData.push_back((uint8_t)(offset & 0xFF));
            }

            // Expansion dans outbuf
            if (offset == 0) {
                uint8_t byte_to_repeat = outbuf.back();
                for (size_t i = 0; i < length; ++i) {
                    outbuf.push_back(byte_to_repeat);
                }
            } else {
                size_t src = outbuf.size() - 1 - offset;
                if (src >= outbuf.size()) return false;
                for (size_t i = 0; i < length; ++i) {
                    outbuf.push_back(outbuf[src]);
                    ++src;
                }
            }
            in_pos += length;
            first_token = false;

        } else {
            // Pas de match → Littéraux
            size_t lit_start = in_pos;
            size_t lit_count = 1;

            outbuf.push_back(inData[in_pos]);
            in_pos++;

            // Accumuler jusqu'à 32 littéraux
            while (in_pos < blockEndPos && lit_count < 32) {
                Match peek = findBestMatchWithRLE(outbuf, inData, in_pos, blockEndPos);
                if (peek.length >= MIN_MATCH) break;
                outbuf.push_back(inData[in_pos]);
                ++in_pos;
                ++lit_count;
            }

            // Émettre les littéraux par chunks de 32
            size_t lit_emitted = 0;
            while (lit_emitted < lit_count) {
                size_t chunk = std::min<size_t>(32, lit_count - lit_emitted);

                uint8_t token = (uint8_t)((chunk - 1) & 0x1F);
                blockData.push_back(token);

                for (size_t i = 0; i < chunk; i++) {
                    blockData.push_back(inData[lit_start + lit_emitted + i]);
                }

                lit_emitted += chunk;
                first_token = false;
            }
        }
    }

    // Écrire le bloc dans outData
    size_t blockSize = blockData.size();

    // Vérifier qu'on a de la place
    if (*outPos + 4 + blockSize > maxOut) {
        return false;
    }

    // Header du bloc (4 octets, big-endian)
    uint32_t be = static_cast<uint32_t>(blockSize);
    outData[*outPos + 0] = static_cast<uint8_t>((be >> 24) & 0xFF);
    outData[*outPos + 1] = static_cast<uint8_t>((be >> 16) & 0xFF);
    outData[*outPos + 2] = static_cast<uint8_t>((be >>  8) & 0xFF);
    outData[*outPos + 3] = static_cast<uint8_t>( be        & 0xFF);

    // Données du bloc
    memcpy(outData + *outPos + 4, blockData.data(), blockSize);

    *outPos += 4 + blockSize;

    return true;
}

// ============================================================================
// FONCTION PRINCIPALE: Compresser tout le fichier en multi-blocs
// ============================================================================
bool iwflz_compress(const uint8_t* inData, size_t inSize,
                    uint8_t* outData, size_t* outSize) {
    if (!inData || !outData || !outSize || inSize == 0) {
        return false;
    }

    std::vector<uint8_t> outbuf;
    outbuf.reserve(inSize);

    size_t in_pos = 0;
    size_t out_pos = 0;
    size_t max_out = *outSize;
    int block_num = 0;

    // ✅ BOUCLE MULTI-BLOCS
    while (in_pos < inSize) {
        // Déterminer la fin du bloc
        size_t block_end = std::min(in_pos + MAX_BLOCK_SIZE_SOURCE, inSize);

        // ✅ CRITIQUE: isFirstBlockOfFile est true seulement pour le premier bloc
        bool isFirstBlockOfFile = (block_num == 0);

        // Compresser ce bloc
        if (!compressBlock(inData, in_pos, block_end, outbuf,
                           outData, &out_pos, max_out, isFirstBlockOfFile)) {
            return false;
        }

        // Avancer
        in_pos = block_end;
        block_num++;
    }

    *outSize = out_pos;
    return true;
}















// // ============================================================================
// // iwflz_compress_LAZY.cpp - VERSION AVEC LAZY MATCHING
// // ============================================================================

// #include "iwflz_compress.h"
// #include <cstring>
// #include <algorithm>
// #include <vector>
// #include <QDebug>

// #define MIN_MATCH 3
// #define MAX_MATCH 1024
// #define MAX_OFFSET 8191

// // Structure pour stocker les matches
// struct Match {
//     int length;
//     size_t offset;

//     // Comparateur pour choisir le meilleur match
//     bool isBetterThan(const Match& other) const {
//         // 1. Priorité au plus long
//         if (length > other.length) return true;
//         if (length < other.length) return false;

//         // 2. À longueur égale, préférer offset plus petit
//         return offset < other.offset;
//     }
// };

// // Recherche de match standard
// static Match findBestMatch(const std::vector<uint8_t>& outbuf,
//                            const uint8_t* inData,
//                            size_t in_pos,
//                            size_t inSize) {
//     Match best = {0, 0};

//     size_t outPos = outbuf.size();
//     size_t available = inSize - in_pos;

//     if (available < MIN_MATCH) return best;

//     // Fenêtre de recherche
//     size_t winStart = (outPos > MAX_OFFSET + 1) ? (outPos - MAX_OFFSET - 1) : 0;

//     // Recherche linéaire dans toute la fenêtre
//     for (size_t pos = winStart; pos < outPos; pos++) {
//         size_t offset = (outPos - 1) - pos;
//         if (offset > MAX_OFFSET) continue;

//         // Calculer la longueur du match
//         int match_len = 0;
//         int max_len = std::min<int>(available, MAX_MATCH);

//         while (match_len < max_len &&
//                pos + match_len < outbuf.size() &&
//                outbuf[pos + match_len] == inData[in_pos + match_len]) {
//             match_len++;
//         }

//         // Garder le meilleur match
//         if (match_len >= MIN_MATCH) {
//             Match candidate = {match_len, offset};
//             if (candidate.isBetterThan(best)) {
//                 best = candidate;
//             }
//         }
//     }

//     return best;
// }

// // Détection RLE améliorée
// static Match findRLE(const std::vector<uint8_t>& outbuf,
//                      const uint8_t* inData,
//                      size_t in_pos,
//                      size_t inSize) {
//     Match rle = {0, 0};

//     if (outbuf.empty()) return rle;

//     uint8_t last = outbuf.back();
//     int rle_len = 0;
//     size_t available = inSize - in_pos;
//     int max_rle = std::min<int>(available, MAX_MATCH);

//     while (rle_len < max_rle && inData[in_pos + rle_len] == last) {
//         rle_len++;
//     }

//     if (rle_len >= MIN_MATCH) {
//         rle.length = rle_len;
//         rle.offset = 0;  // Offset 0 = RLE
//     }

//     return rle;
// }

// // ✅ NOUVEAU: Trouver le meilleur match avec RLE
// static Match findBestMatchWithRLE(const std::vector<uint8_t>& outbuf,
//                                   const uint8_t* inData,
//                                   size_t in_pos,
//                                   size_t inSize) {
//     Match rle = findRLE(outbuf, inData, in_pos, inSize);
//     Match standard = findBestMatch(outbuf, inData, in_pos, inSize);

//     if (rle.isBetterThan(standard)) {
//         return rle;
//     }
//     return standard;
// }

// bool iwflz_compress(const uint8_t* inData, size_t inSize,
//                     uint8_t* outData, size_t* outSize) {
//     if (!inData || !outData || !outSize || inSize == 0) {
//         return false;
//     }

//     std::vector<uint8_t> outbuf;
//     outbuf.reserve(inSize);

//     size_t in_pos  = 0;
//     size_t out_pos = 0;
//     size_t max_out = *outSize;

//     // === RÈGLE SPÉCIALE: Le premier token doit être 8 littéraux
//     bool startsWithIwf =
//         (inSize >= 5 &&
//          inData[0] == 'i' && inData[1] == 'w' && inData[2] == 'f' &&
//          inData[3] == 0x00 && inData[4] == 0x01);

//     size_t first_lit_count = std::min<size_t>(2, inSize);
//     if (startsWithIwf) first_lit_count = 8;

//     if (out_pos + 1 + first_lit_count > max_out) return false;

//     // Token du premier run (toujours 0x27 pour 8 littéraux)
//     uint8_t first_token = (first_lit_count == 8) ? 0x27 : (uint8_t)((first_lit_count - 1) & 0x1F);
//     outData[out_pos++] = first_token;

//     // Copier les premiers littéraux
//     memcpy(outData + out_pos, inData, first_lit_count);
//     for (size_t i = 0; i < first_lit_count; ++i) {
//         outbuf.push_back(inData[i]);
//     }
//     out_pos += first_lit_count;
//     in_pos  += first_lit_count;

//     // === Boucle principale avec LAZY MATCHING
//     while (in_pos < inSize) {
//         // Trouver le meilleur match à la position actuelle
//         Match current = findBestMatchWithRLE(outbuf, inData, in_pos, inSize);

//         // ✅ LAZY MATCHING: Si on a un match, regarder aussi position+1
//         Match next = {0, 0};
//         bool use_lazy = false;

//         if (current.length >= MIN_MATCH && in_pos + 1 < inSize) {
//             // Simuler l'ajout d'un littéral dans outbuf
//             std::vector<uint8_t> temp_outbuf = outbuf;
//             temp_outbuf.push_back(inData[in_pos]);

//             // Chercher à position+1
//             next = findBestMatchWithRLE(temp_outbuf, inData, in_pos + 1, inSize);

//             // Décision lazy: prendre next si nettement meilleur
//             // Critère: next.length > current.length + 1
//             // (compense le coût du littéral émis)
//             if (next.length >= MIN_MATCH && next.length > current.length + 1) {
//                 use_lazy = true;
//             }

//             // Cas spécial: si current est RLE et next aussi, privilégier current
//             // (éviter de casser un long RLE pour rien)
//             if (current.offset == 0 && next.offset == 0 && current.length >= 20) {
//                 use_lazy = false;
//             }
//         }

//         if (use_lazy) {
//             // Stratégie lazy: émettre 1 littéral et prendre le match suivant
//             if (out_pos + 2 > max_out) return false;

//             // Token littéral (1 octet)
//             outData[out_pos++] = 0x00;
//             outData[out_pos++] = inData[in_pos];
//             outbuf.push_back(inData[in_pos]);
//             in_pos++;

//             // Maintenant utiliser le match à position suivante
//             current = next;
//         }

//         // Décider : backref ou littéraux ?
//         if (current.length >= MIN_MATCH && current.offset <= MAX_OFFSET) {
//             // ✅ BACKREF trouvée
//             size_t length = current.length;
//             size_t offset = current.offset;

//             // Encodage du token
//             size_t base = length - 2;
//             if (base <= 6) {
//                 // Format court (longueurs 3-8)
//                 if (out_pos + 2 > max_out) return false;
//                 uint8_t hi3 = (uint8_t)base;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((hi3 << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else if (base == 7) {
//                 // Format long (longueur = 9)
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = 0x00;  // Extension = 0 pour len=9
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else {
//                 // Format long avec extension (longueur >= 10)
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;

//                 size_t ext = length - 9;
//                 while (ext >= 255) {
//                     if (out_pos + 1 > max_out) return false;
//                     outData[out_pos++] = 0xFF;
//                     ext -= 255;
//                 }
//                 if (out_pos + 2 > max_out) return false;
//                 outData[out_pos++] = (uint8_t)ext;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             }

//             // Expansion dans outbuf
//             if (offset == 0) {
//                 // RLE: répéter le dernier octet
//                 uint8_t byte_to_repeat = outbuf.back();
//                 for (size_t i = 0; i < length; ++i) {
//                     outbuf.push_back(byte_to_repeat);
//                 }
//             } else {
//                 // Copie standard
//                 size_t src = outbuf.size() - 1 - offset;
//                 if (src >= outbuf.size()) {
//                     return false;
//                 }

//                 for (size_t i = 0; i < length; ++i) {
//                     outbuf.push_back(outbuf[src]);
//                     ++src;
//                 }
//             }
//             in_pos += length;

//         } else {
//             // ❌ Pas de match → Littéraux

//             // Accumuler des littéraux (jusqu'à 32)
//             size_t lit_start = in_pos;
//             size_t lit_count = 1;

//             outbuf.push_back(inData[in_pos]);
//             in_pos++;

//             // Continuer d'accumuler tant qu'on ne trouve pas de match
//             while (in_pos < inSize && lit_count < 32) {
//                 Match peek = findBestMatchWithRLE(outbuf, inData, in_pos, inSize);

//                 if (peek.length >= MIN_MATCH) {
//                     // Match trouvé, arrêter les littéraux
//                     break;
//                 }

//                 outbuf.push_back(inData[in_pos]);
//                 ++in_pos;
//                 ++lit_count;
//             }

//             // Émettre les littéraux
//             size_t lit_emitted = 0;
//             while (lit_emitted < lit_count) {
//                 size_t chunk = std::min<size_t>(32, lit_count - lit_emitted);
//                 if (out_pos + 1 + chunk > max_out) return false;

//                 uint8_t token = (uint8_t)((chunk - 1) & 0x1F);
//                 outData[out_pos++] = token;

//                 memcpy(outData + out_pos, inData + lit_start + lit_emitted, chunk);
//                 out_pos += chunk;
//                 lit_emitted += chunk;
//             }
//         }
//     }

//     // === Ajouter le header de 4 octets
//     size_t payload_size = out_pos;

//     if (payload_size + 4 > max_out) {
//         return false;
//     }

//     // Décaler le payload de 4 octets vers la droite
//     for (size_t i = payload_size; i-- > 0; ) {
//         outData[4 + i] = outData[i];
//     }

//     // Écrire le header (big-endian)
//     uint32_t be = static_cast<uint32_t>(payload_size);
//     outData[0] = static_cast<uint8_t>((be >> 24) & 0xFF);
//     outData[1] = static_cast<uint8_t>((be >> 16) & 0xFF);
//     outData[2] = static_cast<uint8_t>((be >>  8) & 0xFF);
//     outData[3] = static_cast<uint8_t>( be        & 0xFF);

//     *outSize = payload_size + 4;

//     return true;
// }


















// // ============================================================================
// // iwflz_compress.cpp - VERSION FINALE AVEC RLE
// // ============================================================================

// #include "iwflz_compress.h"
// #include <cstring>
// #include <algorithm>
// #include <vector>
// #include <QDebug>

// #define MIN_MATCH 3
// #define MAX_MATCH 1024
// #define MAX_OFFSET 8191

// // Structure pour stocker les matches
// struct Match {
//     int length;
//     size_t offset;
// };

// // Recherche de match standard (recherche linéaire dans la fenêtre)
// static Match findBestMatch(const std::vector<uint8_t>& outbuf,
//                            const uint8_t* inData,
//                            size_t in_pos,
//                            size_t inSize) {
//     Match best = {0, 0};

//     size_t outPos = outbuf.size();
//     size_t available = inSize - in_pos;

//     if (available < MIN_MATCH) return best;

//     // Fenêtre de recherche
//     size_t winStart = (outPos > MAX_OFFSET + 1) ? (outPos - MAX_OFFSET - 1) : 0;

//     // Recherche linéaire dans toute la fenêtre
//     for (size_t pos = winStart; pos < outPos; pos++) {
//         size_t offset = (outPos - 1) - pos;
//         if (offset > MAX_OFFSET) continue;

//         // Calculer la longueur du match
//         int match_len = 0;
//         int max_len = std::min<int>(available, MAX_MATCH);

//         while (match_len < max_len &&
//                pos + match_len < outbuf.size() &&
//                outbuf[pos + match_len] == inData[in_pos + match_len]) {
//             match_len++;
//         }

//         // Garder le meilleur match (priorité au plus long)
//         if (match_len >= MIN_MATCH) {
//             if (match_len > best.length) {
//                 best.length = match_len;
//                 best.offset = offset;
//             }
//         }
//     }

//     return best;
// }

// bool iwflz_compress(const uint8_t* inData, size_t inSize,
//                     uint8_t* outData, size_t* outSize) {
//     if (!inData || !outData || !outSize || inSize == 0) {
//         return false;
//     }

//     std::vector<uint8_t> outbuf;
//     outbuf.reserve(inSize);

//     size_t in_pos  = 0;
//     size_t out_pos = 0;
//     size_t max_out = *outSize;

//     // === RÈGLE SPÉCIALE: Le premier token doit être des littéraux
//     bool startsWithIwf =
//         (inSize >= 5 &&
//          inData[0] == 'i' && inData[1] == 'w' && inData[2] == 'f' &&
//          inData[3] == 0x00 && inData[4] == 0x01);

//     size_t first_lit_count = std::min<size_t>(2, inSize);
//     if (startsWithIwf) first_lit_count = 8;

//     if (out_pos + 1 + first_lit_count > max_out) return false;

//     // Token du premier run
//     uint8_t first_token;
//     if (first_lit_count == 8) {
//         first_token = 0x27;  // Token spécial pour 8 littéraux
//     } else {
//         first_token = (uint8_t)((first_lit_count - 1) & 0x1F);
//     }

//     outData[out_pos++] = first_token;

//     // Copier les premiers littéraux
//     memcpy(outData + out_pos, inData, first_lit_count);
//     for (size_t i = 0; i < first_lit_count; ++i) {
//         outbuf.push_back(inData[i]);
//     }
//     out_pos += first_lit_count;
//     in_pos  += first_lit_count;

//     // === Boucle principale de compression
//     while (in_pos < inSize) {
//         // ✅ NOUVEAU: Détection RLE (répétitions)
//         Match rle = {0, 0};
//         if (!outbuf.empty()) {
//             uint8_t last = outbuf.back();
//             int rle_len = 0;
//             size_t available = inSize - in_pos;
//             int max_rle = std::min<int>(available, MAX_MATCH);

//             while (rle_len < max_rle && inData[in_pos + rle_len] == last) {
//                 rle_len++;
//             }

//             if (rle_len >= MIN_MATCH) {
//                 rle.length = rle_len;
//                 rle.offset = 0;  // Offset 0 = RLE
//             }
//         }

//         // Chercher aussi un match standard
//         Match m = findBestMatch(outbuf, inData, in_pos, inSize);

//         // ✅ NOUVEAU: Choisir entre RLE et match standard
//         // Priorité au plus long
//         if (rle.length > m.length) {
//             m = rle;
//         }

//         // Décider : backref ou littéraux ?
//         if (m.length >= MIN_MATCH && m.offset <= MAX_OFFSET) {
//             // ✅ BACKREF trouvée
//             size_t length = m.length;
//             size_t offset = m.offset;

//             // Encodage du token
//             size_t base = length - 2;
//             if (base <= 6) {
//                 // Format court (longueurs 3-8)
//                 if (out_pos + 2 > max_out) return false;
//                 uint8_t hi3 = (uint8_t)base;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((hi3 << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else if (base == 7) {
//                 // Format long (longueur = 9)
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = 0x00;  // Extension = 0 pour len=9
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else {
//                 // Format long avec extension (longueur >= 10)
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;

//                 size_t ext = length - 9;
//                 while (ext >= 255) {
//                     if (out_pos + 1 > max_out) return false;
//                     outData[out_pos++] = 0xFF;
//                     ext -= 255;
//                 }
//                 if (out_pos + 2 > max_out) return false;
//                 outData[out_pos++] = (uint8_t)ext;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             }

//             // Expansion dans outbuf (simuler le décodeur)
//             if (offset == 0) {
//                 // ✅ RLE: répéter le dernier octet
//                 uint8_t byte_to_repeat = outbuf.back();
//                 for (size_t i = 0; i < length; ++i) {
//                     outbuf.push_back(byte_to_repeat);
//                 }
//             } else {
//                 // Copie standard avec chevauchement possible
//                 size_t src = outbuf.size() - 1 - offset;
//                 if (src >= outbuf.size()) {
//                     qDebug() << "❌ ERREUR EXPANSION: src" << src << ">= outbuf.size()" << outbuf.size();
//                     return false;
//                 }

//                 for (size_t i = 0; i < length; ++i) {
//                     outbuf.push_back(outbuf[src]);
//                     ++src;
//                 }
//             }
//             in_pos += length;

//         } else {
//             // ❌ Pas de match → Littéraux

//             // Accumuler des littéraux (jusqu'à 32)
//             size_t lit_start = in_pos;
//             size_t lit_count = 1;

//             outbuf.push_back(inData[in_pos]);
//             in_pos++;

//             // Continuer d'accumuler tant qu'on ne trouve pas de match
//             while (in_pos < inSize && lit_count < 32) {
//                 // Vérifier si on trouve un match au prochain octet
//                 Match next_rle = {0, 0};
//                 if (!outbuf.empty()) {
//                     uint8_t last = outbuf.back();
//                     int rle_len = 0;
//                     size_t available = inSize - in_pos;
//                     int max_rle = std::min<int>(available, MAX_MATCH);

//                     while (rle_len < max_rle && inData[in_pos + rle_len] == last) {
//                         rle_len++;
//                     }

//                     if (rle_len >= MIN_MATCH) {
//                         next_rle.length = rle_len;
//                         next_rle.offset = 0;
//                     }
//                 }

//                 Match next = findBestMatch(outbuf, inData, in_pos, inSize);

//                 if (next_rle.length > next.length) {
//                     next = next_rle;
//                 }

//                 if (next.length >= MIN_MATCH) {
//                     // Match trouvé, arrêter les littéraux
//                     break;
//                 }

//                 outbuf.push_back(inData[in_pos]);
//                 ++in_pos;
//                 ++lit_count;
//             }

//             // Émettre les littéraux
//             size_t lit_emitted = 0;
//             while (lit_emitted < lit_count) {
//                 size_t chunk = std::min<size_t>(32, lit_count - lit_emitted);
//                 if (out_pos + 1 + chunk > max_out) return false;

//                 uint8_t token = (uint8_t)((chunk - 1) & 0x1F);
//                 outData[out_pos++] = token;

//                 memcpy(outData + out_pos, inData + lit_start + lit_emitted, chunk);
//                 out_pos += chunk;
//                 lit_emitted += chunk;
//             }
//         }
//     }

//     // === Ajouter le header de 4 octets
//     size_t payload_size = out_pos;

//     if (payload_size + 4 > max_out) {
//         return false;
//     }

//     // Décaler le payload de 4 octets vers la droite
//     for (size_t i = payload_size; i-- > 0; ) {
//         outData[4 + i] = outData[i];
//     }

//     // Écrire le header (big-endian)
//     uint32_t be = static_cast<uint32_t>(payload_size);
//     outData[0] = static_cast<uint8_t>((be >> 24) & 0xFF);
//     outData[1] = static_cast<uint8_t>((be >> 16) & 0xFF);
//     outData[2] = static_cast<uint8_t>((be >>  8) & 0xFF);
//     outData[3] = static_cast<uint8_t>( be        & 0xFF);

//     *outSize = payload_size + 4;

//     return true;
// }















// // ============================================================================
// // iwflz_compress.cpp - VERSION ULTRA-OPTIMISÉE
// // ============================================================================

// #include "iwflz_compress.h"
// #include <cstring>
// #include <algorithm>
// #include <vector>
// #include <unordered_map>
// #include <QDebug>

// #define MIN_MATCH 3
// #define MAX_MATCH 1024
// #define MAX_OFFSET 8191

// // Structure pour stocker les matches
// struct Match {
//     int length;
//     size_t offset;
// };

// // Hash table pour accélérer la recherche
// // Clé = 3 premiers octets, Valeur = liste de positions
// static std::unordered_map<uint32_t, std::vector<size_t>> hashTable;

// // Fonction pour calculer le hash de 3 octets
// static inline uint32_t hash3(const uint8_t* data) {
//     return (uint32_t(data[0]) << 16) | (uint32_t(data[1]) << 8) | uint32_t(data[2]);
// }
// // Ligne 30-77 : Remplacer findBestMatch()
// static Match findBestMatch(const std::vector<uint8_t>& outbuf,
//                            const uint8_t* inData,
//                            size_t in_pos,
//                            size_t inSize) {
//     Match best = {0, 0};

//     size_t outPos = outbuf.size();
//     size_t available = inSize - in_pos;

//     if (available < MIN_MATCH) return best;

//     // ✅ RECHERCHE LINÉAIRE (comme la référence)
//     size_t winStart = (outPos > MAX_OFFSET + 1) ? (outPos - MAX_OFFSET - 1) : 0;

//     for (size_t pos = winStart; pos < outPos; pos++) {
//         size_t offset = (outPos - 1) - pos;
//         if (offset > MAX_OFFSET) continue;

//         // Calculer la longueur du match
//         int match_len = 0;
//         int max_len = std::min<int>(available, MAX_MATCH);

//         while (match_len < max_len &&
//                pos + match_len < outbuf.size() &&
//                outbuf[pos + match_len] == inData[in_pos + match_len]) {
//             match_len++;
//         }

//         // ✅ PRIORITÉ: Match le plus LONG d'abord
//         if (match_len >= MIN_MATCH) {
//             if (match_len > best.length) {
//                 best.length = match_len;
//                 best.offset = offset;
//             }
//         }
//     }

//     return best;
// }

// // // Recherche optimisée de match avec hash table
// // static Match findBestMatch(const std::vector<uint8_t>& outbuf,
// //                            const uint8_t* inData,
// //                            size_t in_pos,
// //                            size_t inSize) {
// //     Match best = {0, 0};

// //     size_t outPos = outbuf.size();
// //     size_t available = inSize - in_pos;

// //     if (available < MIN_MATCH) return best;

// //     // Calculer le hash des 3 premiers octets à matcher
// //     uint32_t h = hash3(inData + in_pos);

// //     // Chercher dans la hash table
// //     auto it = hashTable.find(h);
// //     if (it == hashTable.end()) return best;

// //     // Tester tous les candidats avec le même hash
// //     for (size_t pos : it->second) {
// //         // Vérifier que l'offset est valide
// //         size_t offset = (outPos - 1) - pos;
// //         if (offset > MAX_OFFSET) continue;

// //         // Calculer la longueur du match
// //         int match_len = 0;
// //         int max_len = std::min<int>(available, MAX_MATCH);

// //         while (match_len < max_len &&
// //                pos + match_len < outbuf.size() &&
// //                outbuf[pos + match_len] == inData[in_pos + match_len]) {
// //             match_len++;
// //         }

// //         // Garder le meilleur match
// //         if (match_len >= MIN_MATCH) {
// //             if (match_len > best.length ||
// //                 (match_len == best.length && offset < best.offset)) {
// //                 best.length = match_len;
// //                 best.offset = offset;
// //             }
// //         }
// //     }

// //     return best;
// // }

// bool iwflz_compress(const uint8_t* inData, size_t inSize,
//                     uint8_t* outData, size_t* outSize) {
//     if (!inData || !outData || !outSize || inSize == 0) {
//         return false;
//     }

//     std::vector<uint8_t> outbuf;
//     outbuf.reserve(inSize);

//     // Réinitialiser la hash table
//     hashTable.clear();
//     hashTable.reserve(inSize / 4);

//     size_t in_pos  = 0;
//     size_t out_pos = 0;
//     size_t max_out = *outSize;

//     // === RÈGLE SPÉCIALE: Le premier token doit être des littéraux
//     bool startsWithIwf =
//         (inSize >= 5 &&
//          inData[0] == 'i' && inData[1] == 'w' && inData[2] == 'f' &&
//          inData[3] == 0x00 && inData[4] == 0x01);

//     size_t first_lit_count = std::min<size_t>(2, inSize);
//     if (startsWithIwf) first_lit_count = 8;

//     if (out_pos + 1 + first_lit_count > max_out) return false;

//     // Token du premier run
//     uint8_t first_token;
//     if (first_lit_count == 8) {
//         first_token = 0x27;  // Token spécial pour 8 littéraux
//     } else {
//         first_token = (uint8_t)((first_lit_count - 1) & 0x1F);
//     }

//     outData[out_pos++] = first_token;

//     // Copier les premiers littéraux
//     memcpy(outData + out_pos, inData, first_lit_count);
//     for (size_t i = 0; i < first_lit_count; ++i) {
//         outbuf.push_back(inData[i]);

//         // Ajouter à la hash table (dès qu'on a 3 octets)
//         if (outbuf.size() >= 3) {
//             uint32_t h = hash3(&outbuf[outbuf.size() - 3]);
//             hashTable[h].push_back(outbuf.size() - 3);
//         }
//     }
//     out_pos += first_lit_count;
//     in_pos  += first_lit_count;

//     // === Boucle principale de compression
//     while (in_pos < inSize) {
//         // Chercher le meilleur match
//         Match m = findBestMatch(outbuf, inData, in_pos, inSize);


//         // ✅ DEBUG
//         if (in_pos == 14) {
//             qDebug() << "=== À in_pos=14 (juste après .json) ===";
//             qDebug() << "outbuf.size():" << outbuf.size();
//             qDebug() << "Match trouvé: len=" << m.length << "off=" << m.offset;

//             if (m.length > 0) {
//                 size_t src = outbuf.size() - 1 - m.offset;
//                 qDebug() << "Copie depuis outbuf[" << src << "]";
//                 QString preview = "Octets: ";
//                 for (int i = 0; i < std::min(10, m.length); i++) {
//                     preview += QString("%1 ").arg(outbuf[src + i], 2, 16, QChar('0'));
//                 }
//                 qDebug() << preview;
//             }
//         }

//         // Décider : backref ou littéraux ?
//         if (m.length >= MIN_MATCH && m.offset <= MAX_OFFSET) {
//             // ✅ BACKREF trouvée
//             size_t length = m.length;
//             size_t offset = m.offset;

//             if (offset >= outbuf.size()) {
//                 qDebug() << "❌ ERREUR: offset" << offset << ">= outbuf.size()" << outbuf.size();
//                 return false;
//             }

//             // Encodage du token
//             size_t base = length - 2;
//             if (base <= 6) {
//                 // Format court
//                 if (out_pos + 2 > max_out) return false;
//                 uint8_t hi3 = (uint8_t)base;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((hi3 << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else if (base == 7) {
//                 // Format long (longueur = 9)
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = 0x00;  // Extension = 0 pour len=9
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else {
//                 // Format long avec extension
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5 = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;

//                 size_t ext = length - 9;
//                 while (ext >= 255) {
//                     if (out_pos + 1 > max_out) return false;
//                     outData[out_pos++] = 0xFF;
//                     ext -= 255;
//                 }
//                 if (out_pos + 2 > max_out) return false;
//                 outData[out_pos++] = (uint8_t)ext;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             }

//             // Expansion dans outbuf (simuler le décodeur)
//             size_t src = outbuf.size() - 1 - offset;
//             if (src >= outbuf.size()) {
//                 qDebug() << "❌ ERREUR EXPANSION: src" << src << ">= outbuf.size()" << outbuf.size();
//                 return false;
//             }

//             for (size_t i = 0; i < length; ++i) {
//                 outbuf.push_back(outbuf[src]);

//                 // Ajouter à la hash table
//                 if (outbuf.size() >= 3) {
//                     uint32_t h = hash3(&outbuf[outbuf.size() - 3]);
//                     hashTable[h].push_back(outbuf.size() - 3);
//                 }

//                 ++src;
//             }
//             in_pos += length;

//         } else {
//             // ❌ Pas de match → Littéraux

//             // Accumuler des littéraux (jusqu'à 32)
//             size_t lit_start = in_pos;
//             size_t lit_count = 1;

//             outbuf.push_back(inData[in_pos]);
//             if (outbuf.size() >= 3) {
//                 uint32_t h = hash3(&outbuf[outbuf.size() - 3]);
//                 hashTable[h].push_back(outbuf.size() - 3);
//             }
//             in_pos++;

//             // Continuer d'accumuler tant qu'on ne trouve pas de match
//             while (in_pos < inSize && lit_count < 32) {
//                 Match next = findBestMatch(outbuf, inData, in_pos, inSize);
//                 if (next.length >= MIN_MATCH) {
//                     // Match trouvé, arrêter les littéraux
//                     break;
//                 }

//                 outbuf.push_back(inData[in_pos]);
//                 if (outbuf.size() >= 3) {
//                     uint32_t h = hash3(&outbuf[outbuf.size() - 3]);
//                     hashTable[h].push_back(outbuf.size() - 3);
//                 }
//                 ++in_pos;
//                 ++lit_count;
//             }

//             // Émettre les littéraux
//             size_t lit_emitted = 0;
//             while (lit_emitted < lit_count) {
//                 size_t chunk = std::min<size_t>(32, lit_count - lit_emitted);
//                 if (out_pos + 1 + chunk > max_out) return false;

//                 uint8_t token = (uint8_t)((chunk - 1) & 0x1F);
//                 outData[out_pos++] = token;

//                 memcpy(outData + out_pos, inData + lit_start + lit_emitted, chunk);
//                 out_pos += chunk;
//                 lit_emitted += chunk;
//             }
//         }
//     }

//     // === Ajouter le header de 4 octets
//     size_t payload_size = out_pos;

//     if (payload_size + 4 > max_out) {
//         return false;
//     }

//     // Décaler le payload de 4 octets vers la droite
//     for (size_t i = payload_size; i-- > 0; ) {
//         outData[4 + i] = outData[i];
//     }

//     // Écrire le header (big-endian)
//     uint32_t be = static_cast<uint32_t>(payload_size);
//     outData[0] = static_cast<uint8_t>((be >> 24) & 0xFF);
//     outData[1] = static_cast<uint8_t>((be >> 16) & 0xFF);
//     outData[2] = static_cast<uint8_t>((be >>  8) & 0xFF);
//     outData[3] = static_cast<uint8_t>( be        & 0xFF);

//     *outSize = payload_size + 4;

//     qDebug() << "Compression:" << inSize << "→" << *outSize << "octets"
//              << "(" << (100.0 * *outSize / inSize) << "%)";

//     return true;
// }





















// // ============================================================================
// // iwflz_compress.cpp  —  Version VRAIMENT corrigée
// // ============================================================================

// #include "iwflz_compress.h"
// #include <cstring>
// #include <algorithm>
// #include <vector>
// #include <QDebug>

// #define MIN_MATCH 3
// #define MAX_MATCH 1024
// #define MAX_OFFSET 8191

// bool iwflz_compress(const uint8_t* inData, size_t inSize,
//                     uint8_t* outData, size_t* outSize) {
//     if (!inData || !outData || !outSize || inSize == 0) {
//         return false;
//     }

//     std::vector<uint8_t> outbuf;
//     outbuf.reserve(inSize);

//     size_t in_pos  = 0;
//     size_t out_pos = 0;
//     size_t max_out = *outSize;

//     // === RÈGLE SPÉCIALE: Le premier token doit être des littéraux
//     // Si le fichier commence par "iwf\0\1", forcer 8 littéraux initiaux
//     bool startsWithIwf =
//         (inSize >= 5 &&
//          inData[0] == 'i' && inData[1] == 'w' && inData[2] == 'f' &&
//          inData[3] == 0x00 && inData[4] == 0x01);

//     size_t first_lit_count = std::min<size_t>(2, inSize);
//     if (startsWithIwf) first_lit_count = 8;

//     if (out_pos + 1 + first_lit_count > max_out) return false;

//     // === TOKEN DU PREMIER RUN DE LITTÉRAUX
//     // ✨ RÈGLE IMPORTANTE: Pour le premier token, on utilise UNIQUEMENT low5
//     // Token = (0 << 5) | (count - 1)  où count peut aller jusqu'à 32
//     // MAIS pour matcher la référence avec token 0x27 pour 8 littéraux:
//     // On doit avoir: 0x27 = hi3=1, low5=7, et la formule est low5+1 = 8
//     //
//     // DONC: Le premier token suit la règle: count = low5 + 1
//     // Pour 8 littéraux, on veut low5 = 7, donc token = 0x27 (avec hi3=1 pour une raison inconnue)

//     uint8_t first_token;
//     if (first_lit_count == 8) {
//         // Cas spécial: 8 littéraux initiaux → token = 0x27
//         first_token = 0x27;  // hi3=1, low5=7, décodé comme low5+1 = 8 littéraux
//     } else {
//         // Cas normal: hi3=0, low5=count-1
//         first_token = (uint8_t)((first_lit_count - 1) & 0x1F);
//     }

//     outData[out_pos++] = first_token;

//     // Copier les octets littéraux dans la sortie ET la fenêtre (outbuf)
//     memcpy(outData + out_pos, inData, first_lit_count);
//     for (size_t i = 0; i < first_lit_count; ++i) {
//         outbuf.push_back(inData[i]);
//     }
//     out_pos += first_lit_count;
//     in_pos  += first_lit_count;

//     // === Boucle principale de compression
//     int iteration = 0;
//     while (in_pos < inSize) {
//         iteration++;

//         int    best_len    = 0;
//         size_t best_offset = 0;

//         // Recherche du meilleur match dans outbuf (fenêtre 8191 + 1)
//         size_t outPos   = outbuf.size();
//         size_t winStart = (outPos > MAX_OFFSET + 1) ? (outPos - MAX_OFFSET - 1) : 0;

//         if (iteration <= 10) {
//             qDebug() << "=== Iteration" << iteration << "===";
//             qDebug() << "  in_pos:" << in_pos << "/ inSize:" << inSize;
//             qDebug() << "  outbuf.size():" << outPos;
//             qDebug() << "  winStart:" << winStart;
//             qDebug() << "  Cherche match pour octet:" << QString::number(inData[in_pos], 16);
//         }

//         // Recherche de match
//         for (size_t pos = winStart; pos < outPos; pos++) {
//             // Offset 1-based côté décodeur: src = out.size() - 1 - off
//             size_t offset = (outPos - 1) - pos;
//             if (offset > MAX_OFFSET) continue;

//             int match_len = 0;
//             int max_len   = std::min<int>(int(inSize - in_pos), MAX_MATCH);

//             while (match_len < max_len &&
//                    pos + match_len < outbuf.size() &&
//                    outbuf[pos + match_len] == inData[in_pos + match_len]) {
//                 match_len++;
//             }

//             if (match_len >= MIN_MATCH) {
//                 if (match_len > best_len ||
//                     (match_len == best_len && offset < best_offset)) {
//                     best_len    = match_len;
//                     best_offset = offset;
//                 }
//             }
//         }

//         // Écriture du token
//         if (best_len >= MIN_MATCH && best_offset <= MAX_OFFSET) {
//             if (iteration <= 10) {
//                 qDebug() << "  → BackRef trouvée! len:" << best_len
//                          << "offset:" << best_offset;
//             }

//             // BackRef trouvée
//             size_t length = best_len;
//             size_t offset = best_offset;

//             if (offset >= outbuf.size()) {
//                 qDebug() << "❌ ERREUR CRITIQUE iter" << iteration;
//                 qDebug() << "   offset:" << offset << ">= outbuf.size():" << outbuf.size();
//                 qDebug() << "   in_pos:" << in_pos;
//                 return false;
//             }

//             // Encodage COPY (mêmes règles que le décodeur)
//             size_t base = length - 2;
//             if (base <= 7) {
//                 // Format court: [hi3=len-2][low5=off>>8], puis [off & 0xFF]
//                 if (out_pos + 2 > max_out) return false;
//                 uint8_t high3 = (uint8_t)base;
//                 uint8_t low5  = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((high3 << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else {
//                 // Format long: hi3=7 puis extension (sum of bytes, 0xFF repeat) puis [off low]
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5  = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;

//                 size_t ext = length - 9;
//                 while (ext >= 255) {
//                     if (out_pos + 1 > max_out) return false;
//                     outData[out_pos++] = 0xFF;
//                     ext -= 255;
//                 }
//                 if (out_pos + 2 > max_out) return false;
//                 outData[out_pos++] = (uint8_t)ext;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             }

//             // Expansion en sortie (même algo que le décodeur)
//             size_t src = outbuf.size() - 1 - offset;
//             if (src >= outbuf.size()) {
//                 qDebug() << "❌ ERREUR EXPANSION RLE iter" << iteration;
//                 qDebug() << "   src:" << src << ">= outbuf.size():" << outbuf.size();
//                 return false;
//             }
//             for (int i = 0; i < best_len; ++i) {
//                 outbuf.push_back(outbuf[src]);
//                 ++src; // chevauchement autorisé
//             }
//             in_pos += best_len;

//         } else {
//             if (iteration <= 10) {
//                 qDebug() << "  → Pas de match, littéraux. best_len:" << best_len;
//             }

//             // Accumuler des littéraux (chunks ≤ 32)
//             size_t lit_start = in_pos;
//             size_t lit_count = 1;

//             outbuf.push_back(inData[in_pos]);
//             in_pos++;

//             while (in_pos < inSize && lit_count < 32) {
//                 bool found_match = false;

//                 if (in_pos + MIN_MATCH <= inSize) {
//                     size_t curr_outPos   = outbuf.size();
//                     size_t curr_winStart = (curr_outPos > MAX_OFFSET + 1)
//                                                ? (curr_outPos - MAX_OFFSET - 1) : 0;

//                     for (size_t pos = curr_winStart; pos < curr_outPos; ++pos) {
//                         size_t offset_check = (curr_outPos - 1) - pos;
//                         if (offset_check > MAX_OFFSET) continue;

//                         int len_check = 0;
//                         int max_check = std::min<int>(int(inSize - in_pos), MIN_MATCH);
//                         while (len_check < max_check &&
//                                pos + len_check < outbuf.size() &&
//                                outbuf[pos + len_check] == inData[in_pos + len_check]) {
//                             ++len_check;
//                         }
//                         if (len_check >= MIN_MATCH) { found_match = true; break; }
//                     }
//                 }

//                 if (found_match) break;

//                 outbuf.push_back(inData[in_pos]);
//                 ++in_pos;
//                 ++lit_count;
//             }

//             // Émettre les littéraux par chunks de 32 max (hi3=0)
//             size_t lit_emitted = 0;
//             while (lit_emitted < lit_count) {
//                 size_t chunk = std::min<size_t>(32, lit_count - lit_emitted);
//                 if (out_pos + 1 + chunk > max_out) return false;

//                 uint8_t token = (uint8_t)((chunk - 1) & 0x1F);
//                 outData[out_pos++] = token;

//                 memcpy(outData + out_pos, inData + lit_start + lit_emitted, chunk);
//                 out_pos     += chunk;
//                 lit_emitted += chunk;
//             }
//         }
//     }

//     // === FIN DE COMPRESSION: prépendre l'en-tête de bloc (4 octets BE)
//     // Ici, 'out_pos' = taille du payload compressé (sans en-tête)
//     size_t payload_size = out_pos;

//     // Vérifier l'espace dispo pour décaler de +4
//     if (payload_size + 4 > max_out) {
//         return false; // buffer de sortie trop petit
//     }

//     // Décaler le payload de 4 octets vers la droite (depuis la fin, pour gérer le recouvrement)
//     for (size_t i = payload_size; i-- > 0; ) {
//         outData[4 + i] = outData[i];
//     }

//     // Écrire l'en-tête 4 octets en big-endian = taille du payload
//     uint32_t be = static_cast<uint32_t>(payload_size);
//     outData[0] = static_cast<uint8_t>((be >> 24) & 0xFF);
//     outData[1] = static_cast<uint8_t>((be >> 16) & 0xFF);
//     outData[2] = static_cast<uint8_t>((be >>  8) & 0xFF);
//     outData[3] = static_cast<uint8_t>( be        & 0xFF);

//     // Taille finale = 4 (header) + payload
//     *outSize = payload_size + 4;
//     return true;
// }








//// ============================================================================
//// iwflz_compress.cpp
//// ============================================================================


// #include "iwflz_compress.h"
// #include <cstring>
// #include <algorithm>
// #include <vector>
// #include <QDebug>

// #define MIN_MATCH 3
// #define MAX_MATCH 1024
// #define MAX_OFFSET 8191

// bool iwflz_compress(const uint8_t* inData, size_t inSize,
//                     uint8_t* outData, size_t* outSize) {
//     if (!inData || !outData || !outSize || inSize == 0) {
//         return false;
//     }

//     std::vector<uint8_t> outbuf;
//     outbuf.reserve(inSize);

//     size_t in_pos  = 0;
//     size_t out_pos = 0;
//     size_t max_out = *outSize;

//     // === MODIF: forcer 8 littéraux si le flux commence par "iwf\0x01\0x00\0x06\0x00"
//     bool startsWithIwfHeader =
//         (inSize >= 8 &&
//          inData[0] == 'i' && inData[1] == 'w' && inData[2] == 'f' &&
//          inData[3] == 0x00 && inData[4] == 0x01 &&
//          inData[5] == 0x00 && inData[6] == 0x06 && inData[7] == 0x00);

//     size_t first_lit_count = std::min<size_t>(2, inSize);
//     if (startsWithIwfHeader) first_lit_count = 8;

//     if (out_pos + 1 + first_lit_count > max_out) return false;

//     // === MODIF: token du 1er run de littéraux
//     // Pour matcher la référence, si n=8 → token = 0x27 (hi3=1, low5=7).
//     uint8_t first_token =
//         (startsWithIwfHeader && first_lit_count == 8)
//             ? uint8_t(0x27)
//             : uint8_t((first_lit_count - 1) & 0x1F);

//     outData[out_pos++] = first_token;

//     // Copier les octets littéraux dans la sortie ET la fenêtre (outbuf)
//     memcpy(outData + out_pos, inData, first_lit_count);
//     for (size_t i = 0; i < first_lit_count; ++i) {
//         outbuf.push_back(inData[i]);
//     }
//     out_pos += first_lit_count;
//     in_pos  += first_lit_count;

//     // === Boucle principale de compression
//     int iteration = 0;
//     while (in_pos < inSize) {
//         iteration++;

//         int    best_len    = 0;
//         size_t best_offset = 0;

//         // Recherche du meilleur match dans outbuf (fenêtre 8191 + 1)
//         size_t outPos   = outbuf.size();
//         size_t winStart = (outPos > MAX_OFFSET + 1) ? (outPos - MAX_OFFSET - 1) : 0;

//         if (iteration <= 10) {
//             qDebug() << "=== Iteration" << iteration << "===";
//             qDebug() << "  in_pos:" << in_pos << "/ inSize:" << inSize;
//             qDebug() << "  outbuf.size():" << outPos;
//             qDebug() << "  winStart:" << winStart;
//             qDebug() << "  Cherche match pour octet:" << QString::number(inData[in_pos], 16);
//         }

//         // Recherche de match
//         for (size_t pos = winStart; pos < outPos; pos++) {
//             // Offset 1-based côté décodeur: src = out.size() - 1 - off
//             size_t offset = (outPos - 1) - pos;
//             if (offset > MAX_OFFSET) continue;

//             int match_len = 0;
//             int max_len   = std::min<int>(int(inSize - in_pos), MAX_MATCH);

//             while (match_len < max_len &&
//                    pos + match_len < outbuf.size() &&
//                    outbuf[pos + match_len] == inData[in_pos + match_len]) {
//                 match_len++;
//             }

//             if (match_len >= MIN_MATCH) {
//                 if (match_len > best_len ||
//                     (match_len == best_len && offset < best_offset)) {
//                     best_len    = match_len;
//                     best_offset = offset;
//                 }
//             }
//         }

//         // Écriture du token
//         if (best_len >= MIN_MATCH && best_offset <= MAX_OFFSET) {
//             if (iteration <= 10) {
//                 qDebug() << "  → BackRef trouvée! len:" << best_len
//                          << "offset:" << best_offset;
//             }

//             // BackRef trouvée
//             size_t length = best_len;
//             size_t offset = best_offset;

//             if (offset >= outbuf.size()) {
//                 qDebug() << "❌ ERREUR CRITIQUE iter" << iteration;
//                 qDebug() << "   offset:" << offset << ">= outbuf.size():" << outbuf.size();
//                 qDebug() << "   in_pos:" << in_pos;
//                 return false;
//             }

//             // Encodage COPY (mêmes règles que le décodeur)
//             size_t base = length - 2;
//             if (base <= 7) {
//                 // Format court: [hi3=len-2][low5=off>>8], puis [off & 0xFF]
//                 if (out_pos + 2 > max_out) return false;
//                 uint8_t high3 = (uint8_t)base;
//                 uint8_t low5  = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((high3 << 5) | low5);
//                 outData[out_pos++] = token;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             } else {
//                 // Format long: hi3=7 puis extension (sum of bytes, 0xFF repeat) puis [off low]
//                 if (out_pos + 3 > max_out) return false;
//                 uint8_t low5  = (uint8_t)((offset >> 8) & 0x1F);
//                 uint8_t token = (uint8_t)((7u << 5) | low5);
//                 outData[out_pos++] = token;

//                 size_t ext = length - 9;
//                 while (ext >= 255) {
//                     if (out_pos + 1 > max_out) return false;
//                     outData[out_pos++] = 0xFF;
//                     ext -= 255;
//                 }
//                 if (out_pos + 2 > max_out) return false;
//                 outData[out_pos++] = (uint8_t)ext;
//                 outData[out_pos++] = (uint8_t)(offset & 0xFF);
//             }

//             // Expansion en sortie (même algo que le décodeur)
//             size_t src = outbuf.size() - 1 - offset;
//             if (src >= outbuf.size()) {
//                 qDebug() << "❌ ERREUR EXPANSION RLE iter" << iteration;
//                 qDebug() << "   src:" << src << ">= outbuf.size():" << outbuf.size();
//                 return false;
//             }
//             for (int i = 0; i < best_len; ++i) {
//                 outbuf.push_back(outbuf[src]);
//                 ++src; // chevauchement autorisé
//             }
//             in_pos += best_len;

//         } else {
//             if (iteration <= 10) {
//                 qDebug() << "  → Pas de match, littéraux. best_len:" << best_len;
//             }

//             // Accumuler des littéraux (chunks ≤ 32)
//             size_t lit_start = in_pos;
//             size_t lit_count = 1;

//             outbuf.push_back(inData[in_pos]);
//             in_pos++;

//             while (in_pos < inSize && lit_count < 32) {
//                 bool found_match = false;

//                 if (in_pos + MIN_MATCH <= inSize) {
//                     size_t curr_outPos   = outbuf.size();
//                     size_t curr_winStart = (curr_outPos > MAX_OFFSET + 1)
//                                                ? (curr_outPos - MAX_OFFSET - 1) : 0;

//                     for (size_t pos = curr_winStart; pos < curr_outPos; ++pos) {
//                         size_t offset_check = (curr_outPos - 1) - pos;
//                         if (offset_check > MAX_OFFSET) continue;

//                         int len_check = 0;
//                         int max_check = std::min<int>(int(inSize - in_pos), MIN_MATCH);
//                         while (len_check < max_check &&
//                                pos + len_check < outbuf.size() &&
//                                outbuf[pos + len_check] == inData[in_pos + len_check]) {
//                             ++len_check;
//                         }
//                         if (len_check >= MIN_MATCH) { found_match = true; break; }
//                     }
//                 }

//                 if (found_match) break;

//                 outbuf.push_back(inData[in_pos]);
//                 ++in_pos;
//                 ++lit_count;
//             }

//             // Émettre les littéraux par chunks de 32 max (hi3=0)
//             size_t lit_emitted = 0;
//             while (lit_emitted < lit_count) {
//                 size_t chunk = std::min<size_t>(32, lit_count - lit_emitted);
//                 if (out_pos + 1 + chunk > max_out) return false;

//                 uint8_t token = (uint8_t)((chunk - 1) & 0x1F);
//                 outData[out_pos++] = token;

//                 memcpy(outData + out_pos, inData + lit_start + lit_emitted, chunk);
//                 out_pos     += chunk;
//                 lit_emitted += chunk;
//             }
//         }
//     }

//     // === FIN DE COMPRESSION: prépendre l'en-tête de bloc (4 octets BE)
//     // Ici, 'out_pos' = taille du payload compressé (sans en-tête)
//     size_t payload_size = out_pos;

//     // Vérifier l'espace dispo pour décaler de +4
//     if (payload_size + 4 > max_out) {
//         return false; // buffer de sortie trop petit
//     }

//     // Décaler le payload de 4 octets vers la droite (depuis la fin, pour gérer le recouvrement)
//     for (size_t i = payload_size; i-- > 0; ) {
//         outData[4 + i] = outData[i];
//     }

//     // Écrire l'en-tête 4 octets en big-endian = taille du payload
//     uint32_t be = static_cast<uint32_t>(payload_size);
//     outData[0] = static_cast<uint8_t>((be >> 24) & 0xFF);
//     outData[1] = static_cast<uint8_t>((be >> 16) & 0xFF);
//     outData[2] = static_cast<uint8_t>((be >>  8) & 0xFF);
//     outData[3] = static_cast<uint8_t>( be        & 0xFF);

//     // Taille finale = 4 (header) + payload
//     *outSize = payload_size + 4;
//     return true;
// }


