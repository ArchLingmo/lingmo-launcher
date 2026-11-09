#ifndef ICONITEM_H
#define ICONITEM_H

#include <QQuickItem>
#include <QPixmap>
#include <QPointer>

class IconItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QVariant source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int paintedWidth READ paintedWidth NOTIFY paintedSizeChanged)
    Q_PROPERTY(int paintedHeight READ paintedHeight NOTIFY paintedSizeChanged)
    // Every icon in one shape: tiles clipped to it, free-form icons on a light plate
    Q_PROPERTY(bool shaped READ shaped WRITE setShaped NOTIFY shapedChanged)

public:
    explicit IconItem(QQuickItem *parent = nullptr);

    void setSource(const QVariant &source);
    QVariant source() const;

    bool shaped() const { return m_shaped; }
    void setShaped(bool shaped);

    // A theme icon at pixelSize device pixels, shaped or not, through QPixmapCache
    static QPixmap themePixmap(const QString &name, int pixelSize, bool shaped);
    static QImage shapeImage(const QImage &source, int pixelSize);

    int paintedWidth() const;
    int paintedHeight() const;

    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

    void componentComplete() override;
    void updatePolish() override;

    Q_INVOKABLE void refresh();

private:
    void loadPixmap();

signals:
    void sourceChanged();
    void paintedSizeChanged();
    void shapedChanged();

private:
    QVariant m_source;
    QPixmap m_iconPixmap;
    // What m_iconPixmap was made from: layout passes that don't change it skip the reload
    QSize m_loadedSize;
    QVariant m_loadedSource;
    bool m_textureDirty = true;
    bool m_shaped = false;
    bool m_loadedShaped = false;
};

#endif // ICONITEM_H
