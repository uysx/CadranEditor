#include "pngtoveryfitraw.h"
#include <QtEndian>
#include <QImage>
#include <QMessageBox>
#include <QDebug>
#include <QPainter>

/**
 * Convertit une image PNG (avec canal alpha) en format RAW VeryFit
 *
 * Format RAW VeryFit :
 * - Header : "RAW\0" (4 octets) + width LE (2) + height LE (2) = 8 octets
 * - Données : RGB565 little-endian (width × height × 2 octets)
 * - Padding : Zéros jusqu'à atteindre une taille fixe (14816 pour 74×80)
 *
 * Important :
 * - RGB565 n'a PAS de canal alpha
 * - L'alpha est "aplati" sur fond noir avant conversion
 * - Les pixels transparents deviennent noirs (0,0,0)
 */


// -------------------------------------------------------
// 1) Lire l'image juste pour connaître w,h,alpha potentiel
bool PngToVeryfitRaw::probeImage(const QString &path, int &outW, int &outH, bool &outHasAlpha)
{
    QImage img(path);
    if (img.isNull()) {
        return false;
    }

    img = img.convertToFormat(QImage::Format_RGBA8888);
    outW = img.width();
    outH = img.height();

    outHasAlpha = false;
    const uchar *bits = img.constBits();
    const int stride = img.bytesPerLine();
    for (int y = 0; y < outH && !outHasAlpha; ++y) {
        const uchar *row = bits + y * stride;
        for (int x = 0; x < outW; ++x) {
            quint8 a = row[x*4 + 3];
            if (a != 0xFF) {
                outHasAlpha = true;
                break;
            }
        }
    }
    return true;
}

