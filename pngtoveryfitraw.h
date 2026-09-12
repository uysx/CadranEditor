#pragma once
#include <QString>
#include <QByteArray>

class PngToVeryfitRaw {
public:
    // Convertit une image disque en bloc RAW VeryFit:
    // "RAW\0" + uint16_le(width) + uint16_le(height) + pixels RGB565 (LE)
    static QByteArray convert(const QString &imgPath);
    static QByteArray convertWithCanvas(const QString &imgPath, int canvasW, int canvasH);



    // 1) Analyse seulement (lit l'image)
    //    -> récupère largeur, hauteur, hasAlpha, etc.
    static bool probeImage(const QString &path, int &outW, int &outH, bool &outHasAlpha);

    // 2) Conversion finale en bloc VeryFit
    //    forceOpaquePreview = true => on force flags 0x0085 et pas d'alpha
    static QByteArray convertAuto(const QString &path, bool forceOpaquePreview);

private:
    static quint16 packRGB565(quint8 r, quint8 g, quint8 b);
};

