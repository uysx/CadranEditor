
#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QFileInfo>
#include <QDir>
#include <QJsonParseError>
#include <QFontDatabase>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

#include <QInputDialog>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

#include "jsonwatchface.h"
#include "utils.h"
#include "editdialog.h"
#include "iwflzcompress.h"
#include <QCryptographicHash>
#include "createiwffromfolder.h"
#include "create_sql.h"
#include "dialogeditpicture.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow) {
    ui->setupUi(this);
    setupCheckboxLinks(ui);

    resize(1200, 980);

    QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);

    //*****************************************************
    statusLabel = new QLabel(this);
    //statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);  // pour pouvoir copier
    statusLabel->setStyleSheet("QLabel { font-family: monospace; }");
    statusLabel->setMinimumWidth(400);  // ou plus si nécessaire
    statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusBar()->addPermanentWidget(statusLabel, 1);
    // ui->plainTextEditLog->appendPlainText("✅ Application pr"
    //                                       "ête");

    //***********************************************************

    ui->textEditIwfJson->setReadOnly(true);
    ui->textEditIwfJson->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);



    scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::lightGray);
    scene->setSceneRect(0, 0, 240, 240);
    ui->graphicsView->setScene(scene);

    connect(ui->currentWidgetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::oncurrentWidgetComboChanged);
    connect(ui->currentTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTypeComboChanged);

    connect(ui->widgetComboAdd, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onWidgetComboAddChanged);

    connect(ui->xSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onXChanged);
    connect(ui->ySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onYChanged);
    connect(ui->wSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onWChanged);
    connect(ui->hSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onHChanged);

    connect(ui->xSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int newVal){updateJsonSelectionField("x", newVal);});
    connect(ui->ySpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int newVal){updateJsonSelectionField("y", newVal);});
    connect(ui->wSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int newVal){updateJsonSelectionField("w", newVal);});
    connect(ui->hSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [=](int newVal){updateJsonSelectionField("h", newVal);});


    connect(ui->actionOuvrirUnTxt, &QAction::triggered, this, &MainWindow::ouvrirUnTxt);

    ui->timeEdit->setEnabled(false);
    ui->dateEdit->setEnabled(false);

    // ✅ BLOQUER les signaux avant de définir les valeurs
    ui->timeEdit->blockSignals(true);
    ui->dateEdit->blockSignals(true);
    ui->timeEdit->setTime(QTime(10, 8, 27));
    ui->dateEdit->setDate(QDate::currentDate());
    // ✅ DÉBLOQUER les signaux après
    ui->timeEdit->blockSignals(false);
    ui->dateEdit->blockSignals(false);


    isLoadingJson = false;
    currentItemIndex = -1;
    highlightRect = nullptr;
    currentPixmapItem = nullptr;

    ui->pushButtonCreerPreview->setEnabled(false);
    ui->pushButtonCreatelaunchPicture->setEnabled(false);
    ui->pushButtonCreateIwfLz->setEnabled(false);
    ui->pushButtonCreateSqlFromBin->setEnabled(false);
    ui->pushButtonEnvoyer->setEnabled(false);
    ui->pushButtonChargeIwfLz->setEnabled(false);

    populateWidgetComboAdd();

    // à la fin du constructeur
    QTimer::singleShot(0, this, [this]{
        displayEmptyScene();
    });


}

MainWindow::~MainWindow() {
    delete ui;
}

//*********************************************************************************************************
void MainWindow::openJson()
//*********************************************************************************************************
{


    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();

    // 2. Définir le dossier courant (important pour QFileDialog)
    QDir::setCurrent(dernierDossier);

    //*******************************************************

    // Sinon, comportement classique
    //QDir::setCurrent(dossierCourant);
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Ouvrir le fichier iwf.json",
        dernierDossier,
        "iwf.json (iwf.json)"
        );

    if (fileName.isEmpty())
        return;

    QFileInfo fi(fileName);
    if (fi.fileName() != "iwf.json") {
        QMessageBox::warning(this, "Fichier invalide", "Veuillez sélectionner le fichier iwf.json uniquement.");
        return;
    }

    // 5. Mémoriser le dossier pour la prochaine fois
    QFileInfo infoFichier(fileName);
    QString nouveauDossier = infoFichier.absolutePath();
    settings.setValue("dernierDossier", nouveauDossier);
    //QDir::setCurrent(nouveauDossier); // <- Facultatif ici, utile si tu enchaînes plusieurs opérations

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
    //*******************************************
    //dossierAZipper = fi.absolutePath();
    jsonCharge = true;
    jsonFilePath = fileName;
    ouvrirJson(fileName);
}

//*********************************************************************************************************
void MainWindow::ouvrirJson(QString fileName)
//*********************************************************************************************************
{

    QFileInfo info(fileName);
    QString baseDir = info.absolutePath();
    dossierAZipper = baseDir;
    ui->currentWidgetCombo->clear();
    ui->currentTypeCombo->clear();
    ui->textEditEntete->clear();
    ui->textEditIwfJson->clear();
    ui->textEditFont->clear();


    if (jwf.read(fileName, baseDir)) {
        // //qDebug() << "Nombre d’éléments JSON :" << jwf.items.size();
        isLoadingJson = true;
        displayAllWidget();

        isLoadingJson = false;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Erreur", "Impossible d'ouvrir le fichier iwf.json.");
            return;
        }
        QString contenu = QString::fromUtf8(file.readAll());
        // //qDebug() << contenu.left(100).toUtf8().toHex(' ');

        contenu.replace("\t", "  ");  // ou "    " pour 4 espaces
        ui->textEditIwfJson->setPlainText(contenu);

        file.close();

        fontFilePath = baseDir + "/font.json";
        if (QFileInfo::exists(fontFilePath)) {
            QFile file1(fontFilePath);

            if (!file1.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Erreur", "Impossible d'ouvrir le fichier font.json.");
                return;
            }
            QString contenu1 = QString::fromUtf8(file1.readAll());
            // //qDebug() << contenu.left(100).toUtf8().toHex(' ');

            contenu.replace("\t", "  ");  // ou "    " pour 4 espaces
            ui->textEditFont->setPlainText(contenu1);


            file1.close();
        }
        statusLabel->setText(fileName);

        ui->pushButtonCreerPreview->setEnabled(true);
        ui->pushButtonCreatelaunchPicture->setEnabled(true);
        ui->pushButtonCreateIwfLz->setEnabled(true);
        ui->pushButtonCreateSqlFromBin->setEnabled(true);
        ui->pushButtonEnvoyer->setEnabled(true);
        ui->pushButtonChargeIwfLz->setEnabled(true);



    } else {
        QMessageBox::warning(this, "Erreur", "Impossible de lire le fichier JSON.");
    }
}

