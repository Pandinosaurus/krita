class Document : QObject /NoDefaultCtors/
{
%TypeHeaderCode
#include "Document.h"
%End

%ConvertToSubClassCode
  // Need to add sub class conversion, otherwise a Document passed to a Signal is provided as a QObject instead of
  // a Document object
  // (not sure why, but explicit conversion here fix the problem)
  Document* mc = qobject_cast<Document*>(sipCpp);
  sipType = 0;
  if (mc) {
      sipType = sipType_Document;
  }
%End

    Document(const Document & __0);
public:
    bool operator==(const Document &other) const;
    bool operator!=(const Document &other) const;

public Q_SLOTS:
    QList<qreal> horizontalGuides() const;
    QList<qreal> verticalGuides() const;
    bool guidesVisible() const;
    bool guidesLocked() const;

    Document *clone() const /Factory/;
    Node * activeNode() const  /Factory/;
    void setActiveNode(Node*  value);
    QList<Node*> topLevelNodes() const  /Factory/;
    Node *nodeByName(const QString &node) const  /Factory/;
    Node *nodeByUniqueID(const QUuid &id) const  /Factory/;
    bool batchmode() const;
    void setBatchmode(bool value);
    QString colorDepth() const;
    QString colorModel() const;
    QString colorProfile() const;
    bool setColorProfile(const QString &colorProfile);
    bool setColorSpace(const QString &value, const QString &colorDepth, const QString &colorProfile);
    QColor backgroundColor();
    bool setBackgroundColor(const QColor &color);
    QString documentInfo() const;
    void setDocumentInfo(const QString &document);
    QString fileName() const;
    void setFileName(QString value);
    int height() const;
    void setHeight(int value);
    QString name() const;
    void setName(QString value);
    int resolution() const;
    void setResolution(int value);
    Node * rootNode() const  /Factory/;
    Selection * selection() const  /Factory/;
    void setSelection(Selection*  value);
    int width() const;
    void setWidth(int value);
    int xOffset() const;
    void setXOffset(int x);
    int yOffset() const;
    void setYOffset(int y);
    double xRes() const;
    void setXRes(double xRes) const;
    double yRes() const;
    void setYRes(double yRes) const;
    QByteArray pixelData(int x, int y, int w, int h) const;
    bool close();
    void crop(int x, int y, int w, int h);
    bool exportImage(const QString &filename, const InfoObject & exportConfiguration);
    void flatten();
    void resizeImage(int x, int y, int w, int h);
    void scaleImage(int w, int h, int xres, int yres, QString strategy);
    void rotateImage(double radians);
    void shearImage(double angleX, double angleY);
    bool save();
    bool saveAs(const QString & filename);
    Node *createNode(const QString & name, const QString & nodeType)  /Factory/;
    GroupLayer *createGroupLayer(const QString &name) /Factory/;
    CloneLayer *createCloneLayer(const QString &name, const Node *source) /Factory/;
    FilterLayer *createFilterLayer(const QString &name, Filter &filter, Selection &selection) /Factory/;
    FillLayer *createFillLayer(const QString &name, const QString filterName, InfoObject &configuration, Selection &selection) /Factory/;
    VectorLayer *createVectorLayer(const QString &name) /Factory/;
    FileLayer *createFileLayer(const QString &name, const QString fileName, const QString scalingMethod, const QString scalingFilter = "Bicubic") /Factory/;
    FilterMask *createFilterMask(const QString &name, Filter &filter, Selection &selection) /Factory/;
    FilterMask *createFilterMask(const QString &name, Filter &filter, const Node *selection_source) /Factory/;
    SelectionMask *createSelectionMask(const QString &name) /Factory/;
    TransparencyMask *createTransparencyMask(const QString &name) /Factory/;
    TransformMask *createTransformMask(const QString &name) /Factory/;
    ColorizeMask *createColorizeMask(const QString &name) /Factory/;
    QImage projection(int x = 0, int y = 0, int w = 0, int h = 0) const;
    QImage thumbnail(int w, int h) const;
    void lock();
    void unlock();
    void waitForDone();
    bool tryBarrierLock();
    void refreshProjection();
    void setHorizontalGuides(const QList<qreal> &lines);
    void setVerticalGuides(const QList<qreal> &lines);
    void setGuidesVisible(bool visible);
    void setGuidesLocked(bool locked);
    bool modified() const;
    void setModified(bool modified);
    QRect bounds() const;
    bool importAnimation(const QList<QString> &files, int firstFrame, int step);
    int framesPerSecond();
    void setFramesPerSecond(int fps);
    void setFullClipRangeStartTime(int startTime);
    int fullClipRangeStartTime();
    void setFullClipRangeEndTime(int endTime);
    int fullClipRangeEndTime();
    int animationLength();
    void setPlayBackRange(int start, int stop);
    int playBackStartTime();
    int playBackEndTime();
    int currentTime();
    void setCurrentTime(int time);
    QStringList annotationTypes() const;
    QString annotationDescription(const QString &type) const;
    QByteArray annotation(const QString &type);
    void setAnnotation(const QString &type, const QString &description, const QByteArray &annotation);
    void removeAnnotation(const QString &type);

    void setAutosave(bool active);
    bool autosave();

    GuidesConfig *guidesConfig();
    void setGuidesConfig(GuidesConfig *guidesConfig);

    qreal audioLevel() const;
    void setAudioLevel(const qreal level);

    QList<QString> audioTracks() const;
    bool setAudioTracks(const QList<QString> files) const;

private:

};
