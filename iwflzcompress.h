// ============================================================================
// iwflzcompress.h
// ============================================================================
#ifndef IWFLZCOMPRESS_H
#define IWFLZCOMPRESS_H

#include <QString>

class IwfLzCompress
{
public:
    // Compresse un fichier .iwf vers .iwf.lz
    static bool compressFile(const QString& inPath,
                             QString* errorMsg = nullptr);

    // Compresse des données brutes
    static bool compressData(const QByteArray& inData,
                             QByteArray& outData,
                             QString* errorMsg = nullptr);
};

#endif // IWFLZCOMPRESS_H
