/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
#include "ImageView.h"

#include <QPainter>
#include <QPaintEvent>

namespace {
const QColor kBackground(0x14, 0x17, 0x1c);
const QColor kBorder    (0x2c, 0x33, 0x3d);
const QColor kCaption   (0x9a, 0xa6, 0xb4);
const QColor kHint      (0x5c, 0x66, 0x74);
}

ImageView::ImageView(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, kBackground);
    setPalette(pal);
    setMinimumSize(560, 340);
    m_placeholder = QStringLiteral("No source open");
}

void ImageView::setSingle(const QImage &img, const QString &caption)
{
    m_left = img;  m_capLeft = caption;
    m_right = QImage(); m_capRight.clear();
    m_pair = false;
    update();
}

void ImageView::setPair(const QImage &left, const QImage &right,
                        const QString &leftCap, const QString &rightCap)
{
    m_left = left;   m_capLeft = leftCap;
    m_right = right; m_capRight = rightCap;
    m_pair = true;
    update();
}

void ImageView::clear()
{
    m_left = QImage(); m_right = QImage();
    m_capLeft.clear(); m_capRight.clear();
    m_pair = false;
    update();
}

void ImageView::setPlaceholder(const QString &text)
{
    m_placeholder = text;
    update();
}

void ImageView::drawPanel(QPainter &p, const QRect &box, const QImage &img,
                          const QString &cap)
{
    if (img.isNull() || box.width() < 8 || box.height() < 8) return;

    const int capH = cap.isEmpty() ? 0 : 24;
    const QRect area = box.adjusted(0, capH, 0, 0);

    QSize sz = img.size();
    sz.scale(area.size(), Qt::KeepAspectRatio);
    QRect target(QPoint(0, 0), sz);
    target.moveCenter(area.center());

    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(target, img);

    p.setPen(QPen(kBorder, 1));
    p.drawRect(target.adjusted(0, 0, -1, -1));

    if (capH > 0) {
        p.setPen(kCaption);
        QFont f = p.font();
        f.setPointSizeF(9.5);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
        p.setFont(f);
        p.drawText(QRect(box.left(), box.top(), box.width(), capH),
                   Qt::AlignCenter, cap);
    }
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), kBackground);

    if (m_left.isNull()) {
        p.setPen(kHint);
        QFont f = p.font(); f.setPointSizeF(11); p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, m_placeholder);
        return;
    }

    const QRect r = rect().adjusted(10, 10, -10, -10);
    if (m_pair && !m_right.isNull()) {
        const int gap = 10;
        const int w = (r.width() - gap) / 2;
        drawPanel(p, QRect(r.left(),           r.top(), w, r.height()),
                  m_left, m_capLeft);
        drawPanel(p, QRect(r.left() + w + gap, r.top(), w, r.height()),
                  m_right, m_capRight);
    } else {
        drawPanel(p, r, m_left, m_capLeft);
    }
}