//*********************************************************************************************************
void MainWindow::displayAllWidget()
//*********************************************************************************************************
{
    //qDebug << "╔════════════════════════════════════════════╗";
    //qDebug << "║  🎨 DÉBUT displayAllWidget()               ║";
    //qDebug << "╚════════════════════════════════════════════╝";

    //qDebug << "📊 jwf.items.size() =" << jwf.items.size();
    //qDebug << "📍 currentItemIndex =" << currentItemIndex;
    //qDebug << "🗺️ pixmapItemsMap.size() =" << pixmapItemsMap.size();

    //✅ Nettoyer le rectangle de surbrillance d'abord
    //qDebug << "1️⃣ Nettoyage du rectangle de surbrillance...";
    if (highlightRect) {
        //qDebug() << "   → highlightRect existe";
        if (highlightRect->scene() == scene) {
            //qDebug() << "   → Suppression de la scène";
            scene->removeItem(highlightRect);
        }
        delete highlightRect;
        highlightRect = nullptr;
        //qDebug() << "   → highlightRect supprimé";
    }

    // ✅ Nettoyer tous les items graphiques
    //qDebug() << "2️⃣ Nettoyage des items graphiques...";
    int itemsRemoved = 0;
    for (auto it = pixmapItemsMap.begin(); it != pixmapItemsMap.end(); ++it) {
        QGraphicsItem* item = it.value();
        if (item) {
            if (item->scene() == scene) {
                scene->removeItem(item);
                itemsRemoved++;
            }
            delete item;
        }
    }
    //qDebug << "   → " << itemsRemoved << "items supprimés de la scène";

    pixmapItemsMap.clear();
    currentPixmapItem = nullptr;
    currentItemIndex = -1;
    //qDebug() << "   → pixmapItemsMap vidée";

    // ✅ Nettoyer la scène
    //qDebug() << "3️⃣ Nettoyage de la scène...";
    try {
        scene->clear();
        //qDebug() << "   → scene->clear() OK";
    } catch (...) {
        //qDebug() << "❌ EXCEPTION dans scene->clear()";
        throw;
    }

    // 🔧 Définir une liste de types gérés
    static const QSet<QString> supportedTypes = {
        "date", "time", "hour", "min", "second", "week", "day", "month",
        "calorie", "distance", "heartrate", "redpoint", "battery",
        "step", "icon", "sleep", "bluetooth", "progressbar", "ring", "apm", "shortcut", "anima"
    };

    //qDebug() << "4️⃣ Mise à jour de l'entête...";
    ui->textEditEntete->setPlainText(jwf.headerText);

    // Charger le fond d'écran s'il existe
    //qDebug() << "5️⃣ Chargement du fond d'écran...";
    QString bgPath = QDir(jwf.basePath).filePath(jwf.backgroundImage);
    //qDebug() << "   → bgPath =" << bgPath;

    QPixmap bgPixmap(bgPath);

    if (!bgPixmap.isNull()) {
        //qDebug() << "   → Fond chargé:" << bgPixmap.width() << "x" << bgPixmap.height();
        auto* bgItem = scene->addPixmap(bgPixmap);
        bgItem->setZValue(-1);
        bgItem->setPos(0, 0);
        scene->setSceneRect(0, 0, bgPixmap.width(), bgPixmap.height());
    } else {
        //qDebug() << "   → Création fond noir 240x240";
        QPixmap blackBg(240, 240);
        blackBg.fill(Qt::black);
        auto* bgItem = scene->addPixmap(blackBg);
        bgItem->setZValue(-1);
        bgItem->setPos(0, 0);
        scene->setSceneRect(0, 0, 240, 240);
    }


    //########################################################################################
    // Ajouter une grille de repère à la scène
    //qDebug() << "6️⃣ Ajout de la grille...";
    const int gridSize = 20;
    const int sceneWidth = 240;
    const int sceneHeight = 240;

    QPen gridPen(Qt::white);
    gridPen.setStyle(Qt::DashLine);
    gridPen.setWidth(2);
    gridPen.setCosmetic(true);

    for (int x = 0; x <= sceneWidth; x += gridSize) {
        scene->addLine(x, 0, x, sceneHeight, gridPen);
    }
    for (int y = 0; y <= sceneHeight; y += gridSize) {
        scene->addLine(0, y, sceneWidth, y, gridPen);
    }
    //qDebug() << "   → Grille ajoutée";

    // ✅ AJOUTER le cercle gris de 240px de diamètre (rayon 120)
    QPen circlePen(QColor(128, 128, 128), 2);  // Gris, épaisseur 2 pixels
    circlePen.setCosmetic(true);  // Pour que l'épaisseur reste constante même avec zoom

    // Calculer la position pour centrer le cercle dans la scène 240x240
    // Cercle de 180px → commence à (240-180)/2 = 30
    int circleX = (sceneWidth - 240) / 2;   // 30
    int circleY = (sceneHeight - 240) / 2;  // 30

    scene->addEllipse(circleX, circleY, 240, 240, circlePen, Qt::NoBrush);


    // ✅ Simuler le cadre cylindrique noir de la montre (240px de diamètre)
    QPainterPath maskPath;
    maskPath.addRect(0, 0, sceneWidth, sceneHeight);  // Rectangle noir plein

    // Calculer la position du cercle centré (zone visible de la montre)
    int circleX1 = (sceneWidth - 240) / 2;
    int circleY1 = (sceneHeight - 240) / 2;
    maskPath.addEllipse(circleX1, circleY1, 240, 240);  // Trou circulaire transparent

    // Créer le masque avec la règle OddEvenFill pour créer le "trou"
    maskPath.setFillRule(Qt::OddEvenFill);

    QGraphicsPathItem* blackMask = new QGraphicsPathItem();
    blackMask->setPath(maskPath);
    blackMask->setBrush(QBrush(Qt::black));
    blackMask->setPen(QPen(Qt::NoPen));
    blackMask->setZValue(999);  // Au-dessus du contenu mais sous le rectangle de sélection rouge
    scene->addItem(blackMask);

    // Optionnel : ajouter un cercle de contour pour marquer le bord
    QPen borderPen(QColor(100, 100, 100), 2);  // Gris foncé
    borderPen.setCosmetic(true);
    scene->addEllipse(circleX, circleY, 240, 240, borderPen, Qt::NoBrush)->setZValue(1000);

    //########################################################################################

    // ✅ Afficher tous les widgets
    //qDebug() << "7️⃣ Affichage des widgets...";
    int i = 0;
    int widgetsDisplayed = 0;
    for (const auto& item : jwf.items) {
        const QString& widget = item.widget;
        const QString& type = item.type;

        if (!supportedTypes.contains(type)) {
            //qDebug() << "   ⏭️ Widget" << i << "ignoré (type:" << type << ")";
            ++i;
            continue;
        }

        //qDebug() << "   🖼️ Affichage widget" << i << "- type:" << type << "widget:" << widget;

        int x = item.x;
        int y = item.y;
        int w = item.w;
        //int h = item.h;
        QString font = item.font;
        int fontnum = item.fontnum;
        QString align = item.align.toLower();



        if (widget == "custom") {
            if (type == "time") {

                QTime currentTime = QTime::currentTime();
                QString timeStr;

                if(ui->checkBoxHourSystem->isChecked()){
                    timeStr = currentTime.toString("hhmm");
                } else {
                    // Récupérer l'heure depuis le QTimeEdit
                    QTime fixedTime = ui->timeEdit->time();
                    timeStr = fixedTime.toString("hhmm");
                }


                int totalWidth = 0;
                QList<QPixmap> digits;
                QPixmap separatorPix;

                // 1️⃣ Collecter les images et calculer la largeur totale
                for (int j = 0; j < 4; ++j) {
                    QChar digit = timeStr[j];
                    QString digitPath = QString("%1/%2").arg(font).arg(digit);
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, digitPath);
                    if (!pix.isNull()) {
                        totalWidth += pix.width();
                        digits.append(pix);
                    }

                    // Ajouter ":" entre hh et mm si fontnum >= 11
                    if (j == 1 && fontnum >= 11) {
                        QString separatorPath = QString("%1/10").arg(font);  // "10" représente ":"
                        separatorPix = loadPixmapWithFallback(jwf.basePath, separatorPath);
                        if (!separatorPix.isNull())
                            totalWidth += separatorPix.width();
                    }
                }

                // 2️⃣ Calculer l'alignement
                int xpos = x;
                if (align == "center") {
                    xpos = x + (w - totalWidth) / 2;
                } else if (align == "right") {
                    xpos = x + (w - totalWidth);
                }

                // 3️⃣ Créer le groupe et afficher les images
                QGraphicsItemGroup* group = new QGraphicsItemGroup();
                int index = 0;
                for (const QPixmap& pix : digits) {
                    auto* digitItem = new QGraphicsPixmapItem(pix);
                    digitItem->setPos(xpos - x, 0);
                    group->addToGroup(digitItem);
                    xpos += pix.width();

                    if (index == 1 && !separatorPix.isNull()) {
                        auto* separatorItem = new QGraphicsPixmapItem(separatorPix);
                        separatorItem->setPos(xpos - x, 0);
                        group->addToGroup(separatorItem);
                        xpos += separatorPix.width();
                    }
                    ++index;
                }

                group->setPos(x, y);
                scene->addItem(group);
                pixmapItemsMap[i] = group;
            }
            else if (type == "date") {

                QDate currentDate = QDate::currentDate();
                QString dateStr;

                if(ui->checkBoxHourSystem->isChecked()){
                    dateStr = currentDate.toString("MMdd");
                } else {
                    // Récupérer l'heure depuis le QTimeEdit
                    QDate fixedDate = ui->dateEdit->date();
                    dateStr = fixedDate.toString("MMdd");
                }




                /// QDate currentDate = QDate::currentDate();
                //QString dateStr = currentDate.toString("MMdd");

                int totalWidth = 0;
                QList<QPixmap> digits;
                QPixmap separatorPix;

                // 1️⃣ Collecter les images et calculer la largeur totale
                for (int j = 0; j < 4; ++j) {
                    QChar digit = dateStr[j];
                    QString digitPath = QString("%1/%2").arg(font).arg(digit);
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, digitPath);
                    if (!pix.isNull()) {
                        totalWidth += pix.width();
                        digits.append(pix);
                    }

                    // Ajouter le séparateur entre MM et dd si fontnum >= 11
                    if (j == 1 && fontnum >= 11) {
                        QString separatorPath = QString("%1/10").arg(font);  // "10" = séparateur
                        separatorPix = loadPixmapWithFallback(jwf.basePath, separatorPath);
                        if (!separatorPix.isNull())
                            totalWidth += separatorPix.width();
                    }
                }

                // 2️⃣ Calculer l'alignement
                int xpos = x;
                if (align == "center") {
                    xpos = x + (w - totalWidth) / 2;
                } else if (align == "right") {
                    xpos = x + (w - totalWidth);
                }

                // 3️⃣ Créer le groupe et afficher les images
                QGraphicsItemGroup* group = new QGraphicsItemGroup();
                int index = 0;
                for (const QPixmap& pix : digits) {
                    auto* digitItem = new QGraphicsPixmapItem(pix);
                    digitItem->setPos(xpos - x, 0);
                    group->addToGroup(digitItem);
                    xpos += pix.width();

                    if (index == 1 && !separatorPix.isNull()) {
                        auto* separatorItem = new QGraphicsPixmapItem(separatorPix);
                        separatorItem->setPos(xpos - x, 0);
                        group->addToGroup(separatorItem);
                        xpos += separatorPix.width();
                    }
                    ++index;
                }

                group->setPos(x, y);
                scene->addItem(group);
                pixmapItemsMap[i] = group;
            }


            else if (type == "hour" || type == "min" || type == "second") {

                QString format;
                if (type == "hour") format = "hh";
                else if (type == "min") format = "mm";
                else format = "ss";

                //QString timeStr = QTime::currentTime().toString(format);
                QString timeStr;

                if(ui->checkBoxHourSystem->isChecked()){
                    timeStr = QTime::currentTime().toString(format);
                } else {
                    // Récupérer l'heure depuis le QTimeEdit
                    QTime fixedTime = ui->timeEdit->time();
                    timeStr = fixedTime.toString(format);
                }

                // QString timeStr = QTime::currentTime().toString(format);
                QVector<QPixmap> digits;

                for (QChar digit : timeStr) {
                    QString imagePath = QString("%1/%2").arg(font).arg(digit);
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, imagePath);
                    if (!pix.isNull()) {
                        digits.append(pix);
                    }
                }

                QGraphicsItemGroup* group = createDigitGroupAligned(digits, align, x, y, w);
                scene->addItem(group);
                pixmapItemsMap[i] = group;

            }
            else if (type == "week") {

                static const char* weekPrefixes[7] = {
                    "en_mon", "en_tue", "en_wed", "en_thu", "en_fri", "en_sat", "en_sun"
                };




                int dayIndex;// = QDate::currentDate().dayOfWeek();
                QString prefix;// = weekPrefixes[dayIndex - 1];  // "en_thu"

                // QDate currentDate = QDate::currentDate();
                // QString dateStr;

                if(ui->checkBoxHourSystem->isChecked()){
                    //dateStr = currentDate.toString("MMdd");
                    dayIndex = QDate::currentDate().dayOfWeek();
                    prefix = weekPrefixes[dayIndex - 1];  // "en_thu"
                } else {
                    // Récupérer l'heure depuis le QTimeEdit
                    QDate fixedDate = ui->dateEdit->date();
                    dayIndex = fixedDate.dayOfWeek();
                    prefix = weekPrefixes[dayIndex - 1];  // "en_thu"
                    //dateStr = fixedDate.toString("MMdd");
                }





                QDir dir(jwf.basePath + "/" + font);
                QStringList files = dir.entryList(QDir::Files);

                // Cherche un fichier qui commence par le préfixe
                QString matchedFile;
                for (const QString& file : files) {
                    if (file.startsWith(prefix)) {
                        matchedFile = file;
                        break;
                    }
                }

                if (!matchedFile.isEmpty()) {

                    QString imagePath = QString("%1/%2").arg(font).arg(matchedFile);
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, imagePath);
                    if (!pix.isNull()) {
                        QPixmap pixToUse = pix;
                        auto* item = scene->addPixmap(pixToUse);
                        item->setPos(x, y);
                        pixmapItemsMap[i] = item;
                        //  //qDebug() << "⏳ Item affiché" << type << ":" << matchedFile;
                    }
                } else {
                    //qDebug() << "❌ Image semaine introuvable avec préfixe:" << prefix;
                }
            }

            else if (type == "apm") {

                static const char* apmStr[2] = { "en_am", "en_pm" };

                // Utilise l'heure système actuelle
                int hour = QTime::currentTime().hour();
                int apmIndex = (hour < 12) ? 0 : 1;  // 0 = AM, 1 = PM

                QString imageName = QString("%1/%2").arg(font).arg(apmStr[apmIndex]);
                QPixmap pix = loadPixmapWithFallback(jwf.basePath, imageName);
                QPixmap pixToUse = pix;
                if (!pixToUse.isNull()) {
                    auto* item = scene->addPixmap(pixToUse);
                    item->setPos(x, y);
                    pixmapItemsMap[i] = item;
                    //  //qDebug() << "⏳ Item affiché" << type;
                }
            }

            else if (type == "day") {

                QString dayStr = QString("%1").arg(QDate::currentDate().day(), 2, 10, QChar('0'));

                QVector<QPixmap> digits;
                int totalWidth = 0;

                // Préchargement des chiffres + calcul largeur totale
                for (QChar digit : dayStr) {
                    QString path = QString("%1/%2").arg(font).arg(digit);
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, path);
                    if (!pix.isNull()) {
                        digits.append(pix);
                        totalWidth += pix.width();
                    }
                }

                // Alignement comme pour les autres types
                int xpos = x;
                if (align == "center") xpos = x + (w - totalWidth) / 2;
                else if (align == "right") xpos = x + (w - totalWidth);

                QGraphicsItemGroup* group = new QGraphicsItemGroup();

                for (const QPixmap& pix : digits) {
                    auto* digitItem = new QGraphicsPixmapItem(pix);
                    digitItem->setPos(xpos - x, 0);  // position relative dans le groupe
                    group->addToGroup(digitItem);
                    xpos += pix.width();
                }

                group->setPos(x, y);  // position absolue dans la scène
                scene->addItem(group);
                pixmapItemsMap[i] = group;

            }

            else if (type == "month") {

                int month = QDate::currentDate().month();
                static const char* monthNames[12] = {
                    "en_january", "en_february", "en_march", "en_april", "en_may", "en_june",
                    "en_july", "en_august", "en_september", "en_october", "en_november", "en_december"
                };
                QString path = QString("%1/%2").arg(font).arg(monthNames[month - 1]);
                QPixmap pix = loadPixmapWithFallback(jwf.basePath, path);
                QPixmap pixToUse = pix;
                if (!pixToUse.isNull()) {
                    auto* item = scene->addPixmap(pixToUse);
                    item->setPos(x, y);
                    pixmapItemsMap[i] = item;
                    //  //qDebug() << "⏳ Item affiché" << type;
                }
            }

            else if (type == "calorie" || type == "distance" || type == "heartrate" || type == "step") {

                QString typeStr = QString::number(0);
                if (type == "calorie"){
                    typeStr = QString::number(625);
                }
                else if(type == "distance")
                {
                    typeStr = QString::number(846);
                }
                else if(type == "heartrate")
                {
                    typeStr = QString::number(109);
                }
                else if(type == "step")
                {
                    typeStr = QString::number(12586);
                }


                QList<QPixmap> digits;
                int totalWidth = 0;

                for (QChar digit : typeStr) {
                    QString imageFile = QString("%1/%2").arg(font).arg(digit);
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, imageFile);
                    if (!pix.isNull()) {
                        totalWidth += pix.width();
                        digits.append(pix);
                    }
                }

                int xpos = x;
                if (align == "center") {
                    xpos = x + (w - totalWidth) / 2;
                } else if (align == "right") {
                    xpos = x + (w - totalWidth);
                }

                // ✅ Créer un groupe pour les chiffres
                QGraphicsItemGroup* group = new QGraphicsItemGroup();
                for (const QPixmap& pix : digits) {
                    QPixmap pixToUse = pix;
                    auto* item = new QGraphicsPixmapItem(pixToUse);
                    item->setPos(xpos - x, 0);  // ✅ Corrigé ici
                    //item->setPos(xpos, 0);  // position relative dans le groupe
                    group->addToGroup(item);
                    xpos += pixToUse.width();
                }
                group->setPos(x, y);  // position absolue dans la scène
                scene->addItem(group);

                // ✅ Mémorise le groupe si tu veux le faire bouger plus tard
                pixmapItemsMap[i] = group;  // cast implicite vers QGraphicsPixmapItem* ne fonctionne pas → solution ci-dessous
                //  //qDebug() << "⏳ Item affiché " + type;

            }


            else if (type == "battery") {

                QString batStr = QString::number(28);  // 🔧 Valeur d'exemple, à remplacer par la vraie valeur

                QList<QPixmap> digits;
                int totalWidth = 0;

                // 🔢 Charge les chiffres un à un
                for (QChar digit : batStr) {
                    QString imageFile = QString("%1/%2").arg(font).arg(digit);
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, imageFile);
                    if (!pix.isNull()) {
                        digits.append(pix);
                        totalWidth += pix.width();
                    }
                }
                if(fontnum >= 11){
                    //qDebug() << "fontnum" << fontnum;
                    // ➕ Ajoute l’image "%" qui est nommée "10"
                    QString percentPath = QString("%1/10").arg(font);  // ⚠️ image 10 = symbole %
                    QPixmap percentPix = loadPixmapWithFallback(jwf.basePath, percentPath);
                    if (!percentPix.isNull()) {
                        digits.append(percentPix);
                        totalWidth += percentPix.width();
                    }
                }

                // 📐 Calcul du point de départ horizontal
                int xpos = x;
                if (align == "center") {
                    xpos = x + (w - totalWidth) / 2;
                } else if (align == "right") {
                    xpos = x + (w - totalWidth);
                }

                // 👥 Crée un groupe d’éléments
                QGraphicsItemGroup* group = new QGraphicsItemGroup();
                for (const QPixmap& pix : digits) {
                    QPixmap pixToUse = pix;
                    auto* digitItem = new QGraphicsPixmapItem(pixToUse);
                    digitItem->setPos(xpos - x, 0);  // position relative au groupe
                    group->addToGroup(digitItem);
                    xpos += pixToUse.width();
                }

                group->setPos(x, y);  // position absolue dans la scène
                scene->addItem(group);
                pixmapItemsMap[i] = group;


            }

            else if (type == "redpoint" || type == "icon" || type == "sleep" || type == "bluetooth" || type == "shortcut") {
                QString imageKey;

                if (type == "redpoint") {
                    imageKey = "2"; //
                } else if (type == "icon") {

                    imageKey = item.bg;
                    //qDebug() << "⏳ imageKey = item.bg;" << imageKey;

                } else if (type == "shortcut") {
                    font = "app";
                    imageKey = item.app;
                    //qDebug() << "shortcut = item.app;" << imageKey;
                } else {
                    imageKey = item.bg;
                }

                QString path = QString("%1/%2").arg(font).arg(imageKey);
                QPixmap pix = loadPixmapWithFallback(jwf.basePath, path);
                if (!pix.isNull()) {
                    auto* itemPtr = scene->addPixmap(pix);
                    itemPtr->setPos(x, y);
                    pixmapItemsMap[i] = itemPtr;

                }
            }
            else if (type == "anima") {

                QString imageKey = "0." + item.animaformat;


                QString path = QString("%1/%2").arg(item.animaicon).arg(imageKey);
                QPixmap pix = loadPixmapWithFallback(jwf.basePath, path);
                if (!pix.isNull()) {
                    auto* itemPtr = scene->addPixmap(pix);
                    itemPtr->setPos(x, y);
                    pixmapItemsMap[i] = itemPtr;

                }
            }


        }
        else if (widget == "progressbar") {
            const QString& type = item.type;
            // //qDebug() << "⏩ Type trouvé :" << type;
            if (!supportedTypes.contains(type)) {
                //qDebug() << "⏩ Type ignoré:" << type;
                ++i;
                continue;
            }

            int x = item.x;
            int y = item.y;
            // int w = item.w;
            // int h = item.h;
            QString font = item.font;
            //int fontnum = item.fontnum;
            QString align = item.align.toLower();
            //qDebug() << "progressbar -> Item affiché" << type;
            if (type == "battery") {
                QString imageKey;


                imageKey = item.bg;
                // "bg": "bat_4bit.bmp",
                // "progress": "bat_4bit.bmp"


                QString path = QString("%1/%2").arg(font).arg(imageKey);
                QPixmap pix = loadPixmapWithFallback(jwf.basePath, path);
                if (!pix.isNull()) {
                    auto* itemPtr = scene->addPixmap(pix);
                    itemPtr->setPos(x, y);
                    pixmapItemsMap[i] = itemPtr;

                }

            }
        }
        else if (widget == "ring") {
            QString imageKey = item.bg;


            QString path = QString("%1/%2").arg(font).arg(imageKey);
            QPixmap pix = loadPixmapWithFallback(jwf.basePath, path);
            if (!pix.isNull()) {
                auto* itemPtr = scene->addPixmap(pix);
                itemPtr->setPos(x, y);
                pixmapItemsMap[i] = itemPtr;

            }
        }
        else if (widget == "watch") {
            if (type == "time") {
                // (optionnel) background
                if (!item.type.isEmpty()) {
                    QPixmap bg = loadPixmapWithFallback(jwf.basePath, jwf.basePath + "/" + item.type);
                    if (!bg.isNull()) {
                        auto* it = scene->addPixmap(bg);
                        it->setPos(x, y);
                        it->setZValue(0);
                    }
                }

                auto addHand = [&](const QString& relPath,
                                   int centerx, int centery, int anchorx, int anchory,
                                   qreal z, qreal offsetDeg = 0.0) -> QGraphicsPixmapItem* {
                    if (relPath.isEmpty()) return nullptr;
                    QPixmap pix = loadPixmapWithFallback(jwf.basePath, jwf.basePath + "/" + relPath);
                    if (pix.isNull()) return nullptr;

                    auto* it = scene->addPixmap(pix);
                    it->setPos(x + anchorx - centerx, y + anchory - centery);
                    it->setTransformOriginPoint(centerx, centery); // pivot ≡ axe de l’aiguille
                    it->setZValue(z);
                    it->setData(0, offsetDeg); // garde l’offset si besoin
                    return it;
                };

                // Crée les aiguilles (si présentes)
                QGraphicsPixmapItem* hourItem = addHand(item.hour,
                                                        item.hourcenterx, item.hourcentery, item.houranchorx, item.houranchory, 1.0 /*Z*/, 0.0 /*offset*/);

                QGraphicsPixmapItem* minItem = addHand(item.minute,
                                                       item.mincenterx, item.mincentery, item.minanchorx, item.minanchory, 2.0, 0.0);

                QGraphicsPixmapItem* secItem = addHand(item.second,
                                                       item.seccenterx, item.seccentery, item.secanchorx, item.secanchory, 3.0, 0.0);

                // --- Figer l'heure courante une fois ---

                QTime currentTime = QTime::currentTime();
                QTime now;  // ✅ Déclarer la variable qui sera utilisée

                if(ui->checkBoxHourSystem->isChecked()){
                    now = currentTime;  // ✅ Utiliser l'heure actuelle
                } else {
                    // Récupérer l'heure depuis le QTimeEdit
                    now = ui->timeEdit->time();  // ✅ Utiliser l'heure fixe
                }

                // Maintenant 'now' contient la bonne heure (actuelle ou fixe)
                // Variante A (lisse) : minutes tiennent compte des secondes, heures tiennent compte des minutes
                const double s = now.second();
                const double m = now.minute() + s / 60.0;
                const double h = (now.hour() % 12) + m / 60.0;

                const qreal angleSec  = s * 6.0;              // 360/60
                const qreal angleMin  = m * 6.0;              // 360/60
                const qreal angleHour = h * 30.0;             // 360/12

                // Applique la rotation une seule fois
                if (hourItem) hourItem->setRotation(angleHour + hourItem->data(0).toReal());
                if (minItem)  minItem->setRotation(angleMin  + minItem->data(0).toReal());
                if (secItem)  secItem->setRotation(angleSec  + secItem->data(0).toReal());

                // Si tu stockes dans ta map, évite d’écraser la même clé:
                // pixmapItemsMap[i + 0.01] = hourItem; etc., ou utilise des clés dédiées.
            }
        }


        widgetsDisplayed++;
        ++i;
    }
    //qDebug() << "   → " << widgetsDisplayed << "widgets affichés sur" << jwf.items.size();

    // ✅ Finalisation
    //qDebug() << "8️⃣ Finalisation...";
    ui->graphicsView->resetTransform();

    if (!bgPixmap.isNull()) {
        scene->setSceneRect(0, 0, bgPixmap.width(), bgPixmap.height());
    } else {
        scene->setSceneRect(0, 0, 240, 240);
    }

    //qDebug() << "9️⃣ Population de la combo...";
    if (!jwf.items.isEmpty()) {
        populatecurrentWidgetCombo();
        ui->currentTypeCombo->setCurrentIndex(0);
        oncurrentWidgetComboChanged(0);
        //qDebug() << "   → Combo peuplée, index 0 sélectionné";
    } else {
        //qDebug() << "   → Aucun item, combo vide";
    }

    // ✅ Forcer une mise à jour complète de la vue
    //qDebug() << "🔟 Forçage de la mise à jour de la vue...";

    //*********************************************************************
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView->setAlignment(Qt::AlignCenter);
    ui->graphicsView->viewport()->update();
    ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    scene->update();
    //*********************************************************************
    // ✅ Même logique de scaling


    //qDebug() << "╔════════════════════════════════════════════╗";
    //qDebug() << "║  ✅ FIN displayAllWidget() RÉUSSI         ║";
    //qDebug() << "╚════════════════════════════════════════════╝";

}