// -------------------------------------------------------
// 2) Conversion en bloc RAW VeryFit
QByteArray PngToVeryfitRaw::convertAuto(const QString &path, bool forceOpaquePreview)
{
    QImage img(path);
    if (img.isNull()) {
        return QByteArray();
    }

    img = img.convertToFormat(QImage::Format_RGBA8888);
    const int w = img.width();
    const int h = img.height();

    // Détection alpha
    bool hasAlphaChannel = false;
    if (!forceOpaquePreview) {
        const uchar *bits = img.constBits();
        const int stride = img.bytesPerLine();
        for (int y = 0; y < h && !hasAlphaChannel; ++y) {
            const uchar *row = bits + y * stride;
            for (int x = 0; x < w; ++x) {
                quint8 a = row[x*4 + 3];
                if (a != 0xFF) {
                    hasAlphaChannel = true;
                    break;
                }
            }
        }
    }

    // RGB565
    QByteArray rgb565;
    rgb565.resize(w * h * 2);
    {
        uchar *dst = reinterpret_cast<uchar*>(rgb565.data());
        const uchar *bits = img.constBits();
        const int stride = img.bytesPerLine();

        for (int y = 0; y < h; ++y) {
            const uchar *row = bits + y * stride;
            for (int x = 0; x < w; ++x) {
                quint8 r = row[x*4 + 0];
                quint8 g = row[x*4 + 1];
                quint8 b = row[x*4 + 2];
                // FIX: VeryFit utilise RGB565 standard MAIS en BIG-ENDIAN
                quint16 px565 = packRGB565(r, g, b);
                *dst++ = static_cast<uchar>((px565 >> 8) & 0xFF);  // Byte HAUT d'abord (BIG-ENDIAN)
                *dst++ = static_cast<uchar>(px565 & 0xFF);          // Byte BAS ensuite
            }
        }
    }

    // Alpha 4 bits/pixel - FIX pour largeurs impaires
    QByteArray alphaA4;
    if (hasAlphaChannel && !forceOpaquePreview) {
        const int pixelCount = w * h;
        const int outBytes = (pixelCount + 1) / 2;
        alphaA4.resize(outBytes);
        uchar *dstA = reinterpret_cast<uchar*>(alphaA4.data());
        memset(dstA, 0, outBytes);

        const uchar *bits = img.constBits();
        const int stride = img.bytesPerLine();

        int outIndex = 0;
        for (int y = 0; y < h; ++y) {
            const uchar *row = bits + y * stride;
            for (int x = 0; x < w; x += 2) {
                // Pixel 0 (toujours présent)
                quint8 a0 = row[x*4 + 3];
                quint8 a0_4 = static_cast<quint8>((a0 * 15) / 255);

                // Pixel 1 (peut ne pas exister si largeur impaire et x+1 >= w)
                quint8 a1_4 = 0;
                if (x + 1 < w) {
                    quint8 a1 = row[(x+1)*4 + 3];
                    a1_4 = static_cast<quint8>((a1 * 15) / 255);
                }

                // high nibble = pixel0, low nibble = pixel1
                dstA[outIndex++] = static_cast<uchar>((a0_4 << 4) | (a1_4 & 0x0F));
            }
        }
    }

    // Header 16 octets
    QByteArray header;
    header.resize(16);
    uchar *hdr = reinterpret_cast<uchar*>(header.data());

    // "RAW\0"
    hdr[0] = 'R'; hdr[1] = 'A'; hdr[2] = 'W'; hdr[3] = 0x00;

    // width, height LE
    hdr[4] = static_cast<uchar>( w        & 0xFF);
    hdr[5] = static_cast<uchar>((w >> 8)  & 0xFF);
    hdr[6] = static_cast<uchar>( h        & 0xFF);
    hdr[7] = static_cast<uchar>((h >> 8)  & 0xFF);

    // flags
    // opaque => 0x0085 => bytes [0x85,0x00]
    // alpha  => 0x6685 => bytes [0x85,0x66]
    if (hasAlphaChannel && !forceOpaquePreview) {
        hdr[8] = 0x85;
        hdr[9] = 0x66;
    } else {
        hdr[8] = 0x85;
        hdr[9] = 0x00;
    }

    // [10..11] 0x0000
    hdr[10] = 0x00;
    hdr[11] = 0x00;

    // [12..13] = taille RGB565 si alpha présent, sinon 0
    if (hasAlphaChannel && !forceOpaquePreview) {
        quint32 rgbSize = w * h * 2;
        hdr[12] = static_cast<uchar>( rgbSize       & 0xFF );
        hdr[13] = static_cast<uchar>((rgbSize >> 8) & 0xFF );
    } else {
        hdr[12] = 0x00;
        hdr[13] = 0x00;
    }

    // [14..15] 0x0000
    hdr[14] = 0x00;
    hdr[15] = 0x00;

    // final
    QByteArray out;
    out.reserve(16 + rgb565.size() + alphaA4.size());
    out.append(header);
    out.append(rgb565);
    if (hasAlphaChannel && !forceOpaquePreview) {
        out.append(alphaA4);
    }

    return out;
}

// RGB565 classique
quint16 PngToVeryfitRaw::packRGB565(quint8 r, quint8 g, quint8 b)
{
    quint16 rr = static_cast<quint16>((r >> 3) & 0x1F);
    quint16 gg = static_cast<quint16>((g >> 2) & 0x3F);
    quint16 bb = static_cast<quint16>((b >> 3) & 0x1F);
    return static_cast<quint16>((rr << 11) | (gg << 5) | bb);
}




