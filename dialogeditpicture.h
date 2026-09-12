#ifndef DIALOGEDITPICTURE_H
#define DIALOGEDITPICTURE_H

#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QCoreApplication>

namespace Ui {
class DialogEditPicture;
}

class DialogEditPicture : public QDialog
{
    Q_OBJECT

public:
    explicit DialogEditPicture(const QString& filePath, const bool& newFond, QWidget *parent = nullptr);
    ~DialogEditPicture();

    QPixmap getCroppedImage();  // ✅ Récupérer l'image croppée

private slots:
    void onZoomChanged(int value);
    void onVerticalPosChanged(int value);
    void onHorizontalPosChanged(int value);
    void onOkClicked();  // ✅ Gérer le bouton OK

    void on_pushButtonLoadBg_clicked();

    void on_pushButtonSave_clicked();

    void on_pushButtonQuit_clicked();

private:
    Ui::DialogEditPicture *ui;

    QString filePath;
    QGraphicsScene *scene;
    QGraphicsPixmapItem *imageItem;
    QGraphicsEllipseItem *maskCircle;  // ✅ Le cercle de masque
    qreal baseScale;
    QPixmap croppedResult;  // ✅ Stocker le résultat
    QString dossierTempo = QCoreApplication::applicationDirPath() + "/dossiertempo";


    // === HQ resize (quasi GIMP) ===
    // QImage downscaleHQ(const QImage& src, const QSize& target);
    // QImage makeCover240(const QImage& src); // option "remplir 240x240 sans déformation"
 //   void loadPicture();
    QString findMagickBinary();
    bool resizeImageWithImageMagick(const QString &inputPath, const QString &outputPath, int targetHeight);
    bool nouveau = false;
};

#endif // DIALOGEDITPICTURE_H












// #ifndef DIALOGEDITPICTURE_H
// #define DIALOGEDITPICTURE_H

// #include <QDialog>
// #include <QGraphicsScene>

// namespace Ui {
// class DialogEditPicture;
// }

// class DialogEditPicture : public QDialog
// {
//     Q_OBJECT

// public:
//     explicit DialogEditPicture(const QString& filePath, QWidget *parent = nullptr);
//     ~DialogEditPicture();

// private:
//     Ui::DialogEditPicture *ui;
//     QGraphicsScene *scene;
//     QGraphicsPixmapItem *imageItem;  // ✅ Garder une référence à l'image
//     qreal baseScale;  // ✅ Échelle de base

// private slots:
//     void onZoomChanged(int value);
//     void onVerticalPosChanged(int value);
//     void onHorizontalPosChanged(int value);
// };

// #endif // DIALOGEDITPICTURE_H
