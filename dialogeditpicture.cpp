#include "dialogeditpicture.h"
#include "ui_dialogeditpicture.h"
#include <QDir>
#include <QString>
#include <QPixmap>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <QColorSpace>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>


DialogEditPicture::DialogEditPicture(const QString& filePath, const bool& newFond,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogEditPicture)
    , imageItem(nullptr)
    , maskCircle(nullptr)
    , baseScale(1.0)
{
    ui->setupUi(this);
    nouveau = newFond;

    // Connecter les SpinBox
    connect(ui->spinBoxZoom, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DialogEditPicture::onZoomChanged);
    connect(ui->spinBoxVertical, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DialogEditPicture::onVerticalPosChanged);
    connect(ui->spinBoxHorizontal, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DialogEditPicture::onHorizontalPosChanged);

    // ✅ Connecter le bouton OK
    //connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DialogEditPicture::onOkClicked);

    // Initialiser les valeurs
    ui->spinBoxZoom->setRange(-500, 500);
    ui->spinBoxZoom->setValue(0);
    ui->spinBoxVertical->setRange(-10000, 10000);
    ui->spinBoxVertical->setValue(0);
    ui->spinBoxHorizontal->setRange(-10000, 10000);
    ui->spinBoxHorizontal->setValue(0);

    //******** CREATION DOSSIER TEMPO ***********
    // Vérifie et crée le dossier s’il n’existe pas
    QDir dir;
    if (!dir.exists(dossierTempo)) {
        if (dir.mkpath(dossierTempo)) {
            //  //qDebug() << "📁 Dossier créé :" << dossierTempo;
        } else {
            // //qDebug() << "❌ Échec de création du dossier :" << dossierTempo;
        }
    } else {
        //qDebug() << "📁 Dossier déjà existant :" << dossierTempo;
    }

    scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::lightGray);
    ui->graphicsView->setScene(scene);


    QString bgPath = filePath;
    QPixmap bgPixmap(bgPath);

    if (!bgPixmap.isNull()) {
        imageItem = scene->addPixmap(bgPixmap);
        imageItem->setZValue(-1);
        imageItem->setPos(0, 0);
        imageItem->setTransformOriginPoint(bgPixmap.width() / 2.0, bgPixmap.height() / 2.0);

        // Calculer la dimension la plus grande width ou height
        int maxSize = qMax(bgPixmap.width(), bgPixmap.height());
        scene->setSceneRect(0, 0, maxSize, maxSize);

        ui->graphicsView->setRenderHint(QPainter::Antialiasing);
        ui->graphicsView->setAlignment(Qt::AlignCenter);

        // ✅ Utiliser la scène, pas l'image
        const QRectF r = scene->sceneRect();
        const qreal sceneW = r.width();
        const qreal sceneH = r.height();

        // 100% de la plus petite dimension de la scène (ou 0.8 si tu veux garder une marge)
        const qreal circleDiameter = std::min(sceneW, sceneH); // * 0.8 si besoin
        const QPointF c = r.center();

        // Masque
        QPainterPath maskPath;
        maskPath.addRect(r);
        maskPath.addEllipse(c.x() - circleDiameter/2,
                            c.y() - circleDiameter/2,
                            circleDiameter,
                            circleDiameter);
        maskPath.setFillRule(Qt::OddEvenFill);

        auto* blackMask = new QGraphicsPathItem();
        blackMask->setPath(maskPath);
        blackMask->setBrush(QBrush(QColor(255, 255, 255, 180)));
        blackMask->setPen(Qt::NoPen);
        blackMask->setZValue(999);
        scene->addItem(blackMask);

        // Contour vert
        QPen borderPen(QColor(0,255,0), 2);
        borderPen.setCosmetic(true); // épaisseur en pixels, indépendamment du zoom
        maskCircle = scene->addEllipse(c.x() - circleDiameter/2,
                                       c.y() - circleDiameter/2,
                                       circleDiameter,
                                       circleDiameter,
                                       borderPen, Qt::NoBrush);
        maskCircle->setZValue(1000);

        QTimer::singleShot(0, this, [this]() {
            ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
            baseScale = ui->graphicsView->transform().m11();
        });
    }
}

DialogEditPicture::~DialogEditPicture()
{
    delete ui;
}


void DialogEditPicture::onZoomChanged(int value)
{
    if (!imageItem) return;
    qreal scaleFactor = 1.0 + (value / 100.0);
    imageItem->setScale(scaleFactor);
}

void DialogEditPicture::onVerticalPosChanged(int value)
{
    if (!imageItem) return;
    QPointF currentPos = imageItem->pos();
    imageItem->setPos(currentPos.x(), value);
}

void DialogEditPicture::onHorizontalPosChanged(int value)
{
    if (!imageItem) return;
    QPointF currentPos = imageItem->pos();
    imageItem->setPos(value, currentPos.y());
}