QByteArray PngToVeryfitRaw::convert(const QString &imgPath) {
    QImage img(imgPath);
    if (img.isNull()) {
        qWarning() << "PngToVeryfitRaw: image invalide:" << imgPath;
        return {};
    }

    // Convertir en ARGB32 pour accès facile aux canaux RGBA
    if (img.format() != QImage::Format_ARGB32)
        img = img.convertToFormat(QImage::Format_ARGB32);

    const int w = img.width();
    const int h = img.height();

    // ✅ ÉTAPE 1 : Créer le buffer RGB565 avec alpha aplati sur fond noir
    QByteArray rgb565;
    rgb565.resize(w * h * 2);

    for (int y = 0; y < h; ++y) {
        const QRgb *src = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        quint8 *dst = reinterpret_cast<quint8*>(rgb565.data() + y * w * 2);

        for (int x = 0; x < w; ++x) {
            int r8 = qRed(src[x]);
            int g8 = qGreen(src[x]);
            int b8 = qBlue(src[x]);
            int a8 = qAlpha(src[x]);

            // ✅ APLATIR L'ALPHA SUR FOND NOIR
            // Si alpha < 255, mélanger avec noir (0,0,0)
            if (a8 < 255) {
                r8 = (r8 * a8) / 255;
                g8 = (g8 * a8) / 255;
                b8 = (b8 * a8) / 255;
            }

            // Convertir RGB888 → RGB565
            const quint16 r5 = quint16(r8 >> 3);  // 8 bits → 5 bits
            const quint16 g6 = quint16(g8 >> 2);  // 8 bits → 6 bits
            const quint16 b5 = quint16(b8 >> 3);  // 8 bits → 5 bits
            const quint16 px = quint16((r5 << 11) | (g6 << 5) | b5);

            // FIX: VeryFit utilise BIG-ENDIAN
            dst[2*x + 0] = quint8((px >> 8) & 0xFF);   // Octet haut
            dst[2*x + 1] = quint8(px & 0xFF);          // Octet bas
        }
    }

    // ✅ ÉTAPE 2 : Assembler le fichier RAW
    QByteArray out;

    // Header RAW (8 octets)
    out.append('R');
    out.append('A');
    out.append('W');
    out.append('\0');

    // Width et Height en little-endian
    const quint16 w_le = qToLittleEndian<quint16>(quint16(w));
    const quint16 h_le = qToLittleEndian<quint16>(quint16(h));
    out.append(reinterpret_cast<const char*>(&w_le), sizeof(w_le));
    out.append(reinterpret_cast<const char*>(&h_le), sizeof(h_le));

    // Données RGB565
    out.append(rgb565);

    // ✅ ÉTAPE 3 : Padding avec des zéros jusqu'à la taille attendue
    // La référence VeryFit utilise 14816 octets pour les images 74×80
    const int expectedSize = 14816;  // Taille fixe observée
    const int actualSize = out.size();  // 8 + (74×80×2) = 11848

    if (actualSize < expectedSize) {
        const int padding = expectedSize - actualSize;
        out.append(QByteArray(padding, '\0'));
        qDebug() << "PngToVeryfitRaw: Padding de" << padding << "octets ajouté";
    }

    qDebug() << "PngToVeryfitRaw:"
             << "Image:" << imgPath
             << "Dimensions:" << w << "×" << h
             << "RGB565:" << rgb565.size() << "octets"
             << "Total (avec padding):" << out.size() << "octets";

    return out;
}