//*********************************************************************************************************
void MainWindow::populatecurrentWidgetCombo()
//*********************************************************************************************************
{


    QSet<QString> uniqueTypes;
    for (const auto& item : jwf.items) {
        uniqueTypes.insert(item.widget);
    }

    if (!uniqueTypes.isEmpty()) {
        ui->currentWidgetCombo->clear();
        ui->currentWidgetCombo->addItems(uniqueTypes.values());
    }
}



//*********************************************************************************************************
void MainWindow::updateHighlightForType(const int& index)
//*********************************************************************************************************
{
    //qDebug() << "┌────────────────────────────────────────────┐";
    //qDebug() << "│ 🎯 updateHighlightForType(" << index << ")          │";
    //qDebug() << "└────────────────────────────────────────────┘";

    // ✅ Vérifier que l'index est valide
    if (index < 0 || index >= jwf.items.size()) {
        //qDebug() << "❌ Index invalide:" << index << "/ taille:" << jwf.items.size();
        return;
    }
    //qDebug() << "✅ Index valide";

    // ✅ Supprimer l'ancien rectangle
    //qDebug() << "1️⃣ Suppression ancien rectangle...";
    if (highlightRect) {
        //qDebug() << "   → highlightRect existe";
        if (highlightRect->scene() == scene) {
            //qDebug() << "   → Suppression de la scène";
            scene->removeItem(highlightRect);
        }
        delete highlightRect;
        highlightRect = nullptr;
        //qDebug() << "   → Supprimé";
    } else {
        //qDebug() << "   → Aucun rectangle à supprimer";
    }

    // ✅ Vérifier currentPixmapItem
    //qDebug() << "2️⃣ Vérification currentPixmapItem...";
    if (currentPixmapItem && currentPixmapItem->scene() != scene) {
        qDebug() << "   ⚠️ Référence obsolète, reset";
        currentPixmapItem = nullptr;
    }

    const auto& item = jwf.items[index];
    //qDebug() << "3️⃣ Item sélectionné - widget:" << item.widget << "type:" << item.type;
    //qDebug() << "   Position JSON: x=" << item.x << "y=" << item.y << "w=" << item.w << "h=" << item.h;

    // ✅ Récupérer l'item graphique
    //qDebug() << "4️⃣ Recherche dans pixmapItemsMap...";
    //qDebug() << "   → Taille de la map:" << pixmapItemsMap.size();
    //qDebug() << "   → Contient index" << index << "?" << pixmapItemsMap.contains(index);

    if (pixmapItemsMap.contains(index)) {
        currentPixmapItem = pixmapItemsMap[index];
        if (currentPixmapItem) {
            if (currentPixmapItem->scene() == scene) {
                try {
                    if (item.w > 0 && item.h > 0) {
                        highlightRect = new QGraphicsRectItem(item.x, item.y, item.w, item.h);
                        highlightRect->setPen(QPen(Qt::red, 2));  // ✅ Trait fin
                        highlightRect->setZValue(1000);
                        scene->addItem(highlightRect);
                    } else {
                        qDebug() << "⚠️ Dimensions invalides pour l'index" << index;
                    }
                } catch (const std::exception& e) {
                    qDebug() << "❌ EXCEPTION lors création rectangle:" << e.what();
                } catch (...) {
                    qDebug() << "❌ EXCEPTION INCONNUE lors création rectangle";
                }
            } else {
                goto createFromJson;
            }
        } else {
            goto createFromJson;
        }
    } else {
    createFromJson:
        if (item.w > 0 && item.h > 0) {
            try {
                highlightRect = new QGraphicsRectItem(item.x, item.y, item.w, item.h);
                highlightRect->setPen(QPen(Qt::red, 2));  // ✅ Trait fin
                highlightRect->setZValue(1000);
                scene->addItem(highlightRect);
            } catch (const std::exception& e) {
                qDebug() << "❌ EXCEPTION lors création depuis JSON:" << e.what();
            } catch (...) {
                qDebug() << "❌ EXCEPTION INCONNUE lors création depuis JSON";
            }
        } else {
            qDebug() << "⚠️ Dimensions invalides, pas de rectangle créé";
        }
    }

    currentItemIndex = index;
    //qDebug() << "5️⃣ currentItemIndex =" << currentItemIndex;

    ui->graphicsView->viewport()->update();
    ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    scene->update();

}

//*********************************************************************************************************
void MainWindow::onXChanged(int newX)
//*********************************************************************************************************
{
    if (currentItemIndex >= 0 && currentItemIndex < jwf.items.size()) {
        jwf.items[currentItemIndex].x = newX;
        int y = ui->ySpin->value();
        if (highlightRect) highlightRect->setRect(newX, y, highlightRect->rect().width(), highlightRect->rect().height());
        if (currentPixmapItem) currentPixmapItem->setPos(newX, y);
    }
}

//*********************************************************************************************************
void MainWindow::onYChanged(int newY)
//*********************************************************************************************************
{
    if (currentItemIndex >= 0 && currentItemIndex < jwf.items.size()) {
        jwf.items[currentItemIndex].y = newY;
        int x = ui->xSpin->value();
        if (highlightRect) highlightRect->setRect(x, newY, highlightRect->rect().width(), highlightRect->rect().height());
        if (currentPixmapItem) currentPixmapItem->setPos(x, newY);
    }
}

//*********************************************************************************************************
void MainWindow::onWChanged(int w)
//*********************************************************************************************************
{
    if (currentItemIndex >= 0 && currentItemIndex < jwf.items.size()) {
        jwf.items[currentItemIndex].w = w;
        updateHighlightRect();
    }
}

//*********************************************************************************************************
void MainWindow::onHChanged(int h)
//*********************************************************************************************************
{
    if (currentItemIndex >= 0 && currentItemIndex < jwf.items.size()) {
        jwf.items[currentItemIndex].h = h;
        updateHighlightRect();
    }
}

//*********************************************************************************************************
void MainWindow::updateHighlightRect()
//*********************************************************************************************************
{
    if (highlightRect) {
        const auto& item = jwf.items[currentItemIndex];
        highlightRect->setRect(item.x, item.y, item.w, item.h);
    }
}

//*********************************************************************************************************
void MainWindow::oncurrentWidgetComboChanged(int index)
//*********************************************************************************************************
{
    //qDebug() << "index :" << index;

    populateTypeCombo();

}

