// ScopeView.h - the Outputs plot (LFO, envelope, V/oct piano roll), drawn by the GPU.
//
// Before, a QML Canvas drew the whole plot again 30 times a second. A Canvas paints with the
// CPU into an image as big as the screen area (on a phone ~1000 x 1400 pixels) and sends that
// image to the GPU every time: too slow for a phone, so frames were dropped (the lag).
//
// This item builds the lines and note blocks as triangles (a few thousand points) only when new
// data arrives (5x per second). Scrolling between packs only moves them: one number (a
// translation) per frame, which costs the GPU nothing.
//
//   head   the ESP sample number at the right edge; OutputsPage moves it on every frame
//   span   how many samples fit across (500 = 10 s at 20 ms)
//
// Qt lesson: a QQuickItem draws by returning "scene graph" nodes from updatePaintNode(). That
// function runs on the render thread while the GUI thread waits, so it may read our members.
#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Device;

class ScopeView : public QQuickItem {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QObject *source READ source WRITE setSource NOTIFY sourceChanged)   // the Device
  Q_PROPERTY(double head READ head WRITE setHead NOTIFY headChanged)
  Q_PROPERTY(int span READ span WRITE setSpan NOTIFY spanChanged)
  Q_PROPERTY(double lineWidth MEMBER lineWidth_ NOTIFY styleChanged)
  // The note blocks big enough for a name: [{k0, k1, y, h, text}] (k = sample numbers).
  // QML puts a Text on each (the scene graph has no easy text from C++).
  Q_PROPERTY(QVariantList labels READ labels NOTIFY labelsChanged)

 public:
  explicit ScopeView(QQuickItem *parent = nullptr);

  QObject *source() const;
  void setSource(QObject *d);
  double head() const { return head_; }
  void setHead(double h);
  int span() const { return span_; }
  void setSpan(int s);
  QVariantList labels() const { return labels_; }


 signals:
  void sourceChanged();
  void headChanged();
  void spanChanged();
  void styleChanged();
  void labelsChanged();

 protected:
  QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
  void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

 private:
  struct Segment {   // a run of equal V/oct notes with the envelope open (on) or closed
    int i0, i1;      // indices into the arrays, i1 = one past the end
    int note;        // MIDI
    bool on;
  };
  void onData();
  double pps() const { return span_ > 1 ? width() / (span_ - 1) : 1.0; }  // pixels per sample
  void laneY(int lane, double &top, double &h) const;

  QPointer<Device> dev_;
  double head_ = 0;
  int span_ = 500;
  double lineWidth_ = 1.5;
  QColor lfoColor_{0xf0, 0xa0, 0x20}, envColor_{0x59, 0xff, 0x7a}, voctColor_{0x4e, 0xa1, 0xff};

  // copied from Device in onData (GUI thread), used by updatePaintNode
  QList<Segment> segs_;
  int nlo_ = 0, nhi_ = 0;
  qint64 base_ = 0;            // ESP sample number of array index 0
  bool geomDirty_ = true;
  QVariantList labels_;
};
