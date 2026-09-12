// ==========================
// File: VeryfitLZProfessor.cpp
// ==========================
#include "veryfitlzprofessor.h"

// ---- Little-endian helpers ----
static inline quint16 rd16le(const QByteArray &b, qsizetype off){
    return quint8(b[off]) | (quint16(quint8(b[off+1]))<<8);
}
static inline quint32 rd32le(const QByteArray &b, qsizetype off){
    return quint32(quint8(b[off])) |
           (quint32(quint8(b[off+1]))<<8) |
           (quint32(quint8(b[off+2]))<<16) |
           (quint32(quint8(b[off+3]))<<24);
}

static qsizetype scoreMatch(const QByteArray &produced, const QByteArray &raw, qsizetype rawPos){
    const qsizetype n = qMin(produced.size(), raw.size() - rawPos);
    qsizetype ok = 0;
    for (qsizetype i=0; i<n; ++i) {
        if (quint8(produced[i]) == quint8(raw[rawPos + i])) ok++; else break;
    }
    return ok;
}

struct ProbeResult {
    bool ok = false;
    VFLZToken tok;
    qsizetype lz_advance = 0;
    qsizetype raw_gain   = 0;
    qsizetype score      = 0;
    QString hypothesis;
};

static ProbeResult probe80_9F(const QByteArray &lz, qsizetype lzPos,
                              const QByteArray &raw, qsizetype rawPos)
{
    ProbeResult best; best.ok = false; best.score = 0;
    if (lzPos >= lz.size()) return best;
    const quint8 op = quint8(lz[lzPos]);
    if (op < 0x80 || op > 0x9F) return best;

    auto tryCandidate = [&](const QString &name, auto builder){
        ProbeResult r; r.ok=false;
        VFLZToken t; t.lz_from = lzPos; t.raw_from = rawPos; t.type = name;
        QByteArray produced; qsizetype used = 1;
        if (!builder(produced, used)) return; // builder failed
        const qsizetype s = scoreMatch(produced, raw, rawPos);
        if (s == 0) return;
        t.produced = produced;
        t.raw_to = rawPos + produced.size();
        t.lz_to  = lzPos + used;
        r.ok = true; r.tok = t; r.lz_advance = used; r.raw_gain = produced.size(); r.score = s; r.hypothesis = name;
        if (!best.ok || r.score > best.score || (r.score == best.score && r.lz_advance < best.lz_advance)) best = r;
    };

    // Candidate A: LITERAL with length in low 5 bits of opcode
    tryCandidate(QString("LIT5(len=%1)").arg(op & 0x1F), [&](QByteArray &out, qsizetype &used){
        quint8 len = op & 0x1F; if (!len) return false;
        if (lzPos + 1 + len > lz.size()) return false;
        out = lz.mid(lzPos+1, len);
        used = 1 + len; return true;
    });

    // Candidate B: ZR16 — zeros u16 LE
    tryCandidate("ZR16", [&](QByteArray &out, qsizetype &used){
        if (lzPos + 3 > lz.size()) return false;
        quint16 n = rd16le(lz, lzPos+1); if (!n) return false;
        out = QByteArray(int(n), '\0'); used = 3; return true;
    });

    // Candidate C: ZR16 + LIT8
    tryCandidate("ZR16+LIT8", [&](QByteArray &out, qsizetype &used){
        if (lzPos + 4 > lz.size()) return false;
        quint16 nZ = rd16le(lz, lzPos+1); quint8 L = quint8(lz[lzPos+3]);
        if (lzPos + 4 + L > lz.size()) return false;
        out = QByteArray(int(nZ), '\0'); out += lz.mid(lzPos+4, L);
        used = 1 + 2 + 1 + L; return true;
    });

    // Candidate D: ZR32 + LIT32 (extended)
    tryCandidate("ZR32+LIT32", [&](QByteArray &out, qsizetype &used){
        if (lzPos + 9 > lz.size()) return false;
        quint32 nZ = rd32le(lz, lzPos+1); quint32 L = rd32le(lz, lzPos+5);
        if (L > (1u<<20)) return false; // guard
        if (lzPos + 9 + qsizetype(L) > lz.size()) return false;
        out = QByteArray(int(nZ), '\0'); out += lz.mid(lzPos+9, qsizetype(L));
        used = 1 + 4 + 4 + qsizetype(L); return true;
    });

    // Candidate E: MATCH16(len,dist) from already-produced raw
    tryCandidate("MATCH16(len,dist)", [&](QByteArray &out, qsizetype &used){
        if (lzPos + 5 > lz.size()) return false;
        quint16 len  = rd16le(lz, lzPos+1); quint16 dist = rd16le(lz, lzPos+3);
        if (!len || !dist) return false;
        qsizetype have = rawPos; if (dist > have) return false;
        out.resize(len);
        for (int i=0;i<len;i++) out[i] = raw[rawPos - dist + (i % dist)];
        used = 1 + 2 + 2; return true;
    });

    if (best.ok && best.score >= 8) return best; // acceptance threshold
    ProbeResult empty; return empty;
}