//*********************************************************************************************************
void MainWindow::populateTypeCombo()
//*********************************************************************************************************
{

    ui->currentTypeCombo->clear();

    QString widget = ui->currentWidgetCombo->currentText();

    //ui->currentTypeCombo->clear();

    for (int i = 0; i < jwf.items.size(); ++i) {
        const QString& wid = jwf.items[i].widget;
        const QString& type = jwf.items[i].type;
        if(wid == widget){
            ui->currentTypeCombo->addItem(type, i);  // i est l’index réel dans jwf.items
        }
    }

    if (ui->currentTypeCombo->count() > 0){
        //qDebug() << "currentText :" << ui->currentTypeCombo->currentText();
        ui->currentTypeCombo->setCurrentIndex(0);
        //onTypeComboChanged(0);
    }

}


//*********************************************************************************************************
void MainWindow::onTypeComboChanged(int index)
//*********************************************************************************************************
{
    //qDebug << "Ici Index combo:" << index;

    QVariant data = ui->currentTypeCombo->itemData(index);
    if (!data.isValid()) {
        // qWarning() << "❌ Donnée invalide pour l’index combo" << index;
        return;
    }

    int indexDansJwf = data.toInt();

    if (indexDansJwf < 0 || indexDansJwf >= jwf.items.size()) {
        //qWarning() << "❌ indexDansJwf invalide:" << indexDansJwf;
        return;
    }

    const JsonItem& item = jwf.items[indexDansJwf];
    //qDebug() << "✅ item :" << item.widget;

    currentItemIndex = indexDansJwf;

    // updateCheckboxesFromItem(ui, item);  // DÉPLACÉ APRÈS setOnlyCurrentWidgetAttributVisibleTrue
    updateHighlightForType(indexDansJwf);


    // ✅ BLOQUER les signaux avant de modifier les spinbox
    ui->xSpin->blockSignals(true);
    ui->ySpin->blockSignals(true);
    ui->wSpin->blockSignals(true);
    ui->hSpin->blockSignals(true);

    ui->xSpin->setValue(item.x);
    ui->ySpin->setValue(item.y);
    ui->wSpin->setValue(item.w);
    ui->hSpin->setValue(item.h);

    // ✅ DÉBLOQUER les signaux après
    ui->xSpin->blockSignals(false);
    ui->ySpin->blockSignals(false);
    ui->wSpin->blockSignals(false);
    ui->hSpin->blockSignals(false);


    ui->lineEditWidget->setText(item.widget);
    ui->lineEditType->setText(item.type);
    ui->lineEditAlign->setText(item.align);
    ui->lineEditFont->setText(item.font);
    ui->lineEditFgcolor->setText(item.fgcolor);
    ui->lineEditFgrender->setText(item.fgrender);
    ui->lineEditBgcolor->setText(item.bgcolor);
    ui->lineEditBgrender->setText(item.bgrender);
    ui->lineEditBg->setText(item.bg);
    ui->lineEditAnimaicon->setText(item.animaicon);
    ui->lineEditAnimatype->setText(item.animatype);
    ui->lineEditAnimaformat->setText(item.animaformat);
    ui->lineEditProgress->setText(item.progress);
    ui->lineEditApp->setText(item.app);

    ui->lineEditHour->setText(item.hour);
    ui->lineEditMinute->setText(item.minute);
    ui->lineEditSecond->setText(item.second);


    ui->lineEditFontnum->setText(item.fontnum != -1 ? QString::number(item.fontnum) : "");
    ui->lineEditStyle->setText(item.style != -1 ? QString::number(item.style) : "");
    ui->lineEditTurn->setText(item.turn != -1 ? QString::number(item.turn) : "");
    ui->lineEditFrame->setText(item.frame != -1 ? QString::number(item.frame) : "");
    ui->lineEditTime->setText(item.time != -1 ? QString::number(item.time) : "");

    ui->lineEditAnimabpp->setText(item.animabpp != -1 ? QString::number(item.animabpp) : "");

    ui->lineEditStartangle->setText(item.startangle != -1 ? QString::number(item.startangle) : "");
    ui->lineEditEndangle->setText(item.endangle != -1 ? QString::number(item.endangle) : "");
    ui->lineEditRingedge->setText(item.ringedge != -1 ? QString::number(item.ringedge) : "");
    ui->lineEditMetricinch->setText(item.metricinch != -1 ? QString::number(item.metricinch) : "");

    ui->lineEditHourcenterx->setText(item.hourcenterx != -1 ? QString::number(item.hourcenterx) : "");
    ui->lineEditHourcentery->setText(item.hourcentery != -1 ? QString::number(item.hourcentery) : "");
    ui->lineEditHouranchorx->setText(item.houranchorx != -1 ? QString::number(item.houranchorx) : "");
    ui->lineEditHouranchory->setText(item.houranchory != -1 ? QString::number(item.houranchory) : "");

    ui->lineEditMincenterx->setText(item.mincenterx != -1 ? QString::number(item.mincenterx) : "");
    ui->lineEditMincentery->setText(item.mincentery != -1 ? QString::number(item.mincentery) : "");
    ui->lineEditMinanchorx->setText(item.minanchorx != -1 ? QString::number(item.minanchorx) : "");
    ui->lineEditMinanchory->setText(item.minanchory != -1 ? QString::number(item.minanchory) : "");

    ui->lineEditSeccenterx->setText(item.seccenterx != -1 ? QString::number(item.seccenterx) : "");
    ui->lineEditSeccentery->setText(item.seccentery != -1 ? QString::number(item.seccentery) : "");
    ui->lineEditSecanchorx->setText(item.secanchorx != -1 ? QString::number(item.secanchorx) : "");
    ui->lineEditSecanchory->setText(item.secanchory != -1 ? QString::number(item.secanchory) : "");


    //**********************************************************************************

    setAllAttributVisibleFalse(ui);

    // ✅ CORRECTION : Forcer tous les parents à être visibles
    QWidget* parent = ui->checkBoxFont->parentWidget();
    while (parent) {
        parent->setVisible(true);
        parent = parent->parentWidget();
    }

    setOnlyCurrentWidgetAttributVisibleTrue(ui, item);
    updateCheckboxesFromItem(ui, item);
}
//*********************************************************************************************************
void MainWindow::onWidgetComboAddChanged(int index)
//*********************************************************************************************************
{
    // qDebug() << "index :" << index;

    populateTypeComboAdd();

}

//*********************************************************************************************************
void MainWindow::updateJsonSelectionField(const QString& key, int newValue)
//*********************************************************************************************************

{
    QString currentText = ui->textEditEntete->toPlainText();
    if (currentText.isEmpty()) return;

    // Ex : remplace "x": 107 par "x": 123
    QRegularExpression re(QString("\"%1\"\\s*:\\s*\\d+").arg(QRegularExpression::escape(key)));
    QString updatedText = currentText.replace(re, QString("\"%1\": %2").arg(key).arg(newValue));

    ui->textEditEntete->setPlainText(updatedText);
}



//*********************************************************************************************************
void MainWindow::on_pushButtonAppliquer_clicked()
//*********************************************************************************************************

{
    if (currentItemIndex < 0 || currentItemIndex >= jwf.items.size())
        return;

    JsonItem &item = jwf.items[currentItemIndex];


    //*************************************************************************

    auto update = [&](const QString& field, auto setter, auto clearer) {
        QLineEdit* edit = findChild<QLineEdit*>("lineEdit" + field);
        QCheckBox* check = findChild<QCheckBox*>("checkBox" + field);
        if (edit && check) {
            if (check->isChecked()) {
                QString text = edit->text();
                setter(text);
            } else {
                clearer();
            }
        }
    };

    // QString fields
    update("Widget", [&](const QString& val) { item.widget = val; }, [&]() { item.widget.clear(); });
    update("Type", [&](const QString& val) { item.type = val; }, [&]() { item.type.clear(); });
    update("Align", [&](const QString& val) { item.align = val; }, [&]() { item.align.clear(); });
    update("Font", [&](const QString& val) { item.font = val; }, [&]() { item.font.clear(); });
    update("Fgcolor", [&](const QString& val) { item.fgcolor = val; }, [&]() { item.fgcolor.clear(); });
    update("Fgrender", [&](const QString& val) { item.fgrender = val; }, [&]() { item.fgrender.clear(); });
    update("Bgcolor", [&](const QString& val) { item.bgcolor = val; }, [&]() { item.bgcolor.clear(); });
    update("Bgrender", [&](const QString& val) { item.bgrender = val; }, [&]() { item.bgrender.clear(); });
    update("Bg", [&](const QString& val) { item.bg = val; }, [&]() { item.bg.clear(); });
    update("Animaicon", [&](const QString& val) { item.animaicon = val; }, [&]() { item.animaicon.clear(); });
    update("Animatype", [&](const QString& val) { item.animatype = val; }, [&]() { item.animatype.clear(); });
    update("Animaformat", [&](const QString& val) { item.animaformat = val; }, [&]() { item.animaformat.clear(); });
    update("Progress", [&](const QString& val) { item.progress = val; }, [&]() { item.progress.clear(); });
    update("App", [&](const QString& val) { item.app = val; }, [&]() { item.app.clear(); });

    update("Hour", [&](const QString& val) { item.hour = val; }, [&]() { item.hour.clear(); });
    update("Minute", [&](const QString& val) { item.minute = val; }, [&]() { item.minute.clear(); });
    update("Second", [&](const QString& val) { item.second = val; }, [&]() { item.second.clear(); });




    //*************************************************************************
    // Int fields

    auto updateInt = [&](const QString& field, int& ref, std::function<void()> clearer) {
        QLineEdit* edit = findChild<QLineEdit*>("lineEdit" + field);
        QCheckBox* check = findChild<QCheckBox*>("checkBox" + field);
        if (edit && check) {
            if (check->isChecked()) {
                bool ok = false;
                int val = edit->text().toInt(&ok);
                if (ok) ref = val;
            } else {
                clearer();
            }
        }
    };

    updateInt("X", item.x, [&]() { item.x = -1; });

    updateInt("Y", item.y, [&]() { item.y = -1; });
    updateInt("W", item.w, [&]() { item.w = -1; });
    updateInt("H", item.h, [&]() { item.h = -1; });
    updateInt("Fontnum", item.fontnum, [&]() { item.fontnum = -1; });
    updateInt("Style", item.style, [&]() { item.style = -1; });
    updateInt("Turn", item.turn, [&]() { item.turn = -1; });
    updateInt("Frame", item.frame, [&]() { item.frame = -1; });
    updateInt("Time", item.time, [&]() { item.time = -1; });
    updateInt("Animabpp", item.animabpp, [&]() { item.animabpp = -1; });
    updateInt("Startangle", item.startangle, [&]() { item.startangle = -1; });
    updateInt("Endangle", item.endangle, [&]() { item.endangle = -1; });
    updateInt("Ringedge", item.ringedge, [&]() { item.ringedge = -1; });
    updateInt("Metricinch", item.metricinch, [&]() { item.metricinch = -1; });

    updateInt("Hourcenterx", item.hourcenterx, [&]() { item.hourcenterx = -1; });
    updateInt("Hourcentery", item.hourcentery, [&]() { item.hourcentery = -1; });
    updateInt("Houranchorx", item.houranchorx, [&]() { item.houranchorx = -1; });
    updateInt("Houranchory", item.houranchory, [&]() { item.houranchory = -1; });

    updateInt("Mincenterx", item.mincenterx, [&]() { item.mincenterx = -1; });
    updateInt("Mincentery", item.mincentery, [&]() { item.mincentery = -1; });
    updateInt("Minanchorx", item.minanchorx, [&]() { item.minanchorx = -1; });
    updateInt("Minanchory", item.minanchory, [&]() { item.minanchory = -1; });

    updateInt("Seccenterx", item.seccenterx, [&]() { item.seccenterx = -1; });
    updateInt("Seccentery", item.seccentery, [&]() { item.seccentery = -1; });
    updateInt("Secanchorx", item.secanchorx, [&]() { item.secanchorx = -1; });
    updateInt("Secanchory", item.secanchory, [&]() { item.secanchory = -1; });


    //**************************      SAUVER SI ON VEUT      **************************************************
    on_pushButtonSauver_clicked();  // Sauvegarde
    //*********************************************************************************************************

    // 🔄 Rafraîchir uniquement le widget modifié
    //qDebug() << "🔄 Rafraîchissement du widget" << currentItemIndex;

    // Méthode simple : on redessine tout
    displayAllWidget();

    // Resélectionner le widget
    if (currentItemIndex >= 0 && currentItemIndex < jwf.items.size()) {
        ui->currentTypeCombo->setCurrentIndex(currentItemIndex);
        updateHighlightForType(currentItemIndex);
    }

}


//*********************************************************************************************************
void MainWindow::restartApp()
//*********************************************************************************************************
{

    QString program = QCoreApplication::applicationFilePath();
    QStringList arguments = QCoreApplication::arguments();

    if (!arguments.contains("--restarted"))
        arguments << "--restarted";

    QProcess::startDetached(program, arguments);
    QCoreApplication::quit();
}

//*********************************************************************************************************
void MainWindow::on_pushButtonCharger_clicked()
//*********************************************************************************************************
{
    openJson();
}

