#include "iconitem.h"
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QQuickWindow>
#include <QApplication>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

IconItem::IconItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setSmooth(true);
}

void IconItem::setSource(const QVariant &source)
{
    if (source == m_source)
        return;

    m_source.clear();
    m_source = source;

    // If the QIcon was created with QIcon::fromTheme(), try to load it as svg.
    if (source.canConvert<QIcon>() && source.value<QIcon>().name().isEmpty()) {
        m_source = source.value<QIcon>().name();
    }

    loadPixmap();
    emit sourceChanged();
}

void IconItem::setShaped(bool shaped)
{
    if (shaped == m_shaped)
        return;
    m_shaped = shaped;
    polish();
    emit shapedChanged();
}

// HarmonyOS-like icon grid: every icon takes the same rounded tile. An icon that is
// already a tile (a square or rounded square filling its box) is clipped to the
// common shape; anything else (round logos, documents, mascots) sits centred on a
// light plate of that shape.
QImage IconItem::shapeImage(const QImage &source, int pixelSize)
{
    const QImage src = source.convertToFormat(QImage::Format_ARGB32);
    QRect box;
    int left = src.width(), top = src.height(), right = -1, bottom = -1;
    for (int y = 0; y < src.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            if (qAlpha(line[x]) > 16) {
                left = qMin(left, x); right = qMax(right, x);
                top = qMin(top, y); bottom = qMax(bottom, y);
            }
        }
    }
    if (right < 0)
        return QImage();
    box = QRect(QPoint(left, top), QPoint(right, bottom));

    int solid = 0;
    for (int y = box.top(); y <= box.bottom(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = box.left(); x <= box.right(); ++x)
            solid += qAlpha(line[x]) > 200;
    }
    const qreal ratio = qreal(box.width()) / box.height();
    // A rounded square covers ~96% of its box, a circle 78.5%
    const bool tile = ratio > 0.9 && ratio < 1.1 && box.width() >= src.width() * 0.6
            && solid >= 0.86 * box.width() * box.height();

    QImage out(pixelSize, pixelSize, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    const qreal margin = pixelSize * 0.06;
    const QRectF tileRect(margin, margin, pixelSize - 2 * margin, pixelSize - 2 * margin);
    const qreal radius = tileRect.width() * 0.225;
    QPainterPath shape;
    shape.addRoundedRect(tileRect, radius, radius);

    if (tile) {
        p.setClipPath(shape);
        p.drawImage(tileRect, src, box);
    } else {
        QLinearGradient plate(tileRect.topLeft(), tileRect.bottomLeft());
        plate.setColorAt(0, QColor(255, 255, 255));
        plate.setColorAt(1, QColor(236, 238, 242));
        p.fillPath(shape, plate);
        p.setPen(QPen(QColor(0, 0, 0, 18), 1));
        p.drawPath(shape);

        // The icon itself, at most two thirds of the tile, its aspect kept
        const qreal room = tileRect.width() * 0.66;
        const qreal scale = room / qMax(box.width(), box.height());
        const QSizeF size(box.width() * scale, box.height() * scale);
        const QRectF target(tileRect.center().x() - size.width() / 2, tileRect.center().y() - size.height() / 2,
                            size.width(), size.height());
        p.setClipPath(shape);
        p.drawImage(target, src, box);
    }
    p.end();
    return out;
}

QPixmap IconItem::themePixmap(const QString &name, int pixelSize, bool shaped)
{
    const QString key = QStringLiteral("lingmo-launcher:%1:%2:%3").arg(name).arg(pixelSize).arg(shaped);
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap))
        return pixmap;

    const QIcon icon = QIcon::fromTheme(name, QIcon::fromTheme(QStringLiteral("application-x-desktop")));
    if (shaped) {
        // Rendered at twice the size and scaled down: crisp after cropping the margins
        pixmap = QPixmap::fromImage(shapeImage(icon.pixmap(QSize(pixelSize * 2, pixelSize * 2)).toImage(), pixelSize));
    }
    if (pixmap.isNull())
        pixmap = icon.pixmap(QSize(pixelSize, pixelSize));
    QPixmapCache::insert(key, pixmap);
    return pixmap;
}

QVariant IconItem::source() const
{
    return m_source;
}

int IconItem::paintedWidth() const
{
    return boundingRect().size().toSize().width();
}

int IconItem::paintedHeight() const
{
    return boundingRect().size().toSize().height();
}

