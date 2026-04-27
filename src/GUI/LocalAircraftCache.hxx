// SPDX-FileCopyrightText: 2017 James Turner
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LOCALAIRCRAFTCACHE_HXX
#define LOCALAIRCRAFTCACHE_HXX

#include <memory>
#include <QObject>
#include <QPixmap>
#include <QDateTime>
#include <QUrl>
#include <QSharedPointer>
#include <QDir>
#include <QVariant>

#include <simgear/props/props.hxx>

class QDataStream;
struct AircraftItem;
class SGPropertyNode;

typedef QSharedPointer<AircraftItem> AircraftItemPtr;

struct AircraftItem
{
    AircraftItem() = default;

    bool initFromFile(QDir dir, QString filePath);

    // the file-name without -set.xml suffix
    QString baseName() const;

    QString name() const;

    QString description() const;

    void fromDataStream(QDataStream& ds);

    void toDataStream(QDataStream& ds) const;

    int indexOfVariant(QUrl uri) const;

    bool excluded = false;
    QString path;

    QString authors; // legacy authors data only
    int ratings[4] = {0, 0, 0, 0};
    QString variantOf;
    QDateTime pathModTime;
    QList<AircraftItemPtr> variants;
    bool usesHeliports = false;
    bool usesSeaports = false;
    QList<QUrl> previews;
    bool isPrimary = false;
    QString thumbnailPath;
    bool declaredCompatible = false;
    QStringList tags;
    bool needsMaintenance = false;
    QUrl homepageUrl;
    QUrl wikipediaUrl;
    QUrl supportUrl;
    QUrl communityUrl;
    QVariant status(int variant);


private:
    struct LocalizedStrings {
        QString locale;
        QMap<QByteArray, QString> strings;
    };

    friend QDataStream& operator<<(QDataStream&, const LocalizedStrings&);
    friend QDataStream& operator>>(QDataStream&, LocalizedStrings&);

    using LocalizedStringsVec = QVector<LocalizedStrings>;

    // store all localized strings. We need this to avoid rebuilding
    // the cache when switching languages.
    LocalizedStringsVec _localized;

    // the resolved values for our strings, based on QLocale
    // if we support dynamic switching of language, this would need to
    // be flushed and re-computed
    QMap<QByteArray, QString> _currentStrings;

    void doLocalizeStrings();

    void readLocalizedStrings(SGPropertyNode_ptr simNode);
};

class LocalAircraftCache : public QObject
{
    Q_OBJECT
public:
    ~LocalAircraftCache();

    static LocalAircraftCache* instance();
    static void reset();


    void setPaths(QStringList paths);
    QStringList paths() const;

    void scanDirs();


    /**
     * @helper to determine if a particular path is likely to contain
     * aircraft or not. Checks for -set.xml files one level down in the tree.
     *
     */
    static bool isCandidateAircraftPath(QString path);

    int itemCount() const;

    QVector<AircraftItemPtr> allItems() const;

    AircraftItemPtr itemAt(int index) const;

    AircraftItemPtr findItemWithUri(QUrl aircraftUri) const;
    int findIndexWithUri(QUrl aircraftUri) const;

    AircraftItemPtr primaryItemFor(AircraftItemPtr item) const;

    QVariant aircraftStatus(AircraftItemPtr item) const;

    enum AircraftStatus {
        AircraftOk = 0,
        AircraftUnmaintained,
        AircraftIncompatible
    };

    enum PackageStatus {
        PackageNotInstalled = 0,
        PackageInstalled,
        PackageUpdateAvailable,
        PackageQueued,
        PackageDownloading,
        NotPackaged,
        PackageInstallFailed
    };

    Q_ENUMS(PackageStatus)
    Q_ENUMS(AircraftStatus)

    // rating order is FDM, Systems, Cockpit, External model
    static int ratingFromProperties(SGPropertyNode* node, int ratingIndex);

    enum class ParseSetXMLResult {
        Ok,
        Failed,
        Retry  ///< aircraft scan in progress, try again later
    };
    /**
     * @brief readAircraftProperties - helper to parse a -set.xml, but with the correct
     * path setup (root, aircradft dirs, current aircraft dir)
     * @param path : full path to the -set.xml file
     * @param props : property node to be populated
     * @return status indication
     */
    ParseSetXMLResult readAircraftProperties(const SGPath& path, SGPropertyNode_ptr props);
signals:

    void scanStarted();
    void scanCompleted();

    void cleared();
    void addedItems(int count);
public slots:

private slots:
    void onScanResults();

    void onScanFinished();

private:
    explicit LocalAircraftCache();

    void abandonCurrentScan();

    class AircraftCachePrivate;
    std::unique_ptr<AircraftCachePrivate> d;
};

#endif // LOCALAIRCRAFTCACHE_HXX