//*********************************************************************************************************
void MainWindow::on_pushButtonSauver_clicked()
//*********************************************************************************************************
{
    if (jsonFilePath != "") {


        QString pathToSave = jsonFilePath;
        QStringList lines;

        QString enteteTexte = ui->textEditEntete->toPlainText().trimmed();
        QStringList lignesEntete = enteteTexte.split('\n', Qt::SkipEmptyParts);

        // Ajoute une virgule à chaque ligne sauf la dernière
        for (int i = 0; i < lignesEntete.size(); ++i) {
            lignesEntete[i] = lignesEntete[i].trimmed();
            if (i < lignesEntete.size() - 1 && !lignesEntete[i].endsWith(','))
                lignesEntete[i] += ",";
        }

        lines << "{";  // début du JSON complet
        lines.append(lignesEntete);  // les lignes de l'entête reformatées

        // ✅ Ajoute la virgule finale à la dernière ligne d’en-tête si elle n’y est pas
        if (!lines.last().trimmed().endsWith(',')) {
            lines.last() += ",";
        }
        lines << "  \"item\": [";  // début du tableau des items


        for (int i = 0; i < jwf.items.size(); ++i) {
            const JsonItem& item = jwf.items[i];
            QStringList obj;
            obj << "    {";

            auto add = [&](const QString& key, const QVariant& value, bool forceQuote = false) {
                if (value.typeId() == QMetaType::QString && value.toString().isEmpty())
                    return;
                if (value.typeId() == QMetaType::Int && value.toInt() == -1)
                    return;

                QString valStr = forceQuote || value.typeId() == QMetaType::QString
                                     ? "\"" + value.toString() + "\""
                                     : QString::number(value.toInt());

                obj << QString("      \"%1\": %2,").arg(key, valStr);
            };


            add("widget", item.widget, true);
            add("type", item.type, true);
            add("x", item.x);
            add("y", item.y);
            add("w", item.w);
            add("h", item.h);
            add("bg", item.bg, true);
            add("bgcolor", item.bgcolor, true);
            add("fgcolor", item.fgcolor, true);
            add("align", item.align, true);
            add("font", item.font, true);
            add("fontnum", item.fontnum);
            add("style", item.style);
            add("fgrender", item.fgrender, true);
            add("bgrender", item.bgrender, true);
            add("animaicon", item.animaicon, true);
            add("turn", item.turn);
            add("frame", item.frame);
            add("time", item.time);
            add("animabpp", item.animabpp);
            add("animatype", item.animatype, true);
            add("animaformat", item.animaformat, true);
            add("progress", item.progress, true);
            add("startangle", item.startangle);
            add("endangle", item.endangle);
            add("ringedge", item.ringedge);
            add("app", item.app, true);
            add("metricinch", item.metricinch);

            add("hour", item.hour, true);
            add("minute", item.minute, true);
            add("second", item.second, true);

            add("hourcenterx", item.hourcenterx);
            add("hourcentery", item.hourcentery);
            add("houranchorx", item.houranchorx);
            add("houranchory", item.houranchory);

            add("mincenterx", item.mincenterx);
            add("mincentery", item.mincentery);
            add("minanchorx", item.minanchorx);
            add("minanchory", item.minanchory);

            add("seccenterx", item.seccenterx);
            add("seccentery", item.seccentery);
            add("secanchorx", item.secanchorx);
            add("secanchory", item.secanchory);


            // Supprime la virgule finale s'il y a au moins un champ
            if (!obj.isEmpty() && obj.last().trimmed().endsWith(","))
                obj.last() = obj.last().chopped(1);  // remove last comma

            obj << "    }" + QString(i == jwf.items.size() - 1 ? "" : ","); // virgule entre les objets
            lines << obj.join("\n");
        }

        lines << "  ]";
        lines << "}";

        QString result = lines.join("\n");


        QFile file(pathToSave);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Erreur", "Impossible d’écrire dans le fichier JSON.");
            return;
        }

        QTextStream out(&file);
        out << result;
        file.close();

        // -------- Sauvegarde du fichier font.json --------
        QString fullFontText = ui->textEditFont->toPlainText();
        QString fontPathToSave = fontFilePath;
        if (fullFontText != "") {
            if (fontPathToSave.isEmpty()) {
                fontPathToSave = QFileDialog::getSaveFileName(
                    this,
                    "Sauver le fichier font.json",
                    "",
                    "Fichiers JSON (*.json)"
                    );
                if (fontPathToSave.isEmpty()) return;
            }

            QFile file1(fontPathToSave);
            if (!file1.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Erreur", "Impossible d’écrire dans le fichier font.json.");
                return;
            }

            QTextStream out1(&file1);
            out1 << fullFontText;
            file1.close();
        }

        //*************************************************************************************************
        //qDebug() << "═══════════════════════════════════════════";
        //qDebug() << "🔍 DÉBUT RAFRAÎCHISSEMENT";
        //qDebug() << "═══════════════════════════════════════════";

        if (jsonCharge) {
            //qDebug() << "1️⃣ jsonCharge = true, début du processus";
            //qDebug() << "📁 jsonFilePath =" << jsonFilePath;
            //qDebug() << "📁 absolutePath =" << QFileInfo(jsonFilePath).absolutePath();

            // 📝 Recharger le JSON depuis le fichier qu'on vient de sauvegarder
            //qDebug() << "2️⃣ Appel de jwf.read()...";

            bool readSuccess = false;
            try {
                readSuccess = jwf.read(jsonFilePath, QFileInfo(jsonFilePath).absolutePath());
                //qDebug() << "3️⃣ jwf.read() terminé, résultat =" << readSuccess;
            } catch (const std::exception& e) {
                qDebug() << "❌ EXCEPTION dans jwf.read():" << e.what();
                QMessageBox::critical(this, "Erreur", QString("Exception lors de la lecture: %1").arg(e.what()));
                return;
            } catch (...) {
                qDebug() << "❌ EXCEPTION INCONNUE dans jwf.read()";
                QMessageBox::critical(this, "Erreur", "Exception inconnue lors de la lecture du JSON");
                return;
            }

            if (readSuccess) {
                //qDebug() << "4️⃣ JSON rechargé avec succès";
                //qDebug() << "📊 Nombre de widgets =" << jwf.items.size();
                //qDebug() << "📍 currentItemIndex =" << currentItemIndex;

                // 🎨 Rafraîchir l'affichage
                //qDebug() << "5️⃣ Appel de displayAllWidget()...";

                try {
                    displayAllWidget();
                    //qDebug() << "6️⃣ displayAllWidget() terminé";
                } catch (const std::exception& e) {
                    qDebug() << "❌ EXCEPTION dans displayAllWidget():" << e.what();
                    QMessageBox::critical(this, "Erreur", QString("Exception lors de l'affichage: %1").arg(e.what()));
                    return;
                } catch (...) {
                    qDebug() << "❌ EXCEPTION INCONNUE dans displayAllWidget()";
                    QMessageBox::critical(this, "Erreur", "Exception inconnue lors de l'affichage");
                    return;
                }

                // 🎯 Resélectionner le widget courant
                //qDebug() << "7️⃣ Resélection du widget...";
                if (currentItemIndex >= 0 && currentItemIndex < jwf.items.size()) {
                    //qDebug() << "8️⃣ Index valide, mise à jour de la combo...";

                    try {
                        ui->currentTypeCombo->setCurrentIndex(currentItemIndex);
                        //qDebug() << "9️⃣ Appel de updateHighlightForType()...";

                        updateHighlightForType(currentItemIndex);
                        //qDebug() << "🔟 updateHighlightForType() terminé";
                    } catch (const std::exception& e) {
                        qDebug() << "❌ EXCEPTION dans resélection:" << e.what();
                        QMessageBox::critical(this, "Erreur", QString("Exception lors de la resélection: %1").arg(e.what()));
                        return;
                    } catch (...) {
                        qDebug() << "❌ EXCEPTION INCONNUE dans resélection";
                        QMessageBox::critical(this, "Erreur", "Exception inconnue lors de la resélection");
                        return;
                    }
                } else {
                    //qDebug() << "⚠️ Index invalide, pas de resélection";
                }

                //qDebug() << "✅ SUCCÈS - Affichage du message";
                //QMessageBox::information(this, "Sauvegarde", "✅ Fichier sauvegardé et vue rafraîchie !");
                //qDebug() << "═══════════════════════════════════════════";
                //qDebug() << "✅ FIN RAFRAÎCHISSEMENT RÉUSSI";
                //qDebug() << "═══════════════════════════════════════════";
            } else {
                qDebug() << "❌ Échec du rechargement du JSON";
                QMessageBox::warning(this, "Erreur", "Sauvegarde OK mais impossible de recharger le JSON.");
            }
            return;
        }

        //qDebug() << "ℹ️ jsonCharge = false, pas de rafraîchissement";
    }
}

//*********************************************************************************************************
void MainWindow::populateWidgetComboAdd()
//*********************************************************************************************************
{

    QStringList widgets = {
        "custom", "watch", "ring", "progressbar"
    };

    ui->widgetComboAdd->clear();
    ui->widgetComboAdd->addItems(widgets);


}
//*********************************************************************************************************
void MainWindow::populateTypeComboAdd()
//*********************************************************************************************************
{

    QString selectedWidget = ui->widgetComboAdd->currentText();

    QStringList widgets = {
        "custom", "watch", "ring", "progressbar"
    };

    QStringList types;//; = {
    //     "date", "time", "hour", "min", "second", "week", "day", "month",
    //     "calorie", "distance", "heartrate", "redpoint", "battery",
    //     "step", "icon", "sleep", "bluetooth", "progressbar", "ring", "apm", "shortcut"
    // };

    if(selectedWidget == "custom"){
        types = {
            "date", "time", "hour", "min", "second", "week", "day", "month",
            "calorie", "distance", "heartrate", "redpoint", "battery",
            "step", "icon", "sleep", "bluetooth", "progressbar", "ring", "apm", "shortcut", "anima"
        };
    } else if(selectedWidget == "watch"){
        types = {
            "time"
        };
    } else if(selectedWidget == "ring"){
        types = {
            "battery", "calorie", "distance", "heartrate"
        };
    } else if(selectedWidget == "progressbar"){
        types = {
            "battery", "calorie", "distance", "heartrate"
        };
    }
    ui->typeComboAdd->clear();
    ui->typeComboAdd->addItems(types);
}

//*********************************************************************************************************
void MainWindow::on_pushButtonAjouterWidget_clicked()
//*********************************************************************************************************
{
    QString selectedWidget = ui->widgetComboAdd->currentText();
    QString selectedType = ui->typeComboAdd->currentText();
    if (selectedType.isEmpty()) {
        QMessageBox::warning(this, "Erreur", "Aucun type sélectionné.");
        return;
    }

    // Demande confirmation
    int ret = QMessageBox::question(this, "Confirmation",
                                    "Ajouter l'élément sélectionné ?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    QSettings settings("KsixApp", "CadranEditor");
    QString currentDialFolder = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(currentDialFolder);

    // Création d'un nouvel item avec des valeurs par défaut
    JsonItem item;
    item.widget = selectedWidget;
    item.type = selectedType;
    item.x = 70;
    item.y = 95;
    item.w = 100;
    item.h = 50;

    item.align = "";
    item.font = "";
    item.fontnum = -1;

    // Les autres champs sont laissés vides ou à -1
    item.style = -1;
    item.fgcolor = "";
    item.bgcolor = "";
    item.fgrender = "";
    item.bgrender = "";
    item.bg = "";
    item.animaicon = "";
    item.turn = -1;
    item.frame = -1;
    item.time = -1;
    item.animatype = "";
    item.progress = "";
    item.startangle = -1;
    item.endangle = -1;
    item.ringedge = -1;
    item.app = "";
    item.metricinch = -1;
    item.animabpp = -1;

    item.hour = "";
    item.minute = "";
    item.second = "";

    item.hourcenterx = -1;
    item.hourcentery = -1;
    item.houranchorx = -1;
    item.houranchory = -1;

    item.mincenterx = -1;
    item.mincentery = -1;
    item.minanchorx = -1;
    item.minanchory = -1;

    item.seccenterx = -1;
    item.seccentery = -1;
    item.secanchorx = -1;
    item.secanchory = -1;

    if(selectedWidget == "watch"){
        item.x = 0;
        item.y = 0;
        item.w = 240;
        item.h = 240;


        item.hour = "h.png";
        item.minute = "m.png";
        item.second = "s.png";

        item.hourcenterx = 10;
        item.hourcentery = 114;
        item.houranchorx = 120;
        item.houranchory = 120;

        item.mincenterx = 10;
        item.mincentery = 114;
        item.minanchorx = 120;
        item.minanchory = 120;

        item.seccenterx = 10;
        item.seccentery = 114;
        item.secanchorx = 120;
        item.secanchory = 120;
        // 2) Copier physiquement h/m/s.png
        // >>> chemins source

        QString exeDir = QCoreApplication::applicationDirPath();
        QString srcBase = exeDir + "/assets_template/icone";
        QString srcH = srcBase + "/h.png";
        QString srcM = srcBase + "/m.png";
        QString srcS = srcBase + "/s.png";

        // >>> dossier du cadran courant (doit être défini quelque part dans ta classe)
        // par ex: /.../cadrans/sp31/images

        QString dstBase = currentDialFolder;

        if (!copyFileForce(srcH, dstBase + "/h.png")) {
            qWarning() << "copie h.png échouée";
        }
        if (!copyFileForce(srcM, dstBase + "/m.png")) {
            qWarning() << "copie m.png échouée";
        }
        if (!copyFileForce(srcS, dstBase + "/s.png")) {
            qWarning() << "copie s.png échouée";
        }

    } else if(selectedWidget == "ring"){
        // "widget": "ring",
        //"x": 14,
        //"y": 71,
        //"w": 58,
        //"h": 58,
        //"bgcolor": "0xFFA4A4A4",
        //"bgrender": "0xFF000000",
        //"type": "calorie",
        //"startangle": 540,
        //"endangle": 270,
        //"bg": "round_58x58_4bit.bmp",
        //"ringedge": 0
        item.bg = "ring.png";
        item.startangle = 0;
        item.endangle = 90;
        item.ringedge = 0;


    } else if( selectedWidget == "progressbar"){
        item.bg = "ic.png";
        item.progress = "ic.png";

    } else if( selectedWidget == "custom"){
        if( selectedType == "bluetooth"){

            item.animaicon = "anima";
            item.turn = 0;
            item.frame = 3;
            item.time = 800;
            item.animatype = "return";
        }else if( selectedType == "anima"){

            item.time = 3000;
            item.turn = 0;
            item.animatype = "normal";
            item.animaicon = "anima";
            item.frame = 26;
            item.animabpp = 16;
            item.animaformat = "png";


        }else if( selectedType == "icon"){
            item.x = 95;
            item.y = 95;
            item.w = 50;
            item.h = 50;
            item.bg = "icon_steps.png";
            QString exeDir = QCoreApplication::applicationDirPath();
            QString srcH = exeDir + "/assets_template/icone/icon_steps.png";

            QString dstBase = currentDialFolder;

            if (!copyFileForce(srcH, dstBase + "/icon_steps.png")) {
                qWarning() << "copie h.png échouée";
            }
        }else if( selectedType == "week"){

            item.align = "center";
            item.font = "week";
            item.fontnum = 7;

            // >>> copier tout le pack week
            // source : /assets_template/week/
            // destination : currentCadranPath + "/week/"
            copyWeekFolder();



        }else{
            item.align = "center";
            item.font = "digits";
            item.fontnum = 11;


            // >>> copier tout le pack digits
            // source : /assets_template/digits/
            // destination : currentCadranPath + "/digits/"
            copyDigitsFolder();

        }



    }

    // Ajoute le nouvel item à la liste
    jwf.items.append(item);

    displayAllWidget();

    // 🔧 Mise à jour de la comboBox de manière sécurisée
    //int newIndex = ui->currentTypeCombo->count();
    ui->currentTypeCombo->addItem(item.type);

    //QString widget = item.widget;

    // Vérifie si le widget est déjà présent dans la combo
    int existingIndex = ui->currentWidgetCombo->findText(selectedWidget);
    if (existingIndex == -1) {
        int newIndex = ui->currentWidgetCombo->count();
        ui->currentWidgetCombo->addItem(selectedWidget);
        ui->currentWidgetCombo->setCurrentIndex(newIndex);
        oncurrentWidgetComboChanged(newIndex);  // Mise à jour liée à ce widget
    } else {
        ui->currentWidgetCombo->setCurrentIndex(existingIndex);
        oncurrentWidgetComboChanged(existingIndex);
    }
    on_pushButtonAppliquer_clicked();


}


//*********************************************************************************************************
void MainWindow::on_pushButtonQuitter_clicked()
//*********************************************************************************************************
{

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "",
        "Quitter",
        QMessageBox::Yes | QMessageBox::No
        );
    if (reply != QMessageBox::Yes)
        return;
    close();
}