QByteArray PngToVeryfitRaw::convertWithCanvas(const QString &imgPath,
                                              int canvasW,
                                              int canvasH)
{
    // 1. Charger l'image source
    QImage src(imgPath);
    if (src.isNull()) {
        qWarning() << "convertWithCanvas: image invalide:" << imgPath;
        return {};
    }
    if (src.format() != QImage::Format_ARGB32)
        src = src.convertToFormat(QImage::Format_ARGB32);

    const int wSrc = src.width();
    const int hSrc = src.height();
    if (wSrc > canvasW || hSrc > canvasH) {
        qWarning() << "convertWithCanvas: source plus grande que le canvas"
                   << imgPath
                   << "src:" << wSrc << "x" << hSrc
                   << "canvas:" << canvasW << "x" << canvasH;
        // on continue quand même
    }

    // 2. Canevas final ARGB32
    QImage canvas(canvasW, canvasH, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);

    // 3. Peindre la source dedans
    {
        QPainter p(&canvas);
        p.drawImage(0, 0, src);
        p.end();
    }

    // 4. Est-ce qu'on a besoin d'alpha ?
    bool needA4 = false;
    for (int y = 0; y < canvasH && !needA4; ++y) {
        const QRgb *row = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
        for (int x = 0; x < canvasW; ++x) {
            if (qAlpha(row[x]) < 255) {
                needA4 = true;
                break;
            }
        }
    }

    // 5. Générer le plan RGB565 (2 octets / pixel)
    const int rgb565Len = canvasW * canvasH * 2;
    QByteArray rgb565;
    rgb565.resize(rgb565Len);

    for (int y = 0; y < canvasH; ++y) {
        const QRgb *srcLine = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
        quint8 *dst = reinterpret_cast<quint8*>(rgb565.data() + y * canvasW * 2);

        for (int x = 0; x < canvasW; ++x) {
            int r8 = qRed(srcLine[x]);
            int g8 = qGreen(srcLine[x]);
            int b8 = qBlue(srcLine[x]);

            // On pourrait prémultiplier par alpha si besoin :
            // int a = qAlpha(srcLine[x]);
            // if (a < 255) {
            //     r8 = (r8 * a) / 255;
            //     g8 = (g8 * a) / 255;
            //     b8 = (b8 * a) / 255;
            // }

            const quint16 r5 = quint16(r8 >> 3);
            const quint16 g6 = quint16(g8 >> 2);
            const quint16 b5 = quint16(b8 >> 3);
            const quint16 px16 = quint16((r5 << 11) | (g6 << 5) | b5); // RGB565

            // VeryFit: BIG-ENDIAN (octet haut d'abord)
            dst[2*x + 0] = quint8((px16 >> 8) & 0xFF);  // Octet haut
            dst[2*x + 1] = quint8(px16 & 0xFF);         // Octet bas
        }
    }

    // 6. Générer le plan alpha A4 si besoin (4 bits/pixel, 2 pixels / octet)
    QByteArray a4;
    if (needA4) {
        const int a4RowBytes = (canvasW + 1) / 2;
        a4.resize(a4RowBytes * canvasH);

        for (int y = 0; y < canvasH; ++y) {
            const QRgb *srcLine = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
            quint8 *dst = reinterpret_cast<quint8*>(a4.data() + y * a4RowBytes);

            int x = 0;
            int k = 0;
            for (; x + 1 < canvasW; x += 2, ++k) {
                quint8 a0 = quint8((qAlpha(srcLine[x])   + 8) / 17); // 0..255 -> 0..15
                quint8 a1 = quint8((qAlpha(srcLine[x+1]) + 8) / 17);
                dst[k] = quint8((a0 << 4) | a1);
            }
            if (x < canvasW) {
                quint8 a0 = quint8((qAlpha(srcLine[x]) + 8) / 17);
                dst[k++] = quint8(a0 << 4);
            }
        }
    }

    // 7. Construire le header VeryFit (16 octets)
    // D'après l'analyse :
    // 0-3  : "RAW\0"
    // 4-5  : width  (LE)
    // 6-7  : height (LE)
    // 8-9  : flags :
    //        - 0x0085 (bytes 85 00) si pas d'alpha
    //        - 0x6685 (bytes 85 66) si alpha (chiffres)
    // 10-11: crc ou inconnu -> on met 0x0000
    // 12-13: longueur RGB565 (LE) si alpha présent
    //        sinon 0x0000 pour les previews opaques
    // 14-15: 0x0000
    quint16 flags = needA4 ? quint16(0x6685) : quint16(0x0085);
    quint16 crc   = 0x0000; // on laisse 0
    quint16 rgbLenField = needA4 ? quint16(rgb565Len) : quint16(0x0000);
    quint16 zero  = 0x0000;

    QByteArray out;
    out.reserve(16 + rgb565.size() + a4.size());

    auto append_u16 = [&](quint16 v){
        quint16 le = qToLittleEndian<quint16>(v);
        out.append(reinterpret_cast<const char*>(&le), sizeof(le));
    };

    // "RAW\0"
    out.append('R');
    out.append('A');
    out.append('W');
    out.append('\0');

    append_u16(quint16(canvasW));
    append_u16(quint16(canvasH));
    append_u16(flags);
    append_u16(crc);
    append_u16(rgbLenField);
    append_u16(zero);

    // 8. Ajouter les pixels + alpha
    out.append(rgb565);
    if (needA4) {
        out.append(a4);
    }

    return out;
}