void DialogEditPicture::onOkClicked()
{
    if (!imageItem || !scene) {
        accept();
        return;
    }

    // ✅ CACHER le cercle avant de rendre la scène
    if (maskCircle) {
        maskCircle->setVisible(false);
    }

    // Calculer le rectangle du cercle dans les coordonnées de la scène
    QRectF sceneRect = scene->sceneRect();
    int imageWidth = sceneRect.width();
    int imageHeight = sceneRect.height();

    int circleDiameter = qMin(imageWidth, imageHeight) * 0.8;

    int centerX = imageWidth / 2;
    int centerY = imageHeight / 2;

    QRectF cropRect(centerX - circleDiameter/2,
                    centerY - circleDiameter/2,
                    circleDiameter,
                    circleDiameter);

    // ✅ Créer une image à la TAILLE ORIGINALE du crop (haute résolution)
    QImage resultHighRes(circleDiameter, circleDiameter, QImage::Format_ARGB32);
    resultHighRes.fill(Qt::transparent);

    QPainter painterHighRes(&resultHighRes);
    painterHighRes.setRenderHint(QPainter::Antialiasing);
    painterHighRes.setRenderHint(QPainter::SmoothPixmapTransform);

    // Masque circulaire haute résolution
    QPainterPath clipPathHighRes;
    clipPathHighRes.addEllipse(0, 0, circleDiameter, circleDiameter);
    painterHighRes.setClipPath(clipPathHighRes);

    // ✅ Rendre la scène à la résolution ORIGINALE
    scene->render(&painterHighRes, QRectF(0, 0, circleDiameter, circleDiameter), cropRect);

    painterHighRes.end();

    // // ✅ MAINTENANT redimensionner à 240x240 avec le meilleur algorithme
    // QImage result240 = resultHighRes.scaled(240, 240,
    //                                         Qt::IgnoreAspectRatio,
    //                                         Qt::SmoothTransformation);  // Meilleur algorithme

    // ✅ RÉAFFICHER le cercle après le rendu
    if (maskCircle) {
        maskCircle->setVisible(true);
    }



    croppedResult = QPixmap::fromImage(resultHighRes);

    //QImage src = currentPixmap.toImage();               // ou ton image de travail
    // 2) meilleur rendu 240x240 SANS déformation : makeCover240()
    //    (si tu veux garder le ratio exact même si ça ne remplit pas, utilise downscaleHQ(src, QSize(240,240)) à la place)
    // QImage out240 = makeCover240(resultHighRes);
    // croppedResult = QPixmap::fromImage(out240);

    accept();
}

// ✅ Méthode pour récupérer l'image croppée
QPixmap DialogEditPicture::getCroppedImage()
{
    return croppedResult;
}


void DialogEditPicture::on_pushButtonLoadBg_clicked()
{
    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dernierDossier);

    scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::lightGray);
    ui->graphicsView->setScene(scene);

    QString imageFile = QFileDialog::getOpenFileName(
        this,
        "Sélectionner l'image de fond",
        dernierDossier,
        "Images (*.png *.jpg *.bmp)"
        );

    if (imageFile.isEmpty()) {
        return;
    }

    QString bgPath = imageFile;
    QPixmap bgPixmap(bgPath);

    if (!bgPixmap.isNull()) {
        imageItem = scene->addPixmap(bgPixmap);
        imageItem->setZValue(-1);
        imageItem->setPos(0, 0);
        imageItem->setTransformOriginPoint(bgPixmap.width() / 2.0, bgPixmap.height() / 2.0);

        // Calculer la dimension la plus grande width ou height
        int maxSize = qMax(bgPixmap.width(), bgPixmap.height());
        scene->setSceneRect(0, 0, maxSize, maxSize);

        ui->graphicsView->setRenderHint(QPainter::Antialiasing);
        ui->graphicsView->setAlignment(Qt::AlignCenter);

        // ✅ Utiliser la scène, pas l'image
        const QRectF r = scene->sceneRect();
        const qreal sceneW = r.width();
        const qreal sceneH = r.height();

        // 100% de la plus petite dimension de la scène (ou 0.8 si tu veux garder une marge)
        const qreal circleDiameter = std::min(sceneW, sceneH); // * 0.8 si besoin
        const QPointF c = r.center();

        // Masque
        QPainterPath maskPath;
        maskPath.addRect(r);
        maskPath.addEllipse(c.x() - circleDiameter/2,
                            c.y() - circleDiameter/2,
                            circleDiameter,
                            circleDiameter);
        maskPath.setFillRule(Qt::OddEvenFill);

        auto* blackMask = new QGraphicsPathItem();
        blackMask->setPath(maskPath);
        blackMask->setBrush(QBrush(QColor(255, 255, 255, 180)));
        blackMask->setPen(Qt::NoPen);
        blackMask->setZValue(999);
        scene->addItem(blackMask);

        // Contour vert
        QPen borderPen(QColor(0,255,0), 2);
        borderPen.setCosmetic(true); // épaisseur en pixels, indépendamment du zoom
        maskCircle = scene->addEllipse(c.x() - circleDiameter/2,
                                       c.y() - circleDiameter/2,
                                       circleDiameter,
                                       circleDiameter,
                                       borderPen, Qt::NoBrush);
        maskCircle->setZValue(1000);

        QTimer::singleShot(0, this, [this]() {
            ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
            baseScale = ui->graphicsView->transform().m11();
        });
    }
}