//*********************************************************************************************************
void MainWindow::envoyerCadranVersTelVeryfit()
//*********************************************************************************************************
{

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "",
        "Envoyer à la montre?",
        QMessageBox::Yes | QMessageBox::No
        );
    if (reply != QMessageBox::Yes)
        return;


    QString zipFilePathTempo = dossierTempo + "/cw1.zip";
    QString adbPath = detectAdbPath();

    if (adbPath.isEmpty()) {
        QMessageBox::critical(this, "Erreur", "ADB introuvable.");
        return;
    }

    // 1. Créer le zip
    if (!zipFolder(dossierAZipper, zipFilePathTempo)) {
        // qDebug() << "❌ Erreur lors de la création du zip.";
        QMessageBox::critical(this, "Erreur", "Échec lors de la création du fichier ZIP.");
        return;
    }

    // 2. Supprimer les anciens fichiers
    QString deleteCmd = QString("'%1' shell su -c 'rm -rf "
                                "/data/data/com.watch.life/files/Veryfit/dial/7733/cw1.iwf "
                                "/data/data/com.watch.life/files/Veryfit/dial/7733/cw1 "
                                "/data/data/com.watch.life/files/Veryfit/dial/7733/watchFileTemp'")
                            .arg(adbPath);
    int deleteResult = QProcess::execute("/bin/bash", QStringList() << "-c" << deleteCmd);
    if (deleteResult == 0){
        ajouterLogStatus(QString("Suppression : OK"));
    }else{
        ajouterLogStatus(QString("Suppression : Erreur exit code %1").arg(deleteResult));
    }

    // 3. Push
    QString adbPushCmd = QString("'%1' push \"%2\" /sdcard/").arg(adbPath, zipFilePathTempo);
    int pushResult = QProcess::execute("/bin/bash", QStringList() << "-c" << adbPushCmd);
    if (pushResult == 0){
        ajouterLogStatus(QString("Push : OK"));
    }else{
        ajouterLogStatus(QString("Push : Erreur exit code %1").arg(pushResult));
    }

    // 4. Copie interne
    QString adbCopyCmd = QString("'%1' shell su -c 'cp /sdcard/cw1.zip /data/data/com.watch.life/files/Veryfit/dial/7733/'").arg(adbPath);
    int cpResult = QProcess::execute("/bin/bash", QStringList() << "-c" << adbCopyCmd);
    if (cpResult == 0){
        ajouterLogStatus(QString("Copie : OK"));
    }else{
        ajouterLogStatus(QString("Copie : Erreur exit code %1").arg(cpResult));
    }

    QFile::remove(zipFilePathTempo);  // nettoyage

    // ✅ Affichage du message final
    if (deleteResult != 0 || pushResult != 0 || cpResult != 0) {
        QString message = "Échec lors de l’envoi :\n";
        if (deleteResult != 0)
            message += "• Erreur lors de suppression des iwf.\n";
        if (pushResult != 0)
            message += "• Erreur lors du push vers /sdcard.\n";
        if (cpResult != 0)
            message += "• Erreur lors de la copie interne (shell cp).\n";
        QMessageBox::critical(this, "Erreur d'envoi", message.trimmed());
    }


    //qDebug() << "✅ Fin de l'envoi (push:" << pushResult << ", cp:" << cpResult << ")";
}

//*********************************************************************************************************
QString MainWindow::detectAdbPath()
//*********************************************************************************************************
{
    QProcess process;
    process.start("which", QStringList() << "adb");
    process.waitForFinished();
    QString adb = QString::fromUtf8(process.readAllStandardOutput()).trimmed();

    if (adb.isEmpty()) {
        QMessageBox::critical(this, "Erreur ADB", "Impossible de trouver la commande adb. Vérifiez qu’elle est installée et dans le PATH.");
        // } else {
        //     qDebug() << "✅ adb détecté :" << adb;
    }

    return adb;
}

//*********************************************************************************************************
bool MainWindow::zipFolder(const QString& folderPath, const QString& zipPath)
//*********************************************************************************************************
{
    QDir dir(folderPath);
    if (!dir.exists()) {
        //qDebug() << "❌ Le dossier à zipper n'existe pas:" << folderPath;
        return false;
    }

    QString cmd = QString("zip -r \"%1\" .").arg(zipPath);
    QProcess process;
    process.setWorkingDirectory(folderPath);
    process.start("/bin/bash", QStringList() << "-c" << cmd);
    process.waitForFinished();

    // QString stdout = process.readAllStandardOutput();
    // QString stderr = process.readAllStandardError();
    int exitCode = process.exitCode();

    // qDebug() << "📦 zip stdout:" << stdout;
    // qDebug() << "📦 zip stderr:" << stderr;
    // qDebug() << "📦 zip exit code:" << exitCode;

    return exitCode == 0;
}


//*********************************************************************************************************
void MainWindow::on_pushButtonEnvoyer_clicked()
//*********************************************************************************************************
{
    envoyerCadranVersTelVeryfit();

}

//*********************************************************************************************************
void MainWindow::on_pushButtonChargeIwfLz_clicked()
//*********************************************************************************************************
{
    // 🔍 1. Extraire le nom du cadran depuis le champ textEditEntete
    QString enteteTexte = ui->textEditEntete->toPlainText().trimmed();
    QStringList lignesEntete = enteteTexte.split('\n', Qt::SkipEmptyParts);
    QString cadranName;


    QRegularExpression re("\"name\"\\s*:\\s*\"([^\"]+)\"");

    for (QString ligne : lignesEntete) {
        ligne = ligne.trimmed();
        QRegularExpressionMatch match = re.match(ligne);
        if (match.hasMatch()) {
            cadranName = match.captured(1);  // extrait "sp2", "cw1", etc.
            break;
        }
    }

    if (cadranName.isEmpty()) {
        QMessageBox::warning(this, "Nom introuvable", "Le nom du cadran n’a pas été trouvé dans l’entête.");
        return;
    }

    // 🛠️ Préparation
    QString adbPath = detectAdbPath();
    QString tmpPath = dossierTempo + "/ido_watch_plate_data.iwf.lz";
    QString finalPath = dossierTempo + "/" + cadranName + ".iwf.lz";

    if (adbPath.isEmpty()) {
        QMessageBox::critical(this, "Erreur", "ADB introuvable.");
        return;
    }

    // 2. Copie interne depuis la montre vers /sdcard/
    QString copyCmd = QString("'%1' shell su -c 'cp "
                              "/data/data/com.watch.life/files/Veryfit/dial/7733/watchFileTemp/ido_watch_plate_data.iwf.lz "
                              "/sdcard/ido_watch_plate_data.iwf.lz'")
                          .arg(adbPath);
    int copyResult = QProcess::execute("/bin/bash", QStringList() << "-c" << copyCmd);
    ajouterLogStatus(QString("📥 Copie vers /sdcard : exit code %1").arg(copyResult));

    if (copyResult != 0) {
        QMessageBox::critical(this, "Erreur", "Échec de la copie vers /sdcard/");
        return;
    }

    // 3. Télécharger depuis /sdcard/ vers le PC (fichier temporaire)
    QString pullCmd = QString("'%1' pull /sdcard/ido_watch_plate_data.iwf.lz \"%2\"")
                          .arg(adbPath, tmpPath);
    int pullResult = QProcess::execute("/bin/bash", QStringList() << "-c" << pullCmd);
    ajouterLogStatus(QString("📤 Pull vers PC : exit code %1").arg(pullResult));

    if (pullResult == 0) {
        QFile::remove(finalPath);  // Nettoyage si déjà présent
        QFile::rename(tmpPath, finalPath);  // Renommage

        // ✅ Message facultatif
        // QMessageBox::information(this, "Succès", "✅ Fichier téléchargé et renommé.");
    } else {
        QMessageBox::critical(this, "Erreur", "❌ Échec du téléchargement depuis /sdcard/.");
    }

    // qDebug() << "📁 Fichier copié vers:" << finalPath;
}


//*********************************************************************************************************
void MainWindow::ajouterLogStatus(const QString& message)
//*********************************************************************************************************
{
    ui->plainTextEditLog->appendPlainText(message);
}

//*********************************************************************************************************
void MainWindow::on_pushButtonSupprimerWidget_clicked()
//*********************************************************************************************************
{

    if (currentItemIndex < 0 || currentItemIndex >= jwf.items.size()) {
        QMessageBox::warning(this, "Erreur", "Aucun élément sélectionné à supprimer.");
        return;
    }

    // Demande confirmation
    int ret = QMessageBox::question(this, "Confirmation",
                                    "Supprimer l'élément sélectionné ?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    // 🔥 Supprime l'élément de jwf.items
    jwf.items.removeAt(currentItemIndex);

    // 🔄 Sauvegarder le JSON DIRECTEMENT (sans passer par l'interface)
    on_pushButtonSauver_clicked();

    // 🧼 Réinitialise l'index courant
    if (jwf.items.isEmpty()) {
        currentItemIndex = -1;
    } else {
        currentItemIndex = qMin(currentItemIndex, jwf.items.size() - 1);
    }

    // 🔄 Recharger tout depuis le fichier JSON sauvegardé
    if (!jwf.items.isEmpty() && !jsonFilePath.isEmpty()) {
        ouvrirJson(jsonFilePath);  // ✅ Recharge proprement tout
    } else {
        // Si plus aucun widget, juste rafraîchir l'affichage
        displayAllWidget();
    }
}

//*********************************************************************************************************
void MainWindow::ouvrirUnTxt()
//*********************************************************************************************************
{
    QSettings settings("KsixApp", "CadranEditor");
    QString dossier = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dossier);
    QString data;

    QString selected = tr("Fichiers TXT, JSON (*.json)");
    const QString fichier = QFileDialog::getOpenFileName(
        this,
        tr("Ouvrir un fichier"),
        dossier,
        tr("Fichiers JSON (*.json);;"
           "Fichiers texte (*.txt);;"
           "Tous les fichiers (*)"),
        &selected
        );

    if (!fichier.isEmpty()) {
        //qDebug << "Fichier sélectionné:" << fichier;
        //qDebug << "Filtre utilisé:" << selected;
    }

    QFile file(fichier);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray content = file.readAll();
        data = QString::fromUtf8(content);
        ouvrirEditDialog(data, fichier);
        // } else {
        //     QMessageBox::warning(this, "Erreur", "Impossible d’ouvrir le fichier.");
    }



}

//*********************************************************************************************************
void MainWindow::ouvrirEditDialog(QString data, QString fileName)
//*********************************************************************************************************
{
    editDialog dlg(data, fileName, this);
    dlg.exec();
}

