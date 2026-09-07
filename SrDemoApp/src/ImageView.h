/*
 * Copyright (c) 2026-09-05 Wuhan Wavefront Technology Co., Ltd. All rights reserved.
 * wechat:angaio/13707128443
 */
/******************************************************************************
* File        : ImageView.h
* Description : Aspect-correct image surface, single or side-by-side.
*
* Scaling is deliberately nearest-neighbour: a smooth filter here would blur
* the very pixel-level difference the demo exists to show.
******************************************************************************/
#ifndef ___ImageView_h___
#define ___ImageView_h___

#include <QWidget>
#include <QImage>
#include <QString>

class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(QWidget *parent = nullptr);

    void setSingle(const QImage &img, const QString &caption);
    void setPair(const QImage &left, const QImage &right,
                 const QString &leftCap, const QString &rightCap);
    void clear();
    void setPlaceholder(const QString &text);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void drawPanel(QPainter &p, const QRect &box, const QImage &img,
                   const QString &cap);

    QImage  m_left, m_right;
    QString m_capLeft, m_capRight;
    bool    m_pair = false;
    QString m_placeholder;
};

#endif