void DialogEditPicture::on_pushButtonSave_clicked()
{
    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();

    if (maskCircle) {
        maskCircle->setVisible(false);
    }

    QRectF sceneRect = scene->sceneRect();
    int imageWidth = sceneRect.width();
    int imageHeight = sceneRect.height();

    int circleDiameter = qMin(imageWidth, imageHeight) * 0.8;

    int centerX = imageWidth / 2;
    int centerY = imageHeight / 2;

    QRectF cropRect(centerX - circleDiameter/2,
                    centerY - circleDiameter/2,
                    circleDiameter,
                    circleDiameter);

    // ✅ Créer une image RGB (sans alpha)
    QImage resultHighRes(circleDiameter, circleDiameter, QImage::Format_RGB888);
    resultHighRes.fill(Qt::black);  // ✅ Fond noir au lieu de transparent

    QPainter painterHighRes(&resultHighRes);
    painterHighRes.setRenderHint(QPainter::Antialiasing);
    painterHighRes.setRenderHint(QPainter::SmoothPixmapTransform);

    // Masque circulaire haute résolution
    QPainterPath clipPathHighRes;
    clipPathHighRes.addEllipse(0, 0, circleDiameter, circleDiameter);
    painterHighRes.setClipPath(clipPathHighRes);

    scene->render(&painterHighRes, QRectF(0, 0, circleDiameter, circleDiameter), cropRect);

    painterHighRes.end();

    if (maskCircle) {
        maskCircle->setVisible(true);
    }


    QString fileNameSource = dossierTempo + "/source.png";
    QString fileNameFinal = QFileDialog::getSaveFileName(
        this,
        "Enregistrer l'image...",
        dernierDossier + "/fond.png",
        "Images PNG (*.png);;Images JPEG (*.jpg)"
        );

    if (fileNameFinal.isEmpty()) {
        return;
    }

    // ✅ Sauvegarder directement (déjà sans alpha)
    if (!resultHighRes.save(fileNameSource, "PNG")) {
        qWarning() << "❌ Impossible de sauvegarder" << fileNameSource;
        return;
    }

    if (!resizeImageWithImageMagick(fileNameSource, fileNameFinal, 240)) {
        QMessageBox::warning(this, "ImageMagick", "Le redimensionnement a échoué. Regarde la console pour les logs.");
    } else {
        qDebug() << "👍 out:" << fileNameFinal;
    }
    if(nouveau) accept();
}


void DialogEditPicture::on_pushButtonQuit_clicked()
{
    close();
}

QString DialogEditPicture::findMagickBinary() {
    // Essaie d’abord IM7 ("magick"), sinon IM6 ("convert")
    QProcess p;
    p.start("magick", {"-version"});
    if (p.waitForFinished(2000) && p.exitCode() == 0) return "magick";

    p.start("convert", {"-version"});
    if (p.waitForFinished(2000) && p.exitCode() == 0) return "convert";

    return QString(); // introuvable
}

bool DialogEditPicture::resizeImageWithImageMagick(const QString &inputPath,
                                const QString &outputPath,
                                int targetHeight)
{
    // Vérifs de base
    if (!QFileInfo::exists(inputPath)) {
        qWarning() << "⛔ Fichier source introuvable:" << inputPath;
        return false;
    }
    const QString program = findMagickBinary();
    if (program.isEmpty()) {
        qWarning() << "⛔ ImageMagick introuvable (ni 'magick' ni 'convert'). Installe: sudo apt install imagemagick";
        return false;
    }

    // IM7: "magick input -resize x240 output"
    // IM6: "convert input -resize x240 output"
    QStringList args;
    args << inputPath
         << "-resize" << QString("x%1").arg(targetHeight)
         << outputPath;

    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels); // stdout+stderr ensemble
    proc.start(program, args);

    // (Optionnel) définir un cwd pour l’outil si tu veux:
    // proc.setWorkingDirectory(QFileInfo(outputPath).absolutePath());

    const bool ok = proc.waitForFinished(20000); // 20s
    const QByteArray out = proc.readAll();       // stdout+stderr
    const int code = proc.exitCode();

    qDebug() << "▶ CMD:" << program << args;
    qDebug() << "▶ OUT:\n" << out;
    qDebug() << "▶ exitCode:" << code << " exitStatus:" << proc.exitStatus() << " proc.error:" << proc.error();

    if (!ok) {
        qWarning() << "⛔ Timeout/échec d'exécution ImageMagick";
        return false;
    }
    if (code != 0) {
        qWarning() << "⛔ ImageMagick a renvoyé un code d'erreur:" << code;
        return false;
    }
    if (!QFileInfo::exists(outputPath)) {
        qWarning() << "⛔ ImageMagick terminé mais fichier de sortie manquant:" << outputPath;
        return false;
    }

    qDebug() << "✅ Redimensionnement OK ->" << outputPath;
    return true;
}