//*********************************************************************************************************
void MainWindow::on_pushButtonCreateIwfLz_clicked()
//*********************************************************************************************************
{
    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dernierDossier);

    // 1. Sélection du dossier source (là où il y a iwf.json, font.json, num_hour_*, etc.)
    const QString folderPath = QFileDialog::getExistingDirectory(
        this,
        tr("Select Watchface Folder"),
        dernierDossier
        );
    if (folderPath.isEmpty())
        return;

    // Sauvegarder ce dossier pour la prochaine fois
    settings.setValue("dernierDossier", folderPath);

    // 2. Récupérer le nom du dossier (ex: "sp99")
    QDir sourceDir(folderPath);
    QString folderName = sourceDir.dirName();

    QString outputPath = folderPath + "/" + folderName + ".iwf";

    bool ok = createIwfFromFolder(sourceDir, outputPath);
    if (!ok) {
        QMessageBox::warning(
            this,
            tr("IWF creation failed"),
            tr("La création du fichier IWF a échoué.")
            );
        return;
    }

    // 8. Création du .iwf.lz
    ok = createIwfLz(outputPath);
    if (!ok) {
        QMessageBox::warning(
            this,
            tr("IWF creation failed"),
            tr("La création du fichier IWF a échoué.")
            );
        return;
    }


    QMessageBox::information(this, "Succès", "Fichiers .iwf, .iwf.lz créés");

    // 9. Mémoriser le dossier tempo
    //settings.setValue("DossierTempo", dossierTempo);

    // 10. Message de confirmation
    ui->plainTextEditLog->appendPlainText(
        QString("✅ Fichiers créés :\n  - %1\n  - %2.lz")
            .arg(outputPath)
            .arg(outputPath)
        );
}

// Fonction helper pour copier un dossier récursivement
//*********************************************************************************************************
bool MainWindow::copyDirectoryRecursively(const QString &srcPath, const QString &destPath)
//*********************************************************************************************************
{
    QDir srcDir(srcPath);
    if (!srcDir.exists())
        return false;

    QDir destDir(destPath);
    if (!destDir.exists()) {
        if (!destDir.mkpath("."))
            return false;
    }

    QStringList files = srcDir.entryList(QDir::Files);
    for (const QString &file : files) {
        QString srcFile = srcPath + "/" + file;
        QString destFile = destPath + "/" + file;
        if (!QFile::copy(srcFile, destFile))
            return false;
    }

    QStringList dirs = srcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &dir : dirs) {
        QString srcSubDir = srcPath + "/" + dir;
        QString destSubDir = destPath + "/" + dir;
        if (!copyDirectoryRecursively(srcSubDir, destSubDir))
            return false;
    }

    return true;
}

//*********************************************************************************************************
bool MainWindow::createIwfLz(const QString& inPath)
//*********************************************************************************************************
{

    //QString outPath = QFileInfo(inPath).completeBaseName() + ".iwf.lz";
    //QString outPath = inPath + ".lz";

    QString err;
    if (!IwfLzCompress::compressFile(inPath, &err)) {
        QMessageBox::critical(this, tr("Compression .iwf.lz"), tr("Échec : %1").arg(err));
        return false;
    }
    return true;


}