QSGNode *IconItem::updatePaintNode(QSGNode *oldNode, QQuickItem::UpdatePaintNodeData *updatePaintNodeData)
{
    Q_UNUSED(updatePaintNodeData);

    if (m_iconPixmap.isNull() || width() == 0.0 || height() == 0.0) {
        delete oldNode;
        return nullptr;
    }

    QSGSimpleTextureNode *textureNode = dynamic_cast<QSGSimpleTextureNode *>(oldNode);

    if (!textureNode) {
        delete oldNode;
        textureNode = new QSGSimpleTextureNode;
        textureNode->setOwnsTexture(true);
        m_textureDirty = true;
    }
    // A new pixmap (other icon, other size) needs a new texture
    if (m_textureDirty) {
        textureNode->setTexture(window()->createTextureFromImage(m_iconPixmap.toImage(), QQuickWindow::TextureCanUseAtlas));
        m_textureDirty = false;
    }

    textureNode->setFiltering(smooth() ? QSGTexture::Linear : QSGTexture::Nearest);

    // Size changed
    const QSize newSize = QSize(paintedWidth(), paintedHeight());
    const QRect destRect(QPointF(boundingRect().center() - QPointF(newSize.width(), newSize.height()) / 2).toPoint(), newSize);
    textureNode->setRect(destRect);

    return textureNode;
}

void IconItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    polish();
    update();

    QQuickItem::geometryChange(newGeometry, oldGeometry);
}

void IconItem::componentComplete()
{
    QQuickItem::componentComplete();

    polish();
}

void IconItem::updatePolish()
{
    QQuickItem::updatePolish();

    loadPixmap();
}

void IconItem::refresh()
{
    polish();
}

void IconItem::loadPixmap()
{
    if (!isComponentComplete())
        return;

    int size = qMin(qRound(width()), qRound(height()));

    if (size <= 0) {
        // Clear pixmap
        m_iconPixmap = QPixmap();
        update();
        return;
    }

    const QSize pixelSize(size * qApp->devicePixelRatio(), size * qApp->devicePixelRatio());
    if (!m_iconPixmap.isNull() && m_loadedSize == pixelSize && m_loadedSource == m_source
            && m_loadedShaped == m_shaped)
        return;
    m_loadedSize = pixelSize;
    m_loadedSource = m_source;
    m_loadedShaped = m_shaped;
    m_iconPixmap = QPixmap();

    QString sourceString = m_source.toString();

    if (m_source.canConvert<QIcon>()) {
        QIcon icon = m_source.value<QIcon>();
        m_iconPixmap = icon.pixmap(QSize(size * qApp->devicePixelRatio(),
                                         size * qApp->devicePixelRatio()));
        m_iconPixmap.setDevicePixelRatio(qApp->devicePixelRatio());
    } else if (m_source.canConvert<QImage>()) {
        QImage image = m_source.value<QImage>();
        m_iconPixmap = QPixmap::fromImage(image).scaled(QSize(size * qApp->devicePixelRatio(),
                                                              size * qApp->devicePixelRatio()));
        m_iconPixmap.setDevicePixelRatio(qApp->devicePixelRatio());
    } else if (!m_source.isNull()) {
        QString localFile;

        if (sourceString.startsWith("file:"))
            localFile = QUrl(sourceString).toLocalFile();
        else if (sourceString.startsWith('/'))
            localFile = sourceString;
        else if (sourceString.startsWith("qrc:/"))
            localFile = sourceString.remove(0, 3);
        else if (sourceString.startsWith(":/"))
            localFile = sourceString;

        if (!localFile.isEmpty()) {
            m_iconPixmap.load(localFile);
            if (!m_iconPixmap.isNull()) {
                m_iconPixmap = m_iconPixmap.scaled(QSize(size * qApp->devicePixelRatio(),
                                                         size * qApp->devicePixelRatio()));
                m_iconPixmap.setDevicePixelRatio(qApp->devicePixelRatio());
            }
        }
    }

    if (m_iconPixmap.isNull()) {
        m_iconPixmap = themePixmap(sourceString, pixelSize.width(), m_shaped);
        m_iconPixmap.setDevicePixelRatio(qApp->devicePixelRatio());
    } else if (m_shaped) {
        const QImage shapedImage = shapeImage(m_iconPixmap.toImage(), pixelSize.width());
        if (!shapedImage.isNull()) {
            m_iconPixmap = QPixmap::fromImage(shapedImage);
            m_iconPixmap.setDevicePixelRatio(qApp->devicePixelRatio());
        }
    }

    m_textureDirty = true;
    update();
}
