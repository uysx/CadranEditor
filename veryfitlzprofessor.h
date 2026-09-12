// ==========================
// File: VeryfitLZProfessor.h
// ==========================
#pragma once
#include <QtCore>

struct VFLZToken {
    qsizetype lz_from = 0, lz_to = 0;
    qsizetype raw_from = 0, raw_to = 0;
    QString type;           // e.g. "E0-RLE0(24)", "LIT5(len=...)" / "ZR16+LIT8" / "MATCH16"
    QByteArray produced;    // decompressed bytes produced by this token
};

class VeryfitLZProfessor {
public:
    // Parse whole .lz against raw reference. Returns true on success (or partial true with warnings in log).
    bool parseAll(const QByteArray &lz, const QByteArray &raw,
                  QVector<VFLZToken> &outTokens, QString &log);

    // CSV writer
    static bool writeCsv(const QVector<VFLZToken> &tokens, const QString &path, QString &err);

    // Bit-exact re-encode by concatenating original slices lz[lz_from..lz_to)
    static bool writeReencodedLz(const QVector<VFLZToken> &tokens, const QByteArray &lzOrig,
                                 const QString &path, QString &err);

    // First difference between two byte arrays
    static std::optional<std::tuple<qsizetype,int,int>> firstDiff(const QByteArray &a, const QByteArray &b);
    static QByteArray encodeIwfToVeryfitLz_Minimal(const QByteArray &raw);
    static bool decodeVeryfitLz_Minimal(const QByteArray &lz, QByteArray &rawOut);
};