static bool tryKnownFamilies(const QByteArray &lz, qsizetype &lzPos,
                             const QByteArray &raw, qsizetype &rawPos,
                             VFLZToken &tok, QString &typeName)
{
    Q_UNUSED(raw)
    Q_UNUSED(rawPos)
    if (lzPos >= lz.size()) return false;
    quint8 op = quint8(lz[lzPos]);

    // Example: E0 — RLE zeros (u16)
    if (op == 0xE0) {
        if (lzPos + 3 > lz.size()) return false;
        quint16 n = rd16le(lz, lzPos+1);
        tok.lz_from = lzPos; tok.raw_from = rawPos;
        tok.type = QString("E0-RLE0(%1)").arg(n);
        tok.produced = QByteArray(int(n), '\0');
        lzPos += 3; rawPos += n; tok.lz_to = lzPos; tok.raw_to = rawPos; typeName = tok.type;
        return true;
    }

    // Placeholder: 0x20 — literal of fixed 16 bytes (à ajuster si besoin)
    // NOTE: ceci reflète le harness Python original; ajuste dès que la sémantique exacte est confirmée.
    if (op == 0x20) {
        const qsizetype L = 16; // "SL(16)" placeholder
        if (lzPos + 1 + L > lz.size()) return false;
        tok.lz_from = lzPos; tok.raw_from = rawPos;
        tok.type = QStringLiteral("SL(16)");
        tok.produced = lz.mid(lzPos+1, L);
        lzPos += 1 + L; rawPos += L; tok.lz_to = lzPos; tok.raw_to = rawPos; typeName = tok.type;
        return true;
    }

    return false; // unknown here
}

bool VeryfitLZProfessor::parseAll(const QByteArray &lz, const QByteArray &raw,
                                  QVector<VFLZToken> &outTokens, QString &log)
{
    outTokens.clear(); log.clear();
    qsizetype lzPos = 0, rawPos = 0; int safety = 0;

    while (lzPos < lz.size() && rawPos <= raw.size()) {
        if (++safety > 10'000'000) { log += QStringLiteral("Sécurité: trop de tokens.\n"); return false; }

        VFLZToken tok; QString typeName; qsizetype lzPosBefore = lzPos, rawPosBefore = rawPos;

        if (tryKnownFamilies(lz, lzPos, raw, rawPos, tok, typeName)) { outTokens.push_back(tok); continue; }
        lzPos = lzPosBefore; rawPos = rawPosBefore; // rollback

        quint8 op = quint8(lz[lzPos]);
        if (op >= 0x80 && op <= 0x9F) {
            auto pr = probe80_9F(lz, lzPos, raw, rawPos);
            if (pr.ok) {
                VFLZToken t = pr.tok; t.type = pr.hypothesis; outTokens.push_back(t);
                if (op == 0x9E) {
                    log += QString("0x9E choisi: %1 | lz[%2..%3) -> raw[%4..%5) | score=%6\n")
                    .arg(t.type)
                        .arg(t.lz_from).arg(t.lz_to)
                        .arg(t.raw_from).arg(t.raw_to)
                        .arg(pr.score);
                }
                lzPos += pr.lz_advance; rawPos += pr.raw_gain; continue;
            }
        }

        // Failure → diagnostics
        auto hex = [](const QByteArray &b){ return QString(b.toHex(' ')).toUpper(); };
        log += QString("Blocage à lzPos=%1 (op=%2), rawPos=%3\n")
                   .arg(lzPos).arg(op, 2, 16, QLatin1Char('0')).arg(rawPos);
        log += QStringLiteral("lz@pos:  ") + hex(lz.mid(lzPos, 16)) + QLatin1Char('\n');
        log += QStringLiteral("raw@pos: ") + hex(raw.mid(rawPos, 16)) + QLatin1Char('\n');
        return false;
    }

    if (rawPos != raw.size()) log += QString("Avertissement: brut consommé %1/%2 bytes.\n").arg(rawPos).arg(raw.size());
    else log += QStringLiteral("OK: brut entièrement reconstruit.\n");

    if (lzPos != lz.size()) log += QString("Note: lz restant %1 bytes.\n").arg(lz.size()-lzPos);

    return true;
}

