// ============================================================================
// iwflzcompress.cpp - VERSION AVEC DÉCOUPAGE EN BLOCS
// ============================================================================

#include "iwflzcompress.h"
#include "iwflz_compress.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#define BLOCK_SIZE 4096  // Taille max d'un bloc décompressé

bool IwfLzCompress::compressFile(const QString& inPath,
                                 QString* errorMsg)
{
    // Lecture du fichier source
    QFile inFile(inPath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        if (errorMsg) {
            *errorMsg = QString("Impossible d'ouvrir le fichier source : %1")
            .arg(inFile.errorString());
        }
        return false;
    }

    QByteArray inData = inFile.readAll();
    inFile.close();

    if (inData.isEmpty()) {
        if (errorMsg) {
            *errorMsg = "Le fichier source est vide";
        }
        return false;
    }

    //qDebug() << "Fichier source lu:" << inData.size() << "octets";

    // Compression avec découpage en blocs
    QByteArray outData;
    if (!compressData(inData, outData, errorMsg)) {
        return false;
    }

    //qDebug() << "Compression OK:" << outData.size() << "octets";

    QString outPath = inPath + ".lz";
    // Écriture du fichier compressé
    QFile outFile(outPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        if (errorMsg) {
            *errorMsg = QString("Impossible de créer le fichier de sortie : %1")
                            .arg(outFile.errorString());
        }
        return false;
    }

    qint64 written = outFile.write(outData);
    outFile.close();

    if (written != outData.size()) {
        if (errorMsg) {
            *errorMsg = "Erreur lors de l'écriture du fichier compressé";
        }
        return false;
    }

    return true;
}

bool IwfLzCompress::compressData(const QByteArray& inData,
                                 QByteArray& outData,
                                 QString* errorMsg)
{
    if (inData.isEmpty()) {
        if (errorMsg) {
            *errorMsg = "Données d'entrée vides";
        }
        return false;
    }

    outData.clear();

    size_t offset = 0;
    size_t totalSize = inData.size();
    int blockNum = 0;

    //qDebug() << "════════════════════════════════════════";
    //qDebug() << "🔥 COMPRESSION AVEC BLOCS DE" << BLOCK_SIZE << "OCTETS";
    //qDebug() << "════════════════════════════════════════";

    // Découper en blocs de BLOCK_SIZE octets
    while (offset < totalSize) {
        blockNum++;

        // Taille du bloc actuel
        size_t blockSize = std::min<size_t>(BLOCK_SIZE, totalSize - offset);

        //qDebug() << "Bloc #" << blockNum << ": offset=" << offset
         //        << "taille=" << blockSize << "octets";

        // Buffer temporaire pour ce bloc
        size_t maxOutSize = (blockSize * 2) + 1024;
        std::vector<uint8_t> tempBuffer(maxOutSize);
        size_t actualOutSize = maxOutSize;

        // Compresser ce bloc
        bool success = iwflz_compress(
            reinterpret_cast<const uint8_t*>(inData.constData() + offset),
            blockSize,
            tempBuffer.data(),
            &actualOutSize
            );

        if (!success) {
            if (errorMsg) {
                *errorMsg = QString("Échec de la compression du bloc #%1").arg(blockNum);
            }
            return false;
        }

        //qDebug() << "  → Compressé: " << blockSize << "→" << actualOutSize << "octets"
         //        << "(" << QString::number(100.0 * actualOutSize / blockSize, 'f', 1) << "%)";

        // Ajouter ce bloc au résultat
        outData.append(reinterpret_cast<const char*>(tempBuffer.data()), actualOutSize);

        offset += blockSize;
    }

    //qDebug() << "════════════════════════════════════════";
    //qDebug() << "TOTAL:" << inData.size() << "→" << outData.size() << "octets";
    //qDebug() << "Ratio:" << QString::number(100.0 * outData.size() / inData.size(), 'f', 1) << "%";
    //qDebug() << "Nombre de blocs:" << blockNum;
    //qDebug() << "════════════════════════════════════════";

    return true;
}