//*********************************************************************************************************
void MainWindow::on_pushButtonCreerPreview_clicked()
//*********************************************************************************************************
{
    if (!scene) {
        QMessageBox::warning(this, "Erreur", "Aucune scène à capturer.");
        return;
    }

    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dernierDossier);
    QString defaultPath = QDir(dernierDossier).filePath("preview.png");

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Enregistrer l'image preview",
        defaultPath,
        "Images PNG (*.png);;Images JPEG (*.jpg)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    // Sauvegarder l'état actuel du cadre rouge
    bool highlightWasVisible = false;
    if (highlightRect) {
        highlightWasVisible = highlightRect->isVisible();
        highlightRect->setVisible(false);
    }

    // Cacher temporairement TOUTES les lignes de la grille
    QList<QGraphicsItem*> allItems = scene->items();
    QList<QGraphicsLineItem*> gridLines;

    for (QGraphicsItem* item : allItems) {
        QGraphicsLineItem* lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item);
        if (lineItem) {
            gridLines.append(lineItem);
            lineItem->setVisible(false);
        }
    }

    // Sauvegarder et désactiver le background
    QBrush oldBrush = scene->backgroundBrush();
    scene->setBackgroundBrush(Qt::transparent);

    // Capturer la scène 400x400
    QImage imageTemp(400, 400, QImage::Format_ARGB32);
    imageTemp.fill(Qt::transparent);

    QPainter painterTemp(&imageTemp);
    painterTemp.setRenderHint(QPainter::Antialiasing);
    painterTemp.setRenderHint(QPainter::SmoothPixmapTransform);
    scene->render(&painterTemp, QRectF(), scene->sceneRect());
    painterTemp.end();

    // Restaurer le background
    scene->setBackgroundBrush(oldBrush);

    // Restaurer les lignes de la grille
    for (QGraphicsLineItem* lineItem : gridLines) {
        lineItem->setVisible(true);
    }

    // Restaurer le cadre rouge
    if (highlightRect && highlightWasVisible) {
        highlightRect->setVisible(true);
    }

    // Redimensionner à 180x180
    QImage image180 = imageTemp.scaled(180, 180, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // ✅ AJOUTER un contour circulaire gris
    QPainter painterCircle(&image180);
    painterCircle.setRenderHint(QPainter::Antialiasing);

    // Dessiner le cercle gris (contour uniquement)
    QPen pen(QColor(128, 128, 128), 1);  // Gris, épaisseur 2 pixels
    painterCircle.setPen(pen);
    painterCircle.setBrush(Qt::NoBrush);  // Pas de remplissage
    painterCircle.drawEllipse(1, 1, 178, 178);  // Cercle légèrement plus petit pour centrer le trait

    painterCircle.end();

    // Sauvegarder l'image finale
    if (image180.save(fileName)) {
        QMessageBox::information(this, "Succès",
                                 QString("Preview 180x180 avec contour gris sauvegardée :\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "Erreur", "Échec de la sauvegarde.");
    }
}



//*********************************************************************************************************
void MainWindow::on_pushButtonCreatelaunchPicture_clicked()
//*********************************************************************************************************
{
    if (!scene) {
        QMessageBox::warning(this, "Erreur", "Aucune scène à capturer.");
        return;
    }

    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dernierDossier);
    QString nomDossier = QFileInfo(dernierDossier).fileName();

    QString defaultPath = QDir(dernierDossier).filePath(nomDossier + ".png");

    //QString defaultPath = dernierDossier + "/" + nomDossier + ".png";
    //defaultPath
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Enregistrer l'image launch",
        defaultPath,
        "Images PNG (*.png);;Images JPEG (*.jpg)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    // Sauvegarder l'état actuel du cadre rouge
    bool highlightWasVisible = false;
    if (highlightRect) {
        highlightWasVisible = highlightRect->isVisible();
        highlightRect->setVisible(false);
    }

    // Cacher temporairement TOUTES les lignes de la grille
    QList<QGraphicsItem*> allItems = scene->items();
    QList<QGraphicsLineItem*> gridLines;

    for (QGraphicsItem* item : allItems) {
        QGraphicsLineItem* lineItem = qgraphicsitem_cast<QGraphicsLineItem*>(item);
        if (lineItem) {
            gridLines.append(lineItem);
            lineItem->setVisible(false);
        }
    }

    // Sauvegarder et désactiver le background
    QBrush oldBrush = scene->backgroundBrush();
    scene->setBackgroundBrush(Qt::transparent);

    // Capturer la scène 400x400
    QImage imageTemp(400, 400, QImage::Format_ARGB32);
    imageTemp.fill(Qt::transparent);

    QPainter painterTemp(&imageTemp);
    painterTemp.setRenderHint(QPainter::Antialiasing);
    painterTemp.setRenderHint(QPainter::SmoothPixmapTransform);
    scene->render(&painterTemp, QRectF(), scene->sceneRect());
    painterTemp.end();

    // Restaurer le background
    scene->setBackgroundBrush(oldBrush);

    // Restaurer les lignes de la grille
    for (QGraphicsLineItem* lineItem : gridLines) {
        lineItem->setVisible(true);
    }

    // Restaurer le cadre rouge
    if (highlightRect && highlightWasVisible) {
        highlightRect->setVisible(true);
    }

    // Redimensionner à 320x320
    QImage image320 = imageTemp.scaled(320, 320, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // ✅ CRÉER IMAGE FINALE 320x320 avec masque circulaire et fond transparent
    QImage imageFinal(320, 320, QImage::Format_ARGB32);
    imageFinal.fill(Qt::transparent);  // ← CHANGÉ : Fond transparent au lieu de gris

    QPainter painterFinal(&imageFinal);
    painterFinal.setRenderHint(QPainter::Antialiasing);

    // Créer un masque circulaire pour le cadran
    QPainterPath circle;
    circle.addEllipse(0, 0, 320, 320);

    // Dessiner le cadran avec le masque circulaire
    painterFinal.setClipPath(circle);
    painterFinal.drawImage(0, 0, image320);

    painterFinal.end();

    // Sauvegarder l'image finale
    if (imageFinal.save(fileName)) {
        QMessageBox::information(this, "Succès",
                                 QString("Image 320x320 avec fond transparent sauvegardée :\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, "Erreur", "Échec de la sauvegarde.");
    }
}

//*********************************************************************************************************
void MainWindow::on_pushButtonNew_clicked()
//*********************************************************************************************************
{

    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();

    // 1. Définir le dossier courant (important pour QFileDialog)
    QDir::setCurrent(dernierDossier);

    // 2. Demander le nom du cadran
    bool ok;
    QString cadranName = QInputDialog::getText(
        this,
        "Nouveau cadran",
        "Nom du cadran:",
        QLineEdit::Normal,
        "spX",
        &ok
        );

    if (!ok || cadranName.isEmpty()) {
        return;
    }

    // 3. Choisir le dossier de destination
    QString dossierBase = QFileDialog::getExistingDirectory(
        this,
        "Choisir le dossier de destination"
        );

    if (dossierBase.isEmpty()) {
        return;
    }

    // 4. Créer le dossier du cadran
    QString dossierCadran = dossierBase + "/" + cadranName;

    // Demande confirmation
    int ret = QMessageBox::question(this, "Confirmation",
                                    dossierCadran,
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;
    QDir dir;
    if (!dir.mkpath(dossierCadran)) {
        QMessageBox::critical(this, "Erreur", "Impossible de créer le dossier " + dossierCadran);
        return;
    }
    // 5. Mémoriser le dossier pour la prochaine fois
    settings.setValue("dernierDossier", dossierCadran);


    //########################################   JSON   ####################################################
    // 8. Créer le fichier iwf.json
    QJsonObject root;
    root["version"] = 1;
    root["clouddialversion"] = 3;
    root["preview"] = "preview.png";
    root["name"] = cadranName;
    root["author"] = "sp";
    root["description"] = "spwatch";
    root["bkground"] = "fond.png";

    QJsonArray items;
    QJsonObject item;
    item["widget"] = "custom";
    item["type"] = "time";
    item["x"] = 20;
    item["y"] = 100;
    item["w"] = 200;
    item["h"] = 60;
    item["align"] = "center";
    item["font"] = "digits";
    item["fontnum"] = 11;
    items.append(item);

    root["item"] = items;

    QJsonDocument docIwf(root);
    QString iwfPath = dossierCadran + "/iwf.json";
    QFile fileIwf(iwfPath);
    if (!fileIwf.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Erreur", "Impossible de créer iwf.json");
        return;
    }
    fileIwf.write(docIwf.toJson(QJsonDocument::Indented));
    fileIwf.close();

    // 9. Créer le fichier font.json
    QJsonObject fontRoot;
    QJsonArray fontItems;
    QJsonObject fontItem;
    fontItem["name"] = "digits";
    fontItem["bpp"] = 16;
    fontItem["format"] = "png";
    fontItems.append(fontItem);
    fontRoot["item"] = fontItems;

    QJsonDocument docFont(fontRoot);
    QString fontPath = dossierCadran + "/font.json";
    QFile fileFont(fontPath);
    if (!fileFont.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Erreur", "Impossible de créer font.json");
        return;
    }
    fileFont.write(docFont.toJson(QJsonDocument::Indented));
    fileFont.close();


    // 11. Copier le dossier digits
    copyDigitsFolder();

    //######################################################################################################

    //on_pushButtonEditPicture_clicked();

    // 6. Demander à l'utilisateur de choisir une image de fond
    QString imageFile = QFileDialog::getOpenFileName(
        this,
        "Sélectionner l'image de fond",
        dossierCadran,
        "Images (*.png *.jpg *.bmp)"
        );

    if (imageFile.isEmpty()) {
        return; // L'utilisateur a annulé
    }


    //QString imagePath = "/chemin/vers/ton/image.png";

    DialogEditPicture dialog(imageFile, true, this);

    if (dialog.exec() == QDialog::Accepted) {

        // 7. Charger, redimensionner et sauvegarder l'image de fond
        QString fondDest = dossierCadran + "/fond.png";

        QPixmap originalImage(fondDest);
        if (originalImage.isNull()) {
            QMessageBox::critical(this, "Erreur", "Impossible de charger l'image");
            return;
        }

        // ✅ Redimensionner à 240x240


        // Option 3 : Garder proportions ET remplir tout (crop au centre)
        QPixmap resizedImage = originalImage.scaled(240, 240,
                                                    Qt::KeepAspectRatioByExpanding,
                                                    Qt::SmoothTransformation);


        // ✅ Sauvegarder l'image redimensionnée
        if (!resizedImage.save(fondDest, "PNG")) {
            QMessageBox::critical(this, "Erreur", "Impossible de sauvegarder l'image de fond");
            return;
        }
        // 10. Créer une preview (redimensionnée aussi)
        QString previewDest = dossierCadran + "/preview.png";

        // ✅ Redimensionner à 180x180 pour la preview
        QPixmap previewImage = originalImage.scaled(180, 180,
                                                    Qt::KeepAspectRatioByExpanding,
                                                    Qt::SmoothTransformation);
        // ✅ Sauvegarder l'image redimensionnée
        if (!previewImage.save(previewDest, "PNG")) {
            QMessageBox::critical(this, "Erreur", "Impossible de sauvegarder l'image preview");
            return;
        }


    }else{
        QMessageBox::critical(this, "Erreur", "Impossible de sauvegarder l'image de fond");
        return;
    }

    // 12. Charger le nouveau cadran comme si on ouvrait un fichier existant
    dossierCourant = dossierCadran;

    // Charger le iwf.json qu'on vient de créer
    jsonCharge = true;
    jsonFilePath = iwfPath;
    ouvrirJson(iwfPath);


    ui->plainTextEditLog->appendPlainText("✅ Nouveau cadran créé : " + dossierCadran);
    statusLabel->setText("Cadran: " + dossierCadran);
}

//*********************************************************************************************************
void MainWindow::on_checkBoxHourSystem_toggled(bool checked)
//*********************************************************************************************************
{
    if (checked) {
        ui->timeEdit->setEnabled(false);
        ui->dateEdit->setEnabled(false);
        //qDebug() << "Mode heure actuelle activé";
    } else {
        ui->timeEdit->setEnabled(true);
        ui->dateEdit->setEnabled(true);
        //qDebug() << "Mode heure fixe activé";
    }

    // Rafraîchir l'affichage
    MainWindow::displayAllWidget();
}

//*********************************************************************************************************
void MainWindow::on_timeEdit_timeChanged(const QTime &time)
//*********************************************************************************************************
{
    MainWindow::displayAllWidget();
}

//*********************************************************************************************************
void MainWindow::on_dateEdit_dateChanged(const QDate &date)
//*********************************************************************************************************
{
    MainWindow::displayAllWidget();
}
//*********************************************************************************************************
void MainWindow::on_pushButtonCreateSqlFromBin_clicked()
//*********************************************************************************************************
{
    QSettings settings("KsixApp", "CadranEditor");
    QString dernierDossier = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dernierDossier);

    // 1. Sélection du dossier source (là où il y a iwf.json, font.json, num_hour_*, etc.)
    const QString folderPath = QFileDialog::getExistingDirectory(
        this,
        tr("Select Watchface Folder"),
        dernierDossier
        );
    if (folderPath.isEmpty())
        return;

    // Sauvegarder ce dossier pour la prochaine fois
    settings.setValue("dernierDossier", folderPath);

    // 2. Récupérer le nom du dossier (ex: "sp99")
    QDir sourceDir(folderPath);
    QString folderName = sourceDir.dirName();


    QString lzFPathfile = folderPath + "/" + folderName + ".iwf.lz";
    if (!QFile::exists(lzFPathfile)) {
        QMessageBox::warning(this, tr("Erreur"),
                             tr("Le %1 n'existe pas...").arg(folderName + ".iwf.lz"));
        return;
    }

    bool ok;

    QString outputSql = folderPath + "/" + folderName + ".sql";
    QFile file(lzFPathfile);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray content = file.readAll();

        ok = create_sql::exportCompassTextSql(outputSql, folderName, content);
        if (!ok) {
            QMessageBox::warning(
                this,
                tr("SQL creation failed"),
                tr("La création du fichier SQL a échoué.")
                );
            return;
        }
    }


    QString folderPngName = folderPath + "/" + folderName + ".png";

    if (!QFile::exists(folderPngName)) {
        QMessageBox::warning(this, tr("Erreur"),
                             tr("Le %1 n'existe pas...").arg(folderName + ".png"));
        return;
    }
    QString outputSqlPng = folderPath + "/" + folderName + "_png.sql";
    ok = create_sql::exportCompassPngSql(outputSqlPng , folderName, folderPngName);
    if (!ok) {

        QMessageBox::warning(
            this,
            tr("_PNG.SQL creation failed"),
            tr("La création du fichier _PNG.SQL a échoué.")
            );
        return;
    }


    QMessageBox::information(this, "Succès", "Fichiers SQL créés");

    // 9. Mémoriser le dossier tempo
    //settings.setValue("DossierTempo", dossierTempo);

    // 10. Message de confirmation
    ui->plainTextEditLog->appendPlainText(
        QString("✅ Fichiers créés :\n  - %1\n  - %2")
            .arg(outputSql)
            .arg(outputSqlPng)
        );
}



//*********************************************************************************************************
void MainWindow::on_pushButtonCreateFontJson_clicked()
//*********************************************************************************************************
{
    QSettings settings("KsixApp", "CadranEditor");
    QString dossierCadran = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dossierCadran);
    QString fontPath = dossierCadran + "/font.json";

    if (QFile::exists(fontPath)) {
        QMessageBox::warning(this, tr("Erreur"),
                             tr("font.json existe déjà..."));
        return;
    }

    // Demander confirmation (optionnel)
    int ret = QMessageBox::question(this, "Confirmation",
                                    "Créer font.json ?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    // 1. Créer le fichier font.json
    QJsonObject fontRoot;
    QJsonArray fontItems;
    QJsonObject fontItem;
    fontItem["name"] = "digits";
    fontItem["bpp"] = 16;
    fontItem["format"] = "png";
    fontItems.append(fontItem);
    fontRoot["item"] = fontItems;

    QJsonDocument docFont(fontRoot);
    QFile fileFont(fontPath);
    if (!fileFont.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Erreur", "Impossible de créer font.json");
        return;
    }
    fileFont.write(docFont.toJson(QJsonDocument::Indented));
    fileFont.close();

    // ✅ Afficher le contenu dans l'interface
    QFile file(fontPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString contenu = QString::fromUtf8(file.readAll());
        ui->textEditFont->setPlainText(contenu);
        file.close();
    }

    // ✅ Message de succès
    QMessageBox::information(this, "Succès", "font.json créé !");
    ui->plainTextEditLog->appendPlainText("✅ font.json créé : " + fontPath);
}


//*********************************************************************************************************
void MainWindow::on_pushButtonDeleteFontJson_clicked()
//*********************************************************************************************************
{

    QSettings settings("KsixApp", "CadranEditor");
    QString dossierCadran = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(dossierCadran);
    QString fontPath = dossierCadran + "/font.json";

    if (!QFile::exists(fontPath)) {
        QMessageBox::warning(this, tr("Erreur"),
                             tr("font.json n'existe pas..."));
        return;
    }

    // Demander confirmation
    int ret = QMessageBox::question(this, "Confirmation",
                                    "Supprimer font.json ?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    // ✅ Effacer font.json
    if (QFile::remove(fontPath)) {
        QMessageBox::information(this, "Succès", "font.json supprimé !");
        ui->textEditFont->clear();  // Vider l'affichage
        ui->plainTextEditLog->appendPlainText("🗑️ font.json supprimé");
    } else {
        QMessageBox::critical(this, "Erreur", "Impossible de supprimer font.json");
    }
}

//*********************************************************************************************************
void MainWindow::on_pushButtonEditPicture_clicked()
//*********************************************************************************************************
{
    DialogEditPicture dialog("", false, this);

    dialog.exec();
}



//*********************************************************************************************************
void MainWindow::copyDigitsFolder()
//*********************************************************************************************************
{
    QSettings settings("KsixApp", "CadranEditor");
    QString currentDialFolder = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(currentDialFolder);

    QString exeDir = QCoreApplication::applicationDirPath();
    //QString srcBase = exeDir + "/assets_template/icone";
    QString srcDigitsDir = exeDir + "/assets_template/digits";

    QString dstDigitsDir = currentDialFolder + "/digits";

    if (!copyDirRecursive(srcDigitsDir, dstDigitsDir)) {
        qWarning() << "Copie du dossier digits échouée";
    }
}
//*********************************************************************************************************
void MainWindow::copyWeekFolder()
//*********************************************************************************************************
{
    QSettings settings("KsixApp", "CadranEditor");
    QString currentDialFolder = settings.value("dernierDossier", QDir::homePath()).toString();
    QDir::setCurrent(currentDialFolder);

    QString exeDir = QCoreApplication::applicationDirPath();
    //QString srcBase = exeDir + "/assets_template/icone";
    QString srcDigitsDir = exeDir + "/assets_template/week";

    QString dstDigitsDir = currentDialFolder + "/week";

    if (!copyDirRecursive(srcDigitsDir, dstDigitsDir)) {
        qWarning() << "Copie du dossier digits échouée";
    }
}

// //*********************************************************************************************************
// void MainWindow::displayEmptyScene()
// //*********************************************************************************************************
// {
//     // Nettoyer complètement la scène
//     scene->clear();

//     // Fond noir
//     QPixmap blackBg(240, 240);
//     blackBg.fill(Qt::black);
//     auto* bgItem = scene->addPixmap(blackBg);
//     bgItem->setZValue(-1);
//     bgItem->setPos(0, 0);
//     scene->setSceneRect(0, 0, 240, 240);

//     // Ajouter la grille
//     const int gridSize = 20;
//     const int sceneWidth = 240;
//     const int sceneHeight = 240;

//     QPen gridPen(Qt::gray);
//     gridPen.setStyle(Qt::DashLine);
//     gridPen.setWidth(1);
//     gridPen.setCosmetic(true);

//     for (int x = 0; x <= sceneWidth; x += gridSize) {
//         scene->addLine(x, 0, x, sceneHeight, gridPen);
//     }
//     for (int y = 0; y <= sceneHeight; y += gridSize) {
//         scene->addLine(0, y, sceneWidth, y, gridPen);
//     }

//     // Ajouter le cercle gris
//     QPen circlePen(QColor(128, 128, 128), 2);
//     circlePen.setCosmetic(true);
//     int circleX = (sceneWidth - 240) / 2;
//     int circleY = (sceneHeight - 240) / 2;
//     scene->addEllipse(circleX, circleY, 240, 240, circlePen, Qt::NoBrush);

//     // Masque noir circulaire
//     QPainterPath maskPath;
//     maskPath.addRect(0, 0, sceneWidth, sceneHeight);
//     maskPath.addEllipse(circleX, circleY, 240, 240);
//     maskPath.setFillRule(Qt::OddEvenFill);

//     QGraphicsPathItem* blackMask = new QGraphicsPathItem();
//     blackMask->setPath(maskPath);
//     blackMask->setBrush(QBrush(Qt::black));
//     blackMask->setPen(QPen(Qt::NoPen));
//     blackMask->setZValue(999);
//     scene->addItem(blackMask);

//     // Contour du cercle
//     QPen borderPen(QColor(100, 100, 100), 2);
//     borderPen.setCosmetic(true);
//     scene->addEllipse(circleX, circleY, 240, 240, borderPen, Qt::NoBrush)->setZValue(1000);

//     // ✅ SOLUTION : Calculer le scaling pour remplir le graphicsView
//     ui->graphicsView->resetTransform();

//     // Calculer le facteur de zoom automatiquement
//     QRectF sceneRect = scene->sceneRect();
//     QRectF viewRect = ui->graphicsView->viewport()->rect();

//     qreal scaleX = viewRect.width() / sceneRect.width();
//     qreal scaleY = viewRect.height() / sceneRect.height();
//     qreal scale = qMin(scaleX, scaleY);  // Garde les proportions

//     //ui->graphicsView->resetTransform();
//     ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
//     //ui->graphicsView->scale(scale, scale);
//     // Forcer la mise à jour
//     ui->graphicsView->viewport()->update();
//     scene->update();
// }

//*********************************************************************************************************
void MainWindow::displayEmptyScene()
//*********************************************************************************************************
{
    // Nettoyer complètement la scène
    scene->clear();

    // Fond noir
    QPixmap blackBg(240, 240);
    blackBg.fill(Qt::black);
    auto* bgItem = scene->addPixmap(blackBg);
    bgItem->setZValue(-1);
    bgItem->setPos(0, 0);
    scene->setSceneRect(0, 0, 240, 240);

    // Ajouter la grille
    const int gridSize = 20;
    const int sceneWidth = 240;
    const int sceneHeight = 240;

    QPen gridPen(Qt::white);
    gridPen.setStyle(Qt::DashLine);
    gridPen.setWidth(2);
    gridPen.setCosmetic(true);

    for (int x = 0; x <= sceneWidth; x += gridSize) {
        scene->addLine(x, 0, x, sceneHeight, gridPen);
    }
    for (int y = 0; y <= sceneHeight; y += gridSize) {
        scene->addLine(0, y, sceneWidth, y, gridPen);
    }

    // Ajouter le cercle gris
    QPen circlePen(QColor(128, 128, 128), 2);
    circlePen.setCosmetic(true);
    int circleX = (sceneWidth - 240) / 2;
    int circleY = (sceneHeight - 240) / 2;
    scene->addEllipse(circleX, circleY, 240, 240, circlePen, Qt::NoBrush);

    // Masque noir circulaire
    QPainterPath maskPath;
    maskPath.addRect(0, 0, sceneWidth, sceneHeight);
    maskPath.addEllipse(circleX, circleY, 240, 240);
    maskPath.setFillRule(Qt::OddEvenFill);

    QGraphicsPathItem* blackMask = new QGraphicsPathItem();
    blackMask->setPath(maskPath);
    blackMask->setBrush(QBrush(Qt::black));
    blackMask->setPen(QPen(Qt::NoPen));
    blackMask->setZValue(999);
    scene->addItem(blackMask);

    // Contour du cercle
    QPen borderPen(QColor(100, 100, 100), 2);
    borderPen.setCosmetic(true);
    scene->addEllipse(circleX, circleY, 240, 240, borderPen, Qt::NoBrush)->setZValue(1000);

    // // Adapter la vue
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView->setAlignment(Qt::AlignCenter);
    ui->graphicsView->viewport()->update();
    ui->graphicsView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio);
    scene->update();
}

// //pushButtonCreateFontJson