bool VeryfitLZProfessor::writeCsv(const QVector<VFLZToken> &tokens, const QString &path, QString &err){
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text)) { err = QStringLiteral("Impossible d'ouvrir CSV"); return false; }
    QTextStream ts(&f); ts.setEncoding(QStringConverter::Utf8);
    ts << "lz_from,lz_to,raw_from,raw_to,type,produced_hex\n";
    for (const auto &t : tokens) {
        ts << t.lz_from << "," << t.lz_to << "," << t.raw_from << "," << t.raw_to << ","
           << t.type << "," << QString(t.produced.toHex(' ')).toUpper() << "\n";
    }
    if (!f.commit()) { err = QStringLiteral("Commit CSV échoué"); return false; }
    return true;
}

bool VeryfitLZProfessor::writeReencodedLz(const QVector<VFLZToken> &tokens, const QByteArray &lzOrig,
                                          const QString &path, QString &err)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Truncate)) { err = QStringLiteral("Impossible d'ouvrir .lz sortie"); return false; }
    for (const auto &t : tokens) {
        const qsizetype a = t.lz_from, b = t.lz_to;
        if (b > lzOrig.size() || a > b) { err = QString("Tranche lz invalide [%1..%2)").arg(a).arg(b); return false; }
        if (f.write(lzOrig.constData() + a, b - a) != (b - a)) { err = QStringLiteral("Ecriture partielle"); return false; }
    }
    if (!f.commit()) { err = QStringLiteral("Commit .lz échoué"); return false; }
    return true;
}

std::optional<std::tuple<qsizetype,int,int>> VeryfitLZProfessor::firstDiff(const QByteArray &a, const QByteArray &b){
    const qsizetype n = qMin(a.size(), b.size());
    for (qsizetype i=0;i<n;++i) if (a[i]!=b[i]) return std::make_tuple(i, quint8(a[i]), quint8(b[i]));
    if (a.size()!=b.size()) return std::make_tuple(n, -1, -1);
    return std::nullopt;
}
// Encode .iwf to .iwf.lz using only opcodes 0x20 (literal) and 0xE0 (zeros RLE).
// Header conteneur observé: 00 00 01 CE 27
QByteArray VeryfitLZProfessor::encodeIwfToVeryfitLz_Minimal(const QByteArray &raw) {
    QByteArray out;
    out += QByteArray::fromHex("000001CE27"); // container header

    auto emitLiteral = [&](const char* p, qsizetype len) {
        while (len > 0) {
            quint8 L = quint8(qMin<qsizetype>(255, len));
            out.append(char(0x20));
            out.append(char(L));
            out.append(p, L);
            p   += L;
            len -= L;
        }
    };

    qsizetype i = 0;
    while (i < raw.size()) {
        // détecte un run de zéros
        qsizetype z = 0;
        while (i + z < raw.size() && raw[i + z] == '\0' && z < 65535)
            ++z;

        // Seuil: n'encode en RLE que si >= 8 zéros d'affilée (sinon, laisse en littéral)
        if (z >= 8) {
            qsizetype remain = z, off = i;
            while (remain > 0) {
                quint16 chunk = quint16(qMin<qsizetype>(65535, remain));
                out.append(char(0xE0));
                out.append(char(chunk & 0xFF));
                out.append(char((chunk >> 8) & 0xFF));
                remain -= chunk;
            }
            i += z;
        } else {
            // émet un littéral jusqu'au prochain run de zéros (>= seuil) ou la fin
            const qsizetype start = i;
            qsizetype len = 0;
            while (i < raw.size()) {
                if (raw[i] == '\0') {
                    // peek la longueur du prochain run
                    qsizetype k = 0;
                    while (i + k < raw.size() && raw[i + k] == '\0' && k < 65535) ++k;
                    if (k >= 8) break; // on s'arrête avant ce run pour le passer en E0
                }
                ++i; ++len;
            }
            emitLiteral(raw.constData() + start, len);
        }
    }
    return out;
}
 bool VeryfitLZProfessor::decodeVeryfitLz_Minimal(const QByteArray &lz, QByteArray &rawOut) {
    rawOut.clear();
    if (lz.size() < 5) return false;
    qsizetype pos = 0;

    // header
    const QByteArray header = QByteArray::fromHex("000001CE27");
    if (lz.mid(pos, 5) != header) return false;
    pos += 5;

    while (pos < lz.size()) {
        quint8 op = quint8(lz[pos++]);
        if (op == 0x20) {
            if (pos >= lz.size()) return false;
            quint8 L = quint8(lz[pos++]);
            if (pos + L > lz.size()) return false;
            rawOut.append(lz.constData() + pos, L);
            pos += L;
        } else if (op == 0xE0) {
            if (pos + 2 > lz.size()) return false;
            quint16 n = quint8(lz[pos]) | (quint16(quint8(lz[pos+1])) << 8);
            pos += 2;
            rawOut.append(QByteArray(int(n), '\0'));
        } else {
            // inconnu (notre encodeur n'en émet pas)
            return false;
        }
    }
    return true;
}
